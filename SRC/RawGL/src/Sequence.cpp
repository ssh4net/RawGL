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


#include "Sequence.h"

#include "OpenGLUtils.h"
#include "Timer.h"
#include "Log.h"
#include "GLProgramManager.h"
#include "ImageUtils.h"

#include <charconv>
#include <future>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include <termcolor/termcolor.hpp>

#include "mesh_io.h"

namespace {
struct ParsedOption {
    std::string string_key;
    std::vector<std::string> value;
};

struct ParsedArguments {
    bool showHelp    = false;
    bool showVersion = false;
    int verbosity    = 3;
    std::vector<ParsedOption> options;
};

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
           << "  --pass_mesh, -M <quad|mesh::N> ...\n"
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

static ParsedArguments
parse_arguments(int argc, const char* argv[])
{
    ParsedArguments parsed;

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

        ParsedOption option;
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
upload_mesh_buffers(MeshInput& meshInput)
{
    MeshInput::Mesh& mesh                 = meshInput.mesh;
    MeshInput::VertexBuffer& vertexBuffer = meshInput.VBO;

    GLCall(glCreateVertexArrays(1, &vertexBuffer.vaoId));
    GLCall(glBindVertexArray(vertexBuffer.vaoId));

    GLCall(glCreateBuffers(1, &vertexBuffer.vboId));
    GLCall(glNamedBufferData(vertexBuffer.vboId, mesh.vrtSize, static_cast<const void*>(mesh.pVerts), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(vertexBuffer.vaoId, 0, vertexBuffer.vboId, 0, 3 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(vertexBuffer.vaoId, 0, 3, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(vertexBuffer.vaoId, 0, 0));
    GLCall(glEnableVertexArrayAttrib(vertexBuffer.vaoId, 0));

    GLCall(glCreateBuffers(1, &vertexBuffer.tboId));
    GLCall(glNamedBufferData(vertexBuffer.tboId, mesh.texSize, static_cast<const void*>(mesh.pTexts), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(vertexBuffer.vaoId, 1, vertexBuffer.tboId, 0, 2 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(vertexBuffer.vaoId, 1, 2, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(vertexBuffer.vaoId, 1, 1));
    GLCall(glEnableVertexArrayAttrib(vertexBuffer.vaoId, 1));

    GLCall(glCreateBuffers(1, &vertexBuffer.nboId));
    GLCall(glNamedBufferData(vertexBuffer.nboId, mesh.nrmSize, static_cast<const void*>(mesh.pNorms), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(vertexBuffer.vaoId, 2, vertexBuffer.nboId, 0, 3 * sizeof(float)));
    GLCall(glVertexArrayAttribFormat(vertexBuffer.vaoId, 2, 3, GL_FLOAT, GL_FALSE, 0));
    GLCall(glVertexArrayAttribBinding(vertexBuffer.vaoId, 2, 2));
    GLCall(glEnableVertexArrayAttrib(vertexBuffer.vaoId, 2));

    GLCall(glCreateBuffers(1, &vertexBuffer.cboId));
    GLCall(glNamedBufferData(vertexBuffer.cboId, mesh.clrSize, static_cast<const void*>(mesh.pColrs), GL_STATIC_DRAW));
    GLCall(glVertexArrayVertexBuffer(vertexBuffer.vaoId, 3, vertexBuffer.cboId, 0, 4 * sizeof(unsigned char)));
    GLCall(glVertexArrayAttribIFormat(vertexBuffer.vaoId, 3, 4, GL_UNSIGNED_BYTE, 0));
    GLCall(glVertexArrayAttribBinding(vertexBuffer.vaoId, 3, 3));
    GLCall(glEnableVertexArrayAttrib(vertexBuffer.vaoId, 3));

    GLCall(glCreateBuffers(1, &vertexBuffer.iboId));
    GLCall(glNamedBufferData(vertexBuffer.iboId, mesh.idxSize, static_cast<const void*>(mesh.pIndxs), GL_STATIC_DRAW));
    GLCall(glVertexArrayElementBuffer(vertexBuffer.vaoId, vertexBuffer.iboId));
}
}  // namespace

namespace std {
std::ostream&
operator<<(std::ostream& os, const std::vector<std::string>& vec)
{
    for (auto item : vec)
        os << item << " ";

    return os;
}
}  // namespace std

Sequence::Sequence(int argc, const char* argv[])
    : Sequence()
{
    try {
        const ParsedArguments parsedArguments = parse_arguments(argc, argv);

        // for --help, -h, --version, -v command line arguments just print info and exit
        bool infoExit = false;

        if (parsedArguments.showHelp || argc < 2) {
            std::cout << build_help_text() << std::endl;
            infoExit = true;
        }

        if (parsedArguments.showVersion) {
            std::cout << termcolor::bright_yellow << termcolor::bold;
            std::cout << APP_NAME << " version " << APP_VERSION[0] << "." << APP_VERSION[1] << "." << APP_VERSION[2]
                      << " Copyright (c) " << APP_AUTHOR << std::endl;
            std::cout << "Build from: " << __DATE__ << ", " << __TIME__ << "." << std::endl;
            // Get available GPU features
            get_GPUfeatures();

            infoExit = true;
        }

        if (infoExit)
            exit(1);

        // set the logger verbosity
        Log_SetVerbosity(std::clamp(parsedArguments.verbosity, 0, 5));

        //
        // parse the multi-occurence, multi-token options here
        //
        LOG(debug) << "Starting RawGL sequence" << std::endl;

        Pass* currentPass                       = nullptr;
        PassInput* currentInput                 = nullptr;
        PassOutput* currentOutput               = nullptr;
        MeshInput* currentMeshInput             = nullptr;
        PassInputCounters* currentInputCounters = nullptr;
        size_t currentPassN                     = 0;

        // tired. Don't know correct way to store previous output parameters
        std::string p_internalFormatText = "rgb32f";
        int p_channels                   = 3;
        int p_alphaChannel               = -1;
        int p_bits                       = 16;

        for (const ParsedOption& o : parsedArguments.options) {
            //
            // Pass
            //

            if (o.string_key == "pass_vertfrag" || o.string_key == "pass_comp") {
                // Start a new pass
                currentPassN = m_passes.size();
                LOG(debug) << "Loading pass " << currentPassN;

                std::shared_ptr<GLProgram> program;

                if (o.string_key == "pass_vertfrag") {
                    if (o.value.size() == 1) {
                        // Single text file
                        program = g_glslProgramManager.loadVertFrag(o.value[0]);
                    } else {
                        // Two text/binary files
                        std::string s[] { o.value[0], o.value[1] };
                        program = g_glslProgramManager.loadVertFrag(s);
                    }
                } else {
                    // Single text/binary file
                    program = g_glslProgramManager.loadComp(o.value[0]);
                }

                if (!program->isValid()) {
                    LOG(error) << "Failed to load program for the pass.";
                    exit(1);
                }

                m_passes.push_back(Pass(program, o.string_key == "pass_comp"));

                // reset the pointers
                currentPass   = &m_passes.back();
                currentInput  = nullptr;
                currentOutput = nullptr;

                // Gather all outputs to work with them further a bit later
                //for ()
                //
                // Set pass_size from previous pass
                if (currentPassN > 0) {
                    currentPass->sizeText[0] = m_passes[currentPassN - 1].sizeText[0];
                    currentPass->sizeText[1] = m_passes[currentPassN - 1].sizeText[1];
                }

                continue;
            }

            //
            // generic parameters
            //

            if (o.string_key == "pass_size") {
                // TODO:
                // mtp/mp/scl/sc option for size scale vec2() positive only
                // mg/mrgn option for image margin vec4() (left, right, top, bottom) positive or negative
                if (!currentPass) {
                    LOG(error) << "pass_size: no preceeding pass declaration.";
                    exit(1);
                }

                // specify input data (for GLSL uniform)
                if (o.value.size() < 1 || o.value.size() > 2) {
                    LOG(error) << "pass_size: must have 1 or 2 parameters.";
                    exit(1);
                }

                currentPass->sizeText[0] = o.value[0];
                currentPass->sizeText[1] = o.value.size() > 1 ? o.value[1] : currentPass->sizeText[0];
            }

            if (o.string_key == "pass_workgroupsize") {
                if (!currentPass) {
                    LOG(error) << "pass_workgroupsize: no preceeding pass declaration.";
                    exit(1);
                }

                if (!currentPass->isCompute) {
                    LOG(error) << "pass_workgroupsize: not a compute pass.";
                    exit(1);
                }

                // specify input data (for GLSL uniform)
                if (o.value.size() < 1 || o.value.size() > 2) {
                    LOG(error) << "pass_workgroupsize: must have 1 or 2 parameters.";
                    exit(1);
                }

                currentPass->workGroupSizeText[0] = o.value[0];
                currentPass->workGroupSizeText[1] = o.value.size() > 1 ? o.value[1] : "1";
                //currentPass->workGroupSizeText[2] = o.value.size() > 2 ? o.value[2] : "1";
            }
            // background color
            //
            if (o.string_key == "bg_color") {
                if (!currentPass) {
                    LOG(error) << "bg_color: no preceeding pass declaration.";
                    exit(1);
                }
                int size = o.value.size();
                if (size < 1 || size > 4) {
                    LOG(error) << "bg_color: must have at least 1 parameter.";
                    exit(1);
                }
                // set clearColor
                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(size);
                std::copy(o.value.begin(), o.value.end(), std::back_inserter(val_arr));

                GLfloat tmp_floats[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

                for (uint8_t i = 0; i < size; ++i) {
                    const std::string& str_val = val_arr[i];
                    tmp_floats[i]              = str_to_numeric<float_t>(hr, str_val);
                }

                if (hres::OK == hr) {
                    memcpy(&currentPass->clearColor, &tmp_floats, sizeof(GLfloat) * 4);
                }
                LOG(debug) << "Clear color (bg_color) set as RGBA [" << std::fixed << std::setprecision(4)
                           << tmp_floats[0] << ", " << tmp_floats[1] << ", " << tmp_floats[2] << ", " << tmp_floats[3]
                           << "]";
            }
            //
            // mesh parsing
            //
            if (o.string_key == "pass_mesh") {
                if (!currentPass) {
                    LOG(error) << "pass_mesh: no preceeding pass declaration.";
                    exit(1);
                }

                if (o.value.size() < 1) {
                    LOG(error) << "pass_mesh: must have at least 1 parameter.";
                    exit(1);
                }

                auto val_key   = o.value[0];
                auto type_name = o.value[0];

                const size_t split = o.value[0].find("::");
                type_name          = o.value[0].substr(0, split);

                auto [meshesIt, success] = currentPass->meshes.insert({ o.value[0], MeshInput() });
                currentMeshInput         = &meshesIt->second;

                if (type_name == "mesh") {
                    currentMeshInput->mesh.isQuad = false;

                    if (!success) {
                        LOG(error) << "mesh (" << o.value[0] << "): duplicate id.";
                        exit(1);
                    }

                    // Not work at this moment: Mesh_name::Pass#
                    // in a future for support variable meshes per passess
                    if (split != std::string::npos) {
                        hres hr_convert          = hres::OK;
                        const int ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_key.substr(split + 2));

                        if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                            auto& ref_pass = m_passes[ref_pass_index];

                            auto ref_input_it = ref_pass.inputs.find(type_name);
                            if (ref_input_it != ref_pass.inputs.end()) {
                                auto& ref_input = ref_input_it->second;
                            } else {
                                LOG(error) << "pass_mesh: reference input not found.";
                                exit(1);
                            }
                        };
                    };

                    hres hr = hres::OK;
                    std::vector<std::string> val_arr;
                    val_arr.reserve(o.value.size() - 1);
                    std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));


                    std::string mesh_path = val_arr[0];
                    std::string file_ext  = get_file_ext(mesh_path);
                    if (file_ext != "ply" && file_ext != "obj") {
                        LOG(error) << "Only PLY and OBJ meshes supported.";
                        exit(1);
                    }
                    currentMeshInput->mesh.isQuad   = false;
                    currentMeshInput->mesh.FileName = mesh_path;

                    for (size_t i = 1; i < val_arr.size() - 1; ++i) {
                        val_key = val_arr[i];
                        std::string val_data;

                        val_data = val_arr[i + 1];

                        // Search for texture attributes
                        hres hr_tex_attr = hres::OK;
                        currentMeshInput->eval_mesh_parm(hr_tex_attr, val_key, val_data);

                        if (hr_tex_attr != hres::OK) {
                            LOG(error) << "pass_mesh: unknown mesh parameter.";
                            exit(1);
                        }
                        i++;
                    }

                } else if (type_name == "quad") {
                    currentMeshInput->mesh.isQuad = true;
                    LOG(info) << "pass_mesh: Default Quad";
                } else {
                    LOG(error) << "pass_mesh: unknown mesh type.";
                    exit(1);
                }
            };

            if (o.string_key == "cull") {
                if (!currentPass) {
                    LOG(error) << "cull: no preceeding pass declaration";
                    exit(1);
                }

                if (o.value.size() < 1) {
                    LOG(error) << "cull: must have at least 1 parameter.";
                    exit(1);
                }

                auto val_key = o.value[0];

                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(o.value.size() - 1);
                std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                for (size_t i = 1; i < val_arr.size() - 1; ++i) {
                    val_key = val_arr[i];
                    std::string val_data;

                    val_data = val_arr[i + 1];

                    // Search for texture attributes
                    hres hr_tex_attr = hres::OK;
                    currentPass->eval_cull_parm(hr_tex_attr, val_key, val_data);

                    if (hr_tex_attr != hres::OK) {
                        LOG(error) << "cull: unknown mesh parameter.";
                        exit(1);
                    }
                    i++;
                }
            };

            //
            // input & its parameters
            //

            if (o.string_key == "in") {
                if (!currentPass) {
                    LOG(error) << "in (" << o.value[0] << "): no preceeding pass declaration.";
                    exit(1);
                }

                // specify input data (for GLSL uniform)
                if (o.value.size() < 2) {
                    LOG(error) << "in (" << o.value[0] << "): must have at least 2 parameters - uniform, value(s).";
                    exit(1);
                }

                // get uniform reference
                auto shaderUniform = currentPass->program->findUniform(o.value[0]);

                if (!shaderUniform) {
                    LOG(error) << "in (" << o.value[0] << "): program uniform not found.";
                    continue;
                    //exit(1);
                }

                // link the input to its uniform
                auto [inputIt, success] = currentPass->inputs.insert({ o.value[0], PassInput() });

                if (!success) {
                    LOG(error) << "in (" << o.value[0] << "): duplicate id.";
                    exit(1);
                }

                currentInput          = &inputIt->second;
                currentInput->uniform = shaderUniform;

                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(o.value.size() - 1);
                std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                const GLenum uniform_type = currentInput->uniform->type;

                // Image / sampler values
                if (uniform_type == GL_SAMPLER_2D || uniform_type == GL_IMAGE_2D) {
                    for (size_t i = 0; i < val_arr.size(); ++i) {
                        std::string val_key = val_arr[i];
                        std::string val_data;

                        /////////////
                        if (val_key.find("::") != std::string::npos) {
                            hres hr_convert          = hres::OK;
                            const size_t split       = val_key.find("::");
                            const auto val_name      = val_key.substr(0, split);
                            const int ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_key.substr(split + 2));

                            if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                                auto& ref_pass = m_passes[ref_pass_index];
                                // looks into Outputs list ins ref pass
                                auto ref_output_it = ref_pass.outputs.find(val_name);

                                if (ref_output_it == ref_pass.outputs.end()) {
                                    // if output not found in Ouptut list,
                                    // that can be a output defined in a shader but not used as a cli output
                                    // create a new Output in that Shader Pass
                                    ref_output_it = ref_pass.outputs.insert({ val_name, PassOutput() }).first;
                                }

                                auto& ref_output = ref_output_it->second;

                                hres hr_find_uniform = hres::OK;

                                if (ref_pass.isCompute) {
                                    ref_output.uniform = ref_pass.program->findUniform(val_name);

                                    if (!ref_output.output) {
                                        hr_find_uniform = hres::ERR;
                                    }
                                } else {
                                    ref_output.output = ref_pass.program->findOutput(val_name);

                                    if (!ref_output.output) {
                                        hr_find_uniform = hres::ERR;
                                    }
                                }

                                if (hres::OK == hr_find_uniform) {
                                    i++;  // Jump over next iteration, it contains only value which was already read
                                } else {
                                    LOG(error) << "in (" << o.value[0] << "): referenced program output " << val_key
                                               << " not found.";
                                    exit(1);
                                }
                            }
                            currentInput->path = val_key;
                            i++;
                            continue;
                        }

                        if (i == val_arr.size() - 1) {  // if only Texture name and Path
                            currentInput->path = val_key;
                            break;
                        }

                        val_data = val_arr[i + 1];
                        // Search for texture attributes
                        hres hr_tex_attr = hres::OK;
                        currentInput->eval_tex_attr(hr_tex_attr, val_key, val_data);

                        if (hres::OK == hr_tex_attr) {
                            i++;
                        } else {
                            //LOG(error) << "in (" << o.value[0] << "): " << val_key << "invalid ( " << val_data << " ) value";
                            currentInput->path = val_key;
                        }
                        //else {
                        //    // Search for value from previous passes.
                        //    hres hr_convert = hres::OK;
                        //    const int32_t ref_pass_index = str_to_numeric<int32_t>(hr_convert, val_data);

                        //    if (hres::OK == hr_convert && ref_pass_index >= 0 && ref_pass_index < m_passes.size() - 1) {
                        //        auto& ref_pass = m_passes[ref_pass_index];
                        //        auto ref_output_it = ref_pass.outputs.find(val_key);

                        //        if (ref_output_it == ref_pass.outputs.end()) {
                        //            ref_output_it = ref_pass.outputs.insert({ val_key, PassOutput() }).first;
                        //        }

                        //        auto& ref_output = ref_output_it->second;

                        //        hres hr_find_uniform = hres::OK;

                        //        if (ref_pass.isCompute) {
                        //            ref_output.uniform = ref_pass.program->findUniform(val_key);

                        //            if (!ref_output.output) {
                        //                hr_find_uniform = hres::ERR;
                        //            }
                        //        }
                        //        else {
                        //            ref_output.output = ref_pass.program->findOutput(val_key);

                        //            if (!ref_output.output) {
                        //                hr_find_uniform = hres::ERR;
                        //            }
                        //        }

                        //        if (hres::OK == hr_find_uniform) {
                        //            i++; // Jump over next iteration, it contains only value which was already read
                        //        }
                        //        else {
                        //            LOG(error) << "in (" << o.value[0] << "): referenced program output " << val_data.c_str() << " not found.";
                        //            exit(1);
                        //        }
                        //        //hacky fix for previous pass output
                        //        currentInput->path = (val_key + "::" + val_data);
                        //    }
                        //    else {
                        //        // no usable hr_attrib was found
                        //        currentInput->path = val_key;
                        //    }
                        //}
                    }
                } else {  // Numeric values
                    uint8_t num_fields = 1;
                    bool is_floats     = false;
                    bool is_double     = false;

                    switch (currentInput->uniform->type) {
                    case GL_BOOL:
                    case GL_INT: num_fields = 1; break;
                    case GL_BOOL_VEC2:
                    case GL_INT_VEC2: num_fields = 2; break;
                    case GL_BOOL_VEC3:
                    case GL_INT_VEC3: num_fields = 3; break;
                    case GL_BOOL_VEC4:
                    case GL_INT_VEC4: num_fields = 4; break;

                    case GL_FLOAT:
                        is_floats  = true;
                        num_fields = 1;
                        break;
                    case GL_FLOAT_VEC2:
                        is_floats  = true;
                        num_fields = 2;
                        break;
                    case GL_FLOAT_VEC3:
                        is_floats  = true;
                        num_fields = 3;
                        break;
                    case GL_FLOAT_VEC4:
                    case GL_FLOAT_MAT2:
                        is_floats  = true;
                        num_fields = 4;
                        break;
                    case GL_FLOAT_MAT2x3:
                    case GL_FLOAT_MAT3x2:
                        is_floats  = true;
                        num_fields = 6;
                        break;
                    case GL_FLOAT_MAT2x4:
                    case GL_FLOAT_MAT4x2:
                        is_floats  = true;
                        num_fields = 8;
                        break;
                    case GL_FLOAT_MAT3:
                        is_floats  = true;
                        num_fields = 9;
                        break;
                    case GL_FLOAT_MAT3x4:
                    case GL_FLOAT_MAT4x3:
                        is_floats  = true;
                        num_fields = 12;
                        break;
                    case GL_FLOAT_MAT4:
                        is_floats  = true;
                        num_fields = 16;
                        break;
                    case GL_DOUBLE:
                        is_double  = true;
                        num_fields = 1;
                        break;
                    case GL_DOUBLE_VEC2:
                        is_double  = true;
                        num_fields = 2;
                        break;
                    case GL_DOUBLE_VEC3:
                        is_double  = true;
                        num_fields = 3;
                        break;
                    case GL_DOUBLE_VEC4:
                        is_double  = true;
                        num_fields = 4;
                        break;
                    case GL_DOUBLE_MAT2:
                        is_double  = true;
                        num_fields = 4;
                        break;
                    case GL_DOUBLE_MAT2x3:
                    case GL_DOUBLE_MAT3x2:
                        is_double  = true;
                        num_fields = 6;
                        break;
                    case GL_DOUBLE_MAT2x4:
                    case GL_DOUBLE_MAT4x2:
                        is_double  = true;
                        num_fields = 8;
                        break;
                    case GL_DOUBLE_MAT3:
                        is_double  = true;
                        num_fields = 9;
                        break;
                    case GL_DOUBLE_MAT3x4:
                    case GL_DOUBLE_MAT4x3:
                        is_double  = true;
                        num_fields = 12;
                        break;
                    case GL_DOUBLE_MAT4:
                        is_double  = true;
                        num_fields = 16;
                        break;
                    default: hr = hres::ERR; break;
                    }

                    if (val_arr.size() < num_fields) {
                        hr = hres::ERR;
                        LOG(error) << "Value is missing values: " << val_arr.size() << '/' << num_fields;
                        return;
                    }

                    GLint tmp_ints[PassInput::NUM_INTS]          = { 0 };
                    GLfloat tmp_floats[PassInput::NUM_FLOATS]    = { 0.0f };
                    GLdouble tmp_doubles[PassInput::NUM_DOUBLES] = { 0.0 };

                    for (uint8_t i = 0; i < num_fields; ++i) {
                        const std::string& str_val = val_arr[i];

                        if (is_floats) {
                            tmp_floats[i] = str_to_numeric<float_t>(hr, str_val);
                        } else if (is_double) {
                            tmp_doubles[i] = str_to_numeric<double_t>(hr, str_val);
                        } else {
                            tmp_ints[i] = str_to_numeric<int32_t>(hr, str_val);
                        }
                    }

                    if (hres::OK == hr) {
                        if (is_floats) {
                            memcpy(&currentInput->floats, &tmp_floats, sizeof(GLfloat) * PassInput::NUM_FLOATS);
                        } else if (is_double) {
                            memcpy(&currentInput->doubles, &tmp_doubles, sizeof(GLdouble) * PassInput::NUM_DOUBLES);
                        } else {
                            memcpy(&currentInput->ints, &tmp_ints, sizeof(GLint) * PassInput::NUM_INTS);
                        }
                    }
                }

            } else if (o.string_key == "atomic") {
                if (!currentPass) {
                    LOG(error) << "atomic (" << o.value[0] << "): no preceeding input declaration.";
                    exit(1);
                }
                if (o.value.size() < 2) {
                    LOG(error) << "atomic (" << o.value[0] << "): must have at least 2 parameters.";
                    exit(1);
                }

                if (o.value[0] == "cntr") {
                    hres hr = hres::OK;
                    std::vector<std::string> val_arr;
                    val_arr.reserve(o.value.size() - 1);
                    std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));

                    GLuint tmp_uint = 0;

                    if (val_arr.size() > 2) {
                        LOG(error) << "atomic (" << o.value[0] << "): can only have a single value";
                    } else {
                        tmp_uint = static_cast<GLuint>(str_to_numeric<uint32_t>(hr, val_arr[1]));
                    }
                    if (hr == hres::ERR) {
                        LOG(error) << "atomic (" << o.value[0] << "): -> " << o.value[1] << " <- is invalid parameters";
                        exit(1);
                    }

                    auto counterSh = currentPass->program->findCounter(val_arr[0]);
                    if (!counterSh) {
                        LOG(error) << "atomic (" << o.value[0] << "): referenced counter " << val_arr[0].c_str()
                                   << " not found.";
                        exit(1);
                    }

                    auto [i_counterIt, success] = currentPass->inputCounters.insert(
                        { val_arr[0], Pass::inputCounter() });
                    //auto [counterIt, success] = m_aCounters.insert({ val_arr[0], PassInputCounters() });
                    i_counterIt->second.size  = counterSh->size;
                    i_counterIt->second.value = { tmp_uint };

                    LOG(debug) << "atomic counter: " << val_arr[0].c_str() << " set to " << tmp_uint;

                    if (!success) {
                        LOG(error) << "atomic counter: " << val_arr[0].c_str() << " already exists.";
                        exit(1);
                    }
                } else if (o.value[0] == "buff") {
                    hres hr = hres::OK;
                    std::vector<std::string> val_arr;
                    val_arr.reserve(o.value.size() - 1);
                    std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));
                } else {
                    LOG(error) << "atomic (" << o.value[0] << "): unknown atomic buffer type";
                    //exit(1);
                };
                // link the input to its atomic counters
                //auto [inputIt, success] = currentPass->u_aBuffers.insert({ o.value[0], PassInputCounters() });
                /*
                hres hr = hres::OK;
                std::vector<std::string> val_arr;
                val_arr.reserve(o.value.size() - 1);
                std::copy(o.value.begin() + 1, o.value.end(), std::back_inserter(val_arr));
				
                int cntr_binding = 0;
                int cntr_offset = 0;
                GLuint cntr_val = 0;

                for (size_t i = 0; i < val_arr.size(); ++i) {
                    std::string val_key = val_arr[i];
                    std::string val_data;

                    if (i < val_arr.size() - 1) {
                        val_data = val_arr[i + 1];
                    }

                    // Search for texture attributes
                    hres hr_tex_parm = hres::OK;
                    currentInputCounters->eval_counter_parm(hr_tex_parm, val_key);

                    if (hres::OK != hr_tex_parm) {
                        LOG(error) << "atomic (" << o.value[0] << "): -> " << val_key << " <- is invalid parameters";
                        exit(1);
                    }
                    int tmp_int = str_to_numeric<int32_t>(hr, val_data);
                    
                    //if (hr == hres::ERR) {
                    //    LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- is invalid parameters";
                    //    exit(1);
                    //}

                    if (val_key == "vl") {
						
                        if (hr == hres::ERR && val_data.find("::") != std::string::npos)
                        {
                            const size_t split = val_data.find("::");
                            const auto refInputAtomicName = val_data.substr(0, split);
                            const int refPassIndex = str_to_numeric<int32_t>(val_data.substr(split + 2));
                            auto refPass = m_passes[refPassIndex].u_aCounters.find(refInputAtomicName);
							
						    if (refPass == m_passes[refPassIndex].u_aCounters.end()) {
                                LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- reference counter not found";
                                exit(1);
                            }
                            if (currentPassN <= refPassIndex) {
                                LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- reffering to counter from same or future pass";
                                exit(1);
                            }
                            inputIt->second.path = val_data;
                            //std::cout << "Atomic Name :" << refInputAtomicName << " from pass: " << refPassIndex;
                            break;
                        }
                        cntr_val = tmp_int;
                    }
                    else if (hr == hres::ERR) {
                        LOG(error) << "atomic (" << o.value[0] << "): -> " << val_data << " <- is invalid parameters";
                           exit(1);
                    }
					
                    i++;
                }
                // get atomic counter reference from Atomic Buffers list by defined binding
                //auto shaderAtomics = currentPass->program->findBuffer(o.value[1]);

                int counterBinding = cntr_binding + cntr_offset / sizeof(GLuint);
                std::string name = "test";
                auto passCounter = currentPass->program->findCounter(name);

                if (!passCounter)
                {
                    LOG(error) << "atomic (" << o.value[0] << " (binding = " << cntr_binding 
                        << " ,offset = " << cntr_offset << " ): program atomic counter not found.";
                    continue;
                    //exit(1);
                }
                inputIt->second.binding = cntr_binding;
                inputIt->second.offset = cntr_offset;
                if (inputIt->second.path == "") {
                    inputIt->second.value = cntr_val;
                }

                switch ( val_arr.size() )
                {
				case 2:
                    LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << val_arr[1] << " ), initial value = " << 0;
                    break;
                case 4:
                    LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << val_arr[1] << " ), initial value = " << val_arr[3];
                    break;
                case 6:
                    LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << val_arr[1] << " , offset = " << val_arr[3] << " ), initial value = " << val_arr[5];
                    break;
                default:
                    break;
                }
				
				//LOG(trace) << "Atomic counter: " << o.value[0] << " " << "( binding = " << o.value[1] << " ), initial value = " << o.value[2];
*/
                //currentInput->counter = o.value[1];
                LOG(trace) << "Test output only.";
            } else if (o.string_key == "in_attr") {
                if (!currentInput) {
                    LOG(error) << "in_attr (" << o.value[0] << "): no preceeding input declaration.";
                    exit(1);
                }

                switch (currentInput->uniform->type) {
                case GL_SAMPLER_2D:
                case GL_IMAGE_2D: break;
                default:
                    LOG(error) << "in_attr (" << o.value[0]
                               << "): attributes can only be specified for a texture input.";
                    exit(1);
                }

                if (o.value.size() < 2) {
                    LOG(error) << "in_attr (" << o.value[0] << "): must have 2 parameters.";
                    exit(1);
                }

                currentInput->attributes[o.value[0]] = o.value[1];
            }

            //
            // output & its parameters
            //

            if (o.string_key == "out") {
                if (!currentPass) {
                    LOG(error) << "out (" << o.value[0] << "): no preceeding pass declaration.";
                    exit(1);
                }

                // configure output file path
                if (o.value.size() != 2) {
                    LOG(error) << "out (" << o.value[0] << "): must have 2 parameters.";
                    exit(1);
                }

                // Unlike the input reference based creation,
                // here we can omit checking if the pass output already exists,
                // and check for a duplicate instead.
                auto [outputIt, success] = currentPass->outputs.insert({ o.value[0], PassOutput() });

                if (!success) {
                    // can only configure program output once per pass
                    LOG(error) << "out (" << o.value[0] << "): duplicate id.";
                    exit(1);
                }

                currentOutput = &outputIt->second;

                // Set pass_size from previous pass
                if (currentPassN > 0) {
                    currentOutput->internalFormatText = p_internalFormatText;
                    currentOutput->channels           = p_channels;
                    currentOutput->alphaChannel       = p_alphaChannel;
                    currentOutput->bits               = p_bits;
                }

                // TODO: Check for output path duplicates using a pass-wide map.
                currentOutput->path = o.value[1];

                bool fail = false;

                if (currentPass->isCompute) {
                    // link it to the program output uniform
                    currentOutput->uniform = currentPass->program->findUniform(o.value[0]);

                    if (!currentOutput->uniform)
                        fail = true;
                } else {
                    // link it to the program output
                    currentOutput->output = currentPass->program->findOutput(o.value[0]);

                    if (!currentOutput->output)
                        fail = true;
                }

                if (fail) {
                    LOG(error) << "out (" << o.value[0] << "): program output not found.";
                    exit(1);
                }

            } else if (o.string_key == "out_channels") {
                if (!currentOutput) {
                    LOG(error) << "out_channels (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->channels = str_to_numeric<int32_t>(o.value[0]);
                p_channels              = currentOutput->channels;
            } else if (o.string_key == "out_alpha_channel") {
                if (!currentOutput) {
                    LOG(error) << "out_alpha_channel (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->alphaChannel = str_to_numeric<int32_t>(o.value[0]);

                if (currentOutput->alphaChannel > 3) {
                    LOG(error) << "out_alpha_channel (" << o.value[0] << "): index > 3 is unsupported.";
                    exit(1);
                }

                p_alphaChannel = currentOutput->alphaChannel;
            } else if (o.string_key == "out_bits") {
                if (!currentOutput) {
                    LOG(error) << "out_bits (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->bits = str_to_numeric<int32_t>(o.value[0]);
                p_bits              = currentOutput->bits;
            } else if (o.string_key == "out_format") {
                if (!currentOutput) {
                    LOG(error) << "out_format (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }

                currentOutput->internalFormatText = o.value[0];
                p_internalFormatText              = o.value[0];
            } else if (o.string_key == "out_attr") {
                if (!currentOutput) {
                    LOG(error) << "out_attr (" << o.value[0] << "): no preceeding output declaration.";
                    exit(1);
                }
                /*
                        // Output is always a file
                        switch(currentOutput->uniform->type)
                                {
                        case GL_SAMPLER_2D:
                            break;
                        default:
                                    LOG(info) << "out_attr (%s): attributes can only be specified for a file output.\n", o.value[0].c_str());
                                    exit(1);
                                }
                */
                if (o.value.size() < 2) {
                    LOG(error) << "out_attr (" << o.value[0] << "): must have 2 parameters.";
                    exit(1);
                }

                currentOutput->attributes[o.value[0]] = o.value[1];
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    {
        int i = 0;
        for (auto& pass : m_passes) {
            size_t p = pass.u_aCounters.size();
            size_t a = pass.program->BuffersSize();
            if (p < a) {
                LOG(debug) << "Pass #" << i << ": " << p << " from " << a << " atomic counters are initialized";
                i++;
            }
        }
    };
    initCommon();
}

Sequence::~Sequence()
{
    for (auto& pass : m_passes) {
        if (pass.fboId) {
            glDeleteFramebuffers(1, &pass.fboId);
        }

        for (auto& meshIt : pass.meshes) {
            MeshInput::VertexBuffer& vertexBuffer = meshIt.second.VBO;

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
        }
    }
}

void
Sequence::initCommon()
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
            LOG(error) << "Failed to prepare texture: " << exception.what();
            exit(1);
        }

        if (!textureData.valid) {
            LOG(error) << "Failed to load an input texture.";
            exit(1);
        }

        m_textures.insert({ pendingLoad.key,
                            std::make_shared<Texture>(textureData.width, textureData.height, textureData.internalFormat,
                                                      textureData.type, textureData.data, textureData.alphaChannel) });

        release_loaded_texture_data(textureData);
    }

    int passIndex = 0;

    for (auto& pass : m_passes) {
        MeshInput& meshInput = ensure_primary_mesh(pass);

        assert(!pass.sizeText[0].empty());

        for (int axis = 0; axis < 2; ++axis) {
            if (pass.sizeText[axis].find("::") == std::string::npos) {
                pass.size[axis] = str_to_numeric<int32_t>(pass.sizeText[axis]);
                continue;
            }

            const std::size_t split        = pass.sizeText[axis].find("::");
            const std::string refInputName = pass.sizeText[axis].substr(0, split);
            const int refPassIndex         = str_to_numeric<int32_t>(pass.sizeText[axis].substr(split + 2));

            if (refInputName.empty()) {
                LOG(error) << "pass_size (" << passIndex << "): empty referenced input name.";
                exit(1);
            }

            if (refPassIndex < 0 || refPassIndex >= static_cast<int>(m_passes.size())) {
                LOG(error) << "pass_size (" << passIndex << "): wrong referenced pass index " << refPassIndex;
                exit(1);
            }

            const Pass& refPass = m_passes[refPassIndex];
            auto refInputIt     = refPass.inputs.find(refInputName);

            if (refInputIt == refPass.inputs.end()) {
                LOG(error) << "pass_size (" << passIndex << "): input " << refInputName
                           << " not found in referenced pass " << refPassIndex;
                exit(1);
            }

            pass.size[axis] = axis == 0 ? refInputIt->second.texture->getWidth()
                                        : refInputIt->second.texture->getHeight();
        }

        LOG(debug) << "Pass " << passIndex << ": pass_size is " << pass.size[0] << " x " << pass.size[1];

        if (pass.isCompute) {
            for (int axis = 0; axis < 2; ++axis) {
                pass.workGroupSize[axis] = str_to_numeric<int32_t>(pass.workGroupSizeText[axis]);
            }
        } else {
            GLCall(glGenFramebuffers(1, &pass.fboId));
            GLCall(glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId));
            pass.glbObject.FBO.push_back(Pass::FBOobject { pass.fboId });
        }

        for (auto& outputIt : pass.outputs) {
            PassOutput& output = outputIt.second;

            if (output.alphaChannel >= output.channels) {
                LOG(error) << "out_alpha_channel (" << outputIt.first
                           << "): index exceeds max channel index for this image.";
                exit(1);
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

            auto textureIt
                = m_textures
                      .insert({ textureName, std::make_shared<Texture>(pass.size[0], pass.size[1], internalFormat,
                                                                       GL_FLOAT, nullptr, output.alphaChannel) })
                      .first;
            output.texture = textureIt->second;

            if (!pass.isCompute) {
                LOG(debug) << "Pass " << passIndex << ": attaching output " << output.output->location << " "
                           << outputIt.first << " to FBO";
                GLCall(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + output.output->location,
                                              GL_TEXTURE_2D, textureIt->second->getId(), 0));
            }
        }

        if (!pass.isCompute) {
            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                LOG(error) << "Pass " << passIndex << ": unable to setup FBO.";
                exit(1);
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
                LOG(error) << "input (" << inputIt.first << "): referenced texture is missing.";
                exit(1);
            }

            input.texture = textureIt->second;
        }

        try {
            load_mesh_data(meshInput.mesh);
        } catch (const std::exception& exception) {
            LOG(error) << exception.what();
            exit(1);
        }

        upload_mesh_buffers(meshInput);
        release_mesh_cpu_data(meshInput.mesh);

        ++passIndex;
    }
}

void
Sequence::run()
{
    Timer timer;

    LOG(debug) << "Rendering...";

    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);

