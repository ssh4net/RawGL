// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.


#include "program_manager.h"
#include "log.h"

namespace {
enum class CombinedStage {
    vertex,
    fragment,
};

struct CombinedStageFrame {
    CombinedStage stage;
    bool negated          = false;
    bool current_selected = false;
    bool saw_else         = false;
};

struct CombinedSourceFrame {
    enum class Kind {
        generic,
        stage,
    };

    Kind kind = Kind::generic;
    CombinedStageFrame stage = {};
};

static std::string
trim_left_copy(const std::string& text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos) {
        return "";
    }
    return text.substr(begin);
}

static std::string
trim_copy(const std::string& text)
{
    const std::size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos) {
        return "";
    }
    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}

static bool
parse_combined_stage_condition(const std::string& text, CombinedStage& stage, bool& negated)
{
    const std::string trimmed = trim_copy(text);

    if (trimmed == "RAWGL_VERTEX_SHADER") {
        stage   = CombinedStage::vertex;
        negated = false;
        return true;
    }
    if (trimmed == "RAWGL_FRAGMENT_SHADER") {
        stage   = CombinedStage::fragment;
        negated = false;
        return true;
    }
    if (trimmed == "defined(RAWGL_VERTEX_SHADER)") {
        stage   = CombinedStage::vertex;
        negated = false;
        return true;
    }
    if (trimmed == "defined(RAWGL_FRAGMENT_SHADER)") {
        stage   = CombinedStage::fragment;
        negated = false;
        return true;
    }
    if (trimmed == "!defined(RAWGL_VERTEX_SHADER)") {
        stage   = CombinedStage::vertex;
        negated = true;
        return true;
    }
    if (trimmed == "!defined(RAWGL_FRAGMENT_SHADER)") {
        stage   = CombinedStage::fragment;
        negated = true;
        return true;
    }

    return false;
}

static bool
parse_combined_stage_directive(const std::string& line,
                               std::string& directive,
                               CombinedStage& stage,
                               bool& negated)
{
    const std::string trimmed = trim_left_copy(line);
    if (trimmed.empty() || trimmed[0] != '#') {
        return false;
    }

    const std::size_t directive_begin = trimmed.find_first_not_of(" \t", 1);
    if (directive_begin == std::string::npos) {
        return false;
    }

    const std::size_t directive_end = trimmed.find_first_of(" \t", directive_begin);
    directive = trimmed.substr(directive_begin,
                               directive_end == std::string::npos ? std::string::npos
                                                                  : directive_end - directive_begin);

    const std::string remainder
        = directive_end == std::string::npos ? ""
                                             : trim_copy(trimmed.substr(directive_end + 1));

    if (directive == "ifdef") {
        if (!parse_combined_stage_condition(remainder, stage, negated) || negated) {
            return false;
        }
        negated = false;
        return true;
    }
    if (directive == "ifndef") {
        if (!parse_combined_stage_condition(remainder, stage, negated) || negated) {
            return false;
        }
        negated = true;
        return true;
    }
    if (directive == "if" || directive == "elif") {
        return parse_combined_stage_condition(remainder, stage, negated);
    }

    return false;
}

static bool
is_stage_selected(const CombinedStage target_stage, const CombinedStage stage, const bool negated)
{
    const bool matched = (target_stage == stage);
    return negated ? !matched : matched;
}

