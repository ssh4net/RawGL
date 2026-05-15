// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_resources.h"

#include "mesh_io.h"
#include "graph_shared.h"

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace rawgl {
namespace {

static void
append_default_normals(std::vector<float>& normals, const size_t vertexCount)
{
    normals.reserve(vertexCount * 3u);
    for (size_t vertexIndex = 0u; vertexIndex < vertexCount; ++vertexIndex) {
        normals.push_back(0.0f);
        normals.push_back(0.0f);
        normals.push_back(1.0f);
    }
}

static size_t
byte_size_for_gl_type(const GLenum type)
{
    switch (type) {
    case GL_UNSIGNED_BYTE: return 1u;
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT: return 2u;
    case GL_UNSIGNED_INT:
    case GL_FLOAT: return 4u;
    default: break;
    }

    return 0u;
}

static std::shared_ptr<SequenceSharedMeshData>
create_shared_mesh_from_host_mesh(const HostMeshData& hostMesh)
{
    const size_t vertexCount = hostMesh.positions.size() / 3u;

    std::shared_ptr<SequenceSharedMeshData> sharedMesh = std::make_shared<SequenceSharedMeshData>();
    sharedMesh->verts = hostMesh.positions;
    sharedMesh->indices = hostMesh.indices;

    if (hostMesh.texcoords.empty()) {
        sharedMesh->texcoords.assign(vertexCount * 2u, 0.0f);
    } else {
        sharedMesh->texcoords = hostMesh.texcoords;
    }

    if (hostMesh.normals.empty()) {
        append_default_normals(sharedMesh->normals, vertexCount);
    } else {
        sharedMesh->normals = hostMesh.normals;
    }

    if (hostMesh.colors.empty()) {
        sharedMesh->colors.assign(vertexCount * 4u, 255u);
    } else {
        sharedMesh->colors = hostMesh.colors;
    }

    if (hostMesh.id0.empty()) {
        sharedMesh->materialIds.assign(vertexCount, 0u);
    } else {
        sharedMesh->materialIds = hostMesh.id0;
    }

    sharedMesh->attributes.reserve(hostMesh.attributes.size());
    for (const HostMeshAttribute& sourceAttribute : hostMesh.attributes) {
        SequenceSharedMeshAttribute attribute;
        attribute.name       = sourceAttribute.name;
        attribute.location   = sourceAttribute.location;
        attribute.components = static_cast<GLint>(sourceAttribute.components);
        attribute.type       = sourceAttribute.glType;
        attribute.integer    = sourceAttribute.integer;
        attribute.stride     = static_cast<GLsizei>(sourceAttribute.components * byte_size_for_gl_type(sourceAttribute.glType));
        attribute.bytes      = sourceAttribute.bytes;
        sharedMesh->attributes.push_back(std::move(attribute));
    }

    sharedMesh->vrtSize  = static_cast<GLsizei>(sharedMesh->verts.size() * sizeof(float));
    sharedMesh->texSize  = static_cast<GLsizei>(sharedMesh->texcoords.size() * sizeof(float));
    sharedMesh->nrmSize  = static_cast<GLsizei>(sharedMesh->normals.size() * sizeof(float));
    sharedMesh->clrSize  = static_cast<GLsizei>(sharedMesh->colors.size() * sizeof(uint8_t));
    sharedMesh->matSize  = static_cast<GLsizei>(sharedMesh->materialIds.size() * sizeof(uint32_t));
    sharedMesh->idxSize  = static_cast<GLsizei>(sharedMesh->indices.size() * sizeof(uint32_t));
    sharedMesh->numIndxs = static_cast<GLsizei>(sharedMesh->indices.size());
    return sharedMesh;
}

static int
channel_count_for_internal_format(const GLenum internalFormat)
{
    switch (internalFormat) {
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
    case GL_R8_SNORM:
    case GL_R16:
    case GL_R16I:
    case GL_R16UI:
    case GL_R16_SNORM:
    case GL_R16F:
    case GL_R32I:
    case GL_R32UI:
    case GL_R32F: return 1;
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_RG8_SNORM:
    case GL_RG16:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG16_SNORM:
    case GL_RG16F:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG32F: return 2;
    case GL_RGB8:
    case GL_RGB8I:
    case GL_RGB8UI:
    case GL_RGB8_SNORM:
    case GL_RGB10_A2:
    case GL_R11F_G11F_B10F:
    case GL_RGB16:
    case GL_RGB16I:
    case GL_RGB16UI:
    case GL_RGB16_SNORM:
    case GL_RGB16F:
    case GL_RGB32I:
    case GL_RGB32UI:
    case GL_RGB32F:
    case GL_SRGB8: return 3;
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_RGBA8_SNORM:
    case GL_RGBA16:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA16_SNORM:
    case GL_RGBA16F:
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA32F:
    case GL_SRGB8_ALPHA8: return 4;
    default: break;
    }

    return 0;
}

static void
validate_host_image_data(const HostImageData& hostImage, const std::string& context)
{
    if (hostImage.width <= 0 || hostImage.height <= 0) {
        throw std::runtime_error(context + ": host texture dimensions must be > 0");
    }
    if (hostImage.channels <= 0 || hostImage.channels > 4) {
        throw std::runtime_error(context + ": host texture channel count must be between 1 and 4");
    }
    if (hostImage.alphaChannel >= hostImage.channels) {
        throw std::runtime_error(context + ": host texture alpha channel exceeds the channel count");
    }

    const int inferredChannels = channel_count_for_internal_format(hostImage.glInternalFormat);
    if (inferredChannels == 0) {
        throw std::runtime_error(context + ": unsupported OpenGL internal format");
    }
    if (inferredChannels != hostImage.channels) {
        throw std::runtime_error(context + ": host texture channel count does not match the OpenGL internal format");
    }

    const size_t bytesPerComponent = byte_size_for_gl_type(hostImage.glType);
    if (bytesPerComponent == 0u) {
        throw std::runtime_error(context + ": unsupported OpenGL host texture element type");
    }

    const size_t expectedByteCount = static_cast<size_t>(hostImage.width) * static_cast<size_t>(hostImage.height)
                                     * static_cast<size_t>(hostImage.channels) * bytesPerComponent;
    if (hostImage.bytes.size() != expectedByteCount) {
        throw std::runtime_error(context + ": host texture byte size does not match width, height, channels, and type");
    }
}

static std::shared_ptr<SequenceSharedMeshData>
load_cached_mesh_resource(const RawGLContextState& contextState, const GraphMeshDefinition& mesh)
{
    if (mesh.sourceKind == GraphMeshSourceKind::hostMesh) {
        if (!mesh.hostMesh) {
            throw std::runtime_error("Failed to load host mesh");
        }
        return create_shared_mesh_from_host_mesh(*mesh.hostMesh);
    }

    const std::string cacheKey = build_mesh_resource_key(mesh);

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

    std::unique_ptr<TriMesh> triMesh(parse_mesh_file(mesh.path.c_str(), assumeTriangles));
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
    if (triMesh->materialId != nullptr) {
        sharedMesh->materialIds.assign(triMesh->materialId, triMesh->materialId + triMesh->numVerts);
    } else {
        sharedMesh->materialIds.assign(triMesh->numVerts, 0u);
    }
    sharedMesh->indices.assign(triMesh->indices, triMesh->indices + triMesh->numIndices);
    sharedMesh->vrtSize  = static_cast<GLsizei>(triMesh->numVerts * 3 * sizeof(float));
    sharedMesh->texSize  = static_cast<GLsizei>(triMesh->numVerts * 2 * sizeof(float));
    sharedMesh->nrmSize  = static_cast<GLsizei>(triMesh->numVerts * 3 * sizeof(float));
    sharedMesh->clrSize  = static_cast<GLsizei>(triMesh->numVerts * 4 * sizeof(unsigned char));
    sharedMesh->matSize  = static_cast<GLsizei>(triMesh->numVerts * sizeof(uint32_t));
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
    if (mesh.sourceKind == GraphMeshSourceKind::hostMesh) {
        return Sequence_CreateSharedGpuMesh(*sharedMesh);
    }

    const std::string cacheKey = build_mesh_resource_key(mesh);

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

std::shared_ptr<Texture>
create_host_texture_resource(const HostImageData& hostImage, const std::string& context)
{
    validate_host_image_data(hostImage, context);

    return std::make_shared<Texture>(hostImage.width,
                                     hostImage.height,
                                     hostImage.glInternalFormat,
                                     hostImage.glType,
                                     hostImage.bytes.empty() ? nullptr : hostImage.bytes.data(),
                                     hostImage.alphaChannel);
}

SequenceExecutionInputOverride
build_sequence_execution_input_override(const GraphInputOverride& inputOverride)
{
    SequenceExecutionInputOverride sequenceOverride;
    sequenceOverride.passIndex = inputOverride.passIndex;
    sequenceOverride.inputName =
        build_addressed_resource_name(inputOverride.name, inputOverride.usesArrayElement, inputOverride.arrayElement);
    sequenceOverride.usesArrayElement = inputOverride.usesArrayElement;
    sequenceOverride.arrayElement     = inputOverride.arrayElement;

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
    case GraphInputSourceKind::hostTexture:
        if (!inputOverride.hostTexture) {
            throw std::runtime_error("input override (" + inputOverride.name + "): host texture payload is missing");
        }
        sequenceOverride.kind    = SequenceExecutionInputOverrideKind::texture;
        sequenceOverride.texture = create_host_texture_resource(*inputOverride.hostTexture,
                                                                "input override (" + inputOverride.name + ")");
        break;
    default:
        throw std::runtime_error("input override (" + inputOverride.name + "): unsupported override kind");
    }

    return sequenceOverride;
}

SequenceExecutionMeshUpdate
build_sequence_execution_mesh_update(const GraphMeshUpdate& meshUpdate)
{
    SequenceExecutionMeshUpdate sequenceUpdate;
    sequenceUpdate.usesPassIndex = meshUpdate.usesPassIndex;
    sequenceUpdate.passIndex = meshUpdate.passIndex;
    sequenceUpdate.meshName = meshUpdate.name;
    sequenceUpdate.positions = meshUpdate.positions;
    sequenceUpdate.normals = meshUpdate.normals;
    return sequenceUpdate;
}

SequenceExecutionMeshOverride
build_sequence_execution_mesh_override(const GraphMeshOverride& meshOverride)
{
    if (!meshOverride.hostMesh) {
        throw std::runtime_error("mesh override (" + meshOverride.name + "): host mesh payload is missing");
    }

    SequenceExecutionMeshOverride sequenceOverride;
    sequenceOverride.usesPassIndex = meshOverride.usesPassIndex;
    sequenceOverride.passIndex = meshOverride.passIndex;
    sequenceOverride.meshName = meshOverride.name;
    sequenceOverride.mesh = create_shared_mesh_from_host_mesh(*meshOverride.hostMesh);
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

        for (const GraphMeshDefinition& meshDefinition : resourcePass.meshes) {
            if (meshDefinition.sourceKind != GraphMeshSourceKind::file
                && meshDefinition.sourceKind != GraphMeshSourceKind::hostMesh) {
                continue;
            }

            const std::string cacheKey = build_mesh_resource_key(meshDefinition);
            if (resourcePlan.sharedMeshes.find(cacheKey) != resourcePlan.sharedMeshes.end()) {
                continue;
            }

            std::shared_ptr<SequenceSharedMeshData> sharedMesh = load_cached_mesh_resource(contextState, meshDefinition);
            resourcePlan.sharedMeshes[cacheKey]    = sharedMesh;
            resourcePlan.sharedGpuMeshes[cacheKey] = load_cached_gpu_mesh_resource(contextState, meshDefinition, sharedMesh);
        }

        resourcePlan.passes.push_back(std::move(resourcePass));
    }

    return resourcePlan;
}

}  // namespace rawgl
