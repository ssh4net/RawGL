// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_validation.h"

#include "graph_shared.h"
#include "shader_interface_cache.h"

#include <stdexcept>
#include <unordered_set>

namespace rawgl {
namespace {

static void
validate_array_element_access(const ShaderResourceInfo& resource,
                              const bool usesArrayElement,
                              const size_t arrayElement,
                              const std::string& context)
{
    if (!resource.isArray || resource.arrayLength <= 1) {
        if (usesArrayElement) {
            throw std::runtime_error(context + ": array element was specified for a non-array resource");
        }
        return;
    }

    if (!usesArrayElement) {
        throw std::runtime_error(context + ": array resource requires an explicit array element");
    }

    if (arrayElement >= resource.arrayLength) {
        throw std::runtime_error(context + ": array element " + std::to_string(arrayElement)
                                 + " exceeds reflected array length " + std::to_string(resource.arrayLength));
    }
}

static size_t
byte_size_for_gl_type(const GLenum type)
{
    switch (type) {
    case GL_UNSIGNED_BYTE: return 1u;
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT: return 2u;
    case GL_UNSIGNED_INT:
    case GL_FLOAT: return 4u;
    default: break;
    }

    return 0u;
}

static int
channel_count_for_internal_format(const GLenum internalFormat)
{
    switch (internalFormat) {
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
    case GL_R8_SNORM:
    case GL_R16:
    case GL_R16I:
    case GL_R16UI:
    case GL_R16_SNORM:
    case GL_R16F:
    case GL_R32I:
    case GL_R32UI:
    case GL_R32F: return 1;
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_RG8_SNORM:
    case GL_RG16:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG16_SNORM:
    case GL_RG16F:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG32F: return 2;
    case GL_RGB8:
    case GL_RGB8I:
    case GL_RGB8UI:
    case GL_RGB8_SNORM:
    case GL_RGB10_A2:
    case GL_R11F_G11F_B10F:
    case GL_RGB16:
    case GL_RGB16I:
    case GL_RGB16UI:
    case GL_RGB16_SNORM:
    case GL_RGB16F:
    case GL_RGB32I:
    case GL_RGB32UI:
    case GL_RGB32F:
    case GL_SRGB8: return 3;
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_RGBA8_SNORM:
    case GL_RGBA16:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA16_SNORM:
    case GL_RGBA16F:
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA32F:
    case GL_SRGB8_ALPHA8: return 4;
    default: break;
    }

    return 0;
}

static void
validate_host_image_data(const HostImageData& hostImage, const std::string& context)
{
    if (hostImage.width <= 0 || hostImage.height <= 0) {
        throw std::runtime_error(context + ": host texture dimensions must be > 0");
    }
    if (hostImage.channels <= 0 || hostImage.channels > 4) {
        throw std::runtime_error(context + ": host texture channel count must be between 1 and 4");
    }
    if (hostImage.alphaChannel >= hostImage.channels) {
        throw std::runtime_error(context + ": host texture alpha channel exceeds the channel count");
    }

    const int inferredChannels = channel_count_for_internal_format(hostImage.glInternalFormat);
    if (inferredChannels == 0) {
        throw std::runtime_error(context + ": unsupported OpenGL internal format");
    }
    if (inferredChannels != hostImage.channels) {
        throw std::runtime_error(context + ": host texture channel count does not match the OpenGL internal format");
    }

    const size_t bytesPerComponent = byte_size_for_gl_type(hostImage.glType);
    if (bytesPerComponent == 0u) {
        throw std::runtime_error(context + ": unsupported OpenGL host texture element type");
    }

    const size_t expectedByteCount = static_cast<size_t>(hostImage.width) * static_cast<size_t>(hostImage.height)
                                     * static_cast<size_t>(hostImage.channels) * bytesPerComponent;
    if (hostImage.bytes.size() != expectedByteCount) {
        throw std::runtime_error(context + ": host texture byte size does not match width, height, channels, and type");
    }
}

static void
validate_host_mesh_data(const HostMeshData& hostMesh)
{
    if (hostMesh.positions.empty() || hostMesh.positions.size() % 3u != 0u) {
        throw std::runtime_error("pass_mesh: host mesh positions must be float32[N, 3].");
    }
    if (hostMesh.indices.empty()) {
        throw std::runtime_error("pass_mesh: host mesh indices must not be empty.");
    }

    const size_t vertexCount = hostMesh.positions.size() / 3u;
    if (!hostMesh.texcoords.empty() && hostMesh.texcoords.size() != vertexCount * 2u) {
        throw std::runtime_error("pass_mesh: host mesh texcoords must be float32[N, 2].");
    }
    if (!hostMesh.normals.empty() && hostMesh.normals.size() != vertexCount * 3u) {
        throw std::runtime_error("pass_mesh: host mesh normals must be float32[N, 3].");
    }
    if (!hostMesh.colors.empty() && hostMesh.colors.size() != vertexCount * 4u) {
        throw std::runtime_error("pass_mesh: host mesh colors must be uint8[N, 4].");
    }
    if (!hostMesh.id0.empty() && hostMesh.id0.size() != vertexCount) {
        throw std::runtime_error("pass_mesh: host mesh id0 values must be uint32[N].");
    }

    std::unordered_set<uint32_t> usedAttributeLocations;
    for (const HostMeshAttribute& attribute : hostMesh.attributes) {
        if (attribute.location < 5u) {
            throw std::runtime_error("pass_mesh: custom host mesh attributes must use locations >= 5.");
        }
        if (!usedAttributeLocations.insert(attribute.location).second) {
            throw std::runtime_error("pass_mesh: duplicate custom host mesh attribute location.");
        }
        if (attribute.components < 1u || attribute.components > 4u) {
            throw std::runtime_error("pass_mesh: custom host mesh attribute components must be in range 1..4.");
        }
        const size_t bytesPerComponent = byte_size_for_gl_type(attribute.glType);
        if (bytesPerComponent == 0u) {
            throw std::runtime_error("pass_mesh: custom host mesh attribute uses an unsupported OpenGL type.");
        }

        const size_t expectedByteCount = vertexCount * static_cast<size_t>(attribute.components) * bytesPerComponent;
        if (attribute.bytes.size() != expectedByteCount) {
            throw std::runtime_error("pass_mesh: custom host mesh attribute byte size does not match vertex count.");
        }
    }

    for (const uint32_t index : hostMesh.indices) {
        if (index >= vertexCount) {
            throw std::runtime_error("pass_mesh: host mesh index exceeds vertex count.");
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

    if (definition.sourceKind == GraphMeshSourceKind::hostMesh) {
        if (!definition.hostMesh) {
            throw std::runtime_error("pass_mesh: host mesh payload is missing.");
        }
        if (!definition.path.empty()) {
            throw std::runtime_error("pass_mesh: host mesh must not provide a file path.");
        }
        validate_host_mesh_data(*definition.hostMesh);
        MeshInput meshInput;
        meshInput.mesh.isQuad = false;
        apply_mesh_parameters(meshInput, definition.parameters);
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
        const ShaderResourceInfo* outputResource = find_resource_by_name(pass.shaderInterface.images, definition.name);
        if (!outputResource) {
            throw std::runtime_error("out (" + definition.name + "): program output not found.");
        }
        if (outputResource->isArray && outputResource->arrayLength > 1) {
            throw std::runtime_error("out (" + definition.name + "): image array outputs are not supported yet");
        }
        validate_array_element_access(*outputResource,
                                      definition.usesArrayElement,
                                      definition.arrayElement,
                                      "out (" + definition.name + ")");
    } else {
        const ShaderResourceInfo* outputResource = find_resource_by_name(pass.shaderInterface.outputs, definition.name);
        if (!outputResource) {
            throw std::runtime_error("out (" + definition.name + "): program output not found.");
        }
        validate_array_element_access(*outputResource,
                                      definition.usesArrayElement,
                                      definition.arrayElement,
                                      "out (" + definition.name + ")");
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

    if (definition.sourceKind != GraphInputSourceKind::passOutput && definition.usesReferencedOutputArrayElement) {
        throw std::runtime_error("in (" + definition.name
                                 + "): referenced output array element is only valid for pass-output inputs");
    }

    if (find_resource_by_name(pass.shaderInterface.systemUniforms, definition.name)) {
        throw std::runtime_error("in (" + definition.name + "): system uniform is engine controlled.");
    }

    GLProgramUniform* uniform = pass.program->findUniform(definition.name);
    if (!uniform) {
        throw std::runtime_error("in (" + definition.name + "): program uniform not found");
    }

    if (definition.sourceKind == GraphInputSourceKind::hostTexture
        || definition.sourceKind == GraphInputSourceKind::passOutput
        || definition.sourceKind == GraphInputSourceKind::graphTexture) {
        const ShaderResourceInfo* samplerResource = find_resource_by_name(pass.shaderInterface.samplers, definition.name);
        const ShaderResourceInfo* imageResource   = find_resource_by_name(pass.shaderInterface.images, definition.name);
        if (!samplerResource && !imageResource) {
            throw std::runtime_error("in (" + definition.name + "): program uniform is not a texture/image input");
        }
        if ((samplerResource && samplerResource->isArray && samplerResource->arrayLength > 1)
            || (imageResource && imageResource->isArray && imageResource->arrayLength > 1)) {
            throw std::runtime_error("in (" + definition.name + "): sampler/image arrays are not supported yet");
        }
        if (definition.usesArrayElement) {
            throw std::runtime_error("in (" + definition.name + "): array-addressed texture/image inputs are not supported yet");
        }

        PassInput probeInput;
        probeInput.uniform = uniform;
        apply_texture_attributes(probeInput, definition.attributes);

        if (definition.sourceKind == GraphInputSourceKind::hostTexture) {
            if (!definition.hostTexture) {
                throw std::runtime_error("in (" + definition.name + "): host texture payload is missing");
            }
            validate_host_image_data(*definition.hostTexture, "in (" + definition.name + ")");
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
                const ShaderResourceInfo* referencedOutput =
                    find_resource_by_name(referencedPass.shaderInterface.images, definition.referencedOutputName);
                if (!referencedOutput) {
                    throw std::runtime_error("in (" + definition.name + "): referenced program output "
                                             + definition.referencedOutputName + "::"
                                             + std::to_string(definition.referencedPassIndex) + " not found");
                }
                validate_array_element_access(*referencedOutput,
                                              definition.usesReferencedOutputArrayElement,
                                              definition.referencedOutputArrayElement,
                                              "in (" + definition.name + ")");
            } else {
                const ShaderResourceInfo* referencedOutput =
                    find_resource_by_name(referencedPass.shaderInterface.outputs, definition.referencedOutputName);
                if (!referencedOutput) {
                    throw std::runtime_error("in (" + definition.name + "): referenced program output "
                                             + definition.referencedOutputName + "::"
                                             + std::to_string(definition.referencedPassIndex) + " not found");
                }
                validate_array_element_access(*referencedOutput,
                                              definition.usesReferencedOutputArrayElement,
                                              definition.referencedOutputArrayElement,
                                              "in (" + definition.name + ")");
            }
        } else if (definition.graphTextureName.empty()) {
            throw std::runtime_error("in (" + definition.name + "): graph texture name is empty");
        }

        return;
    }

    const ShaderResourceInfo* numericUniform = find_resource_by_name(pass.shaderInterface.uniforms, definition.name);
    if (!numericUniform) {
        throw std::runtime_error("in (" + definition.name + "): program uniform is not a numeric input");
    }
    validate_array_element_access(*numericUniform,
                                  definition.usesArrayElement,
                                  definition.arrayElement,
                                  "in (" + definition.name + ")");

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
    const ShaderResourceInfo* counterResource = find_resource_by_name(pass.shaderInterface.atomicCounters, definition.name);
    if (!counterResource) {
        throw std::runtime_error("atomic (cntr): referenced counter " + definition.name + " not found.");
    }
    validate_array_element_access(*counterResource,
                                  definition.usesArrayElement,
                                  definition.arrayElement,
                                  "atomic (cntr)");
}

static const GraphInputDefinition*
find_graph_input_definition(const RawGLGraphState::ValidatedPass& pass,
                            const std::string& name,
                            const bool usesArrayElement,
                            const size_t arrayElement)
{
    for (const GraphInputDefinition& inputDefinition : pass.definition.inputs) {
        if (inputDefinition.name == name && inputDefinition.usesArrayElement == usesArrayElement
            && (!usesArrayElement || inputDefinition.arrayElement == arrayElement)) {
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
        const std::vector<ShaderModuleDefinition> shaderModules = passDefinition.shaderModules.empty()
                                                                      ? build_file_backed_shader_modules(
                                                                            passDefinition.programKind,
                                                                            passDefinition.shaderPaths)
                                                                      : passDefinition.shaderModules;
        validatedPass.definition.shaderModules = shaderModules;
        const RawGLContextState::CachedShaderInterface cached =
            load_cached_shader_interface(contextState, passDefinition.programKind, shaderModules);
        validatedPass.program         = cached.program;
        validatedPass.shaderInterface = cached.shaderInterface;

        if (!validatedPass.shaderInterface.success) {
            throw std::runtime_error(validatedPass.shaderInterface.errorMessage.empty()
                                         ? "Failed to build shader interface."
                                         : validatedPass.shaderInterface.errorMessage);
        }

        SequencePass::CullMode cullMode;
        apply_cull_parameters(cullMode, passDefinition.cullParameters);
        std::unordered_set<std::string> declaredInputs;
        std::unordered_set<std::string> declaredOutputs;
        std::unordered_set<std::string> declaredCounters;
        std::unordered_set<std::string> declaredMeshes;

        for (const GraphMeshDefinition& meshDefinition : passDefinition.meshes) {
            validate_mesh_definition(meshDefinition);
            if (!meshDefinition.name.empty() && !declaredMeshes.insert(meshDefinition.name).second) {
                throw std::runtime_error("pass_mesh: duplicate mesh name " + meshDefinition.name);
            }
        }

        for (const GraphOutputDefinition& outputDefinition : passDefinition.outputs) {
            validate_output_definition(validatedPass, outputDefinition);
            const std::string addressedOutputName =
                build_addressed_resource_name(outputDefinition.name,
                                              outputDefinition.usesArrayElement,
                                              outputDefinition.arrayElement);
            if (!declaredOutputs.insert(addressedOutputName).second) {
                throw std::runtime_error("out (" + addressedOutputName + "): duplicate output definition");
            }
            if (!outputDefinition.persistentTextureName.empty()
                && !persistentOutputNames.insert(outputDefinition.persistentTextureName).second) {
                throw std::runtime_error("out (" + outputDefinition.name + "): duplicate persistent texture name "
                                         + outputDefinition.persistentTextureName);
            }
        }

        for (const GraphInputDefinition& inputDefinition : passDefinition.inputs) {
            validate_input_definition(validatedGraph, validatedPass, inputDefinition);
            const std::string addressedInputName =
                build_addressed_resource_name(inputDefinition.name,
                                              inputDefinition.usesArrayElement,
                                              inputDefinition.arrayElement);
            if (!declaredInputs.insert(addressedInputName).second) {
                throw std::runtime_error("in (" + addressedInputName + "): duplicate input definition");
            }
        }

        for (const GraphAtomicCounterDefinition& counterDefinition : passDefinition.atomicCounters) {
            validate_atomic_counter_definition(validatedPass, counterDefinition);
            const std::string addressedCounterName =
                build_addressed_resource_name(counterDefinition.name,
                                              counterDefinition.usesArrayElement,
                                              counterDefinition.arrayElement);
            if (!declaredCounters.insert(addressedCounterName).second) {
                throw std::runtime_error("atomic (cntr): duplicate counter definition for " + addressedCounterName);
            }
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
    if (!find_graph_input_definition(pass, inputOverride.name, inputOverride.usesArrayElement, inputOverride.arrayElement)) {
        throw std::runtime_error("input override (" + inputOverride.name + "): graph input is not declared");
    }

    GLProgramUniform* uniform = pass.program->findUniform(inputOverride.name);
    if (!uniform) {
        throw std::runtime_error("input override (" + inputOverride.name + "): program uniform not found");
    }

    switch (inputOverride.sourceKind) {
    case GraphInputSourceKind::hostTexture: {
        const bool isSampler = find_resource_by_name(pass.shaderInterface.samplers, inputOverride.name) != nullptr;
        const bool isImage   = find_resource_by_name(pass.shaderInterface.images, inputOverride.name) != nullptr;
        if (!isSampler && !isImage) {
            throw std::runtime_error(
                "input override (" + inputOverride.name + "): program uniform is not a texture/image input");
        }
        if (inputOverride.usesArrayElement) {
            throw std::runtime_error("input override (" + inputOverride.name
                                     + "): array-addressed texture/image inputs are not supported yet");
        }
        if (inputOverride.sourceKind == GraphInputSourceKind::hostTexture) {
            if (!inputOverride.hostTexture) {
                throw std::runtime_error("input override (" + inputOverride.name + "): host texture payload is missing");
            }
            validate_host_image_data(*inputOverride.hostTexture, "input override (" + inputOverride.name + ")");
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

    const ShaderResourceInfo* numericUniform = find_resource_by_name(pass.shaderInterface.uniforms, inputOverride.name);
    if (numericUniform) {
        validate_array_element_access(*numericUniform,
                                      inputOverride.usesArrayElement,
                                      inputOverride.arrayElement,
                                      "input override (" + inputOverride.name + ")");
    } else if (inputOverride.usesArrayElement) {
        throw std::runtime_error("input override (" + inputOverride.name + "): array element was specified for a non-numeric resource");
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