    for (auto& pass : m_passes) {
        const MeshInput& passMesh = require_primary_mesh(pass);
        glUseProgram(pass.program->getId());

        //
        // Send uniform data, bind textures etc.
        //

        int textureIndex = 0;

        for (auto& inputIt : pass.inputs) {
            auto& input = inputIt.second;

            switch (input.uniform->type) {
            case GL_SAMPLER_2D:
            case GL_IMAGE_2D: {
                GLuint textureId = input.texture->getId();

                if (pass.isCompute) {
                    //GLCall(glBindImageTexture(textureIndex, textureId, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F));
                    //GLCall(glBindImageTexture(textureIndex, textureId, 0, GL_FALSE, 0, GL_READ_ONLY, input.texture->getInternalFormat()));
                    GLCall(glActiveTexture(GL_TEXTURE0 + textureIndex));
                    GLCall(glBindTexture(GL_TEXTURE_2D, textureId));

                    LOG(debug) << "Texture " << textureId << " binding is " << textureIndex;
                } else {
                    GLCall(glActiveTexture(GL_TEXTURE0 + textureIndex));
                    GLCall(glBindTexture(GL_TEXTURE_2D, textureId));
                }

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, input.tex_min);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, input.tex_mag);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, input.tex_s);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, input.tex_t);

