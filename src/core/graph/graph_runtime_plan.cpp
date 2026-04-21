// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_runtime_plan.h"

#include "graph_resources.h"
#include "graph_shared.h"
#include "shader_interface_cache.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rawgl {
namespace {

static PassOutput&
ensure_output_binding(SequenceRuntimePassConfig& passConfig,
                      const ShaderInterface& shaderInterface,
                      const std::string& name,
                      const bool usesArrayElement,
                      const size_t arrayElement)
{
    const std::string addressedName = build_addressed_resource_name(name, usesArrayElement, arrayElement);
    auto [outputIt, inserted]       = passConfig.outputs.insert({ addressedName, PassOutput() });
    (void)inserted;
    PassOutput& output = outputIt->second;
    output.usesArrayElement = usesArrayElement;
    output.arrayElement     = arrayElement;

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
    PassOutput& output = ensure_output_binding(passConfig,
                                               shaderInterface,
                                               definition.name,
                                               definition.usesArrayElement,
                                               definition.arrayElement);
    output.internalFormatText = definition.format;
    output.channels           = definition.channels;
    output.alphaChannel       = definition.alphaChannel;
}

static void
ensure_referenced_output(SequenceRuntimeConfig& runtimeConfig,
                         const size_t referencedPassIndex,
                         const std::string& outputName,
                         const bool usesArrayElement,
                         const size_t arrayElement)
{
    if (referencedPassIndex >= runtimeConfig.passes.size()) {
        throw std::runtime_error("invalid referenced pass index in " + outputName + "::" + std::to_string(referencedPassIndex));
    }

    SequenceRuntimePassConfig& referencedPass = runtimeConfig.passes[referencedPassIndex];
    const ShaderInterface referencedInterface = build_shader_interface(
        referencedPass.program,
        referencedPass.isCompute ? ShaderProgramKind::compute : ShaderProgramKind::vertfrag);
    ensure_output_binding(referencedPass, referencedInterface, outputName, usesArrayElement, arrayElement);
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
            const std::string addressedInputName =
                build_addressed_resource_name(inputDefinition.name,
                                              inputDefinition.usesArrayElement,
                                              inputDefinition.arrayElement);
            GLProgramUniform* uniform = passConfig.program->findUniform(inputDefinition.name);
            PassInput input;
            input.uniform = uniform;
            input.usesArrayElement = inputDefinition.usesArrayElement;

            if (inputDefinition.sourceKind == GraphInputSourceKind::textureFile
                || inputDefinition.sourceKind == GraphInputSourceKind::hostTexture
                || inputDefinition.sourceKind == GraphInputSourceKind::passOutput) {
                apply_texture_attributes(input, inputDefinition.attributes);

                if (inputDefinition.sourceKind == GraphInputSourceKind::textureFile) {
                    const std::string cacheKey =
                        build_texture_resource_key(inputDefinition.texturePath, inputDefinition.attributes);
                    auto textureIt = resourcePlan.sharedTextures.find(cacheKey);
                    if (textureIt == resourcePlan.sharedTextures.end() || !textureIt->second) {
                        throw std::runtime_error("in (" + inputDefinition.name + "): prepared texture resource is missing");
                    }
                    input.texture = textureIt->second;
                } else if (inputDefinition.sourceKind == GraphInputSourceKind::hostTexture) {
                    if (!inputDefinition.hostTexture) {
                        throw std::runtime_error("in (" + inputDefinition.name + "): host texture payload is missing");
                    }
                    input.texture = create_host_texture_resource(*inputDefinition.hostTexture,
                                                                 "in (" + inputDefinition.name + ")");
                } else {
                    ensure_referenced_output(runtimeConfig,
                                             inputDefinition.referencedPassIndex,
                                             inputDefinition.referencedOutputName,
                                             inputDefinition.usesReferencedOutputArrayElement,
                                             inputDefinition.referencedOutputArrayElement);
                    input.path = build_addressed_resource_name(inputDefinition.referencedOutputName,
                                                               inputDefinition.usesReferencedOutputArrayElement,
                                                               inputDefinition.referencedOutputArrayElement)
                                 + "::"
                                 + std::to_string(inputDefinition.referencedPassIndex);
                }
            } else if (inputDefinition.sourceKind == GraphInputSourceKind::graphTexture) {
                apply_texture_attributes(input, inputDefinition.attributes);
                input.runtimeTextureBindingRequired = true;
            } else {
                GraphInputSourceKind expectedKind = GraphInputSourceKind::intValues;
                uint8_t fieldCount                = 0;
                extract_numeric_layout(uniform->type, expectedKind, fieldCount);
                if (inputDefinition.usesArrayElement) {
                    const GLint addressedLocation =
                        glGetUniformLocation(passConfig.program->getId(), addressedInputName.c_str());
                    if (addressedLocation < 0) {
                        throw std::runtime_error("in (" + addressedInputName + "): array element uniform not found");
                    }
                    input.addressedLocation = addressedLocation;
                }

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

            passConfig.inputs.insert({ addressedInputName, input });
        }

        for (const GraphAtomicCounterDefinition& counterDefinition : resourcePass.atomicCounters) {
            std::shared_ptr<GLProgramBuffers> counter = passConfig.program->findCounter(counterDefinition.name);
            auto [counterIt, inserted] =
                passConfig.inputCounters.insert({ counterDefinition.name, SequencePass::inputCounter() });
            (void)inserted;
            SequencePass::inputCounter& inputCounter = counterIt->second;
            if (inputCounter.value.empty() || inputCounter.size != counter->size) {
                inputCounter.size = counter->size;
                inputCounter.value.assign(counter->size, 0u);
            }
            const size_t valueIndex = counterDefinition.usesArrayElement ? counterDefinition.arrayElement : 0u;
            inputCounter.value[valueIndex] = counterDefinition.initialValue;
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
            executionPass.inputNames.push_back(build_addressed_resource_name(inputDefinition.name,
                                                                             inputDefinition.usesArrayElement,
                                                                             inputDefinition.arrayElement));
            if (inputDefinition.sourceKind == GraphInputSourceKind::passOutput) {
                append_unique_dependency(executionPass.dependencyPassIndices, inputDefinition.referencedPassIndex);
            } else if (inputDefinition.sourceKind == GraphInputSourceKind::graphTexture) {
                executionPlan.persistentInputs.push_back(RawGLGraphState::PersistentInputBinding {
                    passIndex,
                    build_addressed_resource_name(inputDefinition.name,
                                                  inputDefinition.usesArrayElement,
                                                  inputDefinition.arrayElement),
                    inputDefinition.graphTextureName,
                });
            }
        }

        for (const GraphOutputDefinition& outputDefinition : resourcePass.outputs) {
            const std::string addressedOutputName = build_addressed_resource_name(outputDefinition.name,
                                                                                  outputDefinition.usesArrayElement,
                                                                                  outputDefinition.arrayElement);
            executionPass.outputNames.push_back(addressedOutputName);
            if (!outputDefinition.persistentTextureName.empty()) {
                executionPlan.persistentOutputs.push_back(RawGLGraphState::PersistentOutputBinding {
                    passIndex,
                    addressedOutputName,
                    outputDefinition.persistentTextureName,
                });
            }
            if (!outputDefinition.path.empty()) {
                RawGLGraphState::FileOutputBinding fileOutput;
                fileOutput.passIndex    = passIndex;
                fileOutput.outputName   = addressedOutputName;
                fileOutput.path         = outputDefinition.path;
                fileOutput.alphaChannel = outputDefinition.alphaChannel;
                fileOutput.bits         = outputDefinition.bits;
                for (const GraphAttribute& attribute : outputDefinition.attributes) {
                    fileOutput.attributes.insert({ attribute.name, attribute.value });
                }
                executionPlan.fileOutputs.push_back(std::move(fileOutput));
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
