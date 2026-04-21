// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.


#include "sequence.h"

#include "gl_utils.h"
#include "io_runtime.h"
#include "timer.h"
#include "log.h"
#include "texture_loader.h"

#include <future>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>

#include "mesh_io.h"

namespace {
template<typename T>
static T
parse_checked_numeric(const std::string& text, const std::string& context)
{
    hres hr = hres::OK;
    T value = str_to_numeric<T>(hr, text);
    if (hr != hres::OK) {
        throw std::runtime_error(context + ": invalid numeric value: " + text);
    }
    return value;
}

static int
parse_checked_positive_int(const std::string& text, const std::string& context)
{
    const int value = parse_checked_numeric<int32_t>(text, context);
    if (value <= 0) {
        throw std::runtime_error(context + ": value must be > 0");
    }
    return value;
}

[[noreturn]] static void
throw_sequence_error(const std::string& message)
{
    throw std::runtime_error(message);
}

struct PendingTextureLoad {
    std::string key;
    std::future<rawgl::io::LoadedTextureData> future;
};

static const float RAWGL_DEFAULT_VERTS[]
    = { -1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f };

static const float RAWGL_DEFAULT_TEXCOORDS[] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };

static const float RAWGL_DEFAULT_NORMALS[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };

static const unsigned char RAWGL_DEFAULT_COLORS[] = { 255, 255, 255, 255, 255, 255, 255, 255,
                                                      255, 255, 255, 255, 255, 255, 255, 255 };

static const unsigned int RAWGL_DEFAULT_INDICES[] = { 0, 1, 2, 0, 2, 3 };

static const SequenceExecutionInputOverride*
find_input_override(const std::vector<SequenceExecutionInputOverride>& inputOverrides,
                    const size_t passIndex,
                    const std::string& inputName)
{
    for (const SequenceExecutionInputOverride& inputOverride : inputOverrides) {
        if (inputOverride.passIndex == passIndex && inputOverride.inputName == inputName) {
            return &inputOverride;
        }
    }

    return nullptr;
}

static const rawgl::io::IoRuntimeService&
resolve_io_runtime(const std::shared_ptr<rawgl::io::IoRuntimeService>& ioRuntime)
{
    static const rawgl::io::IoRuntimeService defaultIoRuntime;
    return ioRuntime ? *ioRuntime : defaultIoRuntime;
}

static std::string
make_disk_texture_key(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    std::ostringstream stream;
    stream << "file:" << path;

    for (const auto& attribute : attributes) {
        stream << '\x1F' << attribute.first << '=' << attribute.second;
    }

    return stream.str();
}

static std::string
make_mesh_cache_key(const MeshInput::Mesh& mesh)
{
    std::ostringstream stream;
    stream << "file:" << mesh.FileName << '\x1F' << "tris=" << (mesh.Triangles ? 1 : 0);
    return stream.str();
}

static GLuint
resolve_fragment_output_location(const PassOutput& output)
{
    return output.output->location + static_cast<GLuint>(output.usesArrayElement ? output.arrayElement : 0u);
}

static void
set_addressed_int_uniform(const PassInput& input, const GLint* values)
{
    switch (input.uniform->type) {
    case GL_BOOL:
    case GL_INT: glUniform1iv(input.addressedLocation, 1, values); break;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2: glUniform2iv(input.addressedLocation, 1, values); break;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3: glUniform3iv(input.addressedLocation, 1, values); break;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4: glUniform4iv(input.addressedLocation, 1, values); break;
    default: throw_sequence_error("unsupported addressed integer uniform type");
    }
}

static void
set_addressed_uint_uniform(const PassInput& input, const GLuint* values)
{
    switch (input.uniform->type) {
    case GL_UNSIGNED_INT: glUniform1uiv(input.addressedLocation, 1, values); break;
    case GL_UNSIGNED_INT_VEC2: glUniform2uiv(input.addressedLocation, 1, values); break;
    case GL_UNSIGNED_INT_VEC3: glUniform3uiv(input.addressedLocation, 1, values); break;
    case GL_UNSIGNED_INT_VEC4: glUniform4uiv(input.addressedLocation, 1, values); break;
    default: throw_sequence_error("unsupported addressed unsigned integer uniform type");
    }
}

static void
set_addressed_float_uniform(const PassInput& input, const GLfloat* values)
{
    switch (input.uniform->type) {
    case GL_FLOAT: glUniform1fv(input.addressedLocation, 1, values); break;
    case GL_FLOAT_VEC2: glUniform2fv(input.addressedLocation, 1, values); break;
    case GL_FLOAT_VEC3: glUniform3fv(input.addressedLocation, 1, values); break;
    case GL_FLOAT_VEC4: glUniform4fv(input.addressedLocation, 1, values); break;
    case GL_FLOAT_MAT2: glUniformMatrix2fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT2x3: glUniformMatrix2x3fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT2x4: glUniformMatrix2x4fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT3: glUniformMatrix3fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT3x2: glUniformMatrix3x2fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT3x4: glUniformMatrix3x4fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT4: glUniformMatrix4fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT4x2: glUniformMatrix4x2fv(input.addressedLocation, 1, false, values); break;
    case GL_FLOAT_MAT4x3: glUniformMatrix4x3fv(input.addressedLocation, 1, false, values); break;
    default: throw_sequence_error("unsupported addressed float uniform type");
    }
}

static void
set_addressed_double_uniform(const PassInput& input, const GLdouble* values)
{
    switch (input.uniform->type) {
    case GL_DOUBLE: glUniform1dv(input.addressedLocation, 1, values); break;
    case GL_DOUBLE_VEC2: glUniform2dv(input.addressedLocation, 1, values); break;
    case GL_DOUBLE_VEC3: glUniform3dv(input.addressedLocation, 1, values); break;
    case GL_DOUBLE_VEC4: glUniform4dv(input.addressedLocation, 1, values); break;
    case GL_DOUBLE_MAT2: glUniformMatrix2dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT2x3: glUniformMatrix2x3dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT2x4: glUniformMatrix2x4dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT3: glUniformMatrix3dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT3x2: glUniformMatrix3x2dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT3x4: glUniformMatrix3x4dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT4: glUniformMatrix4dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT4x2: glUniformMatrix4x2dv(input.addressedLocation, 1, false, values); break;
    case GL_DOUBLE_MAT4x3: glUniformMatrix4x3dv(input.addressedLocation, 1, false, values); break;
    default: throw_sequence_error("unsupported addressed double uniform type");
    }
}