static std::string
split_combined_stage_source(const std::string& source, const CombinedStage target_stage)
{
    std::ostringstream output;
    std::vector<CombinedSourceFrame> frames;

    auto emit_blank_line = [&output]() { output << '\n'; };
    auto current_stage_selected = [&frames]() {
        for (const CombinedSourceFrame& frame : frames) {
            if (frame.kind == CombinedSourceFrame::Kind::stage && !frame.stage.current_selected) {
                return false;
            }
        }
        return true;
    };

    std::size_t line_begin = 0;
    while (line_begin <= source.size()) {
        const std::size_t line_end = source.find('\n', line_begin);
        const bool has_newline     = (line_end != std::string::npos);
        const std::size_t line_size
            = has_newline ? (line_end - line_begin) : (source.size() - line_begin);
        const std::string line = source.substr(line_begin, line_size);
        const std::string trimmed = trim_left_copy(line);

        const bool selected_before_directive = current_stage_selected();

        std::string directive;
        CombinedStage directive_stage {};
        bool directive_negated = false;

        if (parse_combined_stage_directive(line, directive, directive_stage, directive_negated)) {
            if (directive == "elif") {
                if (frames.empty() || frames.back().kind != CombinedSourceFrame::Kind::stage) {
                    throw std::runtime_error("combined vertfrag source has #elif without matching stage guard");
                }

                CombinedStageFrame& frame = frames.back().stage;
                if (frame.saw_else) {
                    throw std::runtime_error("combined vertfrag source has #elif after #else");
                }

                frame.stage            = directive_stage;
                frame.negated          = directive_negated;
                frame.current_selected = is_stage_selected(target_stage, directive_stage, directive_negated);
            } else {
                CombinedSourceFrame frame;
                frame.kind                    = CombinedSourceFrame::Kind::stage;
                frame.stage.stage             = directive_stage;
                frame.stage.negated           = directive_negated;
                frame.stage.current_selected  = is_stage_selected(target_stage, directive_stage, directive_negated);
                frame.stage.saw_else          = false;
                frames.push_back(frame);
            }
            emit_blank_line();
        } else if (trimmed.rfind("#ifdef", 0) == 0 || trimmed.rfind("#ifndef", 0) == 0
                   || trimmed.rfind("#if", 0) == 0) {
            frames.push_back(CombinedSourceFrame {});
            if (selected_before_directive) {
                output << line << '\n';
            } else {
                emit_blank_line();
            }
        } else if (trimmed.rfind("#else", 0) == 0) {
            if (!frames.empty() && frames.back().kind == CombinedSourceFrame::Kind::stage) {
                CombinedStageFrame& frame = frames.back().stage;
                if (frame.saw_else) {
                    throw std::runtime_error("combined vertfrag source has duplicate #else in stage guard");
                }
                frame.saw_else         = true;
                frame.current_selected = !frame.current_selected;
                emit_blank_line();
            } else if (selected_before_directive) {
                output << line << '\n';
            } else {
                emit_blank_line();
            }
        } else if (trimmed.rfind("#endif", 0) == 0) {
            if (!frames.empty()) {
                const bool closing_stage = (frames.back().kind == CombinedSourceFrame::Kind::stage);
                frames.pop_back();
                if (closing_stage) {
                    emit_blank_line();
                } else if (selected_before_directive) {
                    output << line << '\n';
                } else {
                    emit_blank_line();
                }
            } else if (selected_before_directive) {
                output << line << '\n';
            } else {
                emit_blank_line();
            }
        } else if (selected_before_directive) {
            output << line;
            if (has_newline || line_begin < source.size()) {
                output << '\n';
            }
        } else {
            emit_blank_line();
        }

        if (!has_newline) {
            break;
        }
        line_begin = line_end + 1;
    }

    if (!frames.empty()) {
        throw std::runtime_error("combined vertfrag source has unterminated stage guard");
    }

    return output.str();
}

static std::string
module_cache_key_suffix(const rawgl::ShaderModuleDefinition& module)
{
    std::ostringstream stream;
    stream << static_cast<int>(module.role) << ':'
           << static_cast<int>(module.sourceKind) << ':'
           << module.debugLabel << ':'
           << module.path << ':'
           << module.glslText.size() << ':';

    for (const std::byte byteValue : module.spirvBytes) {
        const unsigned int value = std::to_integer<unsigned int>(byteValue);
        const char digits[] = "0123456789abcdef";
        stream << digits[(value >> 4u) & 0x0fu] << digits[value & 0x0fu];
    }

    return stream.str();
}

static std::string
build_modules_cache_key(const std::string& name, const std::vector<rawgl::ShaderModuleDefinition>& modules)
{
    std::ostringstream stream;
    stream << name;
    for (const rawgl::ShaderModuleDefinition& module : modules) {
        stream << '\n' << module_cache_key_suffix(module);
    }
    return stream.str();
}

