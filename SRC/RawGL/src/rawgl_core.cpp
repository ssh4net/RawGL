// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_core.h"

#include "common.h"
#include "gl_program_manager.h"
#include "image_utils.h"
#include "log.h"
#include "mesh_io.h"
#include "opengl_utils.h"
#include "sequence.h"
#include "timer.h"

#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

const char* APP_NAME    = "RawGL";
const char* APP_AUTHOR  = "Erium Vladlen";
const int APP_VERSION[] = { 2, 0, 0 };

namespace rawgl {

struct RawGLContextState {
    struct CachedShaderInterface {
        std::shared_ptr<GLProgram> program;
        ShaderInterface shaderInterface;
    };

    OpenGLHandle glHandle;
    mutable std::shared_mutex shaderCacheMutex;
    mutable std::map<std::string, CachedShaderInterface> shaderCache;
    mutable std::shared_mutex textureCacheMutex;
    mutable std::map<std::string, std::shared_ptr<Texture>> textureCache;
    mutable std::shared_mutex meshCacheMutex;
    mutable std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> meshCache;
    mutable std::shared_mutex meshGpuCacheMutex;
    mutable std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> meshGpuCache;
};

struct RawGLGraphState {
    struct ValidatedPass {
        GraphPassDefinition definition;
        std::shared_ptr<GLProgram> program;
        ShaderInterface shaderInterface;
    };

    struct ValidatedGraph {
        int verbosity = 3;
        std::vector<ValidatedPass> passes;
    };

    struct ResourcePass {
        std::shared_ptr<GLProgram> program;
        ShaderInterface shaderInterface;
        bool isCompute = false;
        int size[2] { 512, 512 };
        int workGroupSize[2] { 16, 16 };
        float clearColor[4] { 0.0f, 0.0f, 0.0f, 0.0f };
        Pass::CullMode cullMode { GL_CW, GL_BACK, true };
        std::vector<GraphInputDefinition> inputs;
        std::vector<GraphAtomicCounterDefinition> atomicCounters;
        std::vector<GraphOutputDefinition> outputs;
        std::vector<GraphMeshDefinition> meshes;
    };

    struct ResourcePlan {
        int verbosity = 3;
        std::vector<ResourcePass> passes;
        std::map<std::string, std::shared_ptr<Texture>> sharedTextures;
        std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> sharedMeshes;
        std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> sharedGpuMeshes;
    };

    struct ExecutionPass {
        size_t passIndex = 0;
        bool isCompute = false;
        std::vector<size_t> dependencyPassIndices;
        std::vector<std::string> inputNames;
        std::vector<std::string> outputNames;
    };

    struct PersistentInputBinding {
        size_t passIndex = 0;
        std::string inputName;
        std::string persistentTextureName;
    };

    struct PersistentOutputBinding {
        size_t passIndex = 0;
        std::string outputName;
        std::string persistentTextureName;
    };

    struct PersistentAtomicCounterBinding {
        size_t passIndex = 0;
        std::string counterName;
        std::string persistentCounterName;
    };

    struct ExecutionPlan {
        SequenceRuntimeConfig sequenceRuntimeConfig;
        std::vector<ExecutionPass> passes;
        std::vector<PersistentInputBinding> persistentInputs;
        std::vector<PersistentOutputBinding> persistentOutputs;
        std::vector<PersistentAtomicCounterBinding> persistentAtomicCounters;
    };

