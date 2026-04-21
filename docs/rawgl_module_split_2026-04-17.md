# RawGL Module Split: core / io / batch

Date: 2026-04-17

Purpose: define the long-term module split between:

- `rawgl_core`
- `rawgl_io`
- `rawgl_batch`

This note is about architectural boundaries and migration order. It does not change the current binary layout yet.

Related notes:

- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)
- [rawgl_type_support_and_python_binding_plan_2026-04-15.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_type_support_and_python_binding_plan_2026-04-15.md)
- [rawgl_batch_design_2026-04-18.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_batch_design_2026-04-18.md)

## 1. Problem statement

The current library build still mixes three responsibilities into one static target:

- synchronous GPU execution
- file and codec IO
- frontend-facing workflow orchestration

Today this shows up in three concrete ways:

1. `rawgl_core` still links OpenImageIO directly in [rawgl_targets.cmake](/mnt/w/VisualStudio/RawGL/cmake/rawgl_targets.cmake).
2. file-backed image loading enters the graph build path in [graph_resources.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_resources.cpp).
3. file output encoding is performed from the runtime path in [pass_output.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/pass_output.cpp).

That is workable for the historical CLI, but it is the wrong long-term shape for:

- Python bindings
- future GUI/editor frontends
- later backend-neutral work
- serious async and parallel batch execution

The current Python path already proves the main split is valid:

- host-memory input uses `HostImageData`
- host-memory upload goes through the core
- host-memory output capture comes back through the core
- none of that fundamentally requires OpenImageIO

## 2. Target modules

### 2.1 `rawgl_core`

Owns:

- shader inspection
- workflow validation
- workflow preparation / compilation
- synchronous execution
- host-memory upload and readback
- GPU resource caches
- compiled program / shader-interface caches
- system uniform handling
- persistent workflow/session state

Does not own:

- file codecs
- image file metadata policies
- file-path convenience loading/saving
- background decode/save workers
- multi-job orchestration queues

Design rule:

- `rawgl_core` should be synchronous by default.
- It should expose the engine contract used by CLI, Python, and later GUI.

### 2.2 `rawgl_io`

Owns:

- image file decode
- image file encode
- OpenImageIO and related codec dependencies
- file metadata and codec attribute handling
- optional raw photo decode behavior
- proxy/cache generation
- async file load/save queues
- CPU-side worker pools for decode/encode

Does not own:

- workflow compilation
- GPU execution planning
- prepared workflow lifetime
- shader/program caches

Design rule:

- `rawgl_io` translates between files and core-friendly host-memory resources.
- `rawgl_io` may use `rawgl_core` public data types such as `HostImageData`, but it should not depend on core internals.

### 2.3 `rawgl_batch`

Owns:

- orchestration of many workflow runs
- overlap of load / upload / execute / readback / save
- batch scheduling
- progress and cancellation
- retry policy
- backpressure and queue limits
- resource lifetime policy across a batch
- coordination between `rawgl_core` and optional `rawgl_io`

Does not own:

- image codec implementation
- low-level GPU execution implementation

Design rule:

- `rawgl_batch` is the right place for async and parallel control.
- `rawgl_batch` should be the advanced Python path for serious throughput workloads.
- `rawgl_core` remains the simple synchronous path.

## 3. Dependency rules

The intended dependency direction is:

- `rawgl_core`
  - no dependency on `rawgl_io`
  - no dependency on `rawgl_batch`
- `rawgl_io`
  - may depend on `rawgl_core` public types
  - must not depend on core internals or runtime-planning internals
- `rawgl_batch`
  - depends on `rawgl_core`
  - may optionally integrate with `rawgl_io`

Frontend composition:

- CLI
  - should depend on `rawgl_core`
  - should usually also depend on `rawgl_io`
  - may later depend on `rawgl_batch` for async batch modes
- Python
  - simple synchronous API should depend on `rawgl_core`
  - optional file helpers may depend on `rawgl_io`
  - advanced async/parallel API should depend on `rawgl_batch`
- GUI
  - likely depends on all three

## 4. Public API shape by module

### 4.1 `rawgl_core`

Expected public concepts:

- `Session`
- `Workflow`
- `Pass`
- `PreparedWorkflow`
- `RunSettings`
- `RunResult`
- `ShaderInspectionRequest`
- `ShaderInterface`
- `HostImageData`

