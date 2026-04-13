/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by   //-V1042
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "sequence.h"

#include "opengl_utils.h"
#include "timer.h"
#include "log.h"
#include "gl_program_manager.h"
#include "image_utils.h"

#include <charconv>
#include <future>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>

#include <termcolor/termcolor.hpp>

#include "mesh_io.h"

namespace {
enum class ParsedOptionMode {
    flag,
    single,
    multi,
};

struct ParsedOptionSpec {
    const char* long_key;
    char short_key;
    ParsedOptionMode mode;
};

static const ParsedOptionSpec RAWGL_OPTION_SPECS[] = {
    { "help", 'h', ParsedOptionMode::flag },
    { "version", 'v', ParsedOptionMode::flag },
    { "verbosity", 'V', ParsedOptionMode::single },
    { "pass_vertfrag", 'P', ParsedOptionMode::multi },
    { "pass_comp", 'C', ParsedOptionMode::single },
    { "pass_size", 'S', ParsedOptionMode::multi },
    { "pass_workgroupsize", 'W', ParsedOptionMode::multi },
    { "bg_color", '\0', ParsedOptionMode::multi },
    { "cull", '\0', ParsedOptionMode::multi },
    { "pass_mesh", 'M', ParsedOptionMode::multi },
    { "in", 'i', ParsedOptionMode::multi },
    { "atomic", 'B', ParsedOptionMode::multi },
    { "in_attr", 't', ParsedOptionMode::multi },
    { "out", 'o', ParsedOptionMode::multi },
    { "out_format", 'f', ParsedOptionMode::single },
    { "out_attr", 'r', ParsedOptionMode::multi },
    { "out_channels", 'n', ParsedOptionMode::single },
    { "out_alpha_channel", 'a', ParsedOptionMode::single },
    { "out_bits", 'b', ParsedOptionMode::single },
};

static const ParsedOptionSpec*
find_option_spec(const std::string& token)
{
    if (token.size() > 2 && token[0] == '-' && token[1] == '-') {
        const std::string option_name = token.substr(2);
        for (const ParsedOptionSpec& spec : RAWGL_OPTION_SPECS) {
            if (option_name == spec.long_key) {
                return &spec;
            }
        }
    } else if (token.size() == 2 && token[0] == '-') {
        for (const ParsedOptionSpec& spec : RAWGL_OPTION_SPECS) {
            if (spec.short_key != '\0' && token[1] == spec.short_key) {
                return &spec;
            }
        }
    }

    return nullptr;
}

static bool
is_option_token(const std::string& token)
{
    return find_option_spec(token) != nullptr;
}

static int
parse_option_int(const std::string& text, const char* option_name)
{
    int value                           = 0;
    const char* begin                   = text.data();
    const char* end                     = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);

    if (result.ec != std::errc() || result.ptr != end) {
        throw std::runtime_error(std::string("Invalid integer value for --") + option_name + ": " + text);
    }

    return value;
}

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

static std::string
build_help_text()
{
    std::ostringstream stream;
    stream << "Options\n"
           << "  --help, -h\n"
           << "  --version, -v\n"
           << "  --verbosity, -V <0-5>\n"
           << "  --pass_vertfrag, -P <file> [file]\n"
           << "  --pass_comp, -C <file>\n"
           << "  --pass_size, -S <X> [Y]\n"
           << "  --pass_workgroupsize, -W <X> [Y]\n"
           << "  --bg_color <R> [G] [B] [A]\n"
           << "  --cull <name value>...\n"
           << "  --pass_mesh, -M <quad|mesh> ...\n"
           << "  --in, -i <uniform> <value...>\n"
           << "  --atomic, -B <mode> <args...>\n"
           << "  --in_attr, -t <name> <value>\n"
           << "  --out, -o <name> <path>\n"
           << "  --out_format, -f <format>\n"
           << "  --out_attr, -r <name> <value>\n"
           << "  --out_channels, -n <count>\n"
           << "  --out_alpha_channel, -a <index>\n"
           << "  --out_bits, -b <bits>\n\n"
           << "Supported texture attributes:\n"
           << PassInput::get_possible_tex_attr_fmt() << '\n'
           << "Supported mesh attributes:\n"
           << MeshInput::get_possible_mesh_parm_fmt() << '\n'
           << "Supported culling attributes:\n"
           << Pass::get_possible_culling_fmt();
    return stream.str();
}

static void
print_version_text()
{
    std::cout << termcolor::bright_yellow << termcolor::bold;
    std::cout << APP_NAME << " version " << APP_VERSION[0] << "." << APP_VERSION[1] << "." << APP_VERSION[2]
              << " Copyright (c) " << APP_AUTHOR << std::endl;
    std::cout << "Build from: " << __DATE__ << ", " << __TIME__ << "." << std::endl;
    std::cout << termcolor::reset;
}

}  // namespace