static void
clone_shared_mesh_data(MeshInput::Mesh& mesh, const SequenceSharedMeshData& sharedMesh)
{
    mesh.pVerts = nullptr;
    mesh.pTexts = nullptr;
    mesh.pNorms = nullptr;
    mesh.pColrs = nullptr;
    mesh.pIndxs = nullptr;

    if (!sharedMesh.verts.empty()) {
        mesh.pVerts = new float[sharedMesh.verts.size()];
        std::memcpy(mesh.pVerts, sharedMesh.verts.data(), sharedMesh.verts.size() * sizeof(float));
    }
    if (!sharedMesh.texcoords.empty()) {
        mesh.pTexts = new float[sharedMesh.texcoords.size()];
        std::memcpy(mesh.pTexts, sharedMesh.texcoords.data(), sharedMesh.texcoords.size() * sizeof(float));
    }
    if (!sharedMesh.normals.empty()) {
        mesh.pNorms = new float[sharedMesh.normals.size()];
        std::memcpy(mesh.pNorms, sharedMesh.normals.data(), sharedMesh.normals.size() * sizeof(float));
    }
    if (!sharedMesh.colors.empty()) {
        mesh.pColrs = new unsigned char[sharedMesh.colors.size()];
        std::memcpy(mesh.pColrs, sharedMesh.colors.data(), sharedMesh.colors.size() * sizeof(unsigned char));
    }
    if (!sharedMesh.indices.empty()) {
        mesh.pIndxs = new uint32_t[sharedMesh.indices.size()];
        std::memcpy(mesh.pIndxs, sharedMesh.indices.data(), sharedMesh.indices.size() * sizeof(uint32_t));
    }

    mesh.vrtSize  = sharedMesh.vrtSize;
    mesh.texSize  = sharedMesh.texSize;
    mesh.nrmSize  = sharedMesh.nrmSize;
    mesh.clrSize  = sharedMesh.clrSize;
    mesh.idxSize  = sharedMesh.idxSize;
    mesh.numIndxs = sharedMesh.numIndxs;
}

static void
apply_shared_mesh_metadata(MeshInput::Mesh& mesh, const SequenceSharedMeshData& sharedMesh)
{
    mesh.vrtSize  = sharedMesh.vrtSize;
    mesh.texSize  = sharedMesh.texSize;
    mesh.nrmSize  = sharedMesh.nrmSize;
    mesh.clrSize  = sharedMesh.clrSize;
    mesh.idxSize  = sharedMesh.idxSize;
    mesh.numIndxs = sharedMesh.numIndxs;
}

static MeshInput&
ensure_primary_mesh(SequencePass& pass)
{
    if (pass.meshes.empty()) {
        pass.meshes.insert({ "quad", MeshInput() });
    }

    return pass.meshes.begin()->second;
}

static const MeshInput&
require_primary_mesh(const SequencePass& pass)
{
    if (pass.meshes.empty()) {
        throw std::runtime_error("Pass mesh was not initialized");
    }

    return pass.meshes.begin()->second;
}