Core-facing input and output should be expressed as:

- host-memory resources
- already-prepared workflow resources
- in-memory shader modules
- optional persistent workflow/session resource names

File paths should not be the long-term normal contract for `rawgl_core`.

### 4.2 `rawgl_io`

Expected public concepts:

- `IoRuntime`
- `ImageLoadRequest`
- `ImageLoadResult`
- `ImageSaveRequest`
- `ImageSaveResult`
- `DecodeOptions`
- `EncodeOptions`
- `ProxyCacheOptions`

Likely API direction:

- sync and async load/save entry points
- conversion between file-backed assets and `HostImageData`
- explicit worker/queue/cache options

### 4.3 `rawgl_batch`

Expected public concepts:

- `BatchRunner`
- `BatchRunnerOptions`
- `BatchJobHandle`
- `BatchProgress`
- `BatchResult`
- `BatchCancellationToken`

Likely API direction:

- prepare once, submit many
- explicit queue and worker controls
- overlap of IO and execution without exposing OpenGL thread/context details to frontend code

## 5. Where async and parallel control should live

The main rule is:

- decode/save queues belong in `rawgl_io`
- workflow orchestration belongs in `rawgl_batch`
- synchronous execution belongs in `rawgl_core`

This avoids pushing too much hidden threading into the execution engine itself.

Why this split is important:

- GPU execution backends, especially OpenGL, are context- and thread-sensitive
- file decoding/encoding is a CPU/IO concern
- batch scheduling has different controls than codec queues

So the main controls should be split too:

### 5.1 `rawgl_io` controls

- decode worker count
- encode worker count
- load queue depth
- save queue depth
- proxy/cache directory
- proxy/cache budgets
- prefetch policy for file assets

### 5.2 `rawgl_batch` controls

- number of in-flight jobs
- number of GPU execution slots
- scheduling and priority policy
- progress aggregation
- cancellation and backpressure
- resource lifetime policy across jobs

### 5.3 `rawgl_core` controls

- synchronous run behavior
- session-level cache and lifetime hints
- explicit prepared workflow reuse

`rawgl_core` should not silently own the general async scheduler for the whole system.

## 6. Current file mapping

This is the recommended target ownership of the current tree.

### 6.1 Stays in `rawgl_core`

- [include/rawgl/rawgl.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl.h)
- [include/rawgl/rawgl_core.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl_core.h)
- [src/core/context.cpp](/mnt/w/VisualStudio/RawGL/src/core/context.cpp)
- [src/core/graph/graph_build.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_build.cpp)
- [src/core/graph/graph_resources.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_resources.cpp)
  - but only the host-memory and in-memory resource path long term
- [src/core/graph/graph_runtime_plan.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_runtime_plan.cpp)
- [src/core/graph/graph_shared.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_shared.cpp)
- [src/core/graph/graph_validation.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_validation.cpp)
- [src/core/graph/shader_interface_cache.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/shader_interface_cache.cpp)
- [src/runtime/sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)
- [src/runtime/pass_input.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/pass_input.cpp)
- [src/gl/program.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program.cpp)
- [src/gl/program_manager.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program_manager.cpp)
- [src/gl/gl_utils.cpp](/mnt/w/VisualStudio/RawGL/src/gl/gl_utils.cpp)
- [src/gl/texture.cpp](/mnt/w/VisualStudio/RawGL/src/gl/texture.cpp)
- [src/support/log.cpp](/mnt/w/VisualStudio/RawGL/src/support/log.cpp)
- [src/io/mesh_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/mesh_io.cpp)
  - unless mesh asset loading is later moved into a broader asset-IO layer

### 6.2 Moves to `rawgl_io`

- [src/io/image_io.h](/mnt/w/VisualStudio/RawGL/src/io/image_io.h)
- [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp)
- file-save logic currently in [src/runtime/pass_output.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/pass_output.cpp)
- file-backed texture loading path currently inside [src/core/graph/graph_resources.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_resources.cpp)
- format-specific output attribute policy now carried through CLI/output structs

Possible later move:

- mesh file loading if RawGL wants one unified asset-IO layer rather than image-only IO

### 6.3 New or later `rawgl_batch`

There is no clean existing `rawgl_batch` module yet.

Likely starting points:

- new `src/batch/` tree
- batch-oriented C++ façade on top of `Session` and `PreparedWorkflow`
- later Python batch wrapper built on the same C++ layer

## 7. First migration steps

The split should happen in this order.

### Step 1: make host-memory the true core boundary

Core should treat image data as:

- input `HostImageData`
- output `HostImageData`

and stop requiring file load/save inside the execution path.

### Step 2: move file-backed image load/save behind an IO adapter

The immediate extraction targets are:

- `image_utils::load_image(...)`
- output save logic from `PassOutput::saveTexture()`

Compatibility shim during transition:

- CLI can still accept file paths
- CLI translates them through `rawgl_io` into host-memory resources or save requests

### Step 3: remove OpenImageIO from `rawgl_core`

After Step 2:

- `rawgl_core` should no longer link OIIO
- OIIO and related codec dependencies move under `rawgl_io`
- Python core wheel gets a much smaller dependency surface

### Step 4: introduce `IoRuntime`

Start with:

- synchronous load/save API
- then add worker/queue options
- then add async load/save

### Step 5: introduce `BatchRunner`

Only after the core/IO split is real:

- build orchestration on top of prepared workflows and optional IO runtime
- keep the default notebook/script path synchronous and simple

## 8. Transitional compatibility rules

During migration:

- current CLI syntax should keep working
- current Python simple API should keep working
- file-backed inputs/outputs may remain exposed temporarily through compatibility fields
- new code should avoid expanding file-path concepts deeper into `rawgl_core`

The main compatibility shim should live at the frontend boundary, not inside the long-term core contracts.

## 9. Acceptance criteria

The split is successful when:

- `rawgl_core` can build and execute workflows using only in-memory resources
- `rawgl_core` no longer links OpenImageIO
- Python can use the core wheel without codec dependencies
- CLI and GUI can still load and save files through `rawgl_io`
- async decode/save control exists in `rawgl_io`
- async and parallel workflow orchestration exists in `rawgl_batch`
- the three modules have one-way dependency rules that can be enforced in CMake

## 10. Concrete CMake target split

The practical target split should happen in phases rather than all at once.

### 10.1 Phase A: introduce the targets without claiming the final dependency purity yet

Add these targets:

- `rawgl_core`
- `rawgl_io`
- `rawgl_batch`

Expected aliases:

- `RawGL::core`
- `RawGL::io`
- `RawGL::batch`

Recommended source ownership in this first phase:

- `rawgl_core`
  - current core/runtime/GL sources
  - may temporarily still include legacy save-request plumbing
- `rawgl_io`
  - [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp)
  - new image-load / image-save adapter sources
  - OIIO/libraw-specific code only
- `rawgl_batch`
  - initially empty or stub-only target
  - introduced early so the module boundary is visible in CMake and install/export metadata

Recommended initial CMake source grouping:

- `RAWGL_CORE_SOURCES`
  - current execution, graph, runtime, GL, support sources
  - remove [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp) from this list in Phase A
- `RAWGL_IO_SOURCES`
  - start with [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp)
  - then add new output-writer and adapter files
- `RAWGL_BATCH_SOURCES`
  - start empty or with a small placeholder implementation until the orchestrator API lands

Linking in Phase A:

- `rawgl_core`
  - OpenGL loader
  - GLFW
  - threads
  - miniply
  - no direct OIIO target once the first extraction is complete
- `rawgl_io`
  - `rawgl_core` public headers/types
  - `${RAWGL_OIIO_TARGETS}`
  - libraw and codec-adjacent dependencies
- `rawgl_batch`
  - `rawgl_core`
  - optional `rawgl_io`

Frontend linking target shape:

- `rawgl` CLI executable
  - should ultimately link `rawgl_core + rawgl_io`
  - may later also link `rawgl_batch` for async batch modes
- Python core wheel
  - should target `rawgl_core`
- optional Python IO/batch helpers later
  - should target `rawgl_io` and `rawgl_batch` separately

### 10.2 Phase B: split install/export metadata

Install/export should eventually expose:

- core headers under `include/rawgl/`
- optional IO headers under `include/rawgl/io/`
- optional batch headers under `include/rawgl/batch/`

Recommended export model:

- always export `RawGL::core`
- export `RawGL::io` only when IO support is built
- export `RawGL::batch` only when batch support is built

### 10.3 Phase C: tighten dependency enforcement