SequenceParsedArguments
Sequence_ParseArguments(int argc, const char* argv[])
{
    SequenceParsedArguments parsed;

    for (int index = 1; index < argc; ++index) {
        const std::string token      = argv[index];
        const ParsedOptionSpec* spec = find_option_spec(token);

        if (spec == nullptr) {
            throw std::runtime_error("Unknown option: " + token);
        }

        if (spec->mode == ParsedOptionMode::flag) {
            if (std::string(spec->long_key) == "help") {
                parsed.showHelp = true;
            } else if (std::string(spec->long_key) == "version") {
                parsed.showVersion = true;
            }

            continue;
        }

        SequenceParsedOption option;
        option.string_key = spec->long_key;

        if (spec->mode == ParsedOptionMode::single) {
            if (index + 1 >= argc) {
                throw std::runtime_error("Missing value for option --" + option.string_key);
            }

            option.value.push_back(argv[++index]);
        } else {
            while (index + 1 < argc) {
                const std::string next_token = argv[index + 1];
                if (is_option_token(next_token)) {
                    break;
                }

                option.value.push_back(next_token);
                ++index;
            }

            if (option.value.empty()) {
                throw std::runtime_error("Missing value for option --" + option.string_key);
            }
        }

        if (option.string_key == "verbosity") {
            parsed.verbosity = parse_option_int(option.value[0], "verbosity");
            continue;
        }

        parsed.options.push_back(std::move(option));
    }

    return parsed;
}

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