static void
configure_vertex_array(GLuint vaoId, const MeshInput::VertexBuffer& vertexBuffer)
{
    GLCall(glBindVertexArray(vaoId));

    GLCall(glVertexArrayVertexBuffer(vaoId, 0, vertexBuffer.vboId, 0, 3 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(vaoId, 0, 3, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(vaoId, 0, 0));
    GLCall(glEnableVertexArrayAttrib(vaoId, 0));

    GLCall(glVertexArrayVertexBuffer(vaoId, 1, vertexBuffer.tboId, 0, 2 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(vaoId, 1, 2, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(vaoId, 1, 1));
    GLCall(glEnableVertexArrayAttrib(vaoId, 1));

    GLCall(glVertexArrayVertexBuffer(vaoId, 2, vertexBuffer.nboId, 0, 3 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(vaoId, 2, 3, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(vaoId, 2, 2));
    GLCall(glEnableVertexArrayAttrib(vaoId, 2));

    GLCall(glVertexArrayVertexBuffer(vaoId, 3, vertexBuffer.cboId, 0, 4 * sizeof(unsigned char)));
    GLCall(glVertexArrayAttribIFormat(vaoId, 3, 4, GL_UNSIGNED_BYTE, 0));
    GLCall(glVertexArrayAttribBinding(vaoId, 3, 3));
    GLCall(glEnableVertexArrayAttrib(vaoId, 3));

    GLCall(glVertexArrayElementBuffer(vaoId, vertexBuffer.iboId));
}

static void
delete_vertex_buffers(MeshInput::VertexBuffer& vertexBuffer)
{
    if (vertexBuffer.vaoId) {
        glDeleteVertexArrays(1, &vertexBuffer.vaoId);
    }
    if (vertexBuffer.vboId) {
        glDeleteBuffers(1, &vertexBuffer.vboId);
    }
    if (vertexBuffer.nboId) {
        glDeleteBuffers(1, &vertexBuffer.nboId);
    }
    if (vertexBuffer.tboId) {
        glDeleteBuffers(1, &vertexBuffer.tboId);
    }
    if (vertexBuffer.cboId) {
        glDeleteBuffers(1, &vertexBuffer.cboId);
    }
    if (vertexBuffer.iboId) {
        glDeleteBuffers(1, &vertexBuffer.iboId);
    }

    vertexBuffer = MeshInput::VertexBuffer();
}

static void
assign_default_mesh_data(MeshInput::Mesh& mesh)
{
    mesh.isQuad    = true;
    mesh.Triangles = true;
    mesh.render    = GL_TRIANGLES;
    mesh.pVerts    = const_cast<float*>(RAWGL_DEFAULT_VERTS);
    mesh.pTexts    = const_cast<float*>(RAWGL_DEFAULT_TEXCOORDS);
    mesh.pNorms    = const_cast<float*>(RAWGL_DEFAULT_NORMALS);
    mesh.pColrs    = const_cast<unsigned char*>(RAWGL_DEFAULT_COLORS);
    mesh.pIndxs    = const_cast<uint32_t*>(RAWGL_DEFAULT_INDICES);
    mesh.vrtSize   = static_cast<GLsizei>(sizeof(RAWGL_DEFAULT_VERTS));
    mesh.texSize   = static_cast<GLsizei>(sizeof(RAWGL_DEFAULT_TEXCOORDS));
    mesh.nrmSize   = static_cast<GLsizei>(sizeof(RAWGL_DEFAULT_NORMALS));
    mesh.clrSize   = static_cast<GLsizei>(sizeof(RAWGL_DEFAULT_COLORS));
    mesh.idxSize   = static_cast<GLsizei>(sizeof(RAWGL_DEFAULT_INDICES));
    mesh.numIndxs  = static_cast<GLsizei>(sizeof(RAWGL_DEFAULT_INDICES) / sizeof(RAWGL_DEFAULT_INDICES[0]));
}

static void
load_mesh_data(MeshInput::Mesh& mesh)
{
    if (mesh.isQuad) {
        assign_default_mesh_data(mesh);
        return;
    }

    Timer timer;
    LOG(debug) << "Loading mesh...";

    TriMesh* trimesh = parse_file_with_miniply(mesh.FileName.c_str(), mesh.Triangles);
    if (trimesh == nullptr) {
        throw std::runtime_error("Failed to load mesh");
    }

    mesh.pVerts   = trimesh->pos;
    mesh.pTexts   = trimesh->uv;
    mesh.pNorms   = trimesh->normal;
    mesh.pColrs   = trimesh->color;
    mesh.pIndxs   = trimesh->indices;
    mesh.vrtSize  = static_cast<GLsizei>(trimesh->numVerts * 3 * sizeof(float));
    mesh.texSize  = static_cast<GLsizei>(trimesh->numVerts * 2 * sizeof(float));
    mesh.nrmSize  = static_cast<GLsizei>(trimesh->numVerts * 3 * sizeof(float));
    mesh.clrSize  = static_cast<GLsizei>(trimesh->numVerts * 4 * sizeof(unsigned char));
    mesh.idxSize  = static_cast<GLsizei>(trimesh->numIndices * sizeof(unsigned int));
    mesh.numIndxs = static_cast<GLsizei>(trimesh->numIndices);

    trimesh->pos     = nullptr;
    trimesh->uv      = nullptr;
    trimesh->normal  = nullptr;
    trimesh->color   = nullptr;
    trimesh->indices = nullptr;
    delete trimesh;

    LOG(debug) << "Mesh loading completed in " << timer.nowText();
}

static void
release_mesh_cpu_data(MeshInput::Mesh& mesh)
{
    if (mesh.isQuad) {
        return;
    }

    delete[] mesh.pVerts;
    delete[] mesh.pTexts;
    delete[] mesh.pNorms;
    delete[] mesh.pColrs;
    delete[] mesh.pIndxs;

    mesh.pVerts = nullptr;
    mesh.pTexts = nullptr;
    mesh.pNorms = nullptr;
    mesh.pColrs = nullptr;
    mesh.pIndxs = nullptr;
}

static void
upload_mesh_buffers_only(const MeshInput::Mesh& mesh, MeshInput::VertexBuffer& vertexBuffer)
{
    GLCall(glCreateBuffers(1, &vertexBuffer.vboId));
    GLCall(glNamedBufferData(vertexBuffer.vboId, mesh.vrtSize, static_cast<const void*>(mesh.pVerts), GL_STATIC_DRAW));

    GLCall(glCreateBuffers(1, &vertexBuffer.tboId));
    GLCall(glNamedBufferData(vertexBuffer.tboId, mesh.texSize, static_cast<const void*>(mesh.pTexts), GL_STATIC_DRAW));

    GLCall(glCreateBuffers(1, &vertexBuffer.nboId));
    GLCall(glNamedBufferData(vertexBuffer.nboId, mesh.nrmSize, static_cast<const void*>(mesh.pNorms), GL_STATIC_DRAW));

    GLCall(glCreateBuffers(1, &vertexBuffer.cboId));
    GLCall(glNamedBufferData(vertexBuffer.cboId, mesh.clrSize, static_cast<const void*>(mesh.pColrs), GL_STATIC_DRAW));

    GLCall(glCreateBuffers(1, &vertexBuffer.iboId));
    GLCall(glNamedBufferData(vertexBuffer.iboId, mesh.idxSize, static_cast<const void*>(mesh.pIndxs), GL_STATIC_DRAW));
}

static void
upload_mesh_buffers(const MeshInput::Mesh& mesh, MeshInput::VertexBuffer& vertexBuffer)
{
    GLCall(glCreateVertexArrays(1, &vertexBuffer.vaoId));
    upload_mesh_buffers_only(mesh, vertexBuffer);
    configure_vertex_array(vertexBuffer.vaoId, vertexBuffer);
}

static void
create_shared_mesh_vertex_array(const MeshInput::VertexBuffer& sharedBuffers, MeshInput::VertexBuffer& vertexBuffer)
{
    vertexBuffer = sharedBuffers;
    GLCall(glCreateVertexArrays(1, &vertexBuffer.vaoId));
    configure_vertex_array(vertexBuffer.vaoId, vertexBuffer);
}
}  // namespace

SequenceSharedGpuMesh::~SequenceSharedGpuMesh()
{
    delete_vertex_buffers(vertexBuffer);
}

std::shared_ptr<SequenceSharedGpuMesh>
Sequence_CreateSharedGpuMesh(const SequenceSharedMeshData& sharedMesh)
{
    MeshInput::Mesh mesh;
    mesh.isQuad    = false;
    mesh.Triangles = true;
    mesh.render    = GL_TRIANGLES;
    mesh.pVerts    = const_cast<float*>(sharedMesh.verts.data());
    mesh.pTexts    = const_cast<float*>(sharedMesh.texcoords.data());
    mesh.pNorms    = const_cast<float*>(sharedMesh.normals.data());
    mesh.pColrs    = const_cast<unsigned char*>(sharedMesh.colors.data());
    mesh.pIndxs    = const_cast<uint32_t*>(sharedMesh.indices.data());
    mesh.vrtSize   = sharedMesh.vrtSize;
    mesh.texSize   = sharedMesh.texSize;
    mesh.nrmSize   = sharedMesh.nrmSize;
    mesh.clrSize   = sharedMesh.clrSize;
    mesh.idxSize   = sharedMesh.idxSize;
    mesh.numIndxs  = sharedMesh.numIndxs;

    std::shared_ptr<SequenceSharedGpuMesh> sharedGpuMesh = std::make_shared<SequenceSharedGpuMesh>();
    upload_mesh_buffers_only(mesh, sharedGpuMesh->vertexBuffer);
    return sharedGpuMesh;
}

namespace std {
std::ostream&
operator<<(std::ostream& os, const std::vector<std::string>& vec)
{
    for (auto item : vec)
        os << item << " ";

    return os;
}
}  // namespace std

void
Sequence::buildPassesFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig)
{
    m_passes.clear();
    m_passes.reserve(runtimeConfig.passes.size());

    for (const SequenceRuntimePassConfig& config : runtimeConfig.passes) {
        SequencePass pass(config.program, config.isCompute);
        pass.inputs        = config.inputs;
        pass.inputCounters = config.inputCounters;
        pass.outputs       = config.outputs;
        pass.meshes        = config.meshes;
        pass.cullMode      = config.cullMode;
        pass.size[0]       = config.size[0];
        pass.size[1]       = config.size[1];
        pass.workGroupSize[0] = config.workGroupSize[0];
        pass.workGroupSize[1] = config.workGroupSize[1];
        pass.sizeText[0].clear();
        pass.sizeText[1].clear();
        pass.workGroupSizeText[0].clear();
        pass.workGroupSizeText[1].clear();
        std::memcpy(pass.clearColor, config.clearColor, sizeof(pass.clearColor));
        m_passes.push_back(std::move(pass));
    }
}

Sequence::Sequence(const SequenceRuntimeConfig& runtimeConfig)
{
    initializeFromRuntimeConfig(runtimeConfig);
}

Sequence::~Sequence()
{
    destroyAtomicCounterBuffers();

    for (auto& pass : m_passes) {
        if (pass.fboId) {
            glDeleteFramebuffers(1, &pass.fboId);
        }

        for (auto& meshIt : pass.meshes) {
            MeshInput::VertexBuffer& vertexBuffer = meshIt.second.VBO;
            if (meshIt.second.sharedGpuMesh) {
                if (vertexBuffer.vaoId) {
                    glDeleteVertexArrays(1, &vertexBuffer.vaoId);
                }
                vertexBuffer.vaoId = 0;
            } else {
                delete_vertex_buffers(vertexBuffer);
            }
        }
    }
}

void
Sequence::initializeFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig)
{
    Log_SetVerbosity(std::clamp(runtimeConfig.verbosity, 0, 5));
    LOG(debug) << "Starting RawGL sequence" << std::endl;

    m_textures        = runtimeConfig.sharedTextures;
    m_sharedMeshes    = runtimeConfig.sharedMeshes;
    m_sharedGpuMeshes = runtimeConfig.sharedGpuMeshes;
    m_ioRuntime       = runtimeConfig.ioRuntime;
    buildPassesFromRuntimeConfig(runtimeConfig);

    int passIndex = 0;
    for (auto& pass : m_passes) {
        const size_t initializedCounters = pass.u_aCounters.size();
        const size_t reflectedCounters   = pass.program->BuffersSize();
        if (initializedCounters < reflectedCounters) {
            LOG(debug) << "Pass #" << passIndex << ": " << initializedCounters << " from " << reflectedCounters
                       << " atomic counters are initialized";
        }
        ++passIndex;
    }

    initCommon();
}

void
Sequence::preloadInputTextures()
{
    std::vector<PendingTextureLoad> pendingTextureLoads;
    std::unordered_map<std::string, std::size_t> pendingTextureIndex;

    for (auto& pass : m_passes) {
        for (auto& inputIt : pass.inputs) {
            PassInput& input = inputIt.second;

            switch (input.uniform->type) {
            case GL_SAMPLER_2D:
            case GL_IMAGE_2D: break;
            default: continue;
            }

            if (input.texture && input.path.empty() && !input.runtimeTextureBindingRequired) {
                continue;
            }

            if (input.runtimeTextureBindingRequired && input.path.empty()) {
                continue;
            }

            if (input.path.find("::") != std::string::npos) {
                continue;
            }

            const std::string textureKey = make_disk_texture_key(input.path, input.attributes);
            if (m_textures.find(textureKey) != m_textures.end()) {
                continue;
            }

            if (pendingTextureIndex.find(textureKey) != pendingTextureIndex.end()) {
                continue;
            }

            PendingTextureLoad pendingLoad;
            pendingLoad.key    = textureKey;
            pendingLoad.future = std::async(std::launch::async,
                                            [this](std::string path, std::map<std::string, std::string> attributes) {
                                                return resolve_io_runtime(m_ioRuntime).loadTextureFileData(path, attributes);
                                            },
                                            input.path,
                                            input.attributes);
            pendingTextureIndex.insert({ textureKey, pendingTextureLoads.size() });
            pendingTextureLoads.push_back(std::move(pendingLoad));
        }
    }

    for (auto& pendingLoad : pendingTextureLoads) {
        rawgl::io::LoadedTextureData textureData;

        try {
            textureData = pendingLoad.future.get();
        } catch (const std::exception& exception) {
            throw_sequence_error(std::string("Failed to prepare texture: ") + exception.what());
        }

        if (!textureData.valid) {
            throw_sequence_error("Failed to load an input texture.");
        }

        m_textures.insert({ pendingLoad.key,
                            std::make_shared<Texture>(textureData.width, textureData.height, textureData.internalFormat,
                                                      textureData.type,
                                                      textureData.bytes.empty() ? nullptr : textureData.bytes.data(),
                                                      textureData.alphaChannel) });
    }
}

void
Sequence::ensurePassOutputTextures(SequencePass& pass, int passIndex)
{
    for (auto& outputIt : pass.outputs) {
        PassOutput& output = outputIt.second;

        if (output.texture) {
            continue;
        }

        if (output.alphaChannel >= output.channels) {
            throw_sequence_error("out_alpha_channel (" + outputIt.first
                                 + "): index exceeds max channel index for this image.");
        }

        const std::string textureName                  = outputIt.first + "::" + std::to_string(passIndex);
        const std::pair<std::string, GLenum> formats[] = {
            { "rgba8", GL_RGBA8 }, { "rgba16", GL_RGBA16 }, { "rgba16f", GL_RGBA16F }, { "rgba32f", GL_RGBA32F },
            { "r8", GL_R8 },       { "r16", GL_R16 },       { "r16f", GL_R16F },       { "r32f", GL_R32F },
            { "rg8", GL_RG8 },     { "rg16", GL_RG16 },     { "rg16f", GL_RG16F },     { "rg32f", GL_RG32F },
            { "rgb8", GL_RGB8 },   { "rgb16", GL_RGB16 },   { "rgb16f", GL_RGB16F },   { "rgb32f", GL_RGB32F }
        };

        GLenum internalFormat = GL_RGBA32F;
        int formatIndex       = 0;

        for (; formatIndex < 16; ++formatIndex) {
            if (output.internalFormatText == formats[formatIndex].first) {
                break;
            }
        }

        if (formatIndex == 16) {
            LOG(warning) << "Pass " << passIndex << ": unknown output framebuffer format "
                         << output.internalFormatText << " changing to rgba32f.";
        } else {
            if (formatIndex > 3 && pass.isCompute) {
                formatIndex %= 4;
                LOG(warning)
                    << "Pass " << passIndex
                    << ": only 4-component output framebuffer formats are allowed for compute shaders, changing to "
                    << formats[formatIndex].first;
            }

            internalFormat = formats[formatIndex].second;
        }

        auto textureIt = m_textures
                             .insert({ textureName,
                                       std::make_shared<Texture>(pass.size[0], pass.size[1], internalFormat, GL_FLOAT,
                                                                 nullptr, output.alphaChannel) })
                             .first;
        output.texture = textureIt->second;

        if (!pass.isCompute) {
            GLCall(glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId));
            const GLuint outputLocation = resolve_fragment_output_location(output);
            LOG(debug) << "Pass " << passIndex << ": attaching output " << outputLocation << " "
                       << outputIt.first << " to FBO";
            GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + outputLocation, GL_TEXTURE_2D,
                                          textureIt->second->getId(), 0));
        }
    }

    if (!pass.isCompute) {
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            throw_sequence_error("Pass " + std::to_string(passIndex) + ": unable to setup FBO.");
        }

        LOG(debug) << "Pass " << passIndex << ": FBO created successfully.";
    }
}

