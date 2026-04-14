// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_validation.h"

#include "graph_shared.h"
#include "shader_interface_cache.h"

#include <stdexcept>
#include <unordered_set>

namespace rawgl {
namespace {

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

}  // namespace

RawGLGraphState::ValidatedGraph
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

void
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
            throw std::runtime_error(
                "input override (" + inputOverride.name + "): program uniform is not a texture/image input");
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

}  // namespace rawgl