                if (input.tex_min != GL_LINEAR && input.tex_min != GL_NEAREST) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1000);  // Default LOD level is 1000
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, 0);
                    GLCall(glGenerateTextureMipmap(textureId));  // NOTE: OpenGL 4.5+
                    LOG(debug) << "Generated mip-maps for " << inputIt.first << " at " << inputIt.second.texture;
                }

                input.uniform->set(textureIndex++);
                //glUniform1i(glGetUniformLocation(pass.program->getId(), "u_texture0"), textureIndex++);
                break;
            }
            case GL_BOOL:
            case GL_BOOL_VEC2:
            case GL_BOOL_VEC3:
            case GL_BOOL_VEC4:
            case GL_INT:
            case GL_INT_VEC2:
            case GL_INT_VEC3:
            case GL_INT_VEC4: input.uniform->set(&input.ints[0]); break;
            case GL_FLOAT:
            case GL_FLOAT_VEC2:
            case GL_FLOAT_VEC3:
            case GL_FLOAT_VEC4: input.uniform->set(&input.floats[0]); break;
            case GL_DOUBLE:
            case GL_DOUBLE_VEC2:
            case GL_DOUBLE_VEC3:
            case GL_DOUBLE_VEC4: input.uniform->set(&input.doubles[0]); break;
            default: input.uniform->set(&input.floats[0]); break;
            }
        }
        //
        // Internal uniforms

        // Frame buffer size
        GLint uniform_loc = glGetUniformLocation(pass.program->getId(), "iFBsize");
        GLCall(glUniform2uiv(uniform_loc, 1, reinterpret_cast<unsigned int*>(pass.size)));
        // Frame buffer aspect ratio
        uniform_loc = glGetUniformLocation(pass.program->getId(), "iFBaspect");
        GLCall(glUniform1f(uniform_loc, pass.size[0] / (float)pass.size[1]));
        // Quad boolean
        uniform_loc = glGetUniformLocation(pass.program->getId(), "isQuad");
        GLCall(glUniform1i(uniform_loc, passMesh.mesh.isQuad));

        //
        // Atomic buffers
        //

        // Setup atomic counters list

        auto& pass_acounters = pass.program->get_m_acounters();

        for (auto& inputCounterIt : pass.inputCounters) {
            // check if the counter counterName is used in the shader
            auto passCounter = pass_acounters.find(inputCounterIt.first);

            if (passCounter == pass_acounters.end()) {
                LOG(warning) << "Atomic counter " << inputCounterIt.first << " is not used in the shader";
                exit(-1);
            }

            auto u_counterIt           = pass.u_aCounters.insert({ passCounter->second->binding, passCounters() });
            u_counterIt->second.buffer = passCounter->second;

            u_counterIt->second.value.resize(passCounter->second->size);
            u_counterIt->second.result.resize(passCounter->second->size);

            auto p_countIt = p_aCounters.insert({ passCounter->second->binding, m_passCounters() });

            p_countIt->second.buffer = passCounter->second;

            p_countIt->second.value.resize(passCounter->second->size);
            p_countIt->second.result.resize(passCounter->second->size);

            if (inputCounterIt.second.value.size() > u_counterIt->second.value.size()) {
                LOG(warning) << "Atomic counter " << inputCounterIt.first << " has more values than the shader";
                exit(-1);
            }

            std::memcpy(u_counterIt->second.value.data(), inputCounterIt.second.value.data(),
                        passCounter->second->size * sizeof(GLuint));
            std::memcpy(p_countIt->second.value.data(), inputCounterIt.second.value.data(),
                        passCounter->second->size * sizeof(GLuint));

            //u_counterIt->second.passIn = pass.fboId;
            u_counterIt->second.passIn = pass.fboId;
            p_countIt->second.passIn.insert({ pass.fboId, true });

            passCounter->second->userInput = true;
        }

        //for (auto& counterIt : pass_acounters)
        GLint bindingID = -1;
        bool cntrGroup  = false;

        for (std::pair<const std::string, std::shared_ptr<GLProgramBuffers>> counterIt : pass_acounters) {
            if (counterIt.second->userInput)
                continue;

            if (bindingID == counterIt.second->binding) {
                cntrGroup = true;
            } else {
                bindingID = counterIt.second->binding;
                cntrGroup = false;
            }

            std::multimap<GLint, passCounters>::iterator u_counterIt = pass.u_aCounters.insert(
                { counterIt.second->binding, passCounters() });

            // check if the binding already used
            //auto bindedCouters = p_aCounters.equal_range(counterIt.second->binding);
            //if (bindedCouters.first != bindedCouters.second) {
            int check = checkCounters(
                counterIt);  // 0 - not found, 1 - same binding, 2 - same binding and offset, 3 - identical

            // get shader counter binding offset and size and check it it overlap or identical

            std::multimap<GLint, m_passCounters>::iterator p_countIt;
            switch (check) {  // no binding
            case 0:
            case 1:
                p_countIt                = p_aCounters.insert({ counterIt.second->binding, m_passCounters() });
                p_countIt->second.buffer = counterIt.second;
                p_countIt->second.value.resize(counterIt.second->size);
                p_countIt->second.result.resize(counterIt.second->size);
                p_countIt->second.passIn.insert({ pass.fboId, false });
                cntrGroup = true;
                break;
            case 3:  // binding, offset and size the same - skip
                break;
            case 2:  // binding and offset the same but size different
                LOG(error) << "Atomic counter " << counterIt.first << " in pass " << pass.fboId
                           << " have different size than in other passes";
                //exit(-1);
            }

            u_counterIt->second.buffer = counterIt.second;

            u_counterIt->second.value.resize(counterIt.second->size);
            u_counterIt->second.result.resize(counterIt.second->size);

            u_counterIt->second.passIn = pass.fboId;


            LOG(trace) << "Atomic counter " << counterIt.first << " binding is " << counterIt.second->binding
                       << std::endl;
        }

        // Binding atomic counters
        std::cout << termcolor::bright_green;
        LOG(trace) << "Binding atomic counters" << std::endl;
        std::cout << termcolor::reset;

        auto it = pass.u_aCounters.begin();

        while (it != pass.u_aCounters.end()) {
            // Check if pass is not a first pass
            // Check if counter is not set
            // CHeck if counter do not have already used binding point

            //if (pass.fboId > 1) {
            //    break;
            //}

            GLuint buff_size = 0;
            //GLuint atomicCounterBufferID;

            auto range = pass.u_aCounters.equal_range(it->first);

            size_t range_size = std::distance(range.first, range.second);
            LOG(trace) << "Binding: " << it->first << " have " << range_size << " counter[s]." << std::endl;

            // Bind the buffer to the binding point

            GLCall(glGenBuffers(1, &it->second.bufferID));
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));

            for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
                // if new offset + counter size is bigger than buffer size than increase buffer size
                GLuint groupSize = groupIt->second.buffer->offset + groupIt->second.buffer->size * sizeof(GLuint);
                buff_size        = std::max(buff_size, groupSize);
                LOG(trace) << groupIt->second.buffer->name << " buff_size: " << buff_size / sizeof(GLuint) << std::endl;
            }
            // Allocate the buffer with null data
            GLCall(glBufferData(GL_ATOMIC_COUNTER_BUFFER, buff_size, nullptr, GL_DYNAMIC_DRAW));

            // set the value of the counter / per counter
            for (auto groupIt = range.first; groupIt != range.second; ++groupIt) {
                auto buffer = groupIt->second.buffer;
                GLCall(glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, buffer->offset, sizeof(GLuint) * buffer->size,
                                       groupIt->second.value.data()));
                LOG(trace) << buffer->name << " offset: " << buffer->offset << " size: " << buffer->size << std::endl;

                groupIt->second.bufferID = it->second.bufferID;
                buffer->isSet            = true;
            }

            GLCall(glBindBufferRange(GL_ATOMIC_COUNTER_BUFFER, it->first, it->second.bufferID, 0, buff_size));

            //auto pass_counter = p_acounters.insert({ it->first, {it->second.bufferID , it->second.buffer }});

            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

            it = range.second;
        }

        GLCall(glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT));