void
Sequence::refreshPassTextureInputs(SequencePass& pass)
{
    for (auto& inputIt : pass.inputs) {
        PassInput& input = inputIt.second;

        switch (input.uniform->type) {
        case GL_SAMPLER_2D:
        case GL_IMAGE_2D: break;
        default: continue;
        }

        if (input.texture && input.path.empty() && !input.runtimeTextureBindingRequired) {
            continue;
        }

        if (input.runtimeTextureBindingRequired && input.path.empty()) {
            continue;
        }

        const std::string textureKey = input.path.find("::") == std::string::npos
                                           ? make_disk_texture_key(input.path, input.attributes)
                                           : input.path;

        auto textureIt = m_textures.find(textureKey);
        if (textureIt == m_textures.end()) {
            throw_sequence_error("input (" + inputIt.first + "): referenced texture is missing.");
        }

        input.texture = textureIt->second;
    }
}

void
Sequence::prepareRunTextures()
{
    if (!m_runTexturesDirty) {
        return;
    }

    for (int passIndex = 0; passIndex < static_cast<int>(m_passes.size()); ++passIndex) {
        ensurePassOutputTextures(m_passes[passIndex], passIndex);
    }

    for (SequencePass& pass : m_passes) {
        refreshPassTextureInputs(pass);
    }

    m_runTexturesDirty = false;
}

