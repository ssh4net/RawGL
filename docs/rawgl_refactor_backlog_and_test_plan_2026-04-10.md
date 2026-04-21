# RawGL Refactor Backlog and Correctness Test Plan

Date: 2026-04-10

This document converts the review in `docs/rawgl_review_2026-04-10.md` into a concrete work backlog and a test plan that can be used during refactoring.

## 1. Goals

The next refactor should achieve five things at the same time:

- Fix the known correctness bugs in runtime behavior.
- Make parsing, validation, reflection, allocation, and execution separable.
- Add regression coverage before larger structural changes land.
- Build a test surface that can survive the move toward a more HPC-style C++ architecture.
- Prepare RawGL as a reusable core library with thin CLI, GUI, and Python binding frontends.
- Support both isolated processing graphs and multiple graphs sharing a common resource context.

## 2. Current test state

Current tests are only smoke launches:

- [tests/test_frag_pass.bat](/mnt/w/VisualStudio/RawGL/tests/test_frag_pass.bat)
  Three-pass fragment chain using pass-to-pass texture references.
- [tests/test_comp_shader.bat](/mnt/w/VisualStudio/RawGL/tests/test_comp_shader.bat)
  One compute pass using sampler input and image output.
- [tests/EmptyLUTGen_fileformats.bat](/mnt/w/VisualStudio/RawGL/tests/EmptyLUTGen_fileformats.bat)
  Large LUT generation and file export path.

Current weaknesses:

- No output verification.
- No error-path verification.
- No parser coverage.
- No shader reflection coverage.
- No atomic coverage.
- No combined `vertfrag` coverage.
- No Linux-oriented test entry point.
- Existing tests are batch files only, so they are not suitable as the long-term verification harness.

## 3. Immediate backlog

### P0: correctness fixes before structural refactor

1. Fix atomic reflection split.
   Files:
   - [program.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program.cpp)

   Acceptance:
   - Atomic counters are reflected even when the shader has no SSBOs.
   - SSBO reflection does not depend on atomic counters being present.

2. Fix compute image input binding.
   Files:
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

   Acceptance:
   - `image2D` inputs use `glBindImageTexture()`.
   - Access mode and format are explicit and validated.

3. Fix compute output reference validation.
   Files:
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

   Acceptance:
   - Compute outputs referenced as `name::passIndex` bind correctly.
   - Failure mode is explicit when the named output uniform does not exist.

4. Remove hardcoded debug SSBO injection from graphics execution.
   Files:
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

   Acceptance:
   - No unconditional `AtBuf` allocation.
   - No unconditional readback/printing in normal execution.

5. Fix `cull` parser behavior.
   Files:
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

   Acceptance:
   - `--cull wind ccw`
   - `--cull face bk`
   - `--cull enable false`
   all modify the pass state as requested.

6. Fix unsigned uniform upload.
   Files:
   - [program.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program.cpp)

   Acceptance:
   - Scalar unsigned uniforms use the correct OpenGL upload call.

7. Remove partial-construction return paths.
   Files:
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

   Acceptance:
   - Invalid numeric input cannot leave `Sequence` half-constructed and still runnable.

### P1: parser and validation extraction

8. Split CLI parsing from pipeline construction.
   Files:
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)
   - new parser/validation files

   Acceptance:
   - One layer parses tokens into typed option records.
   - One validation layer resolves references and reports all structural errors.
   - GL objects are not created during parsing.

9. Extract an explicit shader interface model.
   Files:
   - [program.h](/mnt/w/VisualStudio/RawGL/src/gl/program.h)
   - [program.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program.cpp)
   - [program_manager.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program_manager.cpp)
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)

   Acceptance:
   - Shader compile/link produces a stable interface model for:
     - uniforms
     - sampler inputs
     - image inputs
     - outputs
     - atomic counters
     - SSBOs
   - CLI validation uses this interface model instead of ad hoc `findUniform()` / `findOutput()` / `findCounter()` lookups.
   - Shader-declared names and bindings become the source of truth for validation.
   - The user is not forced to restate shader-defined inputs in the CLI unless the CLI is providing external data or an explicit override.
   - Built-in system uniforms are modeled explicitly alongside user-defined shader resources.

10. Replace string-order mutation with a typed pass graph.

   Acceptance:
   - Passes, inputs, outputs, and references are represented as validated records.
   - The code can reject cycles or illegal forward references explicitly.