    ValidatedGraph validatedGraph;
    ResourcePlan resourcePlan;
    ExecutionPlan executionPlan;
    std::map<std::string, std::shared_ptr<Texture>> persistentTextures;
    std::map<std::string, std::vector<GLuint>> persistentAtomicCounters;
    std::unique_ptr<Sequence> sequence;
};

namespace {

struct GraphTranslationState {
    GraphPassDefinition* currentPass     = nullptr;
    GraphInputDefinition* currentInput   = nullptr;
    GraphOutputDefinition* currentOutput = nullptr;
    std::shared_ptr<GLProgram> currentProgram;
    std::string previousFormat = "rgb32f";
    int previousChannels       = 3;
    int previousAlphaChannel   = -1;
    int previousBits           = 16;
};

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

static ShaderResourceInfo
make_resource_info(const std::string& name, const GLProgramUniform& uniform)
{
    ShaderResourceInfo info;
    info.name     = name;
    info.typeName = uniform.type_name;
    info.location = uniform.location;
    info.size     = uniform.size;
    info.glType   = uniform.type;
    return info;
}

static ShaderResourceInfo
make_resource_info(const std::string& name, const GLProgramOutput& output)
{
    ShaderResourceInfo info;
    info.name     = name;
    info.typeName = output.type_name;
    info.location = static_cast<int>(output.location);
    info.glType   = output.type;
    info.size     = 1;
    return info;
}

static ShaderResourceInfo
make_resource_info(const std::string& name, const GLProgramBuffers& buffer)
{
    ShaderResourceInfo info;
    info.name     = name;
    info.typeName = buffer.type_name;
    info.location = buffer.location;
    info.binding  = buffer.binding;
    info.offset   = buffer.offset;
    info.size     = buffer.size;
    info.glType   = buffer.type;
    return info;
}

static ShaderBufferVariableInfo
make_buffer_variable_info(const std::string& blockName, const std::string& name, const GLProgramBuffers& buffer)
{
    ShaderBufferVariableInfo info;
    info.blockName = blockName;
    info.name      = name;
    info.typeName  = buffer.type_name;
    info.location  = buffer.location;
    info.binding   = buffer.binding;
    info.offset    = buffer.offset;
    info.size      = buffer.size;
    info.glType    = buffer.type;
    return info;
}

static std::shared_ptr<GLProgram>
load_program(const ShaderProgramKind kind, const std::vector<std::string>& paths)
{
    if (kind == ShaderProgramKind::compute) {
        if (paths.size() != 1) {
            throw std::runtime_error("Compute shaders require exactly one path.");
        }
        return g_glslProgramManager.loadComp(paths[0]);
    }

    if (paths.empty() || paths.size() > 2) {
        throw std::runtime_error("Vertex/fragment shaders require one combined file or two stage files.");
    }

    if (paths.size() == 1) {
        return g_glslProgramManager.loadVertFrag(paths[0]);
    }

    std::string shaderPaths[] { paths[0], paths[1] };
    return g_glslProgramManager.loadVertFrag(shaderPaths);
}

static std::string
build_shader_cache_key(const ShaderProgramKind kind, const std::vector<std::string>& paths)
{
    std::ostringstream stream;
    stream << static_cast<int>(kind);
    for (const std::string& path : paths) {
        stream << '\n' << path;
    }
    return stream.str();
}

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

template<typename T>
static T
parse_numeric_value(const std::string& text, const char* context)
{
    hres hr = hres::OK;
    const T value = str_to_numeric<T>(hr, text);
    if (hr != hres::OK) {
        throw std::runtime_error(std::string(context) + ": invalid numeric value: " + text);
    }
    return value;
}

static int
parse_positive_int(const std::string& text, const char* context)
{
    const int value = parse_numeric_value<int32_t>(text, context);
    if (value <= 0) {
        throw std::runtime_error(std::string(context) + ": value must be > 0");
    }
    return value;
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

static std::string
format_numeric_value(const double value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

static std::string
format_numeric_value(const float value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

static void
append_attribute_values(std::vector<std::string>& values, const std::vector<GraphAttribute>& attributes)
{
    for (const GraphAttribute& attribute : attributes) {
        values.push_back(attribute.name);
        values.push_back(attribute.value);
    }
}

static bool
extract_numeric_layout(const GLenum uniformType, GraphInputSourceKind& sourceKind, uint8_t& fieldCount)
{
    switch (uniformType) {
    case GL_BOOL:
    case GL_INT: sourceKind = GraphInputSourceKind::intValues; fieldCount = 1; return true;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2: sourceKind = GraphInputSourceKind::intValues; fieldCount = 2; return true;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3: sourceKind = GraphInputSourceKind::intValues; fieldCount = 3; return true;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4: sourceKind = GraphInputSourceKind::intValues; fieldCount = 4; return true;
    case GL_UNSIGNED_INT: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 1; return true;
    case GL_UNSIGNED_INT_VEC2: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 2; return true;
    case GL_UNSIGNED_INT_VEC3: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 3; return true;
    case GL_UNSIGNED_INT_VEC4: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 4; return true;
    case GL_FLOAT: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 1; return true;
    case GL_FLOAT_VEC2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 2; return true;
    case GL_FLOAT_VEC3: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 3; return true;
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 4; return true;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 6; return true;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 8; return true;
    case GL_FLOAT_MAT3: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 9; return true;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 12; return true;
    case GL_FLOAT_MAT4: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 16; return true;
    case GL_DOUBLE: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 1; return true;
    case GL_DOUBLE_VEC2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 2; return true;
    case GL_DOUBLE_VEC3: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 3; return true;
    case GL_DOUBLE_VEC4:
    case GL_DOUBLE_MAT2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 4; return true;
    case GL_DOUBLE_MAT2x3:
    case GL_DOUBLE_MAT3x2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 6; return true;
    case GL_DOUBLE_MAT2x4:
    case GL_DOUBLE_MAT4x2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 8; return true;
    case GL_DOUBLE_MAT3: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 9; return true;
    case GL_DOUBLE_MAT3x4:
    case GL_DOUBLE_MAT4x3: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 12; return true;
    case GL_DOUBLE_MAT4: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 16; return true;
    default: break;
    }

    return false;
}

static bool
is_system_uniform_name(const std::string& name)
{
    return name == "iFBsize" || name == "iFBaspect" || name == "isQuad" || name == "iTime" || name == "iTimeDelta"
           || name == "iFrame" || name == "iPassIndex";
}

static const ShaderResourceInfo*
find_resource_by_name(const std::vector<ShaderResourceInfo>& resources, const std::string& name)
{
    for (const ShaderResourceInfo& resource : resources) {
        if (resource.name == name) {
            return &resource;
        }
    }

    return nullptr;
}

static ShaderInterface
build_shader_interface(const std::shared_ptr<GLProgram>& program, const ShaderProgramKind kind)
{
    ShaderInterface shaderInterface;
    shaderInterface.isCompute = (kind == ShaderProgramKind::compute);

    if (!program || !program->isValid()) {
        shaderInterface.errorMessage = "Failed to load program for shader inspection.";
        return shaderInterface;
    }

    for (const auto& uniformIt : program->getUniforms()) {
        const ShaderResourceInfo info = make_resource_info(uniformIt.first, uniformIt.second);

        switch (uniformIt.second.type) {
        case GL_SAMPLER_2D: shaderInterface.samplers.push_back(info); break;
        case GL_IMAGE_2D: shaderInterface.images.push_back(info); break;
        default:
            if (is_system_uniform_name(uniformIt.first)) {
                shaderInterface.systemUniforms.push_back(info);
            } else {
                shaderInterface.uniforms.push_back(info);
            }
            break;
        }
    }

    for (const auto& outputIt : program->getOutputs()) {
        shaderInterface.outputs.push_back(make_resource_info(outputIt.first, outputIt.second));
    }

    for (const auto& counterIt : program->getAtomicCounters()) {
        shaderInterface.atomicCounters.push_back(make_resource_info(counterIt.first, *counterIt.second));
    }

    for (const auto& bufferIt : program->getBufferVariables()) {
        shaderInterface.bufferVariables.push_back(
            make_buffer_variable_info(bufferIt.first, bufferIt.second.first, bufferIt.second.second));
    }

    shaderInterface.success = true;
    return shaderInterface;
}

static RawGLContextState::CachedShaderInterface
load_cached_shader_interface(const RawGLContextState& contextState,
                             const ShaderProgramKind kind,
                             const std::vector<std::string>& paths)
{
    const std::string cacheKey = build_shader_cache_key(kind, paths);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.shaderCacheMutex);
        auto cacheIt = contextState.shaderCache.find(cacheKey);
        if (cacheIt != contextState.shaderCache.end()) {
            return cacheIt->second;
        }
    }

    RawGLContextState::CachedShaderInterface cached;
    cached.program = load_program(kind, paths);
    cached.shaderInterface = build_shader_interface(cached.program, kind);

    std::unique_lock<std::shared_mutex> writeLock(contextState.shaderCacheMutex);
    auto [cacheIt, inserted] = contextState.shaderCache.insert({ cacheKey, cached });
    if (!inserted) {
        return cacheIt->second;
    }

    return cached;
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

static GLenum
infer_texture_type_from_internal_format(const GLenum internalFormat)
{
    switch (internalFormat) {
    case GL_R8:
    case GL_RG8:
    case GL_RGB8:
    case GL_RGBA8: return GL_UNSIGNED_BYTE;
    case GL_R16:
    case GL_RG16:
    case GL_RGB16:
    case GL_RGBA16: return GL_UNSIGNED_SHORT;
    case GL_R32UI:
    case GL_RG32UI:
    case GL_RGB32UI:
    case GL_RGBA32UI: return GL_UNSIGNED_INT;
    case GL_R16F:
    case GL_RG16F:
    case GL_RGB16F:
    case GL_RGBA16F: return GL_HALF_FLOAT;
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F: return GL_FLOAT;
    default: return GL_FLOAT;
    }
}

static std::shared_ptr<Texture>
clone_texture_resource(const std::shared_ptr<Texture>& sourceTexture)
{
    std::shared_ptr<Texture> cloneTexture = std::make_shared<Texture>(
        sourceTexture->getWidth(),
        sourceTexture->getHeight(),
        sourceTexture->getInternalFormat(),
        infer_texture_type_from_internal_format(sourceTexture->getInternalFormat()),
        nullptr);

    GLCall(glCopyImageSubData(sourceTexture->getId(), GL_TEXTURE_2D, 0, 0, 0, 0,
                              cloneTexture->getId(), GL_TEXTURE_2D, 0, 0, 0, 0,
                              sourceTexture->getWidth(), sourceTexture->getHeight(), 1));

    return cloneTexture;
}

static void
apply_texture_attributes(PassInput& input, const std::vector<GraphAttribute>& attributes)
{
    for (const GraphAttribute& attribute : attributes) {
        hres result = hres::OK;
        input.eval_tex_attr(result, attribute.name, attribute.value);
        if (result != hres::OK) {
            throw std::runtime_error("in (" + input.uniform->type_name + "): unknown texture attribute " + attribute.name);
        }
        input.attributes[attribute.name] = attribute.value;
    }
}

static void
apply_mesh_parameters(MeshInput& meshInput, const std::vector<GraphAttribute>& parameters)
{
    for (const GraphAttribute& parameter : parameters) {
        hres result = hres::ERR;
        for (const MeshInput::MeshParm& meshParm : MeshInput::MESH_PARM_ARR) {
            if (meshParm.name != parameter.name) {
                continue;
            }

            for (const MeshInput::MeshParmValue& possibleValue : meshParm.possible_values) {
                if (possibleValue.key != parameter.value) {
                    continue;
                }

                meshParm.func(meshInput, possibleValue.gl_value);
                result = hres::OK;
                break;
            }

            if (result == hres::OK) {
                break;
            }
        }

        if (result != hres::OK) {
            throw std::runtime_error("pass_mesh: unknown mesh parameter " + parameter.name);
        }
    }
}

static void
apply_cull_parameters(Pass::CullMode& cullMode, const std::vector<GraphAttribute>& parameters)
{
    for (const GraphAttribute& parameter : parameters) {
        hres result = hres::ERR;
        for (const Pass::CullModeAttr& cullAttr : Pass::CULL_PARM_ARR) {
            if (cullAttr.name != parameter.name) {
                continue;
            }

            for (const Pass::CullModeVal& possibleValue : cullAttr.possible_values) {
                if (possibleValue.key != parameter.value) {
                    continue;
                }

                cullAttr.func(cullMode, possibleValue.gl_value);
                result = hres::OK;
                break;
            }

            if (result == hres::OK) {
                break;
            }
        }

        if (result != hres::OK) {
            throw std::runtime_error("cull: unknown cull parameter " + parameter.name);
        }
    }
}

static void
validate_mesh_definition(const GraphMeshDefinition& definition)
{
    if (definition.sourceKind == GraphMeshSourceKind::quad) {
        if (!definition.path.empty()) {
            throw std::runtime_error("pass_mesh: quad mesh must not provide a file path.");
        }
        if (!definition.parameters.empty()) {
            throw std::runtime_error("pass_mesh: quad mesh must not provide mesh parameters.");
        }
        return;
    }

    if (definition.path.empty()) {
        throw std::runtime_error("pass_mesh: mesh file path not found.");
    }

    MeshInput meshInput;
    meshInput.mesh.isQuad   = false;
    meshInput.mesh.FileName = definition.path;
    apply_mesh_parameters(meshInput, definition.parameters);
}

static void
validate_output_definition(const RawGLGraphState::ValidatedPass& pass, const GraphOutputDefinition& definition)
{
    if (definition.name.empty()) {
        throw std::runtime_error("out: output name is empty.");
    }

    if (pass.definition.programKind == ShaderProgramKind::compute) {
        if (!find_resource_by_name(pass.shaderInterface.images, definition.name)) {
            throw std::runtime_error("out (" + definition.name + "): program output not found.");
        }
    } else {
        if (!find_resource_by_name(pass.shaderInterface.outputs, definition.name)) {
            throw std::runtime_error("out (" + definition.name + "): program output not found.");
        }
    }

    if (definition.channels <= 0) {
        throw std::runtime_error("out (" + definition.name + "): channels must be > 0.");
    }
    if (definition.bits <= 0) {
        throw std::runtime_error("out (" + definition.name + "): bits must be > 0.");
    }
    if (definition.alphaChannel > 3) {
        throw std::runtime_error("out (" + definition.name + "): alpha channel > 3 is unsupported.");
    }
}

static void
validate_input_definition(const RawGLGraphState::ValidatedGraph& graph,
                          const RawGLGraphState::ValidatedPass& pass,
                          const GraphInputDefinition& definition)
{
    if (definition.name.empty()) {
        throw std::runtime_error("in: input name is empty.");
    }

    if (find_resource_by_name(pass.shaderInterface.systemUniforms, definition.name)) {
        throw std::runtime_error("in (" + definition.name + "): system uniform is engine controlled.");
    }

    GLProgramUniform* uniform = pass.program->findUniform(definition.name);
    if (!uniform) {
        throw std::runtime_error("in (" + definition.name + "): program uniform not found");
    }

    if (definition.sourceKind == GraphInputSourceKind::textureFile
        || definition.sourceKind == GraphInputSourceKind::passOutput
        || definition.sourceKind == GraphInputSourceKind::graphTexture) {
        const bool isSampler = find_resource_by_name(pass.shaderInterface.samplers, definition.name) != nullptr;
        const bool isImage   = find_resource_by_name(pass.shaderInterface.images, definition.name) != nullptr;
        if (!isSampler && !isImage) {
            throw std::runtime_error("in (" + definition.name + "): program uniform is not a texture/image input");
        }

        PassInput probeInput;
        probeInput.uniform = uniform;
        apply_texture_attributes(probeInput, definition.attributes);

        if (definition.sourceKind == GraphInputSourceKind::textureFile) {
            if (definition.texturePath.empty()) {
                throw std::runtime_error("in (" + definition.name + "): texture path is empty");
            }
        } else if (definition.sourceKind == GraphInputSourceKind::passOutput) {
            if (definition.referencedOutputName.empty()) {
                throw std::runtime_error("in (" + definition.name + "): empty referenced output name");
            }
            if (definition.referencedPassIndex >= graph.passes.size()) {
                throw std::runtime_error("invalid referenced pass index in " + definition.referencedOutputName + "::"
                                         + std::to_string(definition.referencedPassIndex));
            }

            const RawGLGraphState::ValidatedPass& referencedPass = graph.passes[definition.referencedPassIndex];
            if (referencedPass.definition.programKind == ShaderProgramKind::compute) {
                if (!find_resource_by_name(referencedPass.shaderInterface.images, definition.referencedOutputName)) {
                    throw std::runtime_error("in (" + definition.name + "): referenced program output "
                                             + definition.referencedOutputName + "::"
                                             + std::to_string(definition.referencedPassIndex) + " not found");
                }
            } else if (!find_resource_by_name(referencedPass.shaderInterface.outputs, definition.referencedOutputName)) {
                throw std::runtime_error("in (" + definition.name + "): referenced program output "
                                         + definition.referencedOutputName + "::"
                                         + std::to_string(definition.referencedPassIndex) + " not found");
            }
        } else if (definition.graphTextureName.empty()) {
            throw std::runtime_error("in (" + definition.name + "): graph texture name is empty");
        }

        return;
    }

    GraphInputSourceKind expectedKind = GraphInputSourceKind::intValues;
    uint8_t fieldCount                = 0;
    if (!extract_numeric_layout(uniform->type, expectedKind, fieldCount)) {
        throw std::runtime_error("in (" + definition.name + "): unsupported numeric uniform type");
    }
    if (expectedKind != definition.sourceKind) {
        throw std::runtime_error("in (" + definition.name + "): input source kind does not match shader uniform type");
    }

    size_t providedFields = 0;
    switch (definition.sourceKind) {
    case GraphInputSourceKind::intValues: providedFields = definition.intValues.size(); break;
    case GraphInputSourceKind::uintValues: providedFields = definition.uintValues.size(); break;
    case GraphInputSourceKind::floatValues: providedFields = definition.floatValues.size(); break;
    case GraphInputSourceKind::doubleValues: providedFields = definition.doubleValues.size(); break;
    default: break;
    }

    if (providedFields < fieldCount) {
        throw std::runtime_error("in (" + definition.name + "): missing numeric values");
    }
}

static void
validate_atomic_counter_definition(const RawGLGraphState::ValidatedPass& pass, const GraphAtomicCounterDefinition& definition)
{
    if (!find_resource_by_name(pass.shaderInterface.atomicCounters, definition.name)) {
        throw std::runtime_error("atomic (cntr): referenced counter " + definition.name + " not found.");
    }
}

static RawGLGraphState::ValidatedGraph
validate_graph_definition(const RawGLContextState& contextState, const GraphDefinition& definition)
{
    RawGLGraphState::ValidatedGraph validatedGraph;
    validatedGraph.verbosity = definition.verbosity;
    validatedGraph.passes.reserve(definition.passes.size());
    std::unordered_set<std::string> persistentOutputNames;
    std::unordered_set<std::string> persistentCounterNames;

    for (const GraphPassDefinition& passDefinition : definition.passes) {
        RawGLGraphState::ValidatedPass validatedPass;
        validatedPass.definition = passDefinition;
        const RawGLContextState::CachedShaderInterface cached =
            load_cached_shader_interface(contextState, passDefinition.programKind, passDefinition.shaderPaths);
        validatedPass.program         = cached.program;
        validatedPass.shaderInterface = cached.shaderInterface;

        if (!validatedPass.shaderInterface.success) {
            throw std::runtime_error(validatedPass.shaderInterface.errorMessage.empty()
                                         ? "Failed to build shader interface."
                                         : validatedPass.shaderInterface.errorMessage);
        }

        Pass::CullMode cullMode;
        apply_cull_parameters(cullMode, passDefinition.cullParameters);

        for (const GraphMeshDefinition& meshDefinition : passDefinition.meshes) {
            validate_mesh_definition(meshDefinition);
        }

        for (const GraphOutputDefinition& outputDefinition : passDefinition.outputs) {
            validate_output_definition(validatedPass, outputDefinition);
            if (!outputDefinition.persistentTextureName.empty()
                && !persistentOutputNames.insert(outputDefinition.persistentTextureName).second) {
                throw std::runtime_error("out (" + outputDefinition.name + "): duplicate persistent texture name "
                                         + outputDefinition.persistentTextureName);
            }
        }

        for (const GraphInputDefinition& inputDefinition : passDefinition.inputs) {
            validate_input_definition(validatedGraph, validatedPass, inputDefinition);
        }

        for (const GraphAtomicCounterDefinition& counterDefinition : passDefinition.atomicCounters) {
            validate_atomic_counter_definition(validatedPass, counterDefinition);
            if (!counterDefinition.persistentCounterName.empty()
                && !persistentCounterNames.insert(counterDefinition.persistentCounterName).second) {
                throw std::runtime_error("atomic (cntr): duplicate persistent counter name "
                                         + counterDefinition.persistentCounterName);
            }
        }

        validatedGraph.passes.push_back(std::move(validatedPass));
    }

    return validatedGraph;
}

static const GraphInputDefinition*
find_graph_input_definition(const RawGLGraphState::ValidatedPass& pass, const std::string& name)
{
    for (const GraphInputDefinition& inputDefinition : pass.definition.inputs) {
        if (inputDefinition.name == name) {
            return &inputDefinition;
        }
    }

    return nullptr;
}

static size_t
count_override_fields(const GraphInputOverride& inputOverride)
{
    switch (inputOverride.sourceKind) {
    case GraphInputSourceKind::intValues: return inputOverride.intValues.size();
    case GraphInputSourceKind::uintValues: return inputOverride.uintValues.size();
    case GraphInputSourceKind::floatValues: return inputOverride.floatValues.size();
    case GraphInputSourceKind::doubleValues: return inputOverride.doubleValues.size();
    default: return 0;
    }
}

static void
validate_execution_input_override(const RawGLGraphState::ValidatedGraph& graph, const GraphInputOverride& inputOverride)
{
    if (inputOverride.passIndex >= graph.passes.size()) {
        throw std::runtime_error("input override: invalid pass index " + std::to_string(inputOverride.passIndex));
    }

    const RawGLGraphState::ValidatedPass& pass = graph.passes[inputOverride.passIndex];
    if (!find_graph_input_definition(pass, inputOverride.name)) {
        throw std::runtime_error("input override (" + inputOverride.name + "): graph input is not declared");
    }

    GLProgramUniform* uniform = pass.program->findUniform(inputOverride.name);
    if (!uniform) {
        throw std::runtime_error("input override (" + inputOverride.name + "): program uniform not found");
    }

    switch (inputOverride.sourceKind) {
    case GraphInputSourceKind::textureFile: {
        const bool isSampler = find_resource_by_name(pass.shaderInterface.samplers, inputOverride.name) != nullptr;
        const bool isImage   = find_resource_by_name(pass.shaderInterface.images, inputOverride.name) != nullptr;
        if (!isSampler && !isImage) {
            throw std::runtime_error("input override (" + inputOverride.name + "): program uniform is not a texture/image input");
        }
        if (inputOverride.texturePath.empty()) {
            throw std::runtime_error("input override (" + inputOverride.name + "): texture path is empty");
        }

        PassInput probeInput;
        probeInput.uniform = uniform;
        apply_texture_attributes(probeInput, inputOverride.attributes);
        return;
    }
    case GraphInputSourceKind::passOutput:
        throw std::runtime_error("input override (" + inputOverride.name + "): pass-output overrides are not supported");
    default: break;
    }

    GraphInputSourceKind expectedKind = GraphInputSourceKind::intValues;
    uint8_t fieldCount                = 0;
    if (!extract_numeric_layout(uniform->type, expectedKind, fieldCount)) {
        throw std::runtime_error("input override (" + inputOverride.name + "): unsupported numeric uniform type");
    }
    if (expectedKind != inputOverride.sourceKind) {
        throw std::runtime_error("input override (" + inputOverride.name + "): override kind does not match shader uniform type");
    }
    if (count_override_fields(inputOverride) < fieldCount) {
        throw std::runtime_error("input override (" + inputOverride.name + "): missing numeric values");
    }
}

static SequenceExecutionInputOverride
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

static bool
has_sequence_override(const std::vector<SequenceExecutionInputOverride>& inputOverrides,
                      const size_t passIndex,
                      const std::string& inputName)
{
    for (const SequenceExecutionInputOverride& inputOverride : inputOverrides) {
        if (inputOverride.passIndex == passIndex && inputOverride.inputName == inputName) {
            return true;
        }
    }

    return false;
}

static RawGLGraphState::ResourcePlan
build_resource_plan(const RawGLContextState& contextState, const RawGLGraphState::ValidatedGraph& validatedGraph)
{
    RawGLGraphState::ResourcePlan resourcePlan;
    resourcePlan.verbosity = validatedGraph.verbosity;
    resourcePlan.passes.reserve(validatedGraph.passes.size());

    for (const RawGLGraphState::ValidatedPass& validatedPass : validatedGraph.passes) {
        RawGLGraphState::ResourcePass resourcePass;
        resourcePass.program    = validatedPass.program;
        resourcePass.shaderInterface = validatedPass.shaderInterface;
        resourcePass.isCompute  = (validatedPass.definition.programKind == ShaderProgramKind::compute);
        resourcePass.size[0]    = validatedPass.definition.sizeX;
        resourcePass.size[1]    = validatedPass.definition.sizeY;
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

static void
translate_pass_declaration(const SequenceParsedOption& option,
                           GraphDefinition& definition,
                           GraphTranslationState& state)
{
    GraphPassDefinition pass;
    pass.programKind = (option.string_key == "pass_comp") ? ShaderProgramKind::compute : ShaderProgramKind::vertfrag;
    pass.shaderPaths = option.value;
    if (!definition.passes.empty()) {
        pass.sizeX = definition.passes.back().sizeX;
        pass.sizeY = definition.passes.back().sizeY;
    }

    try {
        state.currentProgram = load_program(pass.programKind, pass.shaderPaths);
    } catch (const std::exception&) {
        if (option.string_key == "pass_vertfrag") {
            throw std::runtime_error("pass_vertfrag: must have one combined shader file or two stage files.");
        }
        throw;
    }
    if (!state.currentProgram || !state.currentProgram->isValid()) {
        throw std::runtime_error("Failed to load program for graph translation.");
    }

    definition.passes.push_back(std::move(pass));
    state.currentPass   = &definition.passes.back();
    state.currentInput  = nullptr;
    state.currentOutput = nullptr;
}

static void
translate_pass_property(const SequenceParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass) {
        throw std::runtime_error(option.string_key + ": no preceding pass declaration.");
    }

    if (option.string_key == "pass_size") {
        if (option.value.empty() || option.value.size() > 2) {
            throw std::runtime_error("pass_size: must have 1 or 2 parameters.");
        }
        try {
            state.currentPass->sizeX = parse_positive_int(option.value[0], "pass_size");
            state.currentPass->sizeY = (option.value.size() > 1) ? parse_positive_int(option.value[1], "pass_size")
                                                                 : state.currentPass->sizeX;
        } catch (const std::exception&) {
            throw std::runtime_error("pass_size (" + option.value[0] + "): invalid numeric value");
        }
        return;
    }

    if (option.string_key == "pass_workgroupsize") {
        if (option.value.empty() || option.value.size() > 2) {
            throw std::runtime_error("pass_workgroupsize: must have 1 or 2 parameters.");
        }
        state.currentPass->workGroupSizeX = parse_positive_int(option.value[0], "pass_workgroupsize");
        state.currentPass->workGroupSizeY =
            (option.value.size() > 1) ? parse_positive_int(option.value[1], "pass_workgroupsize") : 1;
        state.currentPass->hasExplicitWorkGroupSize = true;
        return;
    }

    if (option.string_key == "bg_color") {
        if (option.value.empty() || option.value.size() > 4) {
            throw std::runtime_error("bg_color: must have 1 to 4 parameters.");
        }
        for (size_t i = 0; i < option.value.size(); ++i) {
            state.currentPass->clearColor[i] = parse_numeric_value<float_t>(option.value[i], "bg_color");
        }
        return;
    }

    if (option.string_key == "cull") {
        if (option.value.size() < 2 || (option.value.size() % 2) != 0) {
            throw std::runtime_error("cull: must have key/value pairs.");
        }
        for (size_t i = 0; i < option.value.size(); i += 2) {
            state.currentPass->cullParameters.push_back(GraphAttribute { option.value[i], option.value[i + 1] });
        }
        return;
    }

    if (option.string_key == "pass_mesh") {
        if (option.value.empty()) {
            throw std::runtime_error("pass_mesh: must have at least 1 parameter.");
        }

        GraphMeshDefinition mesh;
        const size_t split = option.value[0].find("::");
        const std::string meshType = option.value[0].substr(0, split);

        if (meshType == "quad") {
            mesh.sourceKind = GraphMeshSourceKind::quad;
            state.currentPass->meshes.push_back(std::move(mesh));
            return;
        }

        if (meshType != "mesh") {
            throw std::runtime_error("pass_mesh: unknown mesh type.");
        }
        if (split != std::string::npos) {
            throw std::runtime_error("pass_mesh: mesh references are not supported");
        }

        mesh.sourceKind = GraphMeshSourceKind::file;
        for (size_t i = 1; i < option.value.size(); ++i) {
            const std::string extension = get_file_ext(option.value[i]);
            if (extension == "ply" || extension == "obj") {
                mesh.path = option.value[i];
                continue;
            }

            if (i + 1 >= option.value.size()) {
                throw std::runtime_error("pass_mesh: mesh attributes must be key/value pairs.");
            }

            mesh.parameters.push_back(GraphAttribute { option.value[i], option.value[i + 1] });
            ++i;
        }

        if (mesh.path.empty()) {
            throw std::runtime_error("pass_mesh: mesh file path not found.");
        }

        state.currentPass->meshes.push_back(std::move(mesh));
    }
}

static void
translate_input_option(const SequenceParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass || !state.currentProgram) {
        throw std::runtime_error("in (" + option.value[0] + "): no preceding pass declaration.");
    }
    if (option.value.size() < 2) {
        throw std::runtime_error("in (" + option.value[0] + "): must have at least 2 parameters.");
    }

    GLProgramUniform* shaderUniform = state.currentProgram->findUniform(option.value[0]);
    if (!shaderUniform) {
        throw std::runtime_error("in (" + option.value[0] + "): program uniform not found");
    }

    GraphInputDefinition input;
    input.name = option.value[0];

    if (shaderUniform->type == GL_SAMPLER_2D || shaderUniform->type == GL_IMAGE_2D) {
        input.sourceKind = GraphInputSourceKind::textureFile;

        for (size_t i = 1; i < option.value.size(); ++i) {
            const std::string& token = option.value[i];

            if (token.find("::") != std::string::npos) {
                const size_t split = token.find("::");
                input.sourceKind        = GraphInputSourceKind::passOutput;
                input.referencedOutputName = token.substr(0, split);
                input.referencedPassIndex  = static_cast<size_t>(
                    parse_numeric_value<int32_t>(token.substr(split + 2), "in pass reference"));
                continue;
            }

            if (i + 1 < option.value.size()) {
                PassInput probeInput;
                hres attrResult = hres::OK;
                probeInput.eval_tex_attr(attrResult, token, option.value[i + 1]);
                if (attrResult == hres::OK) {
                    input.attributes.push_back(GraphAttribute { token, option.value[i + 1] });
                    ++i;
                    continue;
                }
            }

            input.texturePath = token;
        }

        if (input.sourceKind == GraphInputSourceKind::textureFile && input.texturePath.empty()) {
            throw std::runtime_error("in (" + option.value[0] + "): texture path not found");
        }
    } else {
        uint8_t fieldCount = 0;
        if (!extract_numeric_layout(shaderUniform->type, input.sourceKind, fieldCount)) {
            throw std::runtime_error("in (" + option.value[0] + "): unsupported uniform type");
        }
        if ((option.value.size() - 1) < fieldCount) {
            throw std::runtime_error("in (" + option.value[0] + "): missing numeric values");
        }

        for (uint8_t i = 0; i < fieldCount; ++i) {
            const std::string& textValue = option.value[1 + i];
            switch (input.sourceKind) {
            case GraphInputSourceKind::intValues:
                input.intValues.push_back(parse_numeric_value<int32_t>(textValue, "in"));
                break;
            case GraphInputSourceKind::uintValues:
                input.uintValues.push_back(parse_numeric_value<uint32_t>(textValue, "in"));
                break;
            case GraphInputSourceKind::floatValues:
                input.floatValues.push_back(parse_numeric_value<float_t>(textValue, "in"));
                break;
            case GraphInputSourceKind::doubleValues:
                input.doubleValues.push_back(parse_numeric_value<double_t>(textValue, "in"));
                break;
            default: break;
            }
        }
    }

    state.currentPass->inputs.push_back(std::move(input));
    state.currentInput = &state.currentPass->inputs.back();
}

static void
translate_atomic_option(const SequenceParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass) {
        throw std::runtime_error("atomic: no preceding pass declaration.");
    }
    if (option.value.size() < 2) {
        throw std::runtime_error("atomic (" + option.value[0] + "): must have at least 2 parameters.");
    }
    if (option.value[0] != "cntr") {
        throw std::runtime_error("atomic (" + option.value[0] + "): unknown atomic buffer type");
    }
    if (option.value.size() > 3) {
        throw std::runtime_error("atomic (" + option.value[0] + "): can only have a single value");
    }

    GraphAtomicCounterDefinition counter;
    counter.name         = option.value[1];
    counter.initialValue = parse_numeric_value<uint32_t>(option.value[2], "atomic");
    state.currentPass->atomicCounters.push_back(std::move(counter));
}

static void
translate_input_attribute_option(const SequenceParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentInput) {
        throw std::runtime_error("in_attr: no preceding input declaration.");
    }
    if (option.value.size() < 2) {
        throw std::runtime_error("in_attr: must have 2 parameters.");
    }

    state.currentInput->attributes.push_back(GraphAttribute { option.value[0], option.value[1] });
}

static void
translate_output_option(const SequenceParsedOption& option, GraphTranslationState& state)
{
    if (!state.currentPass || !state.currentProgram) {
        throw std::runtime_error(option.string_key + ": no preceding pass declaration.");
    }

    if (option.string_key == "out") {
        if (option.value.size() != 2) {
            throw std::runtime_error("out: must have 2 parameters.");
        }

        GraphOutputDefinition output;
        output.name         = option.value[0];
        output.path         = option.value[1];
        output.format       = state.previousFormat;
        output.channels     = state.previousChannels;
        output.alphaChannel = state.previousAlphaChannel;
        output.bits         = state.previousBits;

        state.currentPass->outputs.push_back(std::move(output));
        state.currentOutput = &state.currentPass->outputs.back();
        return;
    }

    if (!state.currentOutput) {
        throw std::runtime_error(option.string_key + ": no preceding output declaration.");
    }

    if (option.string_key == "out_format") {
        state.currentOutput->format = option.value[0];
        state.previousFormat        = option.value[0];
        return;
    }

    if (option.string_key == "out_channels") {
        state.currentOutput->channels = parse_positive_int(option.value[0], "out_channels");
        state.previousChannels        = state.currentOutput->channels;
        return;
    }

    if (option.string_key == "out_alpha_channel") {
        state.currentOutput->alphaChannel = parse_numeric_value<int32_t>(option.value[0], "out_alpha_channel");
        state.previousAlphaChannel        = state.currentOutput->alphaChannel;
        return;
    }

    if (option.string_key == "out_bits") {
        state.currentOutput->bits = parse_positive_int(option.value[0], "out_bits");
        state.previousBits        = state.currentOutput->bits;
        return;
    }

    if (option.string_key == "out_attr") {
        if (option.value.size() < 2) {
            throw std::runtime_error("out_attr: must have 2 parameters.");
        }
        state.currentOutput->attributes.push_back(GraphAttribute { option.value[0], option.value[1] });
    }
}

static GraphBuildRequest
build_graph_request_from_command_line(const CommandLineRequest& request)
{
    const SequenceParsedArguments parsedArguments = Sequence_ParseArguments(request.argc, request.argv);

    GraphBuildRequest graphRequest;
    graphRequest.definition.verbosity = parsedArguments.verbosity;

    GraphTranslationState state;
    for (const SequenceParsedOption& option : parsedArguments.options) {
        if (option.string_key == "pass_vertfrag" || option.string_key == "pass_comp") {
            translate_pass_declaration(option, graphRequest.definition, state);
            continue;
        }

        if (option.string_key == "pass_size" || option.string_key == "pass_workgroupsize"
            || option.string_key == "bg_color" || option.string_key == "pass_mesh" || option.string_key == "cull") {
            translate_pass_property(option, state);
            continue;
        }

        if (option.string_key == "in") {
            translate_input_option(option, state);
            continue;
        }

        if (option.string_key == "atomic") {
            translate_atomic_option(option, state);
            continue;
        }

        if (option.string_key == "in_attr") {
            translate_input_attribute_option(option, state);
            continue;
        }

        translate_output_option(option, state);
    }

    return graphRequest;
}

static PassOutput&
ensure_output_binding(SequenceRuntimePassConfig& passConfig, const ShaderInterface& shaderInterface, const std::string& name)
{
    auto [outputIt, inserted] = passConfig.outputs.insert({ name, PassOutput() });
    (void)inserted;
    PassOutput& output = outputIt->second;

    if (passConfig.isCompute) {
        if (!find_resource_by_name(shaderInterface.images, name)) {
            throw std::runtime_error("out (" + name + "): program output not found.");
        }

        output.uniform = passConfig.program->findUniform(name);
        if (!output.uniform) {
            throw std::runtime_error("out (" + name + "): program output not found.");
        }
    } else {
        if (!find_resource_by_name(shaderInterface.outputs, name)) {
            throw std::runtime_error("out (" + name + "): program output not found.");
        }

        output.output = passConfig.program->findOutput(name);
        if (!output.output) {
            throw std::runtime_error("out (" + name + "): program output not found.");
        }
    }

    return output;
}

static void
apply_output_definition(SequenceRuntimePassConfig& passConfig,
                        const ShaderInterface& shaderInterface,
                        const GraphOutputDefinition& definition)
{
    PassOutput& output = ensure_output_binding(passConfig, shaderInterface, definition.name);
    output.path               = definition.path;
    output.internalFormatText = definition.format;
    output.channels           = definition.channels;
    output.alphaChannel       = definition.alphaChannel;
    output.bits               = definition.bits;
    output.attributes.clear();
    for (const GraphAttribute& attribute : definition.attributes) {
        output.attributes[attribute.name] = attribute.value;
    }
}

static void
ensure_referenced_output(SequenceRuntimeConfig& runtimeConfig, size_t referencedPassIndex, const std::string& outputName)
{
    if (referencedPassIndex >= runtimeConfig.passes.size()) {
        throw std::runtime_error("invalid referenced pass index in " + outputName + "::" + std::to_string(referencedPassIndex));
    }

    SequenceRuntimePassConfig& referencedPass = runtimeConfig.passes[referencedPassIndex];
    const ShaderInterface referencedInterface = build_shader_interface(
        referencedPass.program,
        referencedPass.isCompute ? ShaderProgramKind::compute : ShaderProgramKind::vertfrag);
    ensure_output_binding(referencedPass, referencedInterface, outputName);
}

static SequenceRuntimeConfig
build_sequence_runtime_config(const RawGLGraphState::ResourcePlan& resourcePlan)
{
    SequenceRuntimeConfig runtimeConfig;
    runtimeConfig.verbosity = resourcePlan.verbosity;
    runtimeConfig.sharedTextures  = resourcePlan.sharedTextures;
    runtimeConfig.sharedMeshes    = resourcePlan.sharedMeshes;
    runtimeConfig.sharedGpuMeshes = resourcePlan.sharedGpuMeshes;

    for (size_t passIndex = 0; passIndex < resourcePlan.passes.size(); ++passIndex) {
        const RawGLGraphState::ResourcePass& resourcePass = resourcePlan.passes[passIndex];
        SequenceRuntimePassConfig passConfig;
        passConfig.isCompute = resourcePass.isCompute;
        passConfig.program   = resourcePass.program;
        passConfig.size[0]   = resourcePass.size[0];
        passConfig.size[1]   = resourcePass.size[1];
        passConfig.workGroupSize[0] = resourcePass.workGroupSize[0];
        passConfig.workGroupSize[1] = resourcePass.workGroupSize[1];
        std::memcpy(passConfig.clearColor, resourcePass.clearColor, sizeof(passConfig.clearColor));
        passConfig.cullMode = resourcePass.cullMode;

        for (const GraphMeshDefinition& meshDefinition : resourcePass.meshes) {
            MeshInput meshInput;
            if (meshDefinition.sourceKind == GraphMeshSourceKind::file) {
                meshInput.mesh.isQuad   = false;
                meshInput.mesh.FileName = meshDefinition.path;
                apply_mesh_parameters(meshInput, meshDefinition.parameters);
            }

            const std::string meshName = "mesh" + std::to_string(passConfig.meshes.size());
            passConfig.meshes.insert({ meshName, meshInput });
        }

        for (const GraphOutputDefinition& outputDefinition : resourcePass.outputs) {
            apply_output_definition(passConfig, resourcePass.shaderInterface, outputDefinition);
        }

        for (const GraphInputDefinition& inputDefinition : resourcePass.inputs) {
            GLProgramUniform* uniform = passConfig.program->findUniform(inputDefinition.name);
            PassInput input;
            input.uniform = uniform;

            if (inputDefinition.sourceKind == GraphInputSourceKind::textureFile
                || inputDefinition.sourceKind == GraphInputSourceKind::passOutput) {
                apply_texture_attributes(input, inputDefinition.attributes);

                if (inputDefinition.sourceKind == GraphInputSourceKind::textureFile) {
                    input.path = inputDefinition.texturePath;
                } else {
                    ensure_referenced_output(runtimeConfig,
                                             inputDefinition.referencedPassIndex,
                                             inputDefinition.referencedOutputName);
                    input.path = inputDefinition.referencedOutputName + "::"
                                 + std::to_string(inputDefinition.referencedPassIndex);
                }
            } else if (inputDefinition.sourceKind == GraphInputSourceKind::graphTexture) {
                apply_texture_attributes(input, inputDefinition.attributes);
                input.runtimeTextureBindingRequired = true;
            } else {
                GraphInputSourceKind expectedKind = GraphInputSourceKind::intValues;
                uint8_t fieldCount                = 0;
                extract_numeric_layout(uniform->type, expectedKind, fieldCount);

                switch (inputDefinition.sourceKind) {
                case GraphInputSourceKind::intValues:
                    std::copy_n(inputDefinition.intValues.begin(),
                                fieldCount,
                                reinterpret_cast<int32_t*>(&input.ints[0]));
                    break;
                case GraphInputSourceKind::uintValues:
                    std::copy_n(inputDefinition.uintValues.begin(), fieldCount, &input.uints[0]);
                    break;
                case GraphInputSourceKind::floatValues:
                    std::copy_n(inputDefinition.floatValues.begin(), fieldCount, &input.floats[0]);
                    break;
                case GraphInputSourceKind::doubleValues:
                    std::copy_n(inputDefinition.doubleValues.begin(), fieldCount, &input.doubles[0]);
                    break;
                default: break;
                }
            }

            passConfig.inputs.insert({ inputDefinition.name, input });
        }

        for (const GraphAtomicCounterDefinition& counterDefinition : resourcePass.atomicCounters) {
            std::shared_ptr<GLProgramBuffers> counter = passConfig.program->findCounter(counterDefinition.name);
            Pass::inputCounter inputCounter;
            inputCounter.size  = counter->size;
            inputCounter.value.assign(counter->size, 0u);
            inputCounter.value[0] = counterDefinition.initialValue;
            passConfig.inputCounters.insert({ counterDefinition.name, inputCounter });
        }

        runtimeConfig.passes.push_back(std::move(passConfig));
    }

    return runtimeConfig;
}

static void
append_unique_dependency(std::vector<size_t>& dependencies, size_t passIndex)
{
    for (size_t existingIndex : dependencies) {
        if (existingIndex == passIndex) {
            return;
        }
    }

    dependencies.push_back(passIndex);
}

static RawGLGraphState::ExecutionPlan
build_execution_plan(const RawGLGraphState::ResourcePlan& resourcePlan)
{
    RawGLGraphState::ExecutionPlan executionPlan;
    executionPlan.sequenceRuntimeConfig = build_sequence_runtime_config(resourcePlan);
    executionPlan.passes.reserve(resourcePlan.passes.size());

    for (size_t passIndex = 0; passIndex < resourcePlan.passes.size(); ++passIndex) {
        const RawGLGraphState::ResourcePass& resourcePass = resourcePlan.passes[passIndex];
        RawGLGraphState::ExecutionPass executionPass;
        executionPass.passIndex = passIndex;
        executionPass.isCompute = resourcePass.isCompute;
        executionPass.inputNames.reserve(resourcePass.inputs.size());
        executionPass.outputNames.reserve(resourcePass.outputs.size());

        for (const GraphInputDefinition& inputDefinition : resourcePass.inputs) {
            executionPass.inputNames.push_back(inputDefinition.name);
            if (inputDefinition.sourceKind == GraphInputSourceKind::passOutput) {
                append_unique_dependency(executionPass.dependencyPassIndices, inputDefinition.referencedPassIndex);
            } else if (inputDefinition.sourceKind == GraphInputSourceKind::graphTexture) {
                executionPlan.persistentInputs.push_back(RawGLGraphState::PersistentInputBinding {
                    passIndex,
                    inputDefinition.name,
                    inputDefinition.graphTextureName,
                });
            }
        }

        for (const GraphOutputDefinition& outputDefinition : resourcePass.outputs) {
            executionPass.outputNames.push_back(outputDefinition.name);
            if (!outputDefinition.persistentTextureName.empty()) {
                executionPlan.persistentOutputs.push_back(RawGLGraphState::PersistentOutputBinding {
                    passIndex,
                    outputDefinition.name,
                    outputDefinition.persistentTextureName,
                });
            }
        }

        for (const GraphAtomicCounterDefinition& counterDefinition : resourcePass.atomicCounters) {
            if (!counterDefinition.persistentCounterName.empty()) {
                executionPlan.persistentAtomicCounters.push_back(RawGLGraphState::PersistentAtomicCounterBinding {
                    passIndex,
                    counterDefinition.name,
                    counterDefinition.persistentCounterName,
                });
            }
        }

        executionPlan.passes.push_back(std::move(executionPass));
    }

    return executionPlan;
}

}  // namespace

RawGLContext::RawGLContext()
    : m_state(std::make_shared<RawGLContextState>())
{
    Log_Init();
}

RawGLContext::~RawGLContext() = default;

ShaderInterface
RawGLContext::inspectShaderInterface(const ShaderInspectionRequest& request) const
{
    try {
        return load_cached_shader_interface(*m_state, request.kind, request.paths).shaderInterface;
    } catch (const std::exception& exception) {
        ShaderInterface result;
        result.isCompute    = (request.kind == ShaderProgramKind::compute);
        result.errorMessage = exception.what();
        return result;
    }
}

GraphBuildResult
RawGLContext::buildGraph(const GraphBuildRequest& request) const
{
    GraphBuildResult result;

    try {
        auto state                 = std::make_unique<RawGLGraphState>();
        state->validatedGraph      = validate_graph_definition(*m_state, request.definition);
        state->resourcePlan       = build_resource_plan(*m_state, state->validatedGraph);
        state->executionPlan      = build_execution_plan(state->resourcePlan);
        result.graph              = std::make_unique<RawGLGraph>(m_state, std::move(state));
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

ContextCacheStats
RawGLContext::cacheStats() const
{
    ContextCacheStats stats;

    {
        std::shared_lock<std::shared_mutex> readLock(m_state->shaderCacheMutex);
        stats.shaderInterfaces = m_state->shaderCache.size();
    }
    {
        std::shared_lock<std::shared_mutex> readLock(m_state->textureCacheMutex);
        stats.textures = m_state->textureCache.size();
    }
    {
        std::shared_lock<std::shared_mutex> readLock(m_state->meshCacheMutex);
        stats.meshesHost = m_state->meshCache.size();
    }
    {
        std::shared_lock<std::shared_mutex> readLock(m_state->meshGpuCacheMutex);
        stats.meshesGpu = m_state->meshGpuCache.size();
    }

    return stats;
}

RawGLGraph::RawGLGraph(std::shared_ptr<RawGLContextState> contextState, std::unique_ptr<RawGLGraphState> state)
    : m_contextState(std::move(contextState))
    , m_state(std::move(state))
{
}

RawGLGraph::~RawGLGraph() = default;

GraphExecutionResult
RawGLGraph::execute(const GraphExecutionRequest& request)
{
    GraphExecutionResult result;

    try {
        std::vector<SequenceExecutionInputOverride> inputOverrides;
        inputOverrides.reserve(request.inputOverrides.size());
        for (const GraphInputOverride& inputOverride : request.inputOverrides) {
            validate_execution_input_override(m_state->validatedGraph, inputOverride);
            inputOverrides.push_back(build_sequence_execution_input_override(*m_contextState, inputOverride));
        }
        std::map<std::string, std::shared_ptr<Texture>> persistentTextureSnapshot;
        for (const auto& persistentTexture : m_state->persistentTextures) {
            persistentTextureSnapshot[persistentTexture.first] = clone_texture_resource(persistentTexture.second);
        }
        for (const RawGLGraphState::PersistentInputBinding& persistentInput : m_state->executionPlan.persistentInputs) {
            if (has_sequence_override(inputOverrides, persistentInput.passIndex, persistentInput.inputName)) {
                continue;
            }

            auto textureIt = persistentTextureSnapshot.find(persistentInput.persistentTextureName);
            if (textureIt == persistentTextureSnapshot.end() || !textureIt->second) {
                continue;
            }

            SequenceExecutionInputOverride inputOverride;
            inputOverride.passIndex = persistentInput.passIndex;
            inputOverride.inputName = persistentInput.inputName;
            inputOverride.kind      = SequenceExecutionInputOverrideKind::texture;
            inputOverride.texture   = textureIt->second;
            inputOverrides.push_back(std::move(inputOverride));
        }

        if (!m_state->sequence) {
            m_state->sequence = std::make_unique<Sequence>(m_state->executionPlan.sequenceRuntimeConfig);
        }

        for (const RawGLGraphState::PersistentAtomicCounterBinding& persistentCounter :
             m_state->executionPlan.persistentAtomicCounters) {
            auto counterIt = m_state->persistentAtomicCounters.find(persistentCounter.persistentCounterName);
            if (counterIt == m_state->persistentAtomicCounters.end() || counterIt->second.empty()) {
                continue;
            }

            m_state->sequence->setPassAtomicCounterValues(persistentCounter.passIndex,
                                                          persistentCounter.counterName,
                                                          counterIt->second);
        }

        SequenceSystemUniformState systemUniforms;
        systemUniforms.timeSeconds      = request.systemUniforms.timeSeconds;
        systemUniforms.deltaTimeSeconds = request.systemUniforms.deltaTimeSeconds;
        systemUniforms.frameNumber      = request.systemUniforms.frameNumber;
        systemUniforms.passIndex        = request.systemUniforms.passIndex;
        m_state->sequence->run(systemUniforms, inputOverrides);
        for (const RawGLGraphState::PersistentOutputBinding& persistentOutput : m_state->executionPlan.persistentOutputs) {
            std::shared_ptr<Texture> outputTexture =
                m_state->sequence->getPassOutputTexture(persistentOutput.passIndex, persistentOutput.outputName);
            if (!outputTexture) {
                throw std::runtime_error("persistent output capture failed for " + persistentOutput.outputName);
            }
            m_state->persistentTextures[persistentOutput.persistentTextureName] = outputTexture;
        }
        for (const RawGLGraphState::PersistentAtomicCounterBinding& persistentCounter :
             m_state->executionPlan.persistentAtomicCounters) {
            const std::vector<GLuint> counterValues =
                m_state->sequence->getPassAtomicCounterValues(persistentCounter.passIndex, persistentCounter.counterName);
            if (counterValues.empty()) {
                throw std::runtime_error("persistent atomic counter capture failed for "
                                         + persistentCounter.counterName);
            }
            m_state->persistentAtomicCounters[persistentCounter.persistentCounterName] = counterValues;
        }
        m_state->sequence->releaseRunOutputTextures();
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

CoreSession::CoreSession(const GraphBuildRequest& request)
{
    GraphBuildResult buildResult = m_context.buildGraph(request);
    if (!buildResult.success || !buildResult.graph) {
        throw std::runtime_error(buildResult.errorMessage.empty() ? "Failed to build RawGL graph."
                                                                  : buildResult.errorMessage);
    }

    m_graph = std::move(buildResult.graph);
}

CoreSession::~CoreSession() = default;

void
CoreSession::run()
{
    GraphExecutionResult executionResult = m_graph->execute(GraphExecutionRequest {});
    if (!executionResult.success) {
        throw std::runtime_error(executionResult.errorMessage.empty() ? "Failed to execute RawGL graph."
                                                                      : executionResult.errorMessage);
    }
}

int
RunCommandLine(int argc, const char* argv[])
{
    return Run(CommandLineRequest { argc, argv }).exitCode;
}

CommandLineResult
Run(const CommandLineRequest& request)
{
    Log_Init();

    Timer timer;
    CommandLineResult result;

    int immediateExitCode = 0;
    if (Sequence_HandleImmediateCommandLine(request.argc, request.argv, immediateExitCode)) {
        result.exitCode = immediateExitCode;
        result.immediateExit = true;
        return result;
    }

    try {
        RawGLContext context;
        const GraphBuildRequest graphRequest = build_graph_request_from_command_line(request);
        GraphBuildResult buildResult = context.buildGraph(graphRequest);
        if (!buildResult.success || !buildResult.graph) {
            throw std::runtime_error(buildResult.errorMessage.empty() ? "Failed to build RawGL graph."
                                                                      : buildResult.errorMessage);
        }

        GraphExecutionResult executionResult = buildResult.graph->execute(GraphExecutionRequest {});
        if (!executionResult.success) {
            throw std::runtime_error(executionResult.errorMessage.empty() ? "Failed to execute RawGL graph."
                                                                          : executionResult.errorMessage);
        }

        std::cout << std::endl;
        LOG(info) << "Total processing time : " << timer.nowText() << std::endl;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << std::endl;
        result.exitCode = 1;
        return result;
    }

    result.executed = true;
    return result;
}

ShaderInterface
InspectShaderInterface(const ShaderInspectionRequest& request)
{
    RawGLContext context;
    return context.inspectShaderInterface(request);
}

}  // namespace rawgl
