// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <rawgl/rawgl_core.h>

namespace rawgl {

namespace batch {
class BatchRunner;
}

/// Artist-facing workflow façade over the current graph-oriented core API.
///
/// `rawgl::Workflow` is the memory-first 2.0 surface. File-backed inputs and
/// outputs belong to `rawgl::io`.

/// Name/value metadata attached to workflow resources.
struct Attribute {
    std::string name;
    std::string value;
};

/// Source variant for one input binding.
enum class InputSourceKind {
    intValues,
    uintValues,
    floatValues,
    doubleValues,
    hostTexture,
    passOutput,
    workflowTexture,
};

/// Shader-visible input binding for one pass.
struct InputBinding {
    std::string name;
    InputSourceKind sourceKind = InputSourceKind::intValues;
    std::vector<int32_t> intValues;
    std::vector<uint32_t> uintValues;
    std::vector<float> floatValues;
    std::vector<double> doubleValues;
    std::string referencedOutputName;
    size_t referencedPassIndex = 0;
    std::string workflowTextureName;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
    bool usesReferencedOutputArrayElement = false;
    size_t referencedOutputArrayElement = 0;
    std::vector<Attribute> attributes;
    std::shared_ptr<HostImageData> hostTexture;
};

/// Atomic counter binding for one pass.
struct CounterBinding {
    std::string name;
    uint32_t initialValue = 0;
    std::string persistentCounterName;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
};

/// Declared output binding for one pass.
struct OutputBinding {
    std::string name;
    std::string format = "rgb32f";
    int channels = 3;
    int alphaChannel = -1;
    int bits = 16;
    std::string persistentTextureName;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
    std::vector<Attribute> attributes;
    bool captureToHost = false;
};

/// Mesh source selector for a render pass.
enum class MeshSourceKind {
    quad,
    file,
    hostMesh,
};

/// Mesh binding for a render pass.
struct MeshBinding {
    std::string name;
    MeshSourceKind sourceKind = MeshSourceKind::quad;
    std::string path;
    std::shared_ptr<HostMeshData> hostMesh;
    std::vector<Attribute> parameters;
};

/// One workflow pass.
struct Pass {
    ShaderProgramKind programKind = ShaderProgramKind::vertfrag;
    /// Shader modules for this pass.
    /// For `vertfrag`, supplying only one fragment module uses the built-in
    /// fullscreen quad vertex shader automatically.
    std::vector<ShaderModuleDefinition> shaderModules;
    int sizeX = 512;
    int sizeY = 512;
    int workGroupSizeX = 16;
    int workGroupSizeY = 16;
    bool hasExplicitWorkGroupSize = false;
    std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<InputBinding> inputs;
    std::vector<CounterBinding> counters;
    std::vector<OutputBinding> outputs;
    std::vector<MeshBinding> meshes;
    std::vector<Attribute> cullParameters;
};

/// User-facing workflow definition for one single-pass or multi-pass job.
struct Workflow {
    /// Verbosity forwarded to the core compiler/executor.
    int verbosity = 3;
    /// Ordered pass list.
    std::vector<Pass> passes;
};

/// Per-run override for one named input binding.
struct InputOverride {
    size_t passIndex = 0;
    std::string name;
    InputSourceKind sourceKind = InputSourceKind::intValues;
    std::vector<int32_t> intValues;
    std::vector<uint32_t> uintValues;
    std::vector<float> floatValues;
    std::vector<double> doubleValues;
    std::vector<Attribute> attributes;
    bool usesArrayElement = false;
    size_t arrayElement = 0;
    std::shared_ptr<HostImageData> hostTexture;
};

/// Fixed-topology mesh update for prepared workflow execution.
struct MeshUpdate {
    /// When false, apply to every pass containing \ref name.
    bool usesPassIndex = false;
    /// Target pass index when \ref usesPassIndex is true.
    size_t passIndex = 0;
    /// Target mesh binding name.
    std::string name;
    /// Optional tightly packed `float32[N, 3]` replacement positions.
    std::vector<float> positions;
    /// Optional tightly packed `float32[N, 3]` replacement normals.
    std::vector<float> normals;
};

/// Full host mesh replacement for prepared workflow execution.
struct MeshOverride {
    /// When false, apply to every pass containing \ref name.
    bool usesPassIndex = false;
    /// Target pass index when \ref usesPassIndex is true.
    size_t passIndex = 0;
    /// Target mesh binding name.
    std::string name;
    /// Replacement host mesh.
    std::shared_ptr<HostMeshData> hostMesh;
};

/// Builds one host-memory input binding for the in-memory core path.
inline InputBinding
HostTextureInput(std::string name, std::shared_ptr<HostImageData> hostTexture, std::vector<Attribute> attributes = {})
{
    InputBinding result;
    result.name = std::move(name);
    result.sourceKind = InputSourceKind::hostTexture;
    result.attributes = std::move(attributes);
    result.hostTexture = std::move(hostTexture);
    return result;
}

/// Builds one host-memory per-run input override for the in-memory core path.
inline InputOverride
HostTextureOverride(size_t passIndex,
                    std::string name,
                    std::shared_ptr<HostImageData> hostTexture,
                    std::vector<Attribute> attributes = {})
{
    InputOverride result;
    result.passIndex = passIndex;
    result.name = std::move(name);
    result.sourceKind = InputSourceKind::hostTexture;
    result.attributes = std::move(attributes);
    result.hostTexture = std::move(hostTexture);
    return result;
}

/// Builds one captured host-memory output binding for the in-memory core path.
inline OutputBinding
CapturedOutput(std::string name,
               std::string format = "rgb32f",
               int channels = 3,
               int alphaChannel = -1,
               int bits = 16,
               std::vector<Attribute> attributes = {})
{
    OutputBinding result;
    result.name = std::move(name);
    result.format = std::move(format);
    result.channels = channels;
    result.alphaChannel = alphaChannel;
    result.bits = bits;
    result.attributes = std::move(attributes);
    result.captureToHost = true;
    return result;
}

/// Session cache and reuse statistics.
struct SessionStats {
    size_t shaderInterfaces = 0;
    size_t textures = 0;
    size_t meshesHost = 0;
    size_t meshesGpu = 0;
};

/// Per-run execution settings for a prepared workflow.
struct RunSettings {
    /// Current system uniform values.
    SystemUniformState systemUniforms;
    /// Per-run input overrides.
    std::vector<InputOverride> overrides;
    /// Per-run fixed-topology mesh updates.
    std::vector<MeshUpdate> meshUpdates;
    /// Per-run full host mesh replacements.
    std::vector<MeshOverride> meshOverrides;
};

/// Result of executing a prepared workflow once.
struct RunResult {
    /// False when execution failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Host-captured outputs keyed by strings such as `name::0` or `name[index]::0`.
    std::map<std::string, HostImageData> capturedOutputs;
    /// Captured counter values keyed by strings such as `name::0` or `name[index]::0`.
    std::map<std::string, std::vector<uint32_t>> capturedCounters;
};

class PreparedWorkflow;

/// Result of preparing a workflow for repeated execution.
struct PrepareResult {
    /// False when workflow preparation failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Prepared workflow when \ref success is true.
    std::unique_ptr<PreparedWorkflow> workflow;
};

namespace detail {

static inline ShaderModuleDefinition
make_builtin_fullscreen_vertex_module()
{
    ShaderModuleDefinition result;
    result.role = ShaderModuleRole::vertex;
    result.sourceKind = ShaderModuleSourceKind::glslText;
    result.glslText = R"(#version 450 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv_co;
layout(location = 0) out vec2 UV;
void main()
{
    UV = uv_co;
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
)";
    result.debugLabel = "rawgl_builtin_fullscreen_vertex";
    return result;
}

static inline GraphAttribute
to_graph(const Attribute& attribute)
{
    GraphAttribute result;
    result.name  = attribute.name;
    result.value = attribute.value;
    return result;
}

static inline GraphInputSourceKind
to_graph(const InputSourceKind sourceKind)
{
    switch (sourceKind) {
    case InputSourceKind::intValues: return GraphInputSourceKind::intValues;
    case InputSourceKind::uintValues: return GraphInputSourceKind::uintValues;
    case InputSourceKind::floatValues: return GraphInputSourceKind::floatValues;
    case InputSourceKind::doubleValues: return GraphInputSourceKind::doubleValues;
    case InputSourceKind::hostTexture: return GraphInputSourceKind::hostTexture;
    case InputSourceKind::passOutput: return GraphInputSourceKind::passOutput;
    case InputSourceKind::workflowTexture: return GraphInputSourceKind::graphTexture;
    }

    return GraphInputSourceKind::intValues;
}

static inline GraphMeshSourceKind
to_graph(const MeshSourceKind sourceKind)
{
    switch (sourceKind) {
    case MeshSourceKind::quad: return GraphMeshSourceKind::quad;
    case MeshSourceKind::file: return GraphMeshSourceKind::file;
    case MeshSourceKind::hostMesh: return GraphMeshSourceKind::hostMesh;
    }

    return GraphMeshSourceKind::quad;
}

static inline std::vector<GraphAttribute>
to_graph(const std::vector<Attribute>& attributes)
{
    std::vector<GraphAttribute> result;
    result.reserve(attributes.size());
    for (const Attribute& attribute : attributes) {
        result.push_back(to_graph(attribute));
    }
    return result;
}

static inline std::shared_ptr<HostImageData>
clone_host_texture_payload(const std::shared_ptr<HostImageData>& hostTexture)
{
    if (!hostTexture) {
        return nullptr;
    }
    return std::make_shared<HostImageData>(*hostTexture);
}

static inline GraphInputDefinition
to_graph(const InputBinding& input)
{
    GraphInputDefinition result;
    result.name = input.name;
    result.sourceKind = to_graph(input.sourceKind);
    result.intValues = input.intValues;
    result.uintValues = input.uintValues;
    result.floatValues = input.floatValues;
    result.doubleValues = input.doubleValues;
    result.referencedOutputName = input.referencedOutputName;
    result.referencedPassIndex = input.referencedPassIndex;
    result.graphTextureName = input.workflowTextureName;
    result.usesArrayElement = input.usesArrayElement;
    result.arrayElement = input.arrayElement;
    result.usesReferencedOutputArrayElement = input.usesReferencedOutputArrayElement;
    result.referencedOutputArrayElement = input.referencedOutputArrayElement;
    result.attributes = to_graph(input.attributes);
    result.hostTexture = clone_host_texture_payload(input.hostTexture);
    return result;
}

static inline GraphAtomicCounterDefinition
to_graph(const CounterBinding& counter)
{
    GraphAtomicCounterDefinition result;
    result.name = counter.name;
    result.initialValue = counter.initialValue;
    result.persistentCounterName = counter.persistentCounterName;
    result.usesArrayElement = counter.usesArrayElement;
    result.arrayElement = counter.arrayElement;
    return result;
}

static inline GraphOutputDefinition
to_graph(const OutputBinding& output)
{
    GraphOutputDefinition result;
    result.name = output.name;
    result.format = output.format;
    result.channels = output.channels;
    result.alphaChannel = output.alphaChannel;
    result.bits = output.bits;
    result.persistentTextureName = output.persistentTextureName;
    result.usesArrayElement = output.usesArrayElement;
    result.arrayElement = output.arrayElement;
    result.attributes = to_graph(output.attributes);
    result.captureToHost = output.captureToHost;
    return result;
}

static inline GraphMeshDefinition
to_graph(const MeshBinding& mesh)
{
    GraphMeshDefinition result;
    result.name = mesh.name;
    result.sourceKind = to_graph(mesh.sourceKind);
    result.path = mesh.path;
    result.hostMesh = mesh.hostMesh;
    result.parameters = to_graph(mesh.parameters);
    return result;
}

static inline bool
append_graph_shader_modules(const Pass& pass, GraphPassDefinition& result, std::string& errorMessage)
{
    if (pass.shaderModules.empty()) {
        errorMessage = "workflow pass requires at least one shader module";
        return false;
    }

    if (pass.programKind == ShaderProgramKind::compute && pass.shaderModules.size() != 1u) {
        errorMessage = "compute workflow pass requires exactly one shader module";
        return false;
    }
    const bool usesBuiltinVertex = pass.programKind == ShaderProgramKind::vertfrag && pass.shaderModules.size() == 1u
        && pass.shaderModules[0].role == ShaderModuleRole::fragment;
    if (pass.programKind == ShaderProgramKind::vertfrag
        && (pass.shaderModules.empty() || pass.shaderModules.size() > 2u)) {
        errorMessage = "vertfrag workflow pass requires one combined module or two stage modules";
        return false;
    }

    result.shaderModules.clear();
    result.shaderModules.reserve(pass.shaderModules.size() + (usesBuiltinVertex ? 1u : 0u));
    result.shaderPaths.clear();
    result.shaderPaths.reserve(pass.shaderModules.size());

    if (usesBuiltinVertex) {
        const ShaderModuleDefinition& module = pass.shaderModules[0];
        if (module.sourceKind == ShaderModuleSourceKind::filePath && module.path.empty()) {
            errorMessage = "shader module file path must not be empty";
            return false;
        }

        result.shaderModules.push_back(make_builtin_fullscreen_vertex_module());
        result.shaderModules.push_back(module);
        if (module.sourceKind == ShaderModuleSourceKind::filePath) {
            result.shaderPaths.push_back(module.path);
        }
        return true;
    }

    result.shaderModules = pass.shaderModules;
    for (size_t moduleIndex = 0; moduleIndex < pass.shaderModules.size(); ++moduleIndex) {
        const ShaderModuleDefinition& module = pass.shaderModules[moduleIndex];
        if (module.sourceKind == ShaderModuleSourceKind::filePath) {
            if (module.path.empty()) {
                errorMessage = "shader module file path must not be empty";
                return false;
            }
        }

        if (pass.programKind == ShaderProgramKind::compute) {
            if (module.role != ShaderModuleRole::automatic && module.role != ShaderModuleRole::compute) {
                errorMessage = "compute workflow pass cannot use vertex or fragment shader module roles";
                return false;
            }
        } else {
            if (module.role == ShaderModuleRole::compute) {
                errorMessage = "vertfrag workflow pass cannot use compute shader module roles";
                return false;
            }
            if (pass.shaderModules.size() == 1u && module.role != ShaderModuleRole::automatic) {
                errorMessage = "single-module vertfrag workflow pass must use the automatic shader module role";
                return false;
            }
            if (pass.shaderModules.size() == 2u) {
                if (moduleIndex == 0u && module.role != ShaderModuleRole::vertex) {
                    errorMessage = "first module of a two-stage vertfrag workflow pass must be a vertex shader";
                    return false;
                }
                if (moduleIndex == 1u && module.role != ShaderModuleRole::fragment) {
                    errorMessage = "second module of a two-stage vertfrag workflow pass must be a fragment shader";
                    return false;
                }
            }
        }
        if (module.sourceKind == ShaderModuleSourceKind::filePath) {
            result.shaderPaths.push_back(module.path);
        }
    }

    return true;
}

static inline bool
to_graph(const Pass& pass, GraphPassDefinition& result, std::string& errorMessage)
{
    result.programKind = pass.programKind;
    if (!append_graph_shader_modules(pass, result, errorMessage)) {
        return false;
    }
    result.sizeX = pass.sizeX;
    result.sizeY = pass.sizeY;
    result.workGroupSizeX = pass.workGroupSizeX;
    result.workGroupSizeY = pass.workGroupSizeY;
    result.hasExplicitWorkGroupSize = pass.hasExplicitWorkGroupSize;
    for (size_t i = 0; i < pass.clearColor.size(); ++i) {
        result.clearColor[i] = pass.clearColor[i];
    }
    result.inputs.reserve(pass.inputs.size());
    for (const InputBinding& input : pass.inputs) {
        result.inputs.push_back(to_graph(input));
    }
    result.atomicCounters.reserve(pass.counters.size());
    for (const CounterBinding& counter : pass.counters) {
        result.atomicCounters.push_back(to_graph(counter));
    }
    result.outputs.reserve(pass.outputs.size());
    for (const OutputBinding& output : pass.outputs) {
        result.outputs.push_back(to_graph(output));
    }
    result.meshes.reserve(pass.meshes.size());
    for (const MeshBinding& mesh : pass.meshes) {
        result.meshes.push_back(to_graph(mesh));
    }
    result.cullParameters = to_graph(pass.cullParameters);
    return true;
}

static inline bool
to_graph(const Workflow& workflow, GraphDefinition& result, std::string& errorMessage)
{
    result.verbosity = workflow.verbosity;
    result.passes.clear();
    result.passes.reserve(workflow.passes.size());
    for (const Pass& pass : workflow.passes) {
        GraphPassDefinition graphPass;
        if (!to_graph(pass, graphPass, errorMessage)) {
            return false;
        }
        result.passes.push_back(std::move(graphPass));
    }
    return true;
}

static inline GraphInputOverride
to_graph(const InputOverride& inputOverride)
{
    GraphInputOverride result;
    result.passIndex = inputOverride.passIndex;
    result.name = inputOverride.name;
    result.sourceKind = to_graph(inputOverride.sourceKind);
    result.intValues = inputOverride.intValues;
    result.uintValues = inputOverride.uintValues;
    result.floatValues = inputOverride.floatValues;
    result.doubleValues = inputOverride.doubleValues;
    result.attributes = to_graph(inputOverride.attributes);
    result.usesArrayElement = inputOverride.usesArrayElement;
    result.arrayElement = inputOverride.arrayElement;
    result.hostTexture = clone_host_texture_payload(inputOverride.hostTexture);
    return result;
}

static inline GraphMeshUpdate
to_graph(const MeshUpdate& meshUpdate)
{
    GraphMeshUpdate result;
    result.usesPassIndex = meshUpdate.usesPassIndex;
    result.passIndex = meshUpdate.passIndex;
    result.name = meshUpdate.name;
    result.positions = meshUpdate.positions;
    result.normals = meshUpdate.normals;
    return result;
}

static inline GraphMeshOverride
to_graph(const MeshOverride& meshOverride)
{
    GraphMeshOverride result;
    result.usesPassIndex = meshOverride.usesPassIndex;
    result.passIndex = meshOverride.passIndex;
    result.name = meshOverride.name;
    result.hostMesh = meshOverride.hostMesh;
    return result;
}

static inline GraphExecutionRequest
to_graph(const RunSettings& settings)
{
    GraphExecutionRequest result;
    result.systemUniforms = settings.systemUniforms;
    result.inputOverrides.reserve(settings.overrides.size());
    for (const InputOverride& overrideValue : settings.overrides) {
        result.inputOverrides.push_back(to_graph(overrideValue));
    }
    result.meshUpdates.reserve(settings.meshUpdates.size());
    for (const MeshUpdate& meshUpdate : settings.meshUpdates) {
        result.meshUpdates.push_back(to_graph(meshUpdate));
    }
    result.meshOverrides.reserve(settings.meshOverrides.size());
    for (const MeshOverride& meshOverride : settings.meshOverrides) {
        result.meshOverrides.push_back(to_graph(meshOverride));
    }
    return result;
}

static inline SessionStats
from_graph(const ContextCacheStats& stats)
{
    SessionStats result;
    result.shaderInterfaces = stats.shaderInterfaces;
    result.textures = stats.textures;
    result.meshesHost = stats.meshesHost;
    result.meshesGpu = stats.meshesGpu;
    return result;
}

static inline RunResult
from_graph(GraphExecutionResult&& coreResult)
{
    RunResult result;
    result.success = coreResult.success;
    result.errorMessage = std::move(coreResult.errorMessage);
    result.capturedOutputs = std::move(coreResult.capturedOutputs);
    result.capturedCounters = std::move(coreResult.capturedAtomicCounters);
    return result;
}

}  // namespace detail

/// Validated reusable executable workflow.
class PreparedWorkflow {
public:
    explicit PreparedWorkflow(std::unique_ptr<RawGLGraph> graph)
        : m_graph(std::move(graph))
    {
    }

