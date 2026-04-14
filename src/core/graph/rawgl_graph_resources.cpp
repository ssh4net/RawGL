// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_graph_resources.h"

#include "image_utils.h"
#include "mesh_io.h"
#include "rawgl_graph_shared.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace rawgl {
namespace {

struct LoadedTextureData {
    bool valid            = false;
    int width             = 0;
    int height            = 0;
    int channels          = 0;
    int alphaChannel      = -1;
    GLenum internalFormat = 0;
    GLenum type           = 0;
    OIIO::TypeDesc format;
    void* data = nullptr;
};

static std::string
build_texture_cache_key(const std::string& path, const std::vector<GraphAttribute>& attributes)
{
    std::ostringstream stream;
    stream << "file:" << path;
    for (const GraphAttribute& attribute : attributes) {
        stream << '\x1F' << attribute.name << '=' << attribute.value;
    }
    return stream.str();
}

static std::string
build_mesh_cache_key(const GraphMeshDefinition& mesh)
{
    bool assumeTriangles = true;
    for (const GraphAttribute& attribute : mesh.parameters) {
        if (attribute.name == "tris") {
            assumeTriangles = (attribute.value == "true");
            break;
        }
    }

    std::ostringstream stream;
    stream << "file:" << mesh.path << '\x1F' << "tris=" << (assumeTriangles ? 1 : 0);
    return stream.str();
}

static void
resolve_texture_storage(LoadedTextureData& texture)
{
    const GLenum formats[5][4] = {
        { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
        { GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 },
        { GL_R32UI, GL_RG32UI, GL_RGB32UI, GL_RGBA32UI },
        { GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F },
        { GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F },
    };

    if (texture.channels < 1 || texture.channels > 4) {
        throw std::runtime_error("Unsupported image channel count");
    }

    switch (texture.format.basetype) {
    case OIIO::TypeDesc::UINT8:
        texture.internalFormat = formats[0][texture.channels - 1];
        texture.type           = GL_UNSIGNED_BYTE;
        break;
    case OIIO::TypeDesc::UINT16:
        texture.internalFormat = formats[1][texture.channels - 1];
        texture.type           = GL_UNSIGNED_SHORT;
        break;
    case OIIO::TypeDesc::UINT32:
        texture.internalFormat = formats[2][texture.channels - 1];
        texture.type           = GL_UNSIGNED_INT;
        break;
    case OIIO::TypeDesc::HALF:
        texture.internalFormat = formats[3][texture.channels - 1];
        texture.type           = GL_HALF_FLOAT;
        break;
    case OIIO::TypeDesc::FLOAT:
        texture.internalFormat = formats[4][texture.channels - 1];
        texture.type           = GL_FLOAT;
        break;
    default: throw std::runtime_error("Unsupported image type");
    }
}

static LoadedTextureData
load_texture_file(const std::string& path, const std::vector<GraphAttribute>& attributes)
{
    LoadedTextureData texture;
    std::map<std::string, std::string> imageAttributes;
    for (const GraphAttribute& attribute : attributes) {
        imageAttributes.insert({ attribute.name, attribute.value });
    }

    if (!image_utils::load_image(path, imageAttributes, texture.width, texture.height, texture.data, texture.channels,
                                 texture.alphaChannel, texture.format)) {
        return texture;
    }

    resolve_texture_storage(texture);
    texture.valid = true;
    return texture;
}

static void
release_loaded_texture_data(LoadedTextureData& texture)
{
    if (texture.data != nullptr) {
        free(texture.data);
        texture.data = nullptr;
    }
}

static std::shared_ptr<Texture>
load_cached_texture_resource(const RawGLContextState& contextState,
                             const std::string& path,
                             const std::vector<GraphAttribute>& attributes)
{
    const std::string cacheKey = build_texture_cache_key(path, attributes);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.textureCacheMutex);
        auto cacheIt = contextState.textureCache.find(cacheKey);
        if (cacheIt != contextState.textureCache.end()) {
            return cacheIt->second;
        }
    }

    LoadedTextureData textureData = load_texture_file(path, attributes);
    if (!textureData.valid) {
        throw std::runtime_error("Failed to load an input texture.");
    }