static std::vector<char>
copy_spirv_bytes(const std::vector<std::byte>& bytes)
{
    std::vector<char> result(bytes.size());
    for (size_t index = 0; index < bytes.size(); ++index) {
        result[index] = static_cast<char>(bytes[index]);
    }
    return result;
}
}  // namespace

//
// Program loading
//

std::shared_ptr<GLProgram>
GLProgramManager::loadVertFrag(const std::string& path)
{
    LOG(info) << "Loading program from a single text file (vertex, fragment): " << path;

    auto it = m_list.find(path);

    if (it == m_list.end()) {
        // Attempt to load
        std::string text;

        if (!loadTextFile(path, text))
            return nullptr;

        std::vector<std::shared_ptr<GLShader>> shaders {
            std::make_shared<GLShader>(GL_VERTEX_SHADER, split_combined_stage_source(text, CombinedStage::vertex)),
            std::make_shared<GLShader>(GL_FRAGMENT_SHADER,
                                       split_combined_stage_source(text, CombinedStage::fragment))
        };

        it = m_list.insert({ path, std::make_shared<GLProgram>(shaders) }).first;
    }

    return it->second;
}

std::shared_ptr<GLProgram>
GLProgramManager::loadVertFrag(const std::string paths[])
{
    LOG(info) << "Loading program from files (vertex, fragment): " << std::endl
              << "\t" << paths[0] << std::endl
              << "\t" << paths[1];

    auto name = paths[0] + ":" + paths[1];
    auto it   = m_list.find(name);

    if (it == m_list.end()) {
        // Attempt to load
        const std::pair<std::string, GLenum> types[] { { ".vert", GL_VERTEX_SHADER },
                                                       { ".frag", GL_FRAGMENT_SHADER },
                                                       { ".vert_spv", GL_VERTEX_SHADER },
                                                       { ".frag_spv", GL_FRAGMENT_SHADER } };

        std::vector<std::shared_ptr<GLShader>> shaders;

        for (int i = 0; i < 2; i++) {
            const std::string ext(std::filesystem::path(paths[i]).extension().string());

            if (ext == types[i].first) {
                std::string text;

                if (!loadTextFile(paths[i], text))
                    return nullptr;

                shaders.push_back(std::make_shared<GLShader>(types[i].second, text));
            } else if (ext == types[i + 2].first) {
                std::vector<char> data;

                if (!loadBinaryFile(paths[i], data))
                    return nullptr;

                shaders.push_back(std::make_shared<GLShader>(types[i + 2].second, data));
            } else {
                LOG(error) << "Unknown shader file extension " << ext;
                return nullptr;
            }
        }

        it = m_list.insert({ name, std::make_shared<GLProgram>(shaders) }).first;
    }

    return it->second;
}

std::shared_ptr<GLProgram>
GLProgramManager::loadVertFragStrings(const std::string& name, const std::string sources[])
{
    LOG(info) << "Loading program from strings (vertex, fragment): " << name;

    std::vector<std::shared_ptr<GLShader>> shaders { std::make_shared<GLShader>(GL_VERTEX_SHADER, sources[0]),
                                                     std::make_shared<GLShader>(GL_FRAGMENT_SHADER, sources[1]) };

    auto it = m_list.insert({ name, std::make_shared<GLProgram>(shaders) }).first;

    return it->second;
}