void
Sequence::initializePass(SequencePass& pass, int passIndex)
{
    MeshInput& meshInput = ensure_primary_mesh(pass);

    for (int axis = 0; axis < 2; ++axis) {
        if (pass.sizeText[axis].empty()) {
            if (pass.size[axis] <= 0) {
                throw_sequence_error("pass_size (" + std::to_string(passIndex) + "): value must be > 0");
            }
            continue;
        }

        if (pass.sizeText[axis].find("::") == std::string::npos) {
            pass.size[axis]
                = parse_checked_positive_int(pass.sizeText[axis],
                                             "pass_size (" + std::to_string(passIndex) + ")");
            continue;
        }

        const std::size_t split        = pass.sizeText[axis].find("::");
        const std::string refInputName = pass.sizeText[axis].substr(0, split);
        const int refPassIndex
            = parse_checked_numeric<int32_t>(pass.sizeText[axis].substr(split + 2),
                                             "pass_size (" + std::to_string(passIndex) + ")");

        if (refInputName.empty()) {
            throw_sequence_error("pass_size (" + std::to_string(passIndex) + "): empty referenced input name.");
        }

        if (refPassIndex < 0 || refPassIndex >= static_cast<int>(m_passes.size())) {
            throw_sequence_error("pass_size (" + std::to_string(passIndex)
                                 + "): wrong referenced pass index " + std::to_string(refPassIndex));
        }

        const SequencePass& refPass = m_passes[refPassIndex];
        auto refInputIt     = refPass.inputs.find(refInputName);

        if (refInputIt == refPass.inputs.end()) {
            throw_sequence_error("pass_size (" + std::to_string(passIndex) + "): input " + refInputName
                                 + " not found in referenced pass " + std::to_string(refPassIndex));
        }

        pass.size[axis] = axis == 0 ? refInputIt->second.texture->getWidth()
                                    : refInputIt->second.texture->getHeight();
    }

    LOG(debug) << "Pass " << passIndex << ": pass_size is " << pass.size[0] << " x " << pass.size[1];

    if (pass.isCompute) {
        for (int axis = 0; axis < 2; ++axis) {
            if (pass.workGroupSizeText[axis].empty()) {
                if (pass.workGroupSize[axis] <= 0) {
                    throw_sequence_error("pass_workgroupsize (" + std::to_string(passIndex) + "): value must be > 0");
                }
                continue;
            }
            pass.workGroupSize[axis]
                = parse_checked_positive_int(pass.workGroupSizeText[axis],
                                             "pass_workgroupsize (" + std::to_string(passIndex) + ")");
        }
    } else {
        GLCall(glGenFramebuffers(1, &pass.fboId));
        GLCall(glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId));
        pass.glbObject.FBO.push_back(SequencePass::FBOobject { pass.fboId });
    }

    ensurePassOutputTextures(pass, passIndex);
    refreshPassTextureInputs(pass);

    try {
        if (meshInput.mesh.isQuad) {
            load_mesh_data(meshInput.mesh);
        } else {
            const auto sharedMeshIt = m_sharedMeshes.find(make_mesh_cache_key(meshInput.mesh));
            const auto sharedGpuMeshIt = m_sharedGpuMeshes.find(make_mesh_cache_key(meshInput.mesh));
            if (sharedGpuMeshIt != m_sharedGpuMeshes.end() && sharedGpuMeshIt->second) {
                if (sharedMeshIt != m_sharedMeshes.end() && sharedMeshIt->second) {
                    apply_shared_mesh_metadata(meshInput.mesh, *sharedMeshIt->second);
                }
                meshInput.sharedGpuMesh = sharedGpuMeshIt->second;
                create_shared_mesh_vertex_array(sharedGpuMeshIt->second->vertexBuffer, meshInput.VBO);
                return;
            }

            if (sharedMeshIt != m_sharedMeshes.end() && sharedMeshIt->second) {
                clone_shared_mesh_data(meshInput.mesh, *sharedMeshIt->second);
            } else {
                load_mesh_data(meshInput.mesh);
            }
        }
    } catch (const std::exception& exception) {
        throw_sequence_error(exception.what());
    }

    upload_mesh_buffers(meshInput.mesh, meshInput.VBO);
    release_mesh_cpu_data(meshInput.mesh);
}

void
Sequence::initCommon()
{
    preloadInputTextures();

    for (int passIndex = 0; passIndex < static_cast<int>(m_passes.size()); ++passIndex) {
        initializePass(m_passes[passIndex], passIndex);
    }

    validatePassSetup();
    buildExecutionPlan();

    for (SequencePass& pass : m_passes) {
        initializePassAtomicCounters(pass);
    }

    m_runTexturesDirty = false;
}

void
Sequence::validatePassSetup() const
{
    for (const SequencePass& pass : m_passes) {
        const MeshInput& mesh = require_primary_mesh(pass);
        (void)mesh;

        for (const auto& inputIt : pass.inputs) {
            const PassInput& input = inputIt.second;
            if (input.uniform == nullptr) {
                throw_sequence_error("input (" + inputIt.first + "): uniform is not initialized.");
            }

            if ((input.uniform->type == GL_SAMPLER_2D || input.uniform->type == GL_IMAGE_2D)
                && input.texture == nullptr
                && !input.runtimeTextureBindingRequired) {
                throw_sequence_error("input (" + inputIt.first + "): texture is not initialized.");
            }
        }

        for (const auto& outputIt : pass.outputs) {
            const PassOutput& output = outputIt.second;
            if (output.texture == nullptr) {
                throw_sequence_error("output (" + outputIt.first + "): texture is not initialized.");
            }

            if (pass.isCompute) {
                if (output.uniform == nullptr) {
                    throw_sequence_error("output (" + outputIt.first + "): output image uniform is not initialized.");
                }
            } else if (output.output == nullptr) {
                throw_sequence_error("output (" + outputIt.first + "): fragment output is not initialized.");
            }
        }
    }
}

void
Sequence::buildExecutionPlan()
{
    m_executionPlan.clear();
    m_executionPlan.reserve(m_passes.size());

    for (int passIndex = 0; passIndex < static_cast<int>(m_passes.size()); ++passIndex) {
        SequencePass& pass = m_passes[passIndex];
        PassExecutionPlan plan;
        plan.pass       = &pass;
        plan.primaryMesh = &require_primary_mesh(pass);
        plan.passIndex   = passIndex;
        plan.inputs.reserve(pass.inputs.size());
        plan.outputs.reserve(pass.outputs.size());

        for (auto& inputIt : pass.inputs) {
            plan.inputs.push_back(PlannedInputBinding { &inputIt.first, &inputIt.second });
        }

        for (auto& outputIt : pass.outputs) {
            plan.outputs.push_back(PlannedOutputBinding { &outputIt.second });
        }

        m_executionPlan.push_back(std::move(plan));
    }
}