    std::shared_ptr<Texture> texture =
        std::make_shared<Texture>(textureData.width, textureData.height, textureData.internalFormat, textureData.type,
                                  textureData.data, textureData.alphaChannel);
    release_loaded_texture_data(textureData);

    std::unique_lock<std::shared_mutex> writeLock(contextState.textureCacheMutex);
    auto [cacheIt, inserted] = contextState.textureCache.insert({ cacheKey, texture });
    if (!inserted) {
        return cacheIt->second;
    }

    return texture;
}

static std::shared_ptr<SequenceSharedMeshData>
load_cached_mesh_resource(const RawGLContextState& contextState, const GraphMeshDefinition& mesh)
{
    const std::string cacheKey = build_mesh_cache_key(mesh);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.meshCacheMutex);
        auto cacheIt = contextState.meshCache.find(cacheKey);
        if (cacheIt != contextState.meshCache.end()) {
            return cacheIt->second;
        }
    }

    bool assumeTriangles = true;
    for (const GraphAttribute& attribute : mesh.parameters) {
        if (attribute.name == "tris") {
            assumeTriangles = (attribute.value == "true");
            break;
        }
    }

    std::unique_ptr<TriMesh> triMesh(parse_file_with_miniply(mesh.path.c_str(), assumeTriangles));
    if (!triMesh) {
        throw std::runtime_error("Failed to load mesh");
    }

    std::shared_ptr<SequenceSharedMeshData> sharedMesh = std::make_shared<SequenceSharedMeshData>();
    sharedMesh->verts.assign(triMesh->pos, triMesh->pos + triMesh->numVerts * 3);
    if (triMesh->uv != nullptr) {
        sharedMesh->texcoords.assign(triMesh->uv, triMesh->uv + triMesh->numVerts * 2);
    }
    if (triMesh->normal != nullptr) {
        sharedMesh->normals.assign(triMesh->normal, triMesh->normal + triMesh->numVerts * 3);
    }
    if (triMesh->color != nullptr) {
        sharedMesh->colors.assign(triMesh->color, triMesh->color + triMesh->numVerts * 4);
    }
    sharedMesh->indices.assign(triMesh->indices, triMesh->indices + triMesh->numIndices);
    sharedMesh->vrtSize  = static_cast<GLsizei>(triMesh->numVerts * 3 * sizeof(float));
    sharedMesh->texSize  = static_cast<GLsizei>(triMesh->numVerts * 2 * sizeof(float));
    sharedMesh->nrmSize  = static_cast<GLsizei>(triMesh->numVerts * 3 * sizeof(float));
    sharedMesh->clrSize  = static_cast<GLsizei>(triMesh->numVerts * 4 * sizeof(unsigned char));
    sharedMesh->idxSize  = static_cast<GLsizei>(triMesh->numIndices * sizeof(unsigned int));
    sharedMesh->numIndxs = static_cast<GLsizei>(triMesh->numIndices);

    std::unique_lock<std::shared_mutex> writeLock(contextState.meshCacheMutex);
    auto [cacheIt, inserted] = contextState.meshCache.insert({ cacheKey, sharedMesh });
    if (!inserted) {
        return cacheIt->second;
    }

    return sharedMesh;
}

static std::shared_ptr<SequenceSharedGpuMesh>
load_cached_gpu_mesh_resource(const RawGLContextState& contextState,
                              const GraphMeshDefinition& mesh,
                              const std::shared_ptr<SequenceSharedMeshData>& sharedMesh)
{
    const std::string cacheKey = build_mesh_cache_key(mesh);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.meshGpuCacheMutex);
        auto cacheIt = contextState.meshGpuCache.find(cacheKey);
        if (cacheIt != contextState.meshGpuCache.end()) {
            return cacheIt->second;
        }
    }

    std::shared_ptr<SequenceSharedGpuMesh> sharedGpuMesh = Sequence_CreateSharedGpuMesh(*sharedMesh);

    std::unique_lock<std::shared_mutex> writeLock(contextState.meshGpuCacheMutex);
    auto [cacheIt, inserted] = contextState.meshGpuCache.insert({ cacheKey, sharedGpuMesh });
    if (!inserted) {
        return cacheIt->second;
    }

    return sharedGpuMesh;
}

}  // namespace

