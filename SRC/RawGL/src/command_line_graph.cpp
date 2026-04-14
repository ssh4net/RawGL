// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "command_line_graph.h"

#include "command_line_parser.h"
#include "common.h"
#include "gl_program_manager.h"
#include "sequence.h"

#include <stdexcept>

namespace rawgl {
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

static std::shared_ptr<GLProgram>
load_program_for_command_line(const ShaderProgramKind kind, const std::vector<std::string>& paths)
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

static void
translate_pass_declaration(const SequenceParsedOption& option, GraphDefinition& definition, GraphTranslationState& state)
{
    GraphPassDefinition pass;
    pass.programKind = (option.string_key == "pass_comp") ? ShaderProgramKind::compute : ShaderProgramKind::vertfrag;
    pass.shaderPaths = option.value;
    if (!definition.passes.empty()) {
        pass.sizeX = definition.passes.back().sizeX;
        pass.sizeY = definition.passes.back().sizeY;
    }

    try {
        state.currentProgram = load_program_for_command_line(pass.programKind, pass.shaderPaths);
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
            state.currentPass->sizeY =
                (option.value.size() > 1) ? parse_positive_int(option.value[1], "pass_size") : state.currentPass->sizeX;
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

    if (option.string_key != "pass_mesh") {
        return;
    }

    if (option.value.empty()) {
        throw std::runtime_error("pass_mesh: must have at least 1 parameter.");
    }

    GraphMeshDefinition mesh;
    const size_t split         = option.value[0].find("::");
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
                input.sourceKind          = GraphInputSourceKind::passOutput;
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

}  // namespace

GraphBuildRequest
BuildGraphRequestFromCommandLine(const CommandLineRequest& request)
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

}  // namespace rawgl
