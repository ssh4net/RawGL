// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace rawgl {

/// Transitional graph-oriented public API.
///
/// New frontend-facing code should prefer `rawgl/rawgl.h`, which provides the
/// workflow-oriented façade names introduced during the API redesign.
/// Command-line entry points live in `rawgl/rawgl_cli.h`.

/// Selects the shader program topology for inspection and graph passes.
enum class ShaderProgramKind {
    vertfrag,
    compute,
};

/// Backing source type for one shader module.
enum class ShaderModuleSourceKind {
    filePath,
    glslText,
    spirvBinary,
};

/// Role of one shader module within a program.
enum class ShaderModuleRole {
    automatic,
    vertex,
    fragment,
    compute,
};

/// One shader module supplied to inspection or graph construction.
struct ShaderModuleDefinition {
    /// Role of this module within the containing program.
    ShaderModuleRole role = ShaderModuleRole::automatic;
    /// Backing source type.
    ShaderModuleSourceKind sourceKind = ShaderModuleSourceKind::filePath;
    /// Backing path when \ref sourceKind is \ref ShaderModuleSourceKind::filePath.
    std::string path;
    /// UTF-8 GLSL source text when \ref sourceKind is \ref ShaderModuleSourceKind::glslText.
    std::string glslText;
    /// Owned SPIR-V bytes when \ref sourceKind is \ref ShaderModuleSourceKind::spirvBinary.
    std::vector<std::byte> spirvBytes;
    /// Optional debug label for diagnostics and cache identity.
    std::string debugLabel;
};

/// Describes a shader interface inspection request.
struct ShaderInspectionRequest {
    /// Program topology to inspect.
    ShaderProgramKind kind = ShaderProgramKind::vertfrag;
    /// Shader paths.
    /// For `vertfrag`, pass either one combined source or `{vertex, fragment}`.
    /// For `compute`, pass exactly one compute shader path.
    std::vector<std::string> paths;
    /// Structured shader modules. When non-empty, these take precedence over \ref paths.
    std::vector<ShaderModuleDefinition> modules;
};

/// High-level reflected shader resource category.
enum class ShaderResourceClass {
    unknown,
    uniform_numeric,
    sampler,
    image,
    output,
    atomic_counter,
    system_uniform,
    buffer_variable,
};

/// Reflected texture/image dimensionality.
enum class ShaderTextureShape {
    unknown,
    tex_2d,
};

/// Reflected information about a shader-visible resource.
struct ShaderResourceInfo {
    /// GLSL resource name, normalized to the base name for arrays.
    std::string name;
    /// Human-readable GLSL type name.
    std::string typeName;
    /// High-level resource category.
    ShaderResourceClass resourceClass = ShaderResourceClass::unknown;
    /// Texture/image dimensionality when applicable.
    ShaderTextureShape textureShape   = ShaderTextureShape::unknown;
    /// True when the reflected resource is an array.
    bool isArray = false;
    /// Array length when \ref isArray is true.
    std::size_t arrayLength = 1;
    /// Vector width for numeric resources, otherwise 1.
    int vectorWidth = 1;
    /// Matrix column count for numeric matrix resources, otherwise 1.
    int matrixColumns = 1;
    /// Matrix row count for numeric matrix resources, otherwise 1.
    int matrixRows = 1;
    /// Uniform/output location when meaningful, otherwise -1.
    int location = -1;
    /// Binding point for bindable resources when meaningful, otherwise -1.
    int binding  = -1;
    /// Byte offset inside a block when meaningful.
    int offset   = 0;
    /// Reflected element count or byte size, depending on resource kind.
    int size     = 0;
    /// Raw OpenGL enum for the reflected GLSL type.
    unsigned int glType = 0;
};

/// Reflected information about a named variable inside a shader buffer block.
struct ShaderBufferVariableInfo {
    /// Containing block name.
    std::string blockName;
    /// Variable name inside the block.
    std::string name;
    /// Human-readable GLSL type name.
    std::string typeName;
    /// Variable location when meaningful, otherwise -1.
    int location = -1;
    /// Block binding when meaningful, otherwise -1.
    int binding  = -1;
    /// Byte offset inside the block.
    int offset   = 0;
    /// Reflected byte size.
    int size     = 0;
    /// Raw OpenGL enum for the reflected GLSL type.
    unsigned int glType = 0;
};

/// Aggregate reflected interface for a shader program.
struct ShaderInterface {
    /// False when inspection failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// True when the inspected program is compute-only.
    bool isCompute = false;
    /// Numeric user uniforms.
    std::vector<ShaderResourceInfo> uniforms;
    /// Sampler uniforms.
    std::vector<ShaderResourceInfo> samplers;
    /// Image uniforms.
    std::vector<ShaderResourceInfo> images;
    /// Fragment or image outputs.
    std::vector<ShaderResourceInfo> outputs;
    /// Atomic counter uniforms.
    std::vector<ShaderResourceInfo> atomicCounters;
    /// Built-in RawGL system uniforms such as `iTime`.
    std::vector<ShaderResourceInfo> systemUniforms;
    /// Reflected named variables inside shader storage blocks.
    std::vector<ShaderBufferVariableInfo> bufferVariables;
};