int
Sequence::bindPassInputs(const PassExecutionPlan& plan,
                         const std::vector<SequenceExecutionInputOverride>& inputOverrides)
{
    SequencePass& pass = *plan.pass;
    int textureIndex = 0;

    for (const PlannedInputBinding& binding : plan.inputs) {
        PassInput& input = *binding.input;
        const SequenceExecutionInputOverride* inputOverride =
            find_input_override(inputOverrides, static_cast<size_t>(plan.passIndex), *binding.name);
        std::shared_ptr<Texture> boundTexture = input.texture;
        if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::texture) {
            boundTexture = inputOverride->texture;
        }
        if ((input.uniform->type == GL_SAMPLER_2D || input.uniform->type == GL_IMAGE_2D) && !boundTexture) {
            throw_sequence_error("input (" + *binding.name + "): runtime texture binding is missing.");
        }

        switch (input.uniform->type) {
        case GL_SAMPLER_2D: {
            const GLuint textureId = boundTexture->getId();

            GLCall(glActiveTexture(GL_TEXTURE0 + textureIndex));
            GLCall(glBindTexture(GL_TEXTURE_2D, textureId));

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, input.tex_min);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, input.tex_mag);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, input.tex_s);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, input.tex_t);

            if (input.tex_min != GL_LINEAR && input.tex_min != GL_NEAREST) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0);
                GLCall(glGenerateTextureMipmap(textureId));
                LOG(debug) << "Generated mip-maps for " << *binding.name << " at " << boundTexture;
            }

            input.uniform->set(textureIndex++);
            break;
        }
        case GL_IMAGE_2D: {
            const GLuint textureId = boundTexture->getId();
            GLCall(glBindImageTexture(textureIndex, textureId, 0, GL_FALSE, 0, GL_READ_ONLY,
                                      boundTexture->getInternalFormat()));
            LOG(debug) << "Image " << textureId << " binding is " << textureIndex;
            input.uniform->set(textureIndex++);
            break;
        }
        case GL_BOOL:
        case GL_BOOL_VEC2:
        case GL_BOOL_VEC3:
        case GL_BOOL_VEC4:
        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::intValues) {
                if (input.usesArrayElement) {
                    set_addressed_int_uniform(input, inputOverride->intValues.data());
                } else {
                    input.uniform->set(inputOverride->intValues.data());
                }
            } else {
                if (input.usesArrayElement) {
                    set_addressed_int_uniform(input, &input.ints[0]);
                } else {
                    input.uniform->set(&input.ints[0]);
                }
            }
            break;
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::uintValues) {
                if (input.usesArrayElement) {
                    set_addressed_uint_uniform(input, inputOverride->uintValues.data());
                } else {
                    input.uniform->set(inputOverride->uintValues.data());
                }
            } else {
                if (input.usesArrayElement) {
                    set_addressed_uint_uniform(input, &input.uints[0]);
                } else {
                    input.uniform->set(&input.uints[0]);
                }
            }
            break;
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::floatValues) {
                if (input.usesArrayElement) {
                    set_addressed_float_uniform(input, inputOverride->floatValues.data());
                } else {
                    input.uniform->set(inputOverride->floatValues.data());
                }
            } else {
                if (input.usesArrayElement) {
                    set_addressed_float_uniform(input, &input.floats[0]);
                } else {
                    input.uniform->set(&input.floats[0]);
                }
            }
            break;
        case GL_DOUBLE:
        case GL_DOUBLE_VEC2:
        case GL_DOUBLE_VEC3:
        case GL_DOUBLE_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::doubleValues) {
                if (input.usesArrayElement) {
                    set_addressed_double_uniform(input, inputOverride->doubleValues.data());
                } else {
                    input.uniform->set(inputOverride->doubleValues.data());
                }
            } else {
                if (input.usesArrayElement) {
                    set_addressed_double_uniform(input, &input.doubles[0]);
                } else {
                    input.uniform->set(&input.doubles[0]);
                }
            }
            break;
        default: input.uniform->set(&input.floats[0]); break;
        }
    }

    return textureIndex;
}

void
Sequence::bindInternalUniforms(const PassExecutionPlan& plan, const SequenceSystemUniformState& systemUniforms)
{
    SequencePass& pass = *plan.pass;
    const GLuint framebufferSizeUint[2] = { static_cast<GLuint>(pass.size[0]), static_cast<GLuint>(pass.size[1]) };
    const GLint framebufferSizeInt[2]   = { pass.size[0], pass.size[1] };
    const GLfloat framebufferSizeFloat[2] = { static_cast<GLfloat>(pass.size[0]), static_cast<GLfloat>(pass.size[1]) };
    const GLdouble framebufferSizeDouble[2] = { static_cast<GLdouble>(pass.size[0]),
                                                static_cast<GLdouble>(pass.size[1]) };
    const GLfloat framebufferAspectFloat     = pass.size[0] / static_cast<GLfloat>(pass.size[1]);
    const GLdouble framebufferAspectDouble   = pass.size[0] / static_cast<GLdouble>(pass.size[1]);
    const GLint isQuadInt                    = plan.primaryMesh->mesh.isQuad ? 1 : 0;
    const GLuint isQuadUint                  = plan.primaryMesh->mesh.isQuad ? 1u : 0u;
    const GLfloat isQuadFloat                = plan.primaryMesh->mesh.isQuad ? 1.0f : 0.0f;
    const GLdouble isQuadDouble              = plan.primaryMesh->mesh.isQuad ? 1.0 : 0.0;
    const GLint frameInt                     = systemUniforms.frameNumber;
    const GLuint frameUint                   = static_cast<GLuint>(std::max(systemUniforms.frameNumber, 0));
    const GLfloat timeFloat                  = static_cast<GLfloat>(systemUniforms.timeSeconds);
    const GLdouble timeDouble                = static_cast<GLdouble>(systemUniforms.timeSeconds);
    const GLfloat deltaTimeFloat             = static_cast<GLfloat>(systemUniforms.deltaTimeSeconds);
    const GLdouble deltaTimeDouble           = static_cast<GLdouble>(systemUniforms.deltaTimeSeconds);
    const GLint passIndexInt = (systemUniforms.passIndex >= 0) ? systemUniforms.passIndex : plan.passIndex;
    const GLuint passIndexUint = static_cast<GLuint>(std::max(passIndexInt, 0));

    GLProgramUniform* uniform = pass.program->findUniform("iFBsize");
    if (uniform) {
        switch (uniform->type) {
        case GL_INT_VEC2:
        case GL_BOOL_VEC2: uniform->set(framebufferSizeInt); break;
        case GL_UNSIGNED_INT_VEC2: uniform->set(framebufferSizeUint); break;
        case GL_FLOAT_VEC2: uniform->set(framebufferSizeFloat); break;
        case GL_DOUBLE_VEC2: uniform->set(framebufferSizeDouble); break;
        default: break;
        }
    }

    uniform = pass.program->findUniform("iFBaspect");
    if (uniform) {
        switch (uniform->type) {
        case GL_DOUBLE: uniform->set(framebufferAspectDouble); break;
        case GL_FLOAT: uniform->set(framebufferAspectFloat); break;
        default: break;
        }
    }

    uniform = pass.program->findUniform("isQuad");
    if (uniform) {
        switch (uniform->type) {
        case GL_BOOL:
        case GL_INT: uniform->set(isQuadInt); break;
        case GL_UNSIGNED_INT: uniform->set(isQuadUint); break;
        case GL_FLOAT: uniform->set(isQuadFloat); break;
        case GL_DOUBLE: uniform->set(isQuadDouble); break;
        default: break;
        }
    }

    uniform = pass.program->findUniform("iTime");
    if (uniform) {
        switch (uniform->type) {
        case GL_DOUBLE: uniform->set(timeDouble); break;
        case GL_FLOAT: uniform->set(timeFloat); break;
        default: break;
        }
    }

    uniform = pass.program->findUniform("iTimeDelta");
    if (uniform) {
        switch (uniform->type) {
        case GL_DOUBLE: uniform->set(deltaTimeDouble); break;
        case GL_FLOAT: uniform->set(deltaTimeFloat); break;
        default: break;
        }
    }

    uniform = pass.program->findUniform("iFrame");
    if (uniform) {
        switch (uniform->type) {
        case GL_BOOL:
        case GL_INT: uniform->set(frameInt); break;
        case GL_UNSIGNED_INT: uniform->set(frameUint); break;
        case GL_FLOAT: uniform->set(static_cast<GLfloat>(frameInt)); break;
        case GL_DOUBLE: uniform->set(static_cast<GLdouble>(frameInt)); break;
        default: break;
        }
    }

    uniform = pass.program->findUniform("iPassIndex");
    if (uniform) {
        switch (uniform->type) {
        case GL_BOOL:
        case GL_INT: uniform->set(passIndexInt); break;
        case GL_UNSIGNED_INT: uniform->set(passIndexUint); break;
        case GL_FLOAT: uniform->set(static_cast<GLfloat>(passIndexInt)); break;
        case GL_DOUBLE: uniform->set(static_cast<GLdouble>(passIndexInt)); break;
        default: break;
        }
    }
}

