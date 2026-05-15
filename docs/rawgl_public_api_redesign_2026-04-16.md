# RawGL Public API Redesign

Date: 2026-04-16

Purpose: define the public API boundary that CLI and Python should use, without exposing internal graph-runtime and execution-planning details as part of the long-term library contract.

Related architectural notes:

- [rawgl_type_support_and_python_binding_plan_2026-04-15.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_type_support_and_python_binding_plan_2026-04-15.md)
- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_batch_design_2026-04-18.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_batch_design_2026-04-18.md)
- [rawgl_metadata_write_and_transfer_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_metadata_write_and_transfer_2026-04-22.md)
  - metadata inspection is active; native metadata write is deferred
- [rawgl_native_metadata_write_jpeg_tiff_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_native_metadata_write_jpeg_tiff_2026-04-22.md)
- [rawgl_native_image_io_backend_plan_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_native_image_io_backend_plan_2026-04-22.md)

## 1. Current problem

The current public header [rawgl_core.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl_core.h) is useful for refactor bring-up, but it is still too close to internal implementation structure.

In particular, the public surface currently exposes concepts that are really internal engine-planning details:

- `GraphDefinition`
- `GraphPassDefinition`
- `GraphInputDefinition`
- `GraphOutputDefinition`
- `GraphInputOverride`
- `GraphAttribute`
- raw reflection details that are only useful for low-level debugging
- class names and types that reflect internal graph compilation rather than user-facing workflows

That is acceptable during migration, but it is the wrong stable contract for:

- CLI
- Python
- later GUI/editor frontends

## 2. Desired boundary

The frontend contract should expose:

- passes
- rendering/compute mode per pass
- shader-visible variables
  - numeric uniforms
  - samplers
  - images
  - outputs
  - atomic counters
- host-memory and file-backed image IO
- pass-to-pass transfers
- optional data lifetime hints for reuse across repeated executions
- execution requests and results

The frontend contract should *not* require users to reason about:

- low-level OpenGL buffer variables
- block offsets
- raw binding-point bookkeeping
- execution-plan staging
- transient resource lifetime classes
- cache-layer internals
- `Sequence` runtime construction details

Rule:

- standard CLI and Python workflows should describe *what the pass graph is* and *what data is connected*.
- internal core code should decide *how that is scheduled and bound*.

## 3. Layer model

The intended layer split is:

### Layer A: engine core

Owns:

- shader inspection
- pass validation
- graph compilation
- resource planning
- execution
- host-memory capture

Public engine-facing concepts should be narrow and workflow-oriented.

### Layer B: CLI frontend

Owns:

- argument parsing
- textual syntax for pass graphs
- file-path and command-line convenience conversions
- help text and immediate commands

CLI should translate into the public engine API, not into internal runtime structs.

### Layer C: Python frontend

Owns:

- ergonomic object wrappers
- NumPy-friendly host image exchange
- convenience constructors and defaults
- optional higher-level helpers for histogram/statistics/image pipelines

Python should bind the same public engine API, plus Python-specific convenience helpers layered on top.

## 4. Proposed public API shape

The public API should move toward a façade built from these categories.

### 4.1 Stable public concepts

- `Session`
  - long-lived owner for caches, backend state, and shared resources
- `ShaderInspectionRequest`
- `ShaderInterface`
- `Workflow`
  - user-facing description of one single-pass or multi-pass job
- `Pass`
- `InputBinding`
- `OutputBinding`
- `CounterBinding`
- `MeshBinding`
- `PreparedWorkflow`
  - validated reusable executable form returned by the session
- `RunSettings`
  - per-run values and overrides
- `RunResult`
- `HostImageData`
- `HostMeshData`

These are the target façade names that CLI and Python should converge on.

### 4.2 Pass-facing concepts that should remain public

Per pass, frontends should be able to define:

- program kind
  - `vertfrag`
  - `compute`
- shader modules
  - file paths
  - in-memory GLSL source text
  - optional SPIR-V bytecode blobs
- output size / dispatch size
- clear color
- culling mode
- mesh source
- input bindings
- outputs
- atomic counters

This matches the user-visible model of RawGL.

Recommended public concept:

- `ShaderModuleDefinition`

Expected fields:

- stage or program role
- source kind
  - `file_path`
  - `glsl_text`
  - `spirv_binary`
- path when file-backed
- UTF-8 GLSL source text when in-memory
- owned byte blob when SPIR-V-backed
- optional virtual name / debug label

Rule:

- Python must be able to define shaders directly from Python strings without writing temporary files.
- CLI can stay path-oriented by default, but it should still translate into the same module definition model.
- SPIR-V should be modeled in the public API, but treated as a backend/capability-dependent path rather than assumed universally available.

This keeps the source model aligned with the batch-processing goal:

- simple workflows can use files
- Python and generated pipelines can use in-memory source
- advanced users can precompile and hand the core bytecode when the backend supports it

### 4.2.1 Mesh data and dynamic mesh workflows

Mesh support should follow the same memory-first direction as image inputs.
File-backed PLY/OBJ loading remains useful, but Python and production systems
must also be able to provide mesh data directly from NumPy without writing
temporary files.

Recommended public concepts:

- `HostMeshData`
  - owned CPU mesh payload
  - positions as tightly packed `float32[N, 3]`
  - indices as tightly packed `uint32[I]` or `uint32[T, 3]`
  - optional named vertex attributes with explicit layout locations
  - convenience slots for the default RawGL layout
- `MeshBinding`
  - stable user name
  - source kind: `quad`, `file`, or `host_mesh`
  - file path for file-backed meshes
  - host mesh pointer for memory-backed meshes
- future `MeshUpdate`
  - fixed-topology position/normal/attribute updates for prepared workflows
  - must update GPU buffers without rebuilding VAO/index buffers when topology
    and attribute layout are unchanged

Default RawGL mesh layout should remain predictable:

```glsl
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in uvec4 color_rgba;
layout(location = 4) in uint id0;
```

The fixed layout is a convenience preset, not the long-term limit. The stable
API should allow explicit attribute locations and integer/float attribute
types so RawGL can support material IDs, source triangle IDs, region IDs,
deformation weights, debug overlays, and future mesh pipelines without adding
project-specific parameters.

Prepared workflow semantics:

- replacing a mesh object may reallocate GPU buffers
- fixed-topology mesh updates should not reallocate
- the same named mesh used by multiple passes should upload once per prepared
  workflow when possible
- multiple meshes per pass should be legal and should draw in deterministic
  binding order

Near-term implementation order:

1. add static `HostMeshData` and Python `make_host_mesh(...)`
2. allow per-pass `host_mesh` bindings
3. draw all meshes in a pass rather than only the primary mesh
4. add prepared `mesh_updates` for positions/normals/attributes
5. generalize vertex attributes beyond the default fixed layout
6. add integer output capture such as `r32ui` returning NumPy `uint32`
7. add friendly sampler aliases such as `min_filter` and `wrap_s`

### 4.3 Variable-facing concepts that should remain public

Frontends should be able to bind:

- scalar/vector/matrix uniforms
- uniform array elements
- sampler inputs
- image inputs
- pass-output references
- persistent graph textures
- host-memory textures
- atomic counters and atomic counter elements

This is the right abstraction level for CLI and Python.

### 4.3.1 Lifetime and persistence hints

The frontend should also be able to give the core an optional hint about how long supplied data is expected to stay useful.

This is especially important for Python, where the user may run:

- the same workflow repeatedly with different frame-dependent values
- the same constants across 10-100 executions
- multiple workflows within one batch that reuse the same static inputs

The frontend should not manage cache layers directly, but it should be able to say:

- this data is one-shot
- this data is stable for this prepared workflow
- this data is stable for a larger batch or shared session

Recommended public concept:

- `enum class ResourceLifetimeHint`

Illustrative values:

- `per_execution`
- `per_workflow`
- `per_batch`
- `shared_session`

Meaning:

- `per_execution`
  - treat as ordinary one-run input unless the core decides otherwise
- `per_workflow`
  - likely reusable across repeated executions of the same prepared workflow
- `per_batch`
  - likely stable across a known batch of multiple executions or multiple related pipelines
- `shared_session`
  - candidate for longer-lived reuse in the session cache

Rule:

- these are hints, not strict ownership or residency commands
- the core remains free to ignore or downgrade a hint when the backend or memory budget requires it
- frontends should never need to say how the core implements the reuse

This keeps the API aligned with user intent:

- "this texture is constant for the whole batch"
- "this LUT is reused across many jobs"
- "this uniform block changes every frame"

instead of forcing frontends to reason about internal cache topology.

### 4.4 Concepts that should not be part of the stable public contract

These should be internal-only:

- `RawGLGraphState`
- `RawGLContextState`
- `SequenceRuntimeConfig`
- internal execution plans
- internal cached-resource descriptors
- reflected buffer-variable offset tables as normal user-facing API

Low-level buffer metadata can still exist for diagnostics and advanced inspection, but it should not drive the default frontend API.