using ShaderInspectionResult = ShaderInterface;

/// Per-execution values for RawGL system uniforms.
struct SystemUniformState {
    double timeSeconds      = 0.0;
    double deltaTimeSeconds = 0.0;
    int frameNumber         = 0;
    int passIndex           = -1;
};

/// Name/value attribute attached to graph resources and outputs.
struct GraphAttribute {
    std::string name;
    std::string value;
};

/// Owned host-memory texture payload used for in-memory graph inputs and captures.
struct HostImageData {
    /// Texture width in pixels.
    int width = 0;
    /// Texture height in pixels.
    int height = 0;
    /// Logical channel count in the supplied byte buffer.
    int channels = 0;
    /// Alpha channel index, or -1 when alpha is absent.
    int alphaChannel = -1;
    /// OpenGL internal texture format, for example `GL_RGBA32F`.
    unsigned int glInternalFormat = 0;
    /// OpenGL upload/readback element type, for example `GL_FLOAT`.
    unsigned int glType = 0;
    /// Owned tightly packed pixel bytes in row-major order.
    std::vector<std::byte> bytes;
};

/// Source variant for a graph input.
enum class GraphInputSourceKind {
    intValues,
    uintValues,
    floatValues,
    doubleValues,
    textureFile,
    hostTexture,
    passOutput,
    graphTexture,
};

/// Declares one named graph input.
/// The active payload fields depend on \ref sourceKind.
struct GraphInputDefinition {
    /// Shader-visible input name.
    std::string name;
    /// Selects which payload fields are consumed.
    GraphInputSourceKind sourceKind = GraphInputSourceKind::intValues;
    /// Integer payload for \ref GraphInputSourceKind::intValues.
    std::vector<int32_t> intValues;
    /// Unsigned integer payload for \ref GraphInputSourceKind::uintValues.
    std::vector<uint32_t> uintValues;
    /// Float payload for \ref GraphInputSourceKind::floatValues.
    std::vector<float> floatValues;
    /// Double payload for \ref GraphInputSourceKind::doubleValues.
    std::vector<double> doubleValues;
    /// Texture path for \ref GraphInputSourceKind::textureFile.
    std::string texturePath;
    /// Referenced output name for \ref GraphInputSourceKind::passOutput.
    std::string referencedOutputName;
    /// Referenced pass index for \ref GraphInputSourceKind::passOutput.
    size_t referencedPassIndex = 0;
    /// Graph texture name for \ref GraphInputSourceKind::graphTexture.
    std::string graphTextureName;
    /// Selects one element of an array-valued target input.
    bool usesArrayElement = false;
    /// Zero-based array element selected when \ref usesArrayElement is true.
    size_t arrayElement = 0;
    /// Selects one element of an array-valued referenced pass output.
    bool usesReferencedOutputArrayElement = false;
    /// Zero-based referenced output element selected when \ref usesReferencedOutputArrayElement is true.
    size_t referencedOutputArrayElement = 0;
    /// Loader or interpretation attributes.
    std::vector<GraphAttribute> attributes;
    /// Host-memory texture payload for \ref GraphInputSourceKind::hostTexture.
    std::shared_ptr<HostImageData> hostTexture;
};

/// Declares one atomic counter input for a pass.
struct GraphAtomicCounterDefinition {
    std::string name;
    uint32_t initialValue = 0;
    std::string persistentCounterName;
    /// Selects one element of an array-valued atomic counter.
    bool usesArrayElement = false;
    /// Zero-based array element selected when \ref usesArrayElement is true.
    size_t arrayElement = 0;
};

/// Declares one named pass output.
struct GraphOutputDefinition {
    /// Shader-visible output name.
    std::string name;
    /// Optional save path. Empty means transient-only unless persisted by name.
    std::string path;
    /// RawGL output format token, for example `rgb32f`.
    std::string format = "rgb32f";
    /// Number of color channels to save.
    int channels       = 3;
    /// Alpha channel index, or -1 when alpha is absent.
    int alphaChannel   = -1;
    /// Output bit depth used by file encoders when relevant.
    int bits           = 16;
    /// Optional graph-persistent texture name for reuse across executions.
    std::string persistentTextureName;
    /// Selects one element of an array-valued shader output.
    bool usesArrayElement = false;
    /// Zero-based array element selected when \ref usesArrayElement is true.
    size_t arrayElement = 0;
    /// Output writer attributes.
    std::vector<GraphAttribute> attributes;
    /// Requests host-memory capture in \ref GraphExecutionResult::capturedOutputs.
    bool captureToHost = false;
};

/// Mesh source variant for a draw pass.
enum class GraphMeshSourceKind {
    quad,
    file,
};