### P2: execution-plan extraction

11. Build immutable per-pass execution descriptors.

   Acceptance:
   - `run()` no longer creates GL objects.
   - Internal uniform locations are precomputed.
   - Per-pass resource bindings are precomputed.

12. Move export scheduling out of the hot pass loop.

   Acceptance:
   - Execution produces GPU results.
   - Export is a separate stage with explicit requested outputs.

### P3: library-core extraction

12.1 Normalize source file naming after the internal module split stabilizes.

   Acceptance:
   - File names reflect one clear responsibility per translation unit.
   - New and renamed core files follow one consistent naming convention instead of the current mixed style.
   - Header/source pairs stay obvious and searchable.
   - CMake and Visual Studio metadata are updated in the same change so renames do not leave stale project entries behind.
   - This happens after parser/builder/runtime boundaries are stable enough that we do not rename files twice.

13. Extract a reusable RawGL core library.
   Files:
   - [main.cpp](/mnt/w/VisualStudio/RawGL/src/app/main.cpp)
   - [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp)
   - new core API files
   - CMake / Visual Studio project files

   Acceptance:
   - Core execution is exposed through a library API rather than only a process-oriented CLI path.
   - CLI becomes a thin adapter over the library.
   - Future GUI and Python `nanobind` bindings can use the same engine API without duplicating orchestration logic.
   - Headless batch processing is possible without CLI-specific state or global process assumptions.

14. Define a batch-oriented engine API.

   Acceptance:
   - The library exposes explicit request/result objects for:
     - shader loading
       - file-backed shader modules
       - in-memory GLSL source modules
       - optional SPIR-V bytecode modules when backend support exists
     - interface inspection
     - pass graph build
     - execution
     - export requests
   - The API is suitable for later async job scheduling and GUI preview control.
   - The API carries a structured system-uniform state for animation-oriented values such as time, frame number, and related built-ins.

15. Add explicit graph and shared-resource context types.

   Acceptance:
   - The core library can build and execute multiple independent processing graphs in one process.
   - The core library can also build multiple graphs that share a common resource context for textures, programs, meshes, or cached allocations when requested.
   - Shared resources are explicit in the API, not hidden in globals.
   - A graph can be destroyed without invalidating unrelated independent graphs.

15.1 Define the shared-resource model and cache layers explicitly.

   Acceptance:
   - The design explicitly covers these shared cases:
     - same graph, different per-run uniforms or textures
     - same graph, animated frame sequences using system uniforms
     - multi-pass graphs with pass-to-pass resource reuse
     - multiple graphs sharing static assets such as textures, LUTs, meshes, and shader sources
     - repeated interactive rebuilds with small shader or pass changes
     - persistent graph-local state such as feedback textures, history buffers, or counters
   - Resource lifetime is explicit and testable:
     - per dispatch
     - per graph run
     - persistent per graph
     - shared per context
   - Atomic counters and image load/store resources are treated as separate resource classes.
   - Cache layers are explicit in the design:
     - shader source
     - preprocessed shader
     - shader interface
     - compiled program
     - host image decode
     - host mesh parse
     - GPU resource
     - execution plan
   - The design does not assume that path-only cache keys are sufficient long term.
   - The public API can provide optional lifetime/reuse hints for user-supplied data so Python and other frontends can signal:
     - one-shot input
     - pipeline-stable input
     - batch-stable input
     - shared-context input
     without exposing internal cache mechanics as part of the normal frontend contract.

15.2 Redesign the public engine API boundary before broad Python exposure.

   Design note:
   - [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)

   Acceptance:
   - The public header exposes workflow-facing engine concepts rather than internal graph/runtime planning types.
   - CLI and Python can define passes, rendering mode, variables, pass-to-pass transfers, host-memory images, and execution requests through the same public façade.
   - Low-level buffer-planning details are not required for standard CLI/Python workflows.
   - The Python binding is implemented against the façade API, not against transitional internal-style graph structs.

