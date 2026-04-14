// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_graph_runtime_plan.h"

#include "rawgl_graph_shared.h"
#include "rawgl_shader_interface_cache.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rawgl {
namespace {

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
    runtimeConfig.verbosity       = resourcePlan.verbosity;
    runtimeConfig.sharedTextures  = resourcePlan.sharedTextures;
    runtimeConfig.sharedMeshes    = resourcePlan.sharedMeshes;
    runtimeConfig.sharedGpuMeshes = resourcePlan.sharedGpuMeshes;

    for (size_t passIndex = 0; passIndex < resourcePlan.passes.size(); ++passIndex) {
        const RawGLGraphState::ResourcePass& resourcePass = resourcePlan.passes[passIndex];
        SequenceRuntimePassConfig passConfig;
        passConfig.isCompute        = resourcePass.isCompute;
        passConfig.program          = resourcePass.program;
        passConfig.size[0]          = resourcePass.size[0];
        passConfig.size[1]          = resourcePass.size[1];
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
            inputCounter.size     = counter->size;
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

}  // namespace

RawGLGraphState::ExecutionPlan
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

}  // namespace rawgl