std::shared_ptr<GLProgram>
GLProgramManager::loadVertFragModules(const std::string& name, const std::vector<rawgl::ShaderModuleDefinition>& modules)
{
    LOG(info) << "Loading program from structured modules (vertex, fragment): " << name;

    const std::string cacheKey = build_modules_cache_key(name, modules);
    auto it                    = m_list.find(cacheKey);
    if (it != m_list.end()) {
        return it->second;
    }

    if (modules.empty() || modules.size() > 2u) {
        LOG(error) << "Structured vertex/fragment program requires one combined module or two stage modules.";
        return nullptr;
    }

    std::vector<std::shared_ptr<GLShader>> shaders;
    if (modules.size() == 1u) {
        const rawgl::ShaderModuleDefinition& module = modules[0];
        if (module.role != rawgl::ShaderModuleRole::automatic) {
            LOG(error) << "Single-module vertex/fragment program must use the automatic module role.";
            return nullptr;
        }

        if (module.sourceKind == rawgl::ShaderModuleSourceKind::filePath) {
            return loadVertFrag(module.path);
        }
        if (module.sourceKind == rawgl::ShaderModuleSourceKind::spirvBinary) {
            LOG(error) << "Single-module vertex/fragment SPIR-V is unsupported.";
            return nullptr;
        }
        if (module.glslText.empty()) {
            LOG(error) << "Single-module vertex/fragment GLSL text must not be empty.";
            return nullptr;
        }

        shaders.push_back(std::make_shared<GLShader>(GL_VERTEX_SHADER,
                                                     split_combined_stage_source(module.glslText, CombinedStage::vertex)));
        shaders.push_back(std::make_shared<GLShader>(GL_FRAGMENT_SHADER,
                                                     split_combined_stage_source(module.glslText, CombinedStage::fragment)));
        it = m_list.insert({ cacheKey, std::make_shared<GLProgram>(shaders) }).first;
        return it->second;
    }

    for (size_t moduleIndex = 0; moduleIndex < modules.size(); ++moduleIndex) {
        const rawgl::ShaderModuleDefinition& module = modules[moduleIndex];
        const GLenum stage = (moduleIndex == 0u) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
        const rawgl::ShaderModuleRole requiredRole =
            (moduleIndex == 0u) ? rawgl::ShaderModuleRole::vertex : rawgl::ShaderModuleRole::fragment;

        if (module.role != requiredRole) {
            LOG(error) << "Structured vertex/fragment module role mismatch.";
            return nullptr;
        }

        if (module.sourceKind == rawgl::ShaderModuleSourceKind::filePath) {
            const std::string extension(std::filesystem::path(module.path).extension().string());
            if (extension == ((stage == GL_VERTEX_SHADER) ? ".vert_spv" : ".frag_spv")) {
                std::vector<char> data;
                if (!loadBinaryFile(module.path, data))
                    return nullptr;
                shaders.push_back(std::make_shared<GLShader>(stage, data));
            } else {
                std::string text;
                if (!loadTextFile(module.path, text))
                    return nullptr;
                shaders.push_back(std::make_shared<GLShader>(stage, text));
            }
            continue;
        }

        if (module.sourceKind == rawgl::ShaderModuleSourceKind::glslText) {
            if (module.glslText.empty()) {
                LOG(error) << "Structured GLSL shader module text must not be empty.";
                return nullptr;
            }
            shaders.push_back(std::make_shared<GLShader>(stage, module.glslText));
            continue;
        }

        if (module.spirvBytes.empty()) {
            LOG(error) << "Structured SPIR-V shader module bytes must not be empty.";
            return nullptr;
        }
        shaders.push_back(std::make_shared<GLShader>(stage, copy_spirv_bytes(module.spirvBytes)));
    }

    it = m_list.insert({ cacheKey, std::make_shared<GLProgram>(shaders) }).first;
    return it->second;
}

std::shared_ptr<GLProgram>
GLProgramManager::loadComp(const std::string& path)
{
    LOG(info) << "Loading program from a text file (compute): " << path;

    auto it = m_list.find(path);

    if (it == m_list.end()) {
        // Attempt to load it
        std::vector<std::shared_ptr<GLShader>> shaders;
        const std::string ext(std::filesystem::path(path).extension().string());

        if (ext == ".comp") {
            std::string text;

            if (!loadTextFile(path, text))
                return nullptr;

            shaders.push_back(std::make_shared<GLShader>(GL_COMPUTE_SHADER, text));
        } else if (ext == ".comp_spv") {
            std::vector<char> data;

            if (!loadBinaryFile(path, data))
                return nullptr;

            shaders.push_back(std::make_shared<GLShader>(GL_COMPUTE_SHADER, data));
        } else {
            LOG(error) << "Unknown shader file extension " << ext;
            return nullptr;
        }

        it = m_list.insert({ path, std::make_shared<GLProgram>(shaders) }).first;
    }

    return it->second;
}