## 5. Reflection policy

The public reflection result should focus on what frontends can actually use.

Keep public:

- resource class
- normalized name
- texture/image shape
- array length
- vector/matrix shape
- whether a resource is writable
- whether a resource is built-in/system-provided

Do not make raw offset/binding internals a required part of standard frontend logic.

For standard workflows, the frontend question is:

- "what variables exist and what can I bind to them?"

not:

- "what byte offset inside a shader block should I care about?"

## 6. Migration direction from the current header

The current [rawgl_core.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl_core.h) should be treated as an intermediate public surface.

Recommended migration:

1. Introduce a smaller façade API in `include/rawgl/` around `Session`, `Workflow`, `PreparedWorkflow`, `RunSettings`, and `RunResult`.
2. Keep the current graph-oriented structs available temporarily as compatibility types.
3. Move internal-only state and planning structs out of the public header completely.
4. Convert CLI to the façade API.
5. Bind Python only to the façade API and Python helpers, not to the transitional internal-style graph types.

### 6.1 Current `rawgl_core.h` classification

The current header should be split this way.

Keep public with little or no semantic change:

- `ShaderProgramKind`
- `ShaderInspectionRequest`
- `ShaderResourceClass`
- `ShaderTextureShape`
- `ShaderResourceInfo`
- `ShaderInterface`
- `HostImageData`

Keep public, but fold into façade-oriented names and ownership:

- `SystemUniformState`
  - likely becomes part of `RunSettings`
- `ContextCacheStats`
  - likely becomes `SessionStats` or `CacheStats`

Rename or restructure into façade concepts:

- `GraphAttribute`
  - becomes `Attribute`
- `GraphInputSourceKind`
  - becomes part of typed input-binding descriptors
- `GraphInputDefinition`
  - becomes one or more `InputBinding` classes
- `GraphAtomicCounterDefinition`
  - becomes `CounterBinding`
- `GraphOutputDefinition`
  - becomes `OutputBinding`
- `GraphMeshSourceKind`
  - becomes `MeshSourceKind`
- `GraphMeshDefinition`
  - becomes `MeshBinding`
- `GraphPassDefinition`
  - becomes `Pass`
- `GraphDefinition`
  - becomes `Workflow`
- `GraphBuildRequest`
  - likely disappears in favor of `Session::prepare(const Workflow&)`
- `GraphInputOverride`
  - becomes part of `RunSettings`
- `GraphExecutionRequest`
  - becomes `RunSettings`
- `GraphExecutionResult`
  - becomes `RunResult`
- `GraphBuildResult`
  - becomes `PrepareResult` only if a separate result object is still needed
- `RawGLContext`
  - becomes `Session`
- `RawGLGraph`
  - becomes `PreparedWorkflow`

Move out of the stable engine façade:

- `CommandLineRequest`
- `CommandLineResult`
- `Run`
- `RunCommandLine`
  - these belong to the CLI layer, not the engine façade
- `CoreSession`
  - transitional convenience wrapper, not a stable public concept

Move fully internal:

- `RawGLContextState`
- `RawGLGraphState`
- execution-plan structs
- cache descriptors
- runtime-construction structs
- reflected buffer-offset bookkeeping as a standard workflow API

### 6.2 Naming rule

Use artist-friendly names in the façade:

- `Session`
- `Workflow`
- `Pass`
- `PreparedWorkflow`
- `RunSettings`
- `RunResult`

Keep GPU-heavy or backend-heavy terms internal unless they are already normal workflow language:

- keep `compute`
- keep `shader`
- keep `GLSL`
- avoid exposing raw graph-compilation and buffer-planning terms by default

## 7. What this means for Python

Python should not bind every current public struct 1:1.

Instead, Python should expose:

- session lifecycle
- shader inspection
- workflow and pass construction
- shader module creation from file paths, GLSL source strings, and optional byte blobs
- variable binding
- host-memory image IO
- execution
- captured outputs
- captured atomic counters

Optional higher-level Python helpers can then be added later:

- image wrappers
- histogram/statistics helpers
- convenience workflow builders

## 8. What this means for the next implementation step

The next implementation step should be:

1. define the façade API boundary around `Session`, `Workflow`, `PreparedWorkflow`, `RunSettings`, and `RunResult`
2. classify current public types into:
   - keep public
   - rename/restructure
   - move internal
3. only then continue expanding nanobind bindings

Reason:

- binding the current draft header too directly will harden the wrong abstraction boundary
- frontends should depend on stable workflow-facing concepts, not current engine-compilation internals