#if 0  //_DEBUG
        std::cout << termcolor::bright_yellow;
        LOG(trace) << "Check Binding atomic counters" << std::endl;
        std::cout << termcolor::reset;

        it = pass.u_aCounters.begin();

        while (it != pass.u_aCounters.end()) {
            std::vector<GLint> boundBuffer(it->second.size);
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));
            GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, it->second.offset, it->second.size * sizeof(GLuint), boundBuffer.data()));

            LOG(trace) << it->second.name << " offset: " << it->second.offset << " size: " << it->second.size;
            for (const auto& elem : boundBuffer) { std::cout << elem << ' '; }

            std::cout << std::endl;

            // Unbind the buffer after checking
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

            it++;
        }
#endif
        //
        // Render/compute
        //

        if (pass.isCompute) {
            // Bind output images
            for (auto& outputIt : pass.outputs) {
                auto& output = outputIt.second;

                //glActiveTexture(GL_TEXTURE0 + textureIndex);
                //glBindTexture(GL_TEXTURE_2D, output.texture->getId());
                GLCall(glBindImageTexture(textureIndex, output.texture->getId(), 0, GL_FALSE, 0, GL_WRITE_ONLY,
                                          output.texture->getInternalFormat()));

                LOG(debug) << "Texture " << output.texture->getId() << " binding is " << textureIndex;

                output.uniform->set(textureIndex++);
            }

            // Distribute the work groups across the number of threads
            GLCall(glDispatchCompute((pass.size[0] + pass.workGroupSize[0] - 1) / pass.workGroupSize[0],
                                     (pass.size[1] + pass.workGroupSize[1] - 1) / pass.workGroupSize[1], 1));
            GLCall(glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT));
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, pass.fboId);

            std::vector<GLenum> buffers(8, GL_NONE);  // 8 is the maximum number of color attachments

            for (auto& outputIt : pass.outputs) {
                auto& output                     = outputIt.second;
                buffers[output.output->location] = GL_COLOR_ATTACHMENT0 + output.output->location;
            }

            //
            // atomic buffers
            //
            GLuint ssbo;
            GLuint bindingPoint = 3;
            const GLint bufSize = 3;
            // make GLuint array bufSize with 0
            GLint initValues[bufSize] = { 0, 65535, 0 };
            GLCall(glGenBuffers(1, &ssbo));
            GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo));
            GLCall(glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * bufSize, initValues, GL_DYNAMIC_DRAW));

            GLCall(glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, ssbo));

            Pass::SSBObject ssbObject(GL_SHADER_STORAGE_BUFFER, ssbo, bindingPoint, bufSize, initValues);
            pass.glbObject.SSBO.insert({ "AtBuf", ssbObject });

            //

            //
            GLuint depthBuffer;
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

            GLCall(glBindVertexArray(passMesh.VBO.vaoId));

            GLCall(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
            GLCall(glDrawElements(passMesh.mesh.render, passMesh.mesh.numIndxs, GL_UNSIGNED_INT, 0));

            // get error code if any
            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                LOG(error) << "OpenGL error: " << err;
                exit(1);
            }

            //  Atomic counters get values after draw
            //GLCall(glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT));
            //GLCall(glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT));

            std::cout << termcolor::bright_cyan;
            LOG(trace) << "Atomic counters results:" << std::endl;
            std::cout << termcolor::reset;

            it = pass.u_aCounters.begin();

            while (it != pass.u_aCounters.end()) {
                auto buffer = it->second.buffer;
                std::vector<GLint> boundBuffer(buffer->size);
                GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, it->second.bufferID));
                GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, buffer->offset, buffer->size * sizeof(GLuint),
                                          boundBuffer.data()));

                LOG(trace) << buffer->name << " offset: " << buffer->offset << " size: " << buffer->size;
                for (const auto& elem : boundBuffer) {
                    std::cout << elem << ' ';
                }

                std::cout << std::endl;

                // Unbind the buffer after checking
                glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

                it++;
            }