/// Declares one mesh input for a pass.
struct GraphMeshDefinition {
    GraphMeshSourceKind sourceKind = GraphMeshSourceKind::quad;
    std::string path;
    std::vector<GraphAttribute> parameters;
};

/// Declares one shader pass in a graph.
struct GraphPassDefinition {
    ShaderProgramKind programKind = ShaderProgramKind::vertfrag;
    std::vector<ShaderModuleDefinition> shaderModules;
    std::vector<std::string> shaderPaths;
    int sizeX                     = 512;
    int sizeY                     = 512;
    int workGroupSizeX            = 16;
    int workGroupSizeY            = 16;
    bool hasExplicitWorkGroupSize = false;
    float clearColor[4]           = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<GraphInputDefinition> inputs;
    std::vector<GraphAtomicCounterDefinition> atomicCounters;
    std::vector<GraphOutputDefinition> outputs;
    std::vector<GraphMeshDefinition> meshes;
    std::vector<GraphAttribute> cullParameters;
};

/// Full graph description used to build an executable RawGL graph.
struct GraphDefinition {
    int verbosity = 3;
    std::vector<GraphPassDefinition> passes;
};

/// Request for graph compilation/validation.
struct GraphBuildRequest {
    GraphDefinition definition;
};

/// Per-execution override for one named graph input.
struct GraphInputOverride {
    size_t passIndex = 0;
    std::string name;
    GraphInputSourceKind sourceKind = GraphInputSourceKind::intValues;
    std::vector<int32_t> intValues;
    std::vector<uint32_t> uintValues;
    std::vector<float> floatValues;
    std::vector<double> doubleValues;
    std::string texturePath;
    std::vector<GraphAttribute> attributes;
    /// Selects one element of an array-valued target input.
    bool usesArrayElement = false;
    /// Zero-based array element selected when \ref usesArrayElement is true.
    size_t arrayElement = 0;
    /// Host-memory texture payload for \ref GraphInputSourceKind::hostTexture.
    std::shared_ptr<HostImageData> hostTexture;
};

/// Request for executing a previously built graph.
struct GraphExecutionRequest {
    SystemUniformState systemUniforms;
    std::vector<GraphInputOverride> inputOverrides;
};

/// Result of executing a built graph.
struct GraphExecutionResult {
    /// False when execution failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Host-captured outputs keyed by `name::passIndex` or `name[index]::passIndex`.
    std::map<std::string, HostImageData> capturedOutputs;
    /// Captured atomic counter values keyed by `name::passIndex` or `name[index]::passIndex`.
    std::map<std::string, std::vector<uint32_t>> capturedAtomicCounters;
};

/// Snapshot of cache usage for a \ref RawGLContext instance.
struct ContextCacheStats {
    size_t shaderInterfaces = 0;
    size_t textures         = 0;
    size_t meshesHost       = 0;
    size_t meshesGpu        = 0;
};

class RawGLGraph;
struct RawGLGraphState;

/// Result of \ref RawGLContext::buildGraph.
struct GraphBuildResult {
    bool success = false;
    std::string errorMessage;
    std::unique_ptr<RawGLGraph> graph;
};

struct RawGLContextState;

/// Long-lived owner for cached shader interfaces and reusable graph resources.
class RawGLContext {
public:
    RawGLContext();
    ~RawGLContext();

    RawGLContext(const RawGLContext&) = delete;
    RawGLContext& operator=(const RawGLContext&) = delete;

    /// Reflects the interface of a shader program without executing it.
    ShaderInterface inspectShaderInterface(const ShaderInspectionRequest& request) const;
    /// Validates and compiles a graph into an executable object.
    GraphBuildResult buildGraph(const GraphBuildRequest& request) const;
    /// Returns current cache statistics for this context.
    ContextCacheStats cacheStats() const;

private:
    std::shared_ptr<RawGLContextState> m_state;
};

/// Executable graph built by \ref RawGLContext::buildGraph.
class RawGLGraph {
public:
    RawGLGraph(std::shared_ptr<RawGLContextState> contextState, std::unique_ptr<RawGLGraphState> state);
    ~RawGLGraph();

    RawGLGraph(const RawGLGraph&) = delete;
    RawGLGraph& operator=(const RawGLGraph&) = delete;

    /// Executes the graph once with the supplied system state and input overrides.
    GraphExecutionResult execute(const GraphExecutionRequest& request);

private:
    std::shared_ptr<RawGLContextState> m_contextState;
    std::unique_ptr<RawGLGraphState> m_state;
};

/// Convenience wrapper that builds and runs one graph-oriented session.
class CoreSession {
public:
    CoreSession(const GraphBuildRequest& request);
    ~CoreSession();

    CoreSession(const CoreSession&) = delete;
    CoreSession& operator=(const CoreSession&) = delete;

    void run();

private:
    RawGLContext m_context;
    std::unique_ptr<RawGLGraph> m_graph;
};

/// Convenience wrapper that creates a temporary context and inspects one shader.
ShaderInterface
InspectShaderInterface(const ShaderInspectionRequest& request);

}  // namespace rawgl