void
Sequence::initializePassAtomicCounters(SequencePass& pass)
{
    pass.u_aCounters.clear();
    pass.capturedAtomicCounterValues.clear();

    auto& pass_acounters = pass.program->get_m_acounters();
    std::unordered_set<std::string> passUserInputCounters;

    for (auto& inputCounterIt : pass.inputCounters) {
        auto passCounter = pass_acounters.find(inputCounterIt.first);

        if (passCounter == pass_acounters.end()) {
            throw_sequence_error("Atomic counter " + inputCounterIt.first + " is not used in the shader");
        }

        auto u_counterIt           = pass.u_aCounters.insert({ passCounter->second->binding, passCounters() });
        u_counterIt->second.buffer = passCounter->second;

        u_counterIt->second.value.resize(passCounter->second->size);
        u_counterIt->second.result.resize(passCounter->second->size);

        if (inputCounterIt.second.value.size() > u_counterIt->second.value.size()) {
            throw_sequence_error("Atomic counter " + inputCounterIt.first + " has more values than the shader");
        }

        const std::size_t copyCount = std::min(inputCounterIt.second.value.size(), u_counterIt->second.value.size());
        std::memcpy(u_counterIt->second.value.data(), inputCounterIt.second.value.data(), copyCount * sizeof(GLuint));

        u_counterIt->second.passIn = pass.fboId;
        passUserInputCounters.insert(inputCounterIt.first);
    }

    for (const std::pair<const std::string, std::shared_ptr<GLProgramBuffers>>& counterIt : pass_acounters) {
        if (passUserInputCounters.find(counterIt.first) != passUserInputCounters.end()) {
            continue;
        }

        auto u_counterIt           = pass.u_aCounters.insert({ counterIt.second->binding, passCounters() });
        u_counterIt->second.buffer = counterIt.second;
        u_counterIt->second.value.resize(counterIt.second->size);
        u_counterIt->second.result.resize(counterIt.second->size);
        u_counterIt->second.passIn = pass.fboId;

        LOG(trace) << "Atomic counter " << counterIt.first << " binding is " << counterIt.second->binding << std::endl;
    }

    auto it = pass.u_aCounters.begin();
    while (it != pass.u_aCounters.end()) {
        GLuint bufferSize = 0;
        auto range        = pass.u_aCounters.equal_range(it->first);

        for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
            const GLuint groupSize =
                groupIt->second.buffer->offset + groupIt->second.buffer->size * sizeof(GLuint);
            bufferSize = std::max(bufferSize, groupSize);
        }

        GLuint bufferId = 0;
        GLCall(glCreateBuffers(1, &bufferId));
        GLCall(glNamedBufferData(bufferId, bufferSize, nullptr, GL_DYNAMIC_DRAW));

        for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
            groupIt->second.bufferID = bufferId;
        }

        it = range.second;
    }
}

void
Sequence::preparePassAtomicCounters(SequencePass& pass)
{
    pass.capturedAtomicCounterValues.clear();

    for (auto& counterIt : pass.u_aCounters) {
        std::fill(counterIt.second.value.begin(), counterIt.second.value.end(), 0u);
    }

    for (const auto& inputCounterIt : pass.inputCounters) {
        bool foundPreparedCounter = false;

        for (auto& preparedCounterIt : pass.u_aCounters) {
            if (!preparedCounterIt.second.buffer || preparedCounterIt.second.buffer->name != inputCounterIt.first) {
                continue;
            }

            if (inputCounterIt.second.value.size() > preparedCounterIt.second.value.size()) {
                throw_sequence_error("Atomic counter " + inputCounterIt.first + " has more values than the shader");
            }

            const std::size_t copyCount =
                std::min(inputCounterIt.second.value.size(), preparedCounterIt.second.value.size());
            std::memcpy(preparedCounterIt.second.value.data(),
                        inputCounterIt.second.value.data(),
                        copyCount * sizeof(GLuint));
            foundPreparedCounter = true;
            break;
        }

        if (!foundPreparedCounter) {
            throw_sequence_error("Atomic counter " + inputCounterIt.first + " is not used in the shader");
        }
    }
}

void
Sequence::bindPassAtomicCounters(SequencePass& pass)
{
    LOG(trace) << "Binding atomic counters" << std::endl;

    auto it = pass.u_aCounters.begin();
    while (it != pass.u_aCounters.end()) {
        GLuint buff_size = 0;
        auto range       = pass.u_aCounters.equal_range(it->first);
        const GLuint bufferId = it->second.bufferID;

        LOG(trace) << "Binding: " << it->first << " have " << std::distance(range.first, range.second)
                   << " counter[s]." << std::endl;

        for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
            const GLuint groupSize =
                groupIt->second.buffer->offset + groupIt->second.buffer->size * sizeof(GLuint);
            buff_size = std::max(buff_size, groupSize);
            LOG(trace) << groupIt->second.buffer->name << " buff_size: " << buff_size / sizeof(GLuint) << std::endl;
        }

        for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
            auto buffer = groupIt->second.buffer;
            GLCall(glNamedBufferSubData(bufferId, buffer->offset, sizeof(GLuint) * buffer->size,
                                        groupIt->second.value.data()));
            LOG(trace) << buffer->name << " offset: " << buffer->offset << " size: " << buffer->size << std::endl;

            buffer->isSet            = true;
        }

        GLCall(glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, it->first, bufferId, 0, buff_size));
        it = range.second;
    }

}

void
Sequence::capturePassAtomicCounterResults(SequencePass& pass)
{
    if (pass.u_aCounters.empty()) {
        return;
    }

    GLCall(glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT));

    for (auto& counterIt : pass.u_aCounters) {
        if (counterIt.second.bufferID == 0 || !counterIt.second.buffer) {
            continue;
        }

        const GLsizeiptr byteCount = static_cast<GLsizeiptr>(sizeof(GLuint) * counterIt.second.buffer->size);
        counterIt.second.result.resize(counterIt.second.buffer->size);
        GLCall(glGetNamedBufferSubData(counterIt.second.bufferID,
                                       counterIt.second.buffer->offset,
                                       byteCount,
                                       counterIt.second.result.data()));
        pass.capturedAtomicCounterValues[counterIt.second.buffer->name] = counterIt.second.result;
    }
}