#if 0
            for (auto& u_counterIt : pass.u_aCounters ) {
				GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, u_counterIt.second.bufferID));
                u_counterIt.second.result.resize(u_counterIt.second.size);
                GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint) * u_counterIt.second.size, u_counterIt.second.result.data()));
                LOG(info) << termcolor::bright_yellow <<
                    "Atomic counter " << u_counterIt.first << " = " << u_counterIt.second.result[0] << termcolor::reset;
			}

                        //GLuint counterValue1, counterValue2;
            auto counterIt = pass.glbObject.BO.find("counter1");
            if (counterIt == pass.glbObject.BO.end()) {
                LOG(error) << "counter1 not found";
                exit(1);
            };
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterIt->second.id));
            GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 2 * sizeof(GLuint), counterIt->second.value));

            auto counterIt2 = pass.glbObject.BO.find("counter2");
            if (counterIt2 == pass.glbObject.BO.end()) {
                LOG(error) << "counter2 not found";
                exit(1);
            };
            GLCall(glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterIt2->second.id));
            GLCall(glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), counterIt2->second.value));
            std::cout << "Max britgtness: " << static_cast<float>(counterIt->second.value[0]) / 65535.0f << std::endl;
            std::cout << "Min britgtness: " << static_cast<float>(counterIt->second.value[1]) / 65535.0f << std::endl;
            std::cout << "Count         : " << static_cast<unsigned int>(*counterIt2->second.value) << std::endl;

