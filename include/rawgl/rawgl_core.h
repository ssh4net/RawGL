// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace rawgl {

/// Low-level graph-oriented public API.
///
/// New frontend-facing code should usually prefer `rawgl/rawgl.h`, which
/// provides the workflow-oriented façade. Command-line entry points live in
/// `rawgl/rawgl_cli.h`.

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
    /// Backing path when `sourceKind` is `filePath`.
    std::string path;
    /// UTF-8 GLSL source text when `sourceKind` is `glslText`.
    std::string glslText;
    /// Owned SPIR-V bytes when `sourceKind` is `spirvBinary`.
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

/// Describes a mesh file inspection request.
struct MeshInspectionRequest {
    /// Mesh path to inspect. Currently OBJ receives detailed text-level inspection.
    std::string path;
};

/// Material name discovered while scanning an OBJ file.
struct MeshMaterialInfo {
    /// Stable numeric ID assigned in first-use order. Faces before any material use ID 0.
    uint32_t id = 0;
    /// OBJ `usemtl` name. Empty for the implicit material 0 bucket.
    std::string name;
    /// Number of source faces using this material ID.
    size_t faceCount = 0;
};

/// Contiguous OBJ group/object span discovered while scanning faces.
struct MeshGroupInfo {
    /// Group or object name. Empty means the source had no active group/object name.
    std::string name;
    /// First source face index covered by this span.
    size_t firstFaceIndex = 0;
    /// Number of source faces in this span.
    size_t faceCount = 0;
};