void
Sequence::executeComputePass(const PassExecutionPlan& plan, int textureIndex)
{
    SequencePass& pass = *plan.pass;

    for (const PlannedOutputBinding& binding : plan.outputs) {
        PassOutput& output = *binding.output;

        GLCall(glBindImageTexture(textureIndex, output.texture->getId(), 0, GL_FALSE, 0, GL_WRITE_ONLY,
                                  output.texture->getInternalFormat()));

        LOG(debug) << "Texture " << output.texture->getId() << " binding is " << textureIndex;
        output.uniform->set(textureIndex++);
    }

    GLCall(glDispatchCompute((pass.size[0] + pass.workGroupSize[0] - 1) / pass.workGroupSize[0],
                             (pass.size[1] + pass.workGroupSize[1] - 1) / pass.workGroupSize[1], 1));
    GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT
                           | GL_TEXTURE_UPDATE_BARRIER_BIT));
}

void
Sequence::executeGraphicsPass(const PassExecutionPlan& plan)
{
    SequencePass& pass = *plan.pass;
    GLCall(glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId));

    std::vector<GLenum> buffers(8, GL_NONE);
    for (const PlannedOutputBinding& binding : plan.outputs) {
        PassOutput& output          = *binding.output;
        const GLuint outputLocation = resolve_fragment_output_location(output);
        buffers[outputLocation]     = GL_COLOR_ATTACHMENT0 + outputLocation;
    }

    GLuint depthBuffer = 0;
    GLCall(glCreateRenderbuffers(1, &depthBuffer));
    GLCall(glNamedRenderbufferStorage(depthBuffer, GL_DEPTH_COMPONENT, pass.size[0], pass.size[1]));
    GLCall(glNamedFramebufferRenderbuffer(pass.fboId, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer));

    GLCall(glDrawBuffers((GLsizei)buffers.size(), &buffers[0]));
    GLCall(glViewport(0, 0, pass.size[0], pass.size[1]));

    GLCall(glClearColor(pass.clearColor[0], pass.clearColor[1], pass.clearColor[2], pass.clearColor[3]));
    GLCall(glFrontFace(pass.cullMode.windOrder));
    GLCall(glCullFace(pass.cullMode.cullFace));

    if (pass.cullMode.cullFaceEnable) {
        GLCall(glEnable(GL_CULL_FACE));
    } else {
        GLCall(glDisable(GL_CULL_FACE));
    }

    GLCall(glEnable(GL_DEPTH_TEST));
    GLCall(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    GLCall(glDepthFunc(GL_LEQUAL));
    GLCall(glBindVertexArray(plan.primaryMesh->VBO.vaoId));
    GLCall(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
    GLCall(glDrawElements(plan.primaryMesh->mesh.render, plan.primaryMesh->mesh.numIndxs, GL_UNSIGNED_INT, 0));

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        GLCall(glDeleteRenderbuffers(1, &depthBuffer));
        throw_sequence_error("OpenGL error: " + std::to_string(err));
    }

    GLCall(glDeleteRenderbuffers(1, &depthBuffer));
    GLCall(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT
                           | GL_TEXTURE_UPDATE_BARRIER_BIT));
}

void
Sequence::destroyAtomicCounterBuffers()
{
    for (auto& pass : m_passes) {
        std::unordered_set<GLuint> deletedBufferIds;
        for (auto& counterIt : pass.u_aCounters) {
            if (counterIt.second.bufferID == 0) {
                continue;
            }

            if (deletedBufferIds.insert(counterIt.second.bufferID).second) {
                GLCall(glDeleteBuffers(1, &counterIt.second.bufferID));
            }
        }

        pass.u_aCounters.clear();
    }
}

void
Sequence::run()
{
    run(SequenceSystemUniformState {}, {});
}

void
Sequence::run(const SequenceSystemUniformState& systemUniforms)
{
    run(systemUniforms, {});
}

void
Sequence::run(const SequenceSystemUniformState& systemUniforms,
              const std::vector<SequenceExecutionInputOverride>& inputOverrides)
{
    Timer timer;

    LOG(debug) << "Rendering...";

    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

    try {
        prepareRunTextures();

        for (const PassExecutionPlan& plan : m_executionPlan) {
            SequencePass& pass = *plan.pass;
            GLCall(glUseProgram(pass.program->getId()));

            const int textureIndex = bindPassInputs(plan, inputOverrides);
            bindInternalUniforms(plan, systemUniforms);
            preparePassAtomicCounters(pass);
            bindPassAtomicCounters(pass);

            if (pass.isCompute) {
                executeComputePass(plan, textureIndex);
            } else {
                executeGraphicsPass(plan);
            }

            capturePassAtomicCounterResults(pass);
        }
    } catch (...) {
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);
        throw;
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    LOG(debug) << "Sequence completed in " << timer.nowText();
}

std::shared_ptr<Texture>
Sequence::getPassOutputTexture(size_t passIndex, const std::string& outputName) const
{
    if (passIndex >= m_passes.size()) {
        return nullptr;
    }

    auto outputIt = m_passes[passIndex].outputs.find(outputName);
    if (outputIt == m_passes[passIndex].outputs.end()) {
        return nullptr;
    }

    return outputIt->second.texture;
}

std::vector<GLuint>
Sequence::getPassAtomicCounterValues(size_t passIndex, const std::string& counterName) const
{
    if (passIndex >= m_passes.size()) {
        return {};
    }

    const SequencePass& pass = m_passes[passIndex];
    const auto capturedIt = pass.capturedAtomicCounterValues.find(counterName);
    if (capturedIt != pass.capturedAtomicCounterValues.end()) {
        return capturedIt->second;
    }

    const auto inputIt = pass.inputCounters.find(counterName);
    if (inputIt != pass.inputCounters.end()) {
        return inputIt->second.value;
    }

    return {};
}

void
Sequence::setPassAtomicCounterValues(size_t passIndex, const std::string& counterName, const std::vector<GLuint>& values)
{
    if (passIndex >= m_passes.size()) {
        throw_sequence_error("invalid pass index for atomic counter override");
    }

    SequencePass& pass = m_passes[passIndex];
    std::shared_ptr<GLProgramBuffers> counter = pass.program->findCounter(counterName);
    if (!counter) {
        throw_sequence_error("atomic counter " + counterName + " is not used in the shader");
    }

    if (values.size() > static_cast<size_t>(counter->size)) {
        throw_sequence_error("atomic counter " + counterName + " has more values than the shader");
    }

    auto& inputCounter = pass.inputCounters[counterName];
    inputCounter.size  = counter->size;
    inputCounter.value.assign(counter->size, 0u);
    std::copy(values.begin(), values.end(), inputCounter.value.begin());
}

void
Sequence::releaseRunOutputTextures()
{
    for (size_t passIndex = 0; passIndex < m_passes.size(); ++passIndex) {
        SequencePass& pass = m_passes[passIndex];
        for (auto& outputIt : pass.outputs) {
            const std::string textureKey = outputIt.first + "::" + std::to_string(passIndex);

            auto textureIt = m_textures.find(textureKey);
            if (textureIt != m_textures.end()) {
                m_textures.erase(textureIt);
            }

            outputIt.second.texture.reset();
        }
    }

    for (auto& pass : m_passes) {
        for (auto& inputIt : pass.inputs) {
            PassInput& input = inputIt.second;
            if (input.path.find("::") != std::string::npos) {
                input.texture.reset();
            }
        }
    }

    m_runTexturesDirty = true;
}