15.3 Split the project into `rawgl_core`, `rawgl_io`, and `rawgl_batch`.

   Design note:
   - [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
   - [rawgl_batch_design_2026-04-18.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_batch_design_2026-04-18.md)

   Acceptance:
   - `rawgl_core` owns synchronous workflow preparation and execution plus host-memory GPU transfer only.
   - `rawgl_core` no longer links OpenImageIO or file-codec dependencies.
   - `rawgl_io` owns file image decode/encode, codec-specific metadata handling, and async file-side worker/queue control.
   - `rawgl_batch` owns orchestration of many jobs, overlap of IO/upload/execute/save, progress, cancellation, and backpressure.
   - Dependency direction is explicit and one-way:
     - `rawgl_core` independent
     - `rawgl_io` may depend on `rawgl_core` public types
     - `rawgl_batch` depends on `rawgl_core` and may integrate with `rawgl_io`
   - CLI and Python keep compatibility through frontend-level shims during the migration.

16. Add explicit built-in system uniform support.

   Acceptance:
   - The core defines a stable set of built-in uniforms for animation and render-control data such as:
     - time
     - delta time
     - frame number
     - pass index
     - framebuffer size / aspect
   - Built-in uniforms have a reserved naming scheme and cannot silently collide with user-defined uniforms.
   - The engine updates built-in uniforms automatically per frame/pass/job as appropriate.
   - CLI, GUI, and Python frontends can override or seed time/frame state through the same engine API when needed.
   - The system-uniform model remains backend-neutral so it can survive a future backend split.

### P4: shader source model cleanup

17. Replace single-file `vertfrag` macro splicing with a proper source split step.

   Acceptance:
   - `#version` placement is preserved.
   - Diagnostics map back to the source correctly enough to debug.
   - Stage-specific source generation is deterministic.

18. Separate reflection responsibilities.

   Acceptance:
   - Uniforms, outputs, images, atomic counters, and SSBOs are reflected by dedicated code paths.
   - The reflection layer feeds the explicit shader interface model rather than only exposing raw lookup helpers.

### P5: backend-neutral execution preparation

19. Keep the internal engine model backend-neutral while continuing to ship OpenGL first.

   Acceptance:
   - Internal graph, shader interface, resource descriptors, and execution plans are not hard-shaped around CLI or OpenGL-specific naming.
   - OpenGL remains the current production backend.
   - The refactor does not introduce new public API assumptions that would block a second backend later.

20. Treat Vulkan as the primary future explicit backend target.

   Acceptance:
   - Future backend notes and internal abstractions assume Vulkan is the main candidate for explicit multi-device, cache-friendly, batch-oriented execution.
   - Shader/interface/cache work added now does not assume OpenGL-only semantics where avoidable.

21. Treat Metal as a possible later backend target through cross-compilation paths only.

   Acceptance:
   - Metal is documented as a possible future target, but not as an immediate implementation goal.
   - Cross-compilation and backend portability are considered in shader-source and interface design, without forcing the current refactor into a premature backend rewrite.

## 3.1 Backend direction

- Continue stabilizing the OpenGL backend first.
- Do not start a Vulkan migration during the current base refactor.
- Keep internal concepts such as shader source, shader interface, resource descriptors, execution plans, and device/context ownership backend-neutral where practical.
- Treat shader/program/interface caching as useful near-term OpenGL work and also as a constraint on future Vulkan design.
- Treat Vulkan as the main future backend candidate for explicit multi-GPU/device selection and stronger cache control.
- Treat Metal as a possible later backend only after the backend-neutral core model is established.

## 3.2 Shared-resource model

The shared-resource model should be shaped around real use cases rather than ad hoc caches.

Primary shared cases:

- Same graph, different input data.
  Example:
  one validated graph reused for thousands of images with different uniforms, textures, and system uniform state.
- Same graph, frame-sequence rendering.
  Example:
  one graph reused across frames with changing `time`, `delta time`, `frame`, and a small set of animated uniforms.
- Multi-pass graph reuse.
  Example:
  one pass output reused as a later pass input within a graph, with optional persistent intermediates across runs.
- Shared static assets across many graphs.
  Example:
  multiple graphs reusing the same LUTs, environment maps, meshes, or common shader sources.
- Repeated interactive rebuild with small edits.
  Example:
  one shader changes while the rest of the graph and cached resources remain reusable.
- Persistent stateful resources.
  Example:
  feedback buffers, history textures, or counters that persist across graph executions.

Resource lifetime classes:

- Per dispatch
- Per graph run
- Persistent per graph
- Shared per context

Planned cache layers:

- Shader source cache
- Preprocessed shader cache
- Shader interface cache
- Compiled program cache
- Host-side decoded image cache
- Host-side parsed mesh cache
- GPU resource cache
- Execution-plan cache

Implementation priorities for reuse:

1. Same graph, different data
2. Multi-pass graph reuse with explicit transient vs persistent resources
3. Shared static assets across graphs
4. Incremental rebuild and partial invalidation
5. Persistent state resources
6. Tiled or partitioned execution

Rules:

- `Session` should own shared backend state and shared caches.
- `PreparedWorkflow` should own one validated workflow plus its execution plan.
- Workflow execution takes per-run bindings and system uniform state.
- Atomic counters and image resources with atomic operations are separate categories.
- Cache keys should move toward content-sensitive identity where practical.
- Async and parallel scheduling should wait until these boundaries are explicit and tested.

## 3.3 Deferred OpenGL safety ideas

- Add a deferred large-input texture upload safety path for OpenGL.
  Scope:
  allocate texture storage first, then upload image data in row stripes with `glTexSubImage2D` instead of one monolithic `glTexImage2D(..., data)` upload for very large inputs.
- Keep this as an implementation detail of texture creation, not a change to graph semantics.
  Rationale:
  this is compatible with RawGL's current full-image execution model and does not require an `imiv`-style visible-tile cache architecture.
- Treat this as an upload-side mitigation only.
  Non-goal:
  it does not replace the Windows TDR / driver-timeout workaround for long-running compute or fragment execution.
- If implemented later, prefer a threshold-driven or configurable path so ordinary images keep the simpler upload path.
- Do not attempt to port `imiv`'s full viewer tiling model into RawGL unless RawGL's execution model changes substantially.

## 3.4 Type-support and Python staging

The next public-API growth should be staged rather than bundled into one large change.

Reference design note:

- [rawgl_type_support_and_python_binding_plan_2026-04-15.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_type_support_and_python_binding_plan_2026-04-15.md)

Immediate planning direction:

1. Wave 1
   - add array-aware reflection metadata
   - add array-element addressing in the graph API
   - support the smallest useful set of missing types
   - add a host-memory `2D` image path so Python bindings are useful without file-only round trips
2. Wave 2
   - replace the current overloaded input model with distinct numeric/sampler/image/buffer bindings
   - add non-`2D` texture/image shapes, array resources, explicit access modes, and structured subresource references
   - expose SSBO/UBO public binding only after the generalized resource model exists

Rule:

- Python bindings should start after Wave 1 is stable.
- Python bindings should not wait for the full Wave 2 generalized resource model.

## 3.5 Documentation track

The public API and core architecture are now stable enough to start incremental API documentation instead of waiting for the final polish pass.

Immediate direction:

1. Add Doxygen comments to the exported API first.
   Scope:
   - start with [include/rawgl/rawgl_core.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl_core.h)
   - document request/result structs, enums, lifetime owners, and execution entry points
   - expand into stable internal headers only after their ownership boundaries stop moving
2. Keep documentation comments short and factual.
   Rule:
   prefer OpenMeta-style `///` API comments over large banner blocks or prose-heavy repetition.
3. Add a Doxygen configuration and Sphinx/Breathe layer after the public API surface is reasonably covered.
   Acceptance:
   - generated API reference for the exported RawGL library surface
   - narrative docs under `docs/` can link into API symbols instead of duplicating signatures
4. Add CI for documentation once the generated output is useful.
   Acceptance:
   - GitHub Actions job that builds Doxygen and Sphinx docs
   - failure is explicit when API docs stop building
   - published docs can be added later without changing the source comment format again

Rule:

- Start documenting while refactoring, but prioritize public and stable surfaces over volatile internals.

## 4. Test strategy

The test plan should be layered. Not every layer needs to exist immediately, but the design should support all of them.

### Layer A: CLI smoke tests

Purpose:

- Verify the executable launches.
- Verify shaders compile/link.
- Verify expected files are created.

Keep:

- Existing `.bat` smoke scripts.

Add:

- Linux shell equivalents.
- Explicit pass/fail exit-code checks instead of `pause`-only scripts.

### Layer B: output correctness regression tests

Purpose:

- Verify the produced image content, not only file existence.

Method:

- Use deterministic synthetic shaders and small output sizes.
- Save as TIFF/EXR or another stable format.
- Compare against reference images with exact compare for integer outputs or tolerance compare for float outputs.

Comparison rules:

- Integer outputs: exact pixel match.
- Float outputs: max absolute error and mean absolute error thresholds.
- File metadata should not be part of image-equality unless the test explicitly targets metadata.

### Layer C: parser and validation tests

Purpose:

- Verify CLI semantics independently of OpenGL once parsing is extracted.

Method:

- Add unit-style tests around the parser/validator layer after extraction.
- Use argument vectors as input.
- Verify typed results and error messages.

### Layer D: reflection and resource-binding tests

Purpose:

- Verify the shader-facing resource model.

Method:

- Use small dedicated shaders with one feature each.
- Confirm that reflected uniforms, outputs, image bindings, atomic counters, and SSBOs match expectations.
- Confirm that shader-declared interfaces are sufficient to drive CLI validation without redundant binding declarations.

### Layer E: negative tests

Purpose:

- Verify that invalid input fails cleanly and deterministically.

Method:

- Feed invalid CLI, invalid references, invalid shader resource names, and unsupported features.
- Verify non-zero exit and stable error text class.

## 5. Initial concrete test matrix

These are the first tests that should exist because they map directly to the current risk profile.

### T01: fragment single-pass constant output

Purpose:

- Verify baseline vertex+fragment execution and file output.

Shader behavior:

- Full-screen quad writes a constant RGB color.

Checks:

- Exit code is zero.
- Output file exists.
- All pixels match the expected color exactly.

### T02: current three-pass fragment chain with output validation

Base files:

- [tests/test_frag_pass.bat](/mnt/w/VisualStudio/RawGL/tests/test_frag_pass.bat)
- [tests/shaders/pass1.frag](/mnt/w/VisualStudio/RawGL/tests/shaders/pass1.frag)
- [tests/shaders/pass2.frag](/mnt/w/VisualStudio/RawGL/tests/shaders/pass2.frag)
- [tests/shaders/pass3.frag](/mnt/w/VisualStudio/RawGL/tests/shaders/pass3.frag)

Purpose:

- Verify pass-to-pass texture references and mip-enabled sampling path.

Checks:

- `pass1.tif`, `pass2.tif`, `pass3.tif` all exist.
- Output images match golden references.
- The channel edits introduced by each pass are visible in the expected outputs.

### T03: compute sampler-input smoke with output validation

Base files:

- [tests/test_comp_shader.bat](/mnt/w/VisualStudio/RawGL/tests/test_comp_shader.bat)
- [tests/shaders/test.comp](/mnt/w/VisualStudio/RawGL/tests/shaders/test.comp)

Purpose:

- Verify compute pass dispatch, sampler input, and image output.

Checks:

- Output exists.
- Pixel values match expected texture-plus-constant math.

### T04: compute image-input correctness test

Purpose:

- Catch the current `GL_IMAGE_2D` binding bug and protect its fix.

Shader behavior:

- `readonly image2D` input
- `writeonly image2D` output
- copy or simple transform

Checks:

- Output matches input exactly or with a known transform.

Status:

- This test should fail on current code until the image-binding bug is fixed.

### T05: combined `vertfrag` with leading `#version`

Purpose:

- Catch the unresolved single-file shader parsing issue.

Shader behavior:

- One `vertfrag` file containing a legal `#version` line and stage guards.

Checks:

- Program should compile and render a known result after the parser fix.

Status:

- Covered by the current `rawgl_single_file_vertfrag` and `rawgl_core_inspect_smoke` regression tests.

### T06: `--cull` parsing test

Purpose:

- Protect the `cull` parser fix.

Shader behavior:

- Simple mesh or quad render where culling state changes the visible result.

Checks:

- `enable false` and `enable true` produce different expected outputs.
- `wind` and `face` combinations affect the output as expected.

### T07: pass-size reference test

Purpose:

- Verify `--pass_size name::passIndex` resolution.

Shader behavior:

- Pass 0 writes a known-size output.
- Pass 1 derives size from pass 0 and writes another output.

Checks:

- Output dimensions are exactly the referenced dimensions.

### T08: invalid reference negative test

Purpose:

- Ensure bad `name::passIndex` references fail deterministically.

Inputs:

- Missing output name
- out-of-range pass index
- forward reference

Checks:

- Non-zero exit.
- Error category is stable.

### T09: uniform numeric arity negative test

Purpose:

- Protect against partial construction on bad numeric input.

Inputs:

- Too few values for `vec3`, `mat3`, `mat4`

Checks:

- Non-zero exit.
- No output file created.

### T10: atomic counter reflection test without SSBO

Purpose:

- Protect the atomic reflection fix.

Shader behavior:

- Fragment or compute shader with atomic counters only.

Checks:

- Reflection finds the counters.
- Initialization and readback succeed.

### T11: SSBO reflection test without atomic counters

Purpose:

- Ensure SSBO reflection remains valid after splitting the reflection code.

Shader behavior:

- Shader with SSBO only.

Checks:

- Reflection finds the buffer and its variables.

### T12: shader-interface-driven CLI validation

Purpose:

- Ensure the extracted shader interface becomes the source of truth for pass inputs and outputs.

Checks:

- CLI accepts shader-declared uniforms and outputs without redundant binding declarations.
- Unknown CLI names fail against the reflected interface with a stable error.
- Sampler, image, and output names are validated from the extracted shader interface rather than from CLI-side assumptions.

### T13: unsupported feature rejection test

Purpose:

- Make current limitations explicit.

Inputs:

- Uniform arrays
- image arrays
- unsupported texture types

Checks:

- Clean non-zero exit with a clear message.

## 6. Test asset design rules

To keep tests stable, new test shaders and images should follow these rules:

- Use small outputs such as `4x4`, `8x8`, `16x16`, or `32x32`.
- Prefer deterministic procedural colors over photographic inputs.
- Prefer exact math that avoids vendor-specific ambiguity.
- Avoid derivatives, undefined mip behavior, and precision-sensitive branches unless the test specifically targets them.
- Prefer one feature per shader.
- Keep one output per test unless the test is specifically about multi-output behavior.

## 7. Recommended harness design

The long-term harness should not stay as batch files only.

Recommended structure:

- Keep `tests/*.bat` as manual entry points for Windows.
- Add `tests/*.sh` for Linux smoke parity.
- Add one scriptable verifier that:
  - runs RawGL
  - captures stdout/stderr
  - checks exit code
  - checks output file existence
  - compares image outputs against goldens or expected scalars

Preferred implementation:

- Python is the most practical harness language here because it can manage process execution and image comparison cleanly.
- The verifier should sit outside the hot C++ runtime and should not affect the product architecture.

Minimal first harness responsibilities:

- `run_case(case_name, args, expected_exit)`
- `assert_file_exists(path)`
- `compare_image_exact(actual, golden)`
- `compare_image_tolerance(actual, golden, max_abs_err, mean_abs_err)`

## 8. Platform matrix

The tests should be tracked against this minimum matrix:

- Windows x64 Debug
- Windows x64 Release
- Linux clang-20 + libc++ Debug
- Linux clang-20 + libc++ Release

Practical rule:

- Smoke tests should run on all matrix entries.
- Output-correctness tests should run at least on one Windows build and one Linux build.
- Parser and validation tests should be platform-independent once extracted.

## 9. Acceptance gates for the next refactor wave

No P0 or P1 refactor should be considered complete unless:

- All existing smoke tests still pass.
- T01 through T09 exist and pass.
- Any intentionally unsupported feature has a negative test.
- The test harness can be run non-interactively.

No reflection or atomic refactor should be considered complete unless:

- T10 and T11 exist and pass.
- T12 exists and passes once shader-interface extraction lands.

No shader-source parser rewrite should be considered complete unless:

- T05 exists and passes.

## 10. Recommended work order

The best order to keep momentum and reduce regression risk is:

1. Add the test harness skeleton and convert the current smoke scripts into non-interactive cases.
2. Add T01, T02, T03, T08, and T09 first.
3. Fix the P0 correctness bugs.
4. Extract parser/validation and then extract the explicit shader interface model.
5. Add T04, T06, T07, T10, T11, and T12 as each fix lands.
6. Replace string-order mutation with a typed validated pass graph and immutable execution descriptors.
7. Extract the core library API, then make CLI a thin adapter over it.
8. Introduce explicit graph objects and shared-resource contexts in the core API.
9. Rewrite combined `vertfrag` handling and land T05.

## 11. Short conclusion

The immediate refactor should be test-first in a narrow sense: not by freezing the whole architecture before edits, but by placing targeted regression tests around the current failure-prone behavior. The first wave should focus on deterministic, small, synthetic correctness cases. Those tests will make the later parser and execution-plan refactors substantially safer.