SequenceExecutionInputOverride
build_sequence_execution_input_override(const RawGLContextState& contextState, const GraphInputOverride& inputOverride)
{
    SequenceExecutionInputOverride sequenceOverride;
    sequenceOverride.passIndex = inputOverride.passIndex;
    sequenceOverride.inputName = inputOverride.name;

    switch (inputOverride.sourceKind) {
    case GraphInputSourceKind::intValues:
        sequenceOverride.kind      = SequenceExecutionInputOverrideKind::intValues;
        sequenceOverride.intValues = inputOverride.intValues;
        break;
    case GraphInputSourceKind::uintValues:
        sequenceOverride.kind       = SequenceExecutionInputOverrideKind::uintValues;
        sequenceOverride.uintValues = inputOverride.uintValues;
        break;
    case GraphInputSourceKind::floatValues:
        sequenceOverride.kind        = SequenceExecutionInputOverrideKind::floatValues;
        sequenceOverride.floatValues = inputOverride.floatValues;
        break;
    case GraphInputSourceKind::doubleValues:
        sequenceOverride.kind         = SequenceExecutionInputOverrideKind::doubleValues;
        sequenceOverride.doubleValues = inputOverride.doubleValues;
        break;
    case GraphInputSourceKind::textureFile:
        sequenceOverride.kind    = SequenceExecutionInputOverrideKind::texture;
        sequenceOverride.texture =
            load_cached_texture_resource(contextState, inputOverride.texturePath, inputOverride.attributes);
        break;
    default:
        throw std::runtime_error("input override (" + inputOverride.name + "): unsupported override kind");
    }

    return sequenceOverride;
}

RawGLGraphState::ResourcePlan
build_resource_plan(const RawGLContextState& contextState, const RawGLGraphState::ValidatedGraph& validatedGraph)
{
    RawGLGraphState::ResourcePlan resourcePlan;
    resourcePlan.verbosity = validatedGraph.verbosity;
    resourcePlan.passes.reserve(validatedGraph.passes.size());

    for (const RawGLGraphState::ValidatedPass& validatedPass : validatedGraph.passes) {
        RawGLGraphState::ResourcePass resourcePass;
        resourcePass.program          = validatedPass.program;
        resourcePass.shaderInterface  = validatedPass.shaderInterface;
        resourcePass.isCompute        = (validatedPass.definition.programKind == ShaderProgramKind::compute);
        resourcePass.size[0]          = validatedPass.definition.sizeX;
        resourcePass.size[1]          = validatedPass.definition.sizeY;
        resourcePass.workGroupSize[0] = validatedPass.definition.workGroupSizeX;
        resourcePass.workGroupSize[1] = validatedPass.definition.workGroupSizeY;
        std::memcpy(resourcePass.clearColor, validatedPass.definition.clearColor, sizeof(resourcePass.clearColor));
        apply_cull_parameters(resourcePass.cullMode, validatedPass.definition.cullParameters);
        resourcePass.inputs         = validatedPass.definition.inputs;
        resourcePass.atomicCounters = validatedPass.definition.atomicCounters;
        resourcePass.outputs        = validatedPass.definition.outputs;
        resourcePass.meshes         = validatedPass.definition.meshes;

        for (const GraphInputDefinition& inputDefinition : resourcePass.inputs) {
            if (inputDefinition.sourceKind == GraphInputSourceKind::textureFile) {
                const std::string cacheKey = build_texture_cache_key(inputDefinition.texturePath, inputDefinition.attributes);
                resourcePlan.sharedTextures[cacheKey] =
                    load_cached_texture_resource(contextState, inputDefinition.texturePath, inputDefinition.attributes);
            }
        }

        for (const GraphMeshDefinition& meshDefinition : resourcePass.meshes) {
            if (meshDefinition.sourceKind != GraphMeshSourceKind::file) {
                continue;
            }

            const std::string cacheKey = build_mesh_cache_key(meshDefinition);
            std::shared_ptr<SequenceSharedMeshData> sharedMesh = load_cached_mesh_resource(contextState, meshDefinition);
            resourcePlan.sharedMeshes[cacheKey]    = sharedMesh;
            resourcePlan.sharedGpuMeshes[cacheKey] = load_cached_gpu_mesh_resource(contextState, meshDefinition, sharedMesh);
        }

        resourcePlan.passes.push_back(std::move(resourcePass));
    }

    return resourcePlan;
}

}  // namespace rawgl