/// CPU-only mesh inspection data useful before building a render workflow.
struct MeshInspectionResult {
    bool success = false;
    std::string errorMessage;
    std::string path;
    size_t vertexCount = 0;
    size_t texcoordCount = 0;
    size_t normalCount = 0;
    size_t faceCount = 0;
    size_t triangleFaceCount = 0;
    size_t quadFaceCount = 0;
    size_t ngonFaceCount = 0;
    size_t generatedTriangleCount = 0;
    bool hasBounds = false;
    bool hasUvRange = false;
    std::array<float, 3> boundsMin = { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> boundsMax = { 0.0f, 0.0f, 0.0f };
    std::array<float, 2> uvMin = { 0.0f, 0.0f };
    std::array<float, 2> uvMax = { 0.0f, 0.0f };
    std::vector<MeshMaterialInfo> materials;
    std::vector<MeshGroupInfo> groups;
};

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

/// Extra per-vertex host-memory attribute.
///
/// Locations 0..4 are reserved by RawGL's default mesh layout. Use locations
/// >= 5 for explicit shader attributes.
struct HostMeshAttribute {
    /// Optional debug label for diagnostics.
    std::string name;
    /// GLSL vertex attribute location.
    uint32_t location = 0;
    /// Number of components per vertex, 1..4.
    uint32_t components = 1;
    /// OpenGL element type such as `GL_FLOAT`, `GL_UNSIGNED_BYTE`, or `GL_UNSIGNED_INT`.
    unsigned int glType = 0;
    /// True when the attribute should be bound with integer attribute semantics.
    bool integer = false;
    /// Owned tightly packed per-vertex attribute bytes.
    std::vector<std::byte> bytes;
};

/// Owned host-memory mesh payload used for in-memory graph mesh inputs.
///
/// The default layout is position/texcoord/normal/color/id0 at attribute
/// locations 0..4. Empty optional arrays are expanded to default values during
/// resource preparation.
struct HostMeshData {
    /// Tightly packed `float32[N, 3]` positions.
    std::vector<float> positions;
    /// Tightly packed `uint32[I]` element indices.
    std::vector<uint32_t> indices;
    /// Optional tightly packed `float32[N, 2]` texture coordinates.
    std::vector<float> texcoords;
    /// Optional tightly packed `float32[N, 3]` normals.
    std::vector<float> normals;
    /// Optional tightly packed `uint8[N, 4]` RGBA vertex colors.
    std::vector<uint8_t> colors;
    /// Optional tightly packed `uint32[N]` ID attribute for location 4.
    std::vector<uint32_t> id0;
    /// Optional explicit per-vertex attributes for locations >= 5.
    std::vector<HostMeshAttribute> attributes;
};

/// Source variant for a graph input.
enum class GraphInputSourceKind {
    intValues,
    uintValues,
    floatValues,
    doubleValues,
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
    /// Integer payload for `intValues` sources.
    std::vector<int32_t> intValues;
    /// Unsigned integer payload for `uintValues` sources.
    std::vector<uint32_t> uintValues;
    /// Float payload for `floatValues` sources.
    std::vector<float> floatValues;
    /// Double payload for `doubleValues` sources.
    std::vector<double> doubleValues;
    /// Referenced output name for `passOutput` sources.
    std::string referencedOutputName;
    /// Referenced pass index for `passOutput` sources.
    size_t referencedPassIndex = 0;
    /// Graph texture name for `graphTexture` sources.
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
    /// Host-memory texture payload for `hostTexture` sources.
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
    hostMesh,
};

/// Declares one mesh input for a pass.
struct GraphMeshDefinition {
    std::string name;
    GraphMeshSourceKind sourceKind = GraphMeshSourceKind::quad;
    std::string path;
    std::shared_ptr<HostMeshData> hostMesh;
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
    std::vector<GraphAttribute> attributes;
    /// Selects one element of an array-valued target input.
    bool usesArrayElement = false;
    /// Zero-based array element selected when \ref usesArrayElement is true.
    size_t arrayElement = 0;
    /// Host-memory texture payload for `hostTexture` sources.
    std::shared_ptr<HostImageData> hostTexture;
};

/// Per-execution fixed-topology mesh buffer update.
struct GraphMeshUpdate {
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

/// Per-execution full host mesh replacement.
struct GraphMeshOverride {
    /// When false, apply to every pass containing \ref name.
    bool usesPassIndex = false;
    /// Target pass index when \ref usesPassIndex is true.
    size_t passIndex = 0;
    /// Target mesh binding name.
    std::string name;
    /// Replacement host mesh payload.
    std::shared_ptr<HostMeshData> hostMesh;
};

/// Request for executing a previously built graph.
struct GraphExecutionRequest {
    SystemUniformState systemUniforms;
    std::vector<GraphInputOverride> inputOverrides;
    std::vector<GraphMeshUpdate> meshUpdates;
    std::vector<GraphMeshOverride> meshOverrides;
};

/// Result of executing a built graph.
struct GraphExecutionResult {
    /// False when execution failed.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Host-captured outputs keyed by strings such as `name::0` or `name[index]::0`.
    std::map<std::string, HostImageData> capturedOutputs;
    /// Captured atomic counter values keyed by strings such as `name::0` or `name[index]::0`.
    std::map<std::string, std::vector<uint32_t>> capturedAtomicCounters;
};

/// Snapshot of the active OpenGL runtime selected by RawGL.
struct RuntimeInfo {
    /// True when RawGL created an OpenGL context and queried the runtime.
    bool success = false;
    /// Failure details when \ref success is false.
    std::string errorMessage;
    /// Requested platform from `--gl_platform` or `RAWGL_GL_PLATFORM`.
    std::string requestedPlatform;
    /// Platform RawGL selected for GLFW, such as `x11` or `wayland`.
    std::string selectedPlatform;
    /// Current `DISPLAY` value.
    std::string display;
    /// Current `WAYLAND_DISPLAY` value.
    std::string waylandDisplay;
    /// OpenGL vendor string.
    std::string vendor;
    /// OpenGL renderer string.
    std::string renderer;
    /// OpenGL version string.
    std::string version;
    /// OpenGL shading language version string.
    std::string shadingLanguageVersion;
    /// True when the renderer looks like a software rasterizer.
    bool softwareRenderer = false;
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
    /// Makes the owned OpenGL context current on the calling thread.
    void makeContextCurrent() const;
    /// Releases the owned OpenGL context from the calling thread.
    void releaseContext() const;

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

/// Inspect a mesh file without compiling or executing a workflow.
MeshInspectionResult
InspectMeshFile(const MeshInspectionRequest& request);

/// Creates a temporary context and returns OpenGL runtime diagnostics.
RuntimeInfo
ProbeRuntimeInfo();

}  // namespace rawgl