    PreparedWorkflow(const PreparedWorkflow&) = delete;
    PreparedWorkflow& operator=(const PreparedWorkflow&) = delete;
    PreparedWorkflow(PreparedWorkflow&&) = default;
    PreparedWorkflow& operator=(PreparedWorkflow&&) = default;

    /// Executes the workflow once with the supplied run settings.
    RunResult
    run(const RunSettings& settings)
    {
        if (!m_graph) {
            RunResult result;
            result.success = false;
            result.errorMessage = "prepared workflow is empty";
            return result;
        }

        GraphExecutionRequest request = detail::to_graph(settings);
        return detail::from_graph(m_graph->execute(request));
    }

private:
    std::unique_ptr<RawGLGraph> m_graph;
};

/// Long-lived owner for reusable backend state, caches, and workflow preparation.
class Session {
public:
    Session() = default;
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    /// Reflects the interface of a shader program without executing it.
    ShaderInterface
    inspectShaderInterface(const ShaderInspectionRequest& request) const
    {
        return m_context.inspectShaderInterface(request);
    }

    /// Validates and prepares a workflow for repeated execution.
    PrepareResult
    prepare(const Workflow& workflow) const
    {
        GraphBuildRequest request;
        std::string conversionError;
        if (!detail::to_graph(workflow, request.definition, conversionError)) {
            PrepareResult result;
            result.success = false;
            result.errorMessage = conversionError.empty() ? "workflow conversion failed" : conversionError;
            return result;
        }

        GraphBuildResult coreResult = m_context.buildGraph(request);

        PrepareResult result;
        result.success = coreResult.success;
        result.errorMessage = std::move(coreResult.errorMessage);
        if (coreResult.graph) {
            result.workflow = std::make_unique<PreparedWorkflow>(std::move(coreResult.graph));
        }
        return result;
    }

    /// Convenience one-shot entry point for simple workflows.
    RunResult
    run(const Workflow& workflow, const RunSettings& settings = {}) const
    {
        PrepareResult prepared = prepare(workflow);
        if (!prepared.success || !prepared.workflow) {
            RunResult result;
            result.success = false;
            result.errorMessage = prepared.errorMessage.empty() ? "workflow preparation failed" : prepared.errorMessage;
            return result;
        }
        return prepared.workflow->run(settings);
    }

    /// Returns current session cache statistics.
    SessionStats
    stats() const
    {
        return detail::from_graph(m_context.cacheStats());
    }

private:
    friend class batch::BatchRunner;

    void makeExecutionContextCurrent() const { m_context.makeContextCurrent(); }
    void releaseExecutionContext() const { m_context.releaseContext(); }

    RawGLContext m_context;
};

}  // namespace rawgl