#endif
            auto aBuffIt = pass.glbObject.SSBO.find("AtBuf");
            if (aBuffIt == pass.glbObject.SSBO.end()) {
                LOG(error) << "AtBuf not found";
                exit(1);
            };
            GLuint bufferId    = aBuffIt->second.id;
            GLuint bufferSize  = aBuffIt->second.size;
            GLint* bufferValue = aBuffIt->second.value;

            GLCall(glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufferId));
            GLCall(glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * bufferSize, bufferValue));
            std::cout << "Atomic buffer [0]: " << bufferValue[0] << std::endl;
            std::cout << "Atomic buffer [1]: " << bufferValue[1] << std::endl;
            std::cout << "Atomic buffer [2]: " << static_cast<float>(bufferValue[2]) / 65535.0f << std::endl;

            GLCall(glDeleteBuffers(1, &ssbo));
            GLCall(glDeleteRenderbuffers(1, &depthBuffer));
            //GLCall(glDeleteBuffers(1, &counterObuff));
            //GLCall(glDeleteBuffers(1, &counterBuffer2));
            //
        }

        glFinish();

        //
        // Write output files, if any
        //

        for (auto& outputIt : pass.outputs) {
            auto& output = outputIt.second;
            output.saveTexture();
        }
    }

    // Destroying all passess atomicCounterBufferID
    for (auto& pass : m_passes) {
        auto counterIt = pass.u_aCounters.begin();
        while (counterIt != pass.u_aCounters.end()) {
            auto range = pass.u_aCounters.equal_range(counterIt->first);

            size_t range_size = std::distance(range.first, range.second);
            if (range_size > 1) {
                counterIt = range.second;
                if (counterIt != pass.u_aCounters.begin()) {
                    --counterIt;
                }
            }
            GLCall(glDeleteBuffers(1, &counterIt->second.bufferID));
            ++counterIt;
        }
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);

    LOG(debug) << "Sequence completed in " << timer.nowText();
}

#define STRING_USED_DEFAULTS "(used default)"
#define STRING_CHANGED_TO_SUPPORTED "(changed to highest supported value for file format)"
