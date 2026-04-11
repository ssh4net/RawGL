/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
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

#include "RawGLCore.h"

#include "Common.h"
#include "GLProgramManager.h"
#include "Log.h"
#include "OpenGLUtils.h"
#include "Sequence.h"
#include "Timer.h"

#include <iostream>
#include <sstream>
#include <stdexcept>

const char* APP_NAME    = "RawGL";
const char* APP_AUTHOR  = "Erium Vladlen";
const int APP_VERSION[] = { 1, 6, 1 };

namespace rawgl {

struct RawGLContextState {
    OpenGLHandle glHandle;
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

static std::shared_ptr<GLProgram>
load_program_for_inspection(const ShaderInspectionRequest& request)
{
    return load_program(request.kind, request.paths);
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

static void
append_option(SequenceParsedArguments& parsedArguments,
              const std::string& key,
              std::vector<std::string>&& values)
{
    parsedArguments.options.push_back(SequenceParsedOption { key, std::move(values) });
}

static SequenceParsedArguments
build_sequence_parsed_arguments(const GraphBuildRequest& request)
{
    SequenceParsedArguments parsedArguments;
    parsedArguments.verbosity = request.definition.verbosity;

    for (const GraphPassDefinition& pass : request.definition.passes) {
        if (pass.programKind == ShaderProgramKind::compute) {
            if (pass.shaderPaths.empty()) {
                throw std::runtime_error("Typed graph compute pass requires a shader path.");
            }
            append_option(parsedArguments, "pass_comp", { pass.shaderPaths[0] });
        } else {
            if (pass.shaderPaths.empty() || pass.shaderPaths.size() > 2) {
                throw std::runtime_error("Typed graph vertfrag pass requires one or two shader paths.");
            }
            append_option(parsedArguments, "pass_vertfrag", std::vector<std::string>(pass.shaderPaths.begin(), pass.shaderPaths.end()));
        }

        append_option(parsedArguments, "pass_size", { std::to_string(pass.sizeX), std::to_string(pass.sizeY) });

        if (pass.programKind == ShaderProgramKind::compute && pass.hasExplicitWorkGroupSize) {
            append_option(parsedArguments,
                          "pass_workgroupsize",
                          { std::to_string(pass.workGroupSizeX), std::to_string(pass.workGroupSizeY) });
        }

        if (pass.clearColor[0] != 0.0f || pass.clearColor[1] != 0.0f || pass.clearColor[2] != 0.0f
            || pass.clearColor[3] != 0.0f) {
            append_option(parsedArguments,
                          "bg_color",
                          { format_numeric_value(pass.clearColor[0]),
                            format_numeric_value(pass.clearColor[1]),
                            format_numeric_value(pass.clearColor[2]),
                            format_numeric_value(pass.clearColor[3]) });
        }

        if (!pass.cullParameters.empty()) {
            std::vector<std::string> values;
            append_attribute_values(values, pass.cullParameters);
            append_option(parsedArguments, "cull", std::move(values));
        }

        for (const GraphMeshDefinition& mesh : pass.meshes) {
            std::vector<std::string> values;
            if (mesh.sourceKind == GraphMeshSourceKind::quad) {
                values.push_back("quad");
            } else {
                values.push_back("mesh");
                append_attribute_values(values, mesh.parameters);
                values.push_back(mesh.path);
            }
            append_option(parsedArguments, "pass_mesh", std::move(values));
        }

        for (const GraphInputDefinition& input : pass.inputs) {
            std::vector<std::string> values;
            values.push_back(input.name);

            switch (input.sourceKind) {
            case GraphInputSourceKind::intValues:
                for (const int32_t value : input.intValues) {
                    values.push_back(std::to_string(value));
                }
                break;
            case GraphInputSourceKind::uintValues:
                for (const uint32_t value : input.uintValues) {
                    values.push_back(std::to_string(value));
                }
                break;
            case GraphInputSourceKind::floatValues:
                for (const float value : input.floatValues) {
                    values.push_back(format_numeric_value(value));
                }
                break;
            case GraphInputSourceKind::doubleValues:
                for (const double value : input.doubleValues) {
                    values.push_back(format_numeric_value(value));
                }
                break;
            case GraphInputSourceKind::textureFile:
                append_attribute_values(values, input.attributes);
                values.push_back(input.texturePath);
                break;
            case GraphInputSourceKind::passOutput:
                values.push_back(input.referencedOutputName + "::" + std::to_string(input.referencedPassIndex));
                append_attribute_values(values, input.attributes);
                break;
            }
            append_option(parsedArguments, "in", std::move(values));
        }

        for (const GraphAtomicCounterDefinition& counter : pass.atomicCounters) {
            append_option(parsedArguments, "atomic", { "cntr", counter.name, std::to_string(counter.initialValue) });
        }

        for (const GraphOutputDefinition& output : pass.outputs) {
            append_option(parsedArguments, "out", { output.name, output.path });
            append_option(parsedArguments, "out_format", { output.format });
            append_option(parsedArguments, "out_channels", { std::to_string(output.channels) });
            append_option(parsedArguments, "out_alpha_channel", { std::to_string(output.alphaChannel) });
            append_option(parsedArguments, "out_bits", { std::to_string(output.bits) });
            for (const GraphAttribute& attribute : output.attributes) {
                append_option(parsedArguments, "out_attr", { attribute.name, attribute.value });
            }
        }
    }

    return parsedArguments;
}

}  // namespace

RawGLContext::RawGLContext()
    : m_state(std::make_shared<RawGLContextState>())
{
    Log_Init();
}

RawGLContext::~RawGLContext() = default;

ShaderInspectionResult
RawGLContext::inspectShaderInterface(const ShaderInspectionRequest& request) const
{
    ShaderInspectionResult result;
    result.isCompute = (request.kind == ShaderProgramKind::compute);

    try {
        std::shared_ptr<GLProgram> program = load_program_for_inspection(request);
        if (!program || !program->isValid()) {
            throw std::runtime_error("Failed to load program for shader inspection.");
        }

        for (const auto& uniformIt : program->getUniforms()) {
            result.uniforms.push_back(make_resource_info(uniformIt.first, uniformIt.second));
        }

        for (const auto& outputIt : program->getOutputs()) {
            result.outputs.push_back(make_resource_info(outputIt.first, outputIt.second));
        }

        for (const auto& counterIt : program->getAtomicCounters()) {
            result.atomicCounters.push_back(make_resource_info(counterIt.first, *counterIt.second));
        }

        for (const auto& bufferIt : program->getBufferVariables()) {
            result.bufferVariables.push_back(
                make_buffer_variable_info(bufferIt.first, bufferIt.second.first, bufferIt.second.second));
        }

        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

GraphBuildResult
RawGLContext::buildGraph(const GraphBuildRequest& request) const
{
    GraphBuildResult result;

    try {
        const SequenceParsedArguments parsedArguments = build_sequence_parsed_arguments(request);
        std::unique_ptr<Sequence> sequence = std::make_unique<Sequence>(parsedArguments);
        result.graph = std::make_unique<RawGLGraph>(m_state, std::move(sequence));
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

RawGLGraph::RawGLGraph(std::shared_ptr<RawGLContextState> contextState, std::unique_ptr<Sequence> sequence)
    : m_contextState(std::move(contextState))
    , m_sequence(std::move(sequence))
{
}

RawGLGraph::~RawGLGraph() = default;

GraphExecutionResult
RawGLGraph::execute(const GraphExecutionRequest& request)
{
    GraphExecutionResult result;
    (void)request;

    try {
        m_sequence->run();
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

Sequence&
RawGLGraph::sequence()
{
    return *m_sequence;
}

const Sequence&
RawGLGraph::sequence() const
{
    return *m_sequence;
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

Sequence&
CoreSession::sequence()
{
    return m_graph->sequence();
}

const Sequence&
CoreSession::sequence() const
{
    return m_graph->sequence();
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

ShaderInspectionResult
InspectShaderInterface(const ShaderInspectionRequest& request)
{
    RawGLContext context;
    return context.inspectShaderInterface(request);
}

}  // namespace rawgl