struct PendingTextureLoad {
    std::string key;
    std::future<LoadedTextureData> future;
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
load_texture_file(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    LoadedTextureData texture;

    if (!image_utils::load_image(path, attributes, texture.width, texture.height, texture.data, texture.channels,
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
ensure_primary_mesh(Pass& pass)
{
    if (pass.meshes.empty()) {
        pass.meshes.insert({ "quad", MeshInput() });
    }

    return pass.meshes.begin()->second;
}

static const MeshInput&
require_primary_mesh(const Pass& pass)
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

bool
Sequence_HandleImmediateCommandLine(int argc, const char* argv[], int& exitCode)
{
    try {
        const SequenceParsedArguments parsedArguments = Sequence_ParseArguments(argc, argv);

        if (parsedArguments.showHelp || argc < 2) {
            std::cout << build_help_text() << std::endl;
            exitCode = 0;
            return true;
        }

        if (parsedArguments.showVersion) {
            print_version_text();
            exitCode = 0;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        exitCode = 1;
        return true;
    }

    return false;
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
Sequence::processPassDeclaration(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    state.currentPassN = m_passConfigs.size();
    LOG(debug) << "Loading pass " << state.currentPassN;

    std::shared_ptr<GLProgram> program;

    if (key == "pass_vertfrag") {
        if (value.size() > 2) {
            throw_sequence_error("pass_vertfrag: must have one combined shader file or two stage files.");
        }
        if (value.size() == 1) {
            program = g_glslProgramManager.loadVertFrag(value[0]);
        } else {
            std::string paths[] { value[0], value[1] };
            program = g_glslProgramManager.loadVertFrag(paths);
        }
    } else {
        program = g_glslProgramManager.loadComp(value[0]);
    }

    if (!program || !program->isValid()) {
        throw_sequence_error("Failed to load program for the pass.");
    }

    m_passConfigs.push_back(SequencePassConfig());

    state.currentPass      = &m_passConfigs.back();
    state.currentInput     = nullptr;
    state.currentOutput    = nullptr;
    state.currentMeshInput = nullptr;
    state.currentPass->program = program;
    state.currentPass->isCompute = (key == "pass_comp");

    if (state.currentPassN > 0) {
        state.currentPass->sizeText[0] = m_passConfigs[state.currentPassN - 1].sizeText[0];
        state.currentPass->sizeText[1] = m_passConfigs[state.currentPassN - 1].sizeText[1];
    }
}

void
Sequence::processPassProperty(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    if (key == "pass_size") {
        if (!state.currentPass) {
            throw_sequence_error("pass_size: no preceeding pass declaration.");
        }
        if (value.empty() || value.size() > 2) {
            throw_sequence_error("pass_size: must have 1 or 2 parameters.");
        }

        state.currentPass->sizeText[0] = value[0];
        state.currentPass->sizeText[1] = value.size() > 1 ? value[1] : state.currentPass->sizeText[0];
        return;
    }

    if (key == "pass_workgroupsize") {
        if (!state.currentPass) {
            throw_sequence_error("pass_workgroupsize: no preceeding pass declaration.");
        }
        if (!state.currentPass->isCompute) {
            throw_sequence_error("pass_workgroupsize: not a compute pass.");
        }
        if (value.empty() || value.size() > 2) {
            throw_sequence_error("pass_workgroupsize: must have 1 or 2 parameters.");
        }

        state.currentPass->workGroupSizeText[0] = value[0];
        state.currentPass->workGroupSizeText[1] = value.size() > 1 ? value[1] : "1";
        return;
    }

    if (key == "bg_color") {
        if (!state.currentPass) {
            throw_sequence_error("bg_color: no preceeding pass declaration.");
        }
        if (value.empty() || value.size() > 4) {
            throw_sequence_error("bg_color: must have at least 1 parameter.");
        }

        GLfloat tmp_floats[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        for (size_t i = 0; i < value.size(); ++i) {
            tmp_floats[i] = parse_checked_numeric<float_t>(value[i], "bg_color");
        }

        memcpy(&state.currentPass->clearColor, &tmp_floats, sizeof(GLfloat) * 4);
        LOG(debug) << "Clear color (bg_color) set as RGBA [" << std::fixed << std::setprecision(4) << tmp_floats[0]
                   << ", " << tmp_floats[1] << ", " << tmp_floats[2] << ", " << tmp_floats[3] << "]";
        return;
    }

    if (key == "pass_mesh") {
        if (!state.currentPass) {
            throw_sequence_error("pass_mesh: no preceeding pass declaration.");
        }
        if (value.empty()) {
            throw_sequence_error("pass_mesh: must have at least 1 parameter.");
        }

        std::string val_key = value[0];
        std::string type_name = value[0];
        const size_t split = value[0].find("::");
        type_name = value[0].substr(0, split);

        auto [meshesIt, success] = state.currentPass->meshes.insert({ value[0], MeshInput() });
        state.currentMeshInput = &meshesIt->second;

        if (type_name == "mesh") {
            state.currentMeshInput->mesh.isQuad = false;

            if (!success) {
                throw_sequence_error("mesh (" + value[0] + "): duplicate id.");
            }
            if (split != std::string::npos) {
                throw std::runtime_error("pass_mesh: mesh references are not supported");
            }

            std::vector<std::string> mesh_args;
            mesh_args.reserve(value.size() - 1);
            std::copy(value.begin() + 1, value.end(), std::back_inserter(mesh_args));

            if (mesh_args.empty()) {
                throw std::runtime_error("pass_mesh: mesh requires a file path");
            }

            std::string mesh_path;
            std::vector<std::string> mesh_params;
            mesh_params.reserve(mesh_args.size());

            for (const std::string& arg : mesh_args) {
                const std::string file_ext = get_file_ext(arg);
                if (file_ext == "ply" || file_ext == "obj") {
                    if (!mesh_path.empty()) {
                        throw std::runtime_error("pass_mesh: multiple mesh paths are not supported");
                    }
                    mesh_path = arg;
                    continue;
                }
                mesh_params.push_back(arg);
            }

            if (mesh_path.empty()) {
                throw std::runtime_error("pass_mesh: mesh file path not found");
            }
            if ((mesh_params.size() % 2) != 0) {
                throw std::runtime_error("pass_mesh: mesh attributes must be key/value pairs");
            }

            state.currentMeshInput->mesh.isQuad = false;
            state.currentMeshInput->mesh.FileName = mesh_path;

            for (size_t i = 0; i < mesh_params.size(); i += 2) {
                val_key = mesh_params[i];
                const std::string& val_data = mesh_params[i + 1];
                hres hr_tex_attr = hres::OK;
                state.currentMeshInput->eval_mesh_parm(hr_tex_attr, val_key, val_data);

                if (hr_tex_attr != hres::OK) {
                    throw std::runtime_error("pass_mesh: unknown mesh parameter");
                }
            }
        } else if (type_name == "quad") {
            state.currentMeshInput->mesh.isQuad = true;
            LOG(info) << "pass_mesh: Default Quad";
        } else {
            throw_sequence_error("pass_mesh: unknown mesh type.");
        }
        return;
    }

    if (key == "cull") {
        if (!state.currentPass) {
            throw_sequence_error("cull: no preceeding pass declaration");
        }
        if (value.size() < 2 || (value.size() % 2) != 0) {
            throw_sequence_error("cull: must have key/value pairs.");
        }

        for (size_t i = 0; i < value.size(); i += 2) {
            hres hr_tex_attr = hres::ERR;
            for (const auto& cull_attr : Pass::CULL_PARM_ARR) {
                if (value[i] != cull_attr.name) {
                    continue;
                }

                for (const auto& possible_val : cull_attr.possible_values) {
                    if (value[i + 1] == possible_val.key) {
                        cull_attr.func(state.currentPass->cullMode, possible_val.gl_value);
                        hr_tex_attr = hres::OK;
                        break;
                    }
                }

                if (hr_tex_attr == hres::OK) {
                    break;
                }
            }

            if (hr_tex_attr != hres::OK) {
                throw_sequence_error("cull: unknown cull parameter.");
            }
        }
    }
}

void
Sequence::processInputOption(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    (void)key;
    if (!state.currentPass) {
        throw_sequence_error("in (" + value[0] + "): no preceeding pass declaration.");
    }
    if (value.size() < 2) {
        throw_sequence_error("in (" + value[0] + "): must have at least 2 parameters - uniform, value(s).");
    }

    auto shaderUniform = state.currentPass->program->findUniform(value[0]);
    if (!shaderUniform) {
        throw std::runtime_error("in (" + value[0] + "): program uniform not found");
    }

    auto [inputIt, success] = state.currentPass->inputs.insert({ value[0], PassInput() });
    if (!success) {
        throw_sequence_error("in (" + value[0] + "): duplicate id.");
    }

    state.currentInput = &inputIt->second;
    state.currentInput->uniform = shaderUniform;

    hres hr = hres::OK;
    std::vector<std::string> val_arr;
    val_arr.reserve(value.size() - 1);
    std::copy(value.begin() + 1, value.end(), std::back_inserter(val_arr));

    const GLenum uniform_type = state.currentInput->uniform->type;
    if (uniform_type == GL_SAMPLER_2D || uniform_type == GL_IMAGE_2D) {
        for (size_t i = 0; i < val_arr.size(); ++i) {
            std::string val_key = val_arr[i];

            if (val_key.find("::") != std::string::npos) {
                hres hr_convert          = hres::OK;
                const size_t split       = val_key.find("::");
                const auto val_name      = val_key.substr(0, split);
                const int ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_key.substr(split + 2));

                if (val_name.empty()) {
                    throw std::runtime_error("in (" + value[0] + "): empty referenced output name");
                }
                if (hres::OK != hr_convert || ref_pass_index < 0
                    || ref_pass_index >= static_cast<int>(m_passConfigs.size())) {
                    throw std::runtime_error("in (" + value[0] + "): invalid referenced pass index in " + val_key);
                }

                auto& ref_pass = m_passConfigs[ref_pass_index];
                auto ref_output_it = ref_pass.outputs.find(val_name);
                if (ref_output_it == ref_pass.outputs.end()) {
                    ref_output_it = ref_pass.outputs.insert({ val_name, PassOutput() }).first;
                }

                auto& ref_output = ref_output_it->second;
                hres hr_find_uniform = hres::OK;

                if (ref_pass.isCompute) {
                    ref_output.uniform = ref_pass.program->findUniform(val_name);
                    if (!ref_output.uniform) {
                        hr_find_uniform = hres::ERR;
                    }
                } else {
                    ref_output.output = ref_pass.program->findOutput(val_name);
                    if (!ref_output.output) {
                        hr_find_uniform = hres::ERR;
                    }
                }

                if (hres::OK != hr_find_uniform) {
                    throw std::runtime_error("in (" + value[0] + "): referenced program output " + val_key
                                             + " not found");
                }

                state.currentInput->path = val_key;
                continue;
            }

            if (i == val_arr.size() - 1) {
                state.currentInput->path = val_key;
                break;
            }

            const std::string& val_data = val_arr[i + 1];
            hres hr_tex_attr = hres::OK;
            state.currentInput->eval_tex_attr(hr_tex_attr, val_key, val_data);

            if (hres::OK == hr_tex_attr) {
                i++;
            } else {
                state.currentInput->path = val_key;
            }
        }
        return;
    }

    uint8_t num_fields = 1;
    bool is_floats     = false;
    bool is_double     = false;
    bool is_uints      = false;

    switch (state.currentInput->uniform->type) {
    case GL_BOOL:
    case GL_INT: num_fields = 1; break;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2: num_fields = 2; break;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3: num_fields = 3; break;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4: num_fields = 4; break;
    case GL_UNSIGNED_INT: is_uints = true; num_fields = 1; break;
    case GL_UNSIGNED_INT_VEC2: is_uints = true; num_fields = 2; break;
    case GL_UNSIGNED_INT_VEC3: is_uints = true; num_fields = 3; break;
    case GL_UNSIGNED_INT_VEC4: is_uints = true; num_fields = 4; break;
    case GL_FLOAT: is_floats = true; num_fields = 1; break;
    case GL_FLOAT_VEC2: is_floats = true; num_fields = 2; break;
    case GL_FLOAT_VEC3: is_floats = true; num_fields = 3; break;
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2: is_floats = true; num_fields = 4; break;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2: is_floats = true; num_fields = 6; break;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2: is_floats = true; num_fields = 8; break;
    case GL_FLOAT_MAT3: is_floats = true; num_fields = 9; break;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3: is_floats = true; num_fields = 12; break;
    case GL_FLOAT_MAT4: is_floats = true; num_fields = 16; break;
    case GL_DOUBLE: is_double = true; num_fields = 1; break;
    case GL_DOUBLE_VEC2: is_double = true; num_fields = 2; break;
    case GL_DOUBLE_VEC3: is_double = true; num_fields = 3; break;
    case GL_DOUBLE_VEC4:
    case GL_DOUBLE_MAT2: is_double = true; num_fields = 4; break;
    case GL_DOUBLE_MAT2x3:
    case GL_DOUBLE_MAT3x2: is_double = true; num_fields = 6; break;
    case GL_DOUBLE_MAT2x4:
    case GL_DOUBLE_MAT4x2: is_double = true; num_fields = 8; break;
    case GL_DOUBLE_MAT3: is_double = true; num_fields = 9; break;
    case GL_DOUBLE_MAT3x4:
    case GL_DOUBLE_MAT4x3: is_double = true; num_fields = 12; break;
    case GL_DOUBLE_MAT4: is_double = true; num_fields = 16; break;
    default: hr = hres::ERR; break;
    }

    if (val_arr.size() < num_fields) {
        throw std::runtime_error("in (" + value[0] + "): missing numeric values: " + std::to_string(val_arr.size())
                                 + "/" + std::to_string(num_fields));
    }

    GLint tmp_ints[PassInput::NUM_INTS]          = { 0 };
    GLuint tmp_uints[PassInput::NUM_INTS]        = { 0 };
    GLfloat tmp_floats[PassInput::NUM_FLOATS]    = { 0.0f };
    GLdouble tmp_doubles[PassInput::NUM_DOUBLES] = { 0.0 };

    for (uint8_t i = 0; i < num_fields; ++i) {
        const std::string& str_val = val_arr[i];
        if (is_floats) {
            tmp_floats[i] = str_to_numeric<float_t>(hr, str_val);
        } else if (is_double) {
            tmp_doubles[i] = str_to_numeric<double_t>(hr, str_val);
        } else if (is_uints) {
            tmp_uints[i] = str_to_numeric<uint32_t>(hr, str_val);
        } else {
            tmp_ints[i] = str_to_numeric<int32_t>(hr, str_val);
        }
    }

    if (hres::OK != hr) {
        throw std::runtime_error("in (" + value[0] + "): invalid numeric value");
    }

    if (is_floats) {
        memcpy(&state.currentInput->floats, &tmp_floats, sizeof(GLfloat) * PassInput::NUM_FLOATS);
    } else if (is_double) {
        memcpy(&state.currentInput->doubles, &tmp_doubles, sizeof(GLdouble) * PassInput::NUM_DOUBLES);
    } else if (is_uints) {
        memcpy(&state.currentInput->uints, &tmp_uints, sizeof(GLuint) * PassInput::NUM_INTS);
    } else {
        memcpy(&state.currentInput->ints, &tmp_ints, sizeof(GLint) * PassInput::NUM_INTS);
    }
}

void
Sequence::processAtomicOption(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    (void)key;
    if (!state.currentPass) {
        throw_sequence_error("atomic (" + value[0] + "): no preceeding input declaration.");
    }
    if (value.size() < 2) {
        throw_sequence_error("atomic (" + value[0] + "): must have at least 2 parameters.");
    }

    if (value[0] == "cntr") {
        hres hr = hres::OK;
        std::vector<std::string> val_arr;
        val_arr.reserve(value.size() - 1);
        std::copy(value.begin() + 1, value.end(), std::back_inserter(val_arr));

        if (val_arr.size() > 2) {
            throw std::runtime_error("atomic (" + value[0] + "): can only have a single value");
        }

        const GLuint tmp_uint = static_cast<GLuint>(str_to_numeric<uint32_t>(hr, val_arr[1]));
        if (hr == hres::ERR) {
            throw_sequence_error("atomic (" + value[0] + "): -> " + value[1] + " <- is invalid parameters");
        }

        auto counterSh = state.currentPass->program->findCounter(val_arr[0]);
        if (!counterSh) {
            throw_sequence_error("atomic (" + value[0] + "): referenced counter " + val_arr[0] + " not found.");
        }

        auto [i_counterIt, success] = state.currentPass->inputCounters.insert({ val_arr[0], Pass::inputCounter() });
        i_counterIt->second.size  = counterSh->size;
        i_counterIt->second.value = { tmp_uint };

        LOG(debug) << "atomic counter: " << val_arr[0].c_str() << " set to " << tmp_uint;

        if (!success) {
            throw_sequence_error("atomic counter: " + val_arr[0] + " already exists.");
        }
    } else if (value[0] != "buff") {
        throw std::runtime_error("atomic (" + value[0] + "): unknown atomic buffer type");
    }

}

void
Sequence::processInputAttributeOption(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    (void)key;
    if (!state.currentInput) {
        throw_sequence_error("in_attr (" + value[0] + "): no preceeding input declaration.");
    }

    switch (state.currentInput->uniform->type) {
    case GL_SAMPLER_2D:
    case GL_IMAGE_2D: break;
    default:
        throw_sequence_error("in_attr (" + value[0] + "): attributes can only be specified for a texture input.");
    }

    if (value.size() < 2) {
        throw_sequence_error("in_attr (" + value[0] + "): must have 2 parameters.");
    }

    state.currentInput->attributes[value[0]] = value[1];
}

void
Sequence::processOutputOption(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    if (key == "out") {
        if (!state.currentPass) {
            throw_sequence_error("out (" + value[0] + "): no preceeding pass declaration.");
        }
        if (value.size() != 2) {
            throw_sequence_error("out (" + value[0] + "): must have 2 parameters.");
        }

        auto [outputIt, success] = state.currentPass->outputs.insert({ value[0], PassOutput() });
        if (!success) {
            throw_sequence_error("out (" + value[0] + "): duplicate id.");
        }

        state.currentOutput = &outputIt->second;

        if (state.currentPassN > 0) {
            state.currentOutput->internalFormatText = state.previousInternalFormatText;
            state.currentOutput->channels           = state.previousChannels;
            state.currentOutput->alphaChannel       = state.previousAlphaChannel;
            state.currentOutput->bits               = state.previousBits;
        }

        state.currentOutput->path = value[1];

        bool fail = false;
        if (state.currentPass->isCompute) {
            state.currentOutput->uniform = state.currentPass->program->findUniform(value[0]);
            if (!state.currentOutput->uniform) {
                fail = true;
            }
        } else {
            state.currentOutput->output = state.currentPass->program->findOutput(value[0]);
            if (!state.currentOutput->output) {
                fail = true;
            }
        }

        if (fail) {
            throw_sequence_error("out (" + value[0] + "): program output not found.");
        }
        return;
    }

    if (key == "out_channels") {
        if (!state.currentOutput) {
            throw_sequence_error("out_channels (" + value[0] + "): no preceeding output declaration.");
        }
        state.currentOutput->channels = parse_checked_positive_int(value[0], "out_channels");
        state.previousChannels        = state.currentOutput->channels;
        return;
    }

    if (key == "out_alpha_channel") {
        if (!state.currentOutput) {
            throw_sequence_error("out_alpha_channel (" + value[0] + "): no preceeding output declaration.");
        }

        state.currentOutput->alphaChannel = parse_checked_numeric<int32_t>(value[0], "out_alpha_channel");
        if (state.currentOutput->alphaChannel > 3) {
            throw_sequence_error("out_alpha_channel (" + value[0] + "): index > 3 is unsupported.");
        }

        state.previousAlphaChannel = state.currentOutput->alphaChannel;
        return;
    }

    if (key == "out_bits") {
        if (!state.currentOutput) {
            throw_sequence_error("out_bits (" + value[0] + "): no preceeding output declaration.");
        }
        state.currentOutput->bits = parse_checked_positive_int(value[0], "out_bits");
        state.previousBits        = state.currentOutput->bits;
        return;
    }

    if (key == "out_format") {
        if (!state.currentOutput) {
            throw_sequence_error("out_format (" + value[0] + "): no preceeding output declaration.");
        }
        state.currentOutput->internalFormatText = value[0];
        state.previousInternalFormatText        = value[0];
        return;
    }

    if (key == "out_attr") {
        if (!state.currentOutput) {
            throw_sequence_error("out_attr (" + value[0] + "): no preceeding output declaration.");
        }
        if (value.size() < 2) {
            throw_sequence_error("out_attr (" + value[0] + "): must have 2 parameters.");
        }

        state.currentOutput->attributes[value[0]] = value[1];
    }
}

void
Sequence::processParsedOption(const std::string& key, const std::vector<std::string>& value, ParseState& state)
{
    if (key == "pass_vertfrag" || key == "pass_comp") {
        processPassDeclaration(key, value, state);
        return;
    }

    if (key == "pass_size" || key == "pass_workgroupsize" || key == "bg_color" || key == "pass_mesh"
        || key == "cull") {
        processPassProperty(key, value, state);
        return;
    }

    if (key == "in") {
        processInputOption(key, value, state);
        return;
    }

    if (key == "atomic") {
        processAtomicOption(key, value, state);
        return;
    }

    if (key == "in_attr") {
        processInputAttributeOption(key, value, state);
        return;
    }

    processOutputOption(key, value, state);
}

void
Sequence::buildPassesFromConfig()
{
    m_passes.clear();
    m_passes.reserve(m_passConfigs.size());

    for (const SequencePassConfig& config : m_passConfigs) {
        Pass pass(config.program, config.isCompute);
        pass.inputs        = config.inputs;
        pass.inputCounters = config.inputCounters;
        pass.outputs       = config.outputs;
        pass.meshes        = config.meshes;
        pass.cullMode      = config.cullMode;
        pass.sizeText[0]   = config.sizeText[0];
        pass.sizeText[1]   = config.sizeText[1];
        pass.workGroupSizeText[0] = config.workGroupSizeText[0];
        pass.workGroupSizeText[1] = config.workGroupSizeText[1];
        std::memcpy(pass.clearColor, config.clearColor, sizeof(pass.clearColor));
        m_passes.push_back(std::move(pass));
    }
}

void
Sequence::buildPassesFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig)
{
    m_passes.clear();
    m_passes.reserve(runtimeConfig.passes.size());

    for (const SequenceRuntimePassConfig& config : runtimeConfig.passes) {
        Pass pass(config.program, config.isCompute);
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

Sequence::Sequence(int argc, const char* argv[])
    : Sequence()
{
    const SequenceParsedArguments parsedArguments = Sequence_ParseArguments(argc, argv);

    bool infoExit = false;
    if (parsedArguments.showHelp || argc < 2) {
        std::cout << build_help_text() << std::endl;
        infoExit = true;
    }

    if (parsedArguments.showVersion) {
        print_version_text();
        get_GPUfeatures();
        infoExit = true;
    }

    if (infoExit) {
        return;
    }

    initializeFromParsedArguments(parsedArguments);
}

Sequence::Sequence(const SequenceParsedArguments& parsedArguments)
    : Sequence()
{
    initializeFromParsedArguments(parsedArguments);
}

Sequence::Sequence(const SequenceBuildConfig& buildConfig)
    : Sequence()
{
    initializeFromBuildConfig(buildConfig);
}

Sequence::Sequence(const SequenceRuntimeConfig& runtimeConfig)
    : Sequence()
{
    initializeFromRuntimeConfig(runtimeConfig);
}

Sequence::~Sequence()
{
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
Sequence::initializeFromParsedArguments(const SequenceParsedArguments& parsedArguments)
{
    Log_SetVerbosity(std::clamp(parsedArguments.verbosity, 0, 5));
    LOG(debug) << "Starting RawGL sequence" << std::endl;

    ParseState parseState;
    for (const SequenceParsedOption& option : parsedArguments.options) {
        processParsedOption(option.string_key, option.value, parseState);
    }

    buildPassesFromConfig();

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
Sequence::initializeFromBuildConfig(const SequenceBuildConfig& buildConfig)
{
    Log_SetVerbosity(std::clamp(buildConfig.verbosity, 0, 5));
    LOG(debug) << "Starting RawGL sequence" << std::endl;

    m_passConfigs = buildConfig.passes;
    buildPassesFromConfig();

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
Sequence::initializeFromRuntimeConfig(const SequenceRuntimeConfig& runtimeConfig)
{
    Log_SetVerbosity(std::clamp(runtimeConfig.verbosity, 0, 5));
    LOG(debug) << "Starting RawGL sequence" << std::endl;

    m_passConfigs.clear();
    m_textures        = runtimeConfig.sharedTextures;
    m_sharedMeshes    = runtimeConfig.sharedMeshes;
    m_sharedGpuMeshes = runtimeConfig.sharedGpuMeshes;
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
            pendingLoad.future = std::async(std::launch::async, &load_texture_file, input.path, input.attributes);
            pendingTextureIndex.insert({ textureKey, pendingTextureLoads.size() });
            pendingTextureLoads.push_back(std::move(pendingLoad));
        }
    }

    for (auto& pendingLoad : pendingTextureLoads) {
        LoadedTextureData textureData;

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
                                                      textureData.type, textureData.data, textureData.alphaChannel) });

        release_loaded_texture_data(textureData);
    }
}

void
Sequence::initializePass(Pass& pass, int passIndex)
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

        const Pass& refPass = m_passes[refPassIndex];
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
        pass.glbObject.FBO.push_back(Pass::FBOobject { pass.fboId });
    }

    for (auto& outputIt : pass.outputs) {
        PassOutput& output = outputIt.second;

        if (output.alphaChannel >= output.channels) {
            throw_sequence_error("out_alpha_channel (" + outputIt.first
                                 + "): index exceeds max channel index for this image.");
        }

        if (!output.path.empty()) {
            output.format = image_utils::get_output_format(output.path, output.bits, output.formatDefaulted);
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
            LOG(debug) << "Pass " << passIndex << ": attaching output " << output.output->location << " "
                       << outputIt.first << " to FBO";
            GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + output.output->location, GL_TEXTURE_2D,
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

    for (auto& inputIt : pass.inputs) {
        PassInput& input = inputIt.second;

        switch (input.uniform->type) {
        case GL_SAMPLER_2D:
        case GL_IMAGE_2D: break;
        default: continue;
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
}

void
Sequence::validatePassSetup() const
{
    for (const Pass& pass : m_passes) {
        const MeshInput& mesh = require_primary_mesh(pass);
        (void)mesh;

        for (const auto& inputIt : pass.inputs) {
            const PassInput& input = inputIt.second;
            if (input.uniform == nullptr) {
                throw_sequence_error("input (" + inputIt.first + "): uniform is not initialized.");
            }

            if ((input.uniform->type == GL_SAMPLER_2D || input.uniform->type == GL_IMAGE_2D) && input.texture == nullptr) {
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
        Pass& pass = m_passes[passIndex];
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
    Pass& pass = *plan.pass;
    int textureIndex = 0;

    for (const PlannedInputBinding& binding : plan.inputs) {
        PassInput& input = *binding.input;
        const SequenceExecutionInputOverride* inputOverride =
            find_input_override(inputOverrides, static_cast<size_t>(plan.passIndex), *binding.name);
        std::shared_ptr<Texture> boundTexture = input.texture;
        if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::texture) {
            boundTexture = inputOverride->texture;
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
                input.uniform->set(inputOverride->intValues.data());
            } else {
                input.uniform->set(&input.ints[0]);
            }
            break;
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::uintValues) {
                input.uniform->set(inputOverride->uintValues.data());
            } else {
                input.uniform->set(&input.uints[0]);
            }
            break;
        case GL_FLOAT:
        case GL_FLOAT_VEC2:
        case GL_FLOAT_VEC3:
        case GL_FLOAT_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::floatValues) {
                input.uniform->set(inputOverride->floatValues.data());
            } else {
                input.uniform->set(&input.floats[0]);
            }
            break;
        case GL_DOUBLE:
        case GL_DOUBLE_VEC2:
        case GL_DOUBLE_VEC3:
        case GL_DOUBLE_VEC4:
            if (inputOverride && inputOverride->kind == SequenceExecutionInputOverrideKind::doubleValues) {
                input.uniform->set(inputOverride->doubleValues.data());
            } else {
                input.uniform->set(&input.doubles[0]);
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
    Pass& pass = *plan.pass;
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
Sequence::preparePassAtomicCounters(Pass& pass)
{
    pass.u_aCounters.clear();

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
}

void
Sequence::bindPassAtomicCounters(Pass& pass)
{
    LOG(trace) << "Binding atomic counters" << std::endl;

    auto it = pass.u_aCounters.begin();
    while (it != pass.u_aCounters.end()) {
        GLuint buff_size = 0;
        auto range       = pass.u_aCounters.equal_range(it->first);

        LOG(trace) << "Binding: " << it->first << " have " << std::distance(range.first, range.second)
                   << " counter[s]." << std::endl;

        GLCall(glGenBuffers(1, &it->second.bufferID));
        GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));

        for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
            const GLuint groupSize =
                groupIt->second.buffer->offset + groupIt->second.buffer->size * sizeof(GLuint);
            buff_size = std::max(buff_size, groupSize);
            LOG(trace) << groupIt->second.buffer->name << " buff_size: " << buff_size / sizeof(GLuint) << std::endl;
        }

        GLCall(glBufferData(GL_ATOMIC_COUNTER_BUFFER, buff_size, nullptr, GL_DYNAMIC_DRAW));

        for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
            auto buffer = groupIt->second.buffer;
            GLCall(glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, buffer->offset, sizeof(GLuint) * buffer->size,
                                   groupIt->second.value.data()));
            LOG(trace) << buffer->name << " offset: " << buffer->offset << " size: " << buffer->size << std::endl;

            groupIt->second.bufferID = it->second.bufferID;
            buffer->isSet            = true;
        }

        GLCall(glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, it->first, it->second.bufferID, 0, buff_size));
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
        it = range.second;
    }

    GLCall(glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT));
}

void
Sequence::executeComputePass(const PassExecutionPlan& plan, int textureIndex)
{
    Pass& pass = *plan.pass;

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
    Pass& pass = *plan.pass;
    GLCall(glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId));

    std::vector<GLenum> buffers(8, GL_NONE);
    for (const PlannedOutputBinding& binding : plan.outputs) {
        PassOutput& output               = *binding.output;
        buffers[output.output->location] = GL_COLOR_ATTACHMENT0 + output.output->location;
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
}

void
Sequence::savePassOutputs(const PassExecutionPlan& plan)
{
    for (const PlannedOutputBinding& binding : plan.outputs) {
        binding.output->saveTexture();
    }
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
        for (const PassExecutionPlan& plan : m_executionPlan) {
            Pass& pass = *plan.pass;
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

            savePassOutputs(plan);
        }
    } catch (...) {
        destroyAtomicCounterBuffers();
        glBindVertexArray(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);
        throw;
    }

    destroyAtomicCounterBuffers();

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    LOG(debug) << "Sequence completed in " << timer.nowText();
}

#define STRING_USED_DEFAULTS "(used default)"
#define STRING_CHANGED_TO_SUPPORTED "(changed to highest supported value for file format)"