Once file-backed load/save is extracted from the execution path:

- remove `${RAWGL_OIIO_TARGETS}` from `rawgl_core`
- remove OIIO compile definitions from `rawgl_core`
- keep OIIO/libraw include paths and link dependencies only on `rawgl_io`

At that point the CMake graph will finally match the intended architecture.

## 11. First extraction patch plan

The first extraction should be small enough to land safely, but real enough to move OIIO outward.

### Patch 1: introduce `rawgl_io` as a real target

Files:

- [cmake/rawgl_targets.cmake](/mnt/w/VisualStudio/RawGL/cmake/rawgl_targets.cmake)
- [cmake/rawgl_dependencies.cmake](/mnt/w/VisualStudio/RawGL/cmake/rawgl_dependencies.cmake)

Goal:

- create `rawgl_io`
- move [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp) out of `RAWGL_CORE_SOURCES`
- keep CLI linking working

Important note:

- this patch alone does not fully remove OIIO from the execution path
- it only creates the target boundary needed for the next patches

### Patch 2: extract file output writing behind an IO-side adapter

Current seam:

- [src/runtime/pass_output.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/pass_output.cpp)
- [src/runtime/sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

Problem:

- `PassOutput::saveTexture()` directly uses OIIO types and codec logic
- as long as that stays in core/runtime code, `rawgl_core` still drags OIIO

Recommended direction:

- introduce an IO-side output writer, for example:
  - `src/io/output_writer.h`
  - `src/io/output_writer.cpp`
- stop making `PassOutput` own the actual file write implementation
- change the runtime to produce a save request or host-image payload instead of directly encoding the file

The crucial point is:

- the final file write must happen in `rawgl_io`, not in `rawgl_core`

### Patch 3: replace `PassOutput` file-save behavior with save requests

Current direct call:

- [src/runtime/sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)
  - `Sequence::savePassOutputs(...)`

Recommended replacement:

- core/runtime produces a list of output save requests containing:
  - output path
  - format token
  - channels / alpha / bits
  - metadata attributes
  - `HostImageData` or an equivalent host-readback payload

Then:

- CLI compatibility layer calls `rawgl_io` to encode those requests

This is the key patch that lets core stay synchronous while no longer owning file codecs.

### Patch 4: extract file-backed input loading behind an IO-side adapter

Current seams:

- [src/core/graph/graph_resources.cpp](/mnt/w/VisualStudio/RawGL/src/core/graph/graph_resources.cpp)
- [src/runtime/sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

Problem:

- file-backed texture loading still calls `image_utils::load_image(...)` directly from core/runtime code

Recommended direction:

- add an IO-side load adapter that returns `HostImageData`
- frontend compatibility layer translates file-path inputs to host-memory inputs before calling core

That means the long-term core-facing path becomes:

- file path outside core
- `HostImageData` inside core

### Patch 5: remove OIIO-specific types from runtime-facing headers

Current leakage:

- [src/runtime/sequence.h](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.h)
  includes [src/io/image_io.h](/mnt/w/VisualStudio/RawGL/src/io/image_io.h)
- `PassOutput` stores `OIIO::TypeDesc`

Problem:

- header-level OIIO types make the split harder than it needs to be

Recommended direction:

- replace `OIIO::TypeDesc` in runtime-facing structs with a small RawGL-owned representation
- keep OIIO type mapping inside IO code only

This patch is necessary before `rawgl_core` can stop including IO headers transitively.

### Patch 6: only then remove OIIO from `rawgl_core`

Once patches 2-5 are in place:

- remove `${RAWGL_OIIO_TARGETS}` from `rawgl_core`
- remove `OIIO_STATIC_DEFINE` from `rawgl_core`
- move OIIO/libraw include usage fully to `rawgl_io`

That is the first point where the architectural split becomes real instead of aspirational.

## 12. Immediate implementation notes

Two current seams matter most:

1. [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp)
   already contains the codec-heavy file decode logic and is the correct first source file to move under `rawgl_io`.

2. [src/runtime/pass_output.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/pass_output.cpp)
   is the hardest blocking seam, because it mixes:
   - runtime output ownership
   - texture readback
   - codec choice
   - file writing

The clean split is:

- texture readback belongs in core
- file encoding belongs in IO

That should guide the first extraction implementation.