std::shared_ptr<GLProgram>
GLProgramManager::loadCompString(const std::string& name, const std::string& source)
{
    LOG(info) << "Loading program from string (compute): " << name;

    std::vector<std::shared_ptr<GLShader>> shaders {
        std::make_shared<GLShader>(GL_COMPUTE_SHADER, source),
    };

    auto it = m_list.insert({ name, std::make_shared<GLProgram>(shaders) }).first;

    return it->second;
}

std::shared_ptr<GLProgram>
GLProgramManager::loadCompModule(const std::string& name, const rawgl::ShaderModuleDefinition& module)
{
    LOG(info) << "Loading program from a structured module (compute): " << name;

    const std::string cacheKey = build_modules_cache_key(name, { module });
    auto it                    = m_list.find(cacheKey);
    if (it != m_list.end()) {
        return it->second;
    }

    if (module.role != rawgl::ShaderModuleRole::automatic && module.role != rawgl::ShaderModuleRole::compute) {
        LOG(error) << "Structured compute module cannot use vertex or fragment roles.";
        return nullptr;
    }

    if (module.sourceKind == rawgl::ShaderModuleSourceKind::filePath) {
        return loadComp(module.path);
    }

    if (module.sourceKind == rawgl::ShaderModuleSourceKind::glslText) {
        if (module.glslText.empty()) {
            LOG(error) << "Structured compute GLSL text must not be empty.";
            return nullptr;
        }
        return loadCompString(cacheKey, module.glslText);
    }

    if (module.spirvBytes.empty()) {
        LOG(error) << "Structured compute SPIR-V bytes must not be empty.";
        return nullptr;
    }

    std::vector<std::shared_ptr<GLShader>> shaders {
        std::make_shared<GLShader>(GL_COMPUTE_SHADER, copy_spirv_bytes(module.spirvBytes)),
    };

    it = m_list.insert({ cacheKey, std::make_shared<GLProgram>(shaders) }).first;
    return it->second;
}

//
// File loading
//

/*
std::unique_ptr<GLShader> GLProgramManager::loadShader(const std::string& path, const std::string &macros)
{
	const std::filesystem::path filePath(path);
	const std::string ext(filePath.extension().string());
	const std::unordered_map<std::string, GLenum> types
	{
		{ "vert", GL_VERTEX_SHADER },
		{ "vert_spv", GL_VERTEX_SHADER },
		{ "frag", GL_FRAGMENT_SHADER },
		{ "frag_spv", GL_FRAGMENT_SHADER },
		{ "comp", GL_COMPUTE_SHADER },
		{ "comp_spv", GL_COMPUTE_SHADER }
	};

	if (ext == "vert_spv" || ext == "frag_spv" || ext == "comp_spv")
	{
		std::ifstream fs(path, std::ios::binary);

		if (!fs.is_open())
		{
			LOG(error) << "Can't find " << path;
			return false;
		}

		std::vector<char> data;
		data.assign(std::istreambuf_iterator<char>(fs), {});

		fs.close();

		return std::make_unique<GLShader>(types[ext].second, data);
	}
	else if (ext == "vert" || ext == "frag" || ext == "comp")
	{
		std::ifstream fs(path);

		if (!fs.is_open())
		{
			LOG(error) << "Can't find " << path;
			return false;
		}

		std::ostringstream ss;
		ss << fs.rdbuf();
		auto text = ss.str();

		fs.close();

		return std::make_unique<GLShader>(types[ext].second, macros + "\n" + text);
	}

	LOG(error) << "Unknown shader file extension " << ext;
}
*/
bool
GLProgramManager::loadTextFile(const std::string& path, std::string& out)
{
    std::ifstream fs(path);

    if (!fs.is_open()) {
        LOG(error) << "Can't find " << path;
        return false;
    }

    std::ostringstream ss;
    ss << fs.rdbuf();
    out = ss.str();

    fs.close();

    return true;
}

bool
GLProgramManager::loadBinaryFile(const std::string& path, std::vector<char>& out)
{
    std::ifstream fs(path, std::ios::binary);

    if (!fs.is_open()) {
        LOG(error) << "Can't find " << path;
        return false;
    }

    out.assign(std::istreambuf_iterator<char>(fs), {});

    fs.close();

    return true;
}
