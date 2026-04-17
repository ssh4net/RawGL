# RawGL Code Review and Refactor Notes

Date: 2026-04-10

Purpose: build a working understanding of the current codebase, identify critical correctness and design issues, isolate unresolved shader parsing/reflection problems, and frame the next refactor steps toward the repository's HPC-C++ target.

Companion document: `docs/rawgl_refactor_backlog_and_test_plan_2026-04-10.md`

Note: file paths below were updated to the current `src/` layout. Line ranges remain the original review snapshot from 2026-04-10 and may drift as refactors continue.

## 1. Current feature set and code map

RawGL is a CLI-driven OpenGL processing pipeline. The current code supports:

- Multi-pass execution with either vertex+fragment passes or compute passes.
- Shader loading from separate GLSL files, combined `vertfrag` text files, and SPIR-V binaries.
- Texture inputs from disk via OpenImageIO/libraw, with per-input OIIO attributes and texture sampler state.
- Numeric inputs for scalar, vector, and matrix uniforms.
- Pass-to-pass texture reuse using the `name::passIndex` convention.
- Default quad rendering and external mesh loading for `.ply` and `.obj`.
- Output textures stored in GPU memory and optional export to disk through OpenImageIO.
- Partial reflection for uniforms, framebuffer outputs, atomic counters, and shader storage blocks.

Main code areas:

- `src/app/main.cpp`
  Entry point. Creates the hidden OpenGL context, initializes logging, constructs `Sequence`, and runs it.
- `src/runtime/sequence.cpp` and `src/runtime/sequence.h`
  Main orchestration layer. CLI parsing, pass construction, texture loading, mesh upload, FBO/output setup, execution, readback, and file output.
- `src/gl/program_manager.cpp`
  Shader/program loading and caching. Handles single-file `vertfrag`, separate shader files, and SPIR-V.
- `src/gl/program.cpp`
  OpenGL program reflection for uniforms, outputs, atomic counters, and SSBO metadata.
- `src/gl/texture.cpp`
  GPU texture creation and synchronous GPU-to-CPU readback.
- `src/runtime/pass_output.cpp`
  Output save path through OpenImageIO.
- `src/io/image_io.cpp`
  Disk image loading, RAW support, and OIIO attribute handling.
- `src/runtime/pass_input.cpp`
  Value parsing and string-to-GL enum dispatch for texture, mesh, and culling attributes.

## 2. Processing flow

The current processing flow is:

1. `main()` creates `OpenGLHandle`, initializes logging, constructs `Sequence(argc, argv)`, then calls `run()`.
2. `Sequence::Sequence()` parses CLI options into a linear stream of pass mutations. This is order-sensitive and relies on mutable state such as `currentPass`, `currentInput`, and `currentOutput`.
3. Each `--pass_vertfrag` or `--pass_comp` creates a `GLProgram` through `GLProgramManager`. Program reflection is performed immediately in `GLProgram`.
4. The constructor records pass-local configuration:
   - pass size
   - compute workgroup size
   - clear color
   - culling settings
   - mesh choice
   - texture and numeric inputs
   - atomic counter input values
   - outputs and file export attributes
5. `Sequence::initCommon()` performs initialization work:
   - asynchronously loads disk textures with `std::async`
   - creates `Texture` objects for input textures
   - resolves pass sizes
   - creates pass output textures and FBO attachments
   - resolves pass-to-pass texture references
   - loads mesh CPU data
   - uploads mesh buffers to GL
   - releases mesh CPU memory
6. `Sequence::run()` executes each pass:
   - binds the GL program
   - binds textures and uploads uniform values
   - writes internal uniforms such as `iFBsize`, `iFBaspect`, `isQuad`
   - rebuilds and binds atomic counter buffers
   - dispatches compute or renders geometry
   - does synchronous debug readback for atomic counters and a hardcoded SSBO path
   - calls `glFinish()`
   - saves any requested outputs to disk with GPU-to-CPU readback
7. `Sequence::~Sequence()` deletes framebuffers and mesh buffers, but not every temporary resource type tracked in the execution path.

## 3. Critical issues in current code and logic

The items below are the highest-value blockers for correctness and refactoring.

### 3.1 Atomic counter reflection is structurally broken when SSBOs are absent

Code: `src/gl/program.cpp:493-500`

- `compileBuffersList()` returns early when `!acount || !bcount`.
- `acount` is the number of atomic counter buffers.
- `bcount` is the number of shader storage blocks.
- This means a shader that uses atomic counters but does not use SSBOs clears `m_acounters` and reports no counters at all.

Impact:

- Atomic counters are not reliably discoverable.
- Any later execution logic using `findCounter()` becomes inconsistent or fails.
- Reflection currently conflates two independent resource classes: atomic counters and SSBOs.

Priority: critical

### 3.2 Compute image inputs are bound incorrectly

Code: `src/runtime/sequence.cpp:1675-1704`

- `GL_IMAGE_2D` inputs are handled through the same path as `GL_SAMPLER_2D`.
- The code binds them with `glActiveTexture()` and `glBindTexture()`.
- Image uniforms require `glBindImageTexture()` with the correct access mode and format, not sampler binding.

Impact:

- Compute shaders using `readonly image2D` or similar image inputs are not bound correctly.
- The program may appear to work for sampler inputs but silently fail for image inputs.

Priority: critical

### 3.3 Compute output reference wiring contains a hard bug

Code: `src/runtime/sequence.cpp:847-855`

- In the compute branch, `ref_output.uniform` is assigned with `findUniform(val_name)`.
- The next validation checks `if (!ref_output.output)` instead of `if (!ref_output.uniform)`.

Impact:

- Pass-to-pass references from compute outputs can fail even when the uniform exists.
- This is a direct correctness bug in inter-pass resource wiring.

Priority: critical

### 3.4 Execution path still contains hardcoded debug SSBO injection

Code: `src/runtime/sequence.cpp:1960-1975` and `src/runtime/sequence.cpp:2074-2089`

- Every graphics pass allocates a hardcoded SSBO named `AtBuf`.
- It is always bound to binding point `3`.
- It is read back and printed unconditionally after the draw call.

Impact:

- Binding collisions with real shader resources are likely.
- Graphics pass behavior depends on debug-only code still living in production flow.
- It adds avoidable allocations, synchronization, and state mutation per pass.

Priority: critical

### 3.5 Synchronization is too coarse and likely incorrect for several producer/consumer paths

Code: `src/runtime/sequence.cpp:1946-1949`, `src/runtime/sequence.cpp:2015-2043`, `src/runtime/sequence.cpp:2096`

- Compute passes use `glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT)` after image writes.
- The code then samples or exports textures in later operations, but the barrier does not clearly match the actual access pattern.
- The code also reads atomic counter and SSBO results without a clearly paired barrier for those buffer reads.
- `glFinish()` is called after every pass.

Impact:

- Inter-pass visibility rules are underspecified and likely wrong for some shaders.
- `glFinish()` serializes the GPU and destroys throughput.
- This is both a correctness risk and a severe performance limit.

Inference:

- Based on the code, the intended producer/consumer path is image store -> later sample/export. That typically needs a barrier strategy centered on shader image access plus the actual next consumer, not only `GL_TEXTURE_UPDATE_BARRIER_BIT`.

Priority: critical

### 3.6 Atomic counters are rebuilt inside the hot execution loop

Code: `src/runtime/sequence.cpp:1740-1902`

- Atomic counter grouping, buffer allocation, initialization, and binding happen in `run()`.
- The code re-creates GL buffer objects per pass execution instead of creating immutable execution state during initialization.

Impact:

- Unbounded dynamic work in the hot path.
- Difficult to reason about lifetime and pass determinism.
- Prevents clean future parallel scheduling because execution mutates global and pass-local resource maps.

Priority: critical

### 3.7 `cull` option parsing is wrong

Code: `src/runtime/sequence.cpp:741-775`

- `o.value[0]` already holds the first culling key.
- The code then copies `o.value.begin() + 1` into `val_arr`.
- The loop starts at `i = 1`, which skips the first key/value pair entirely.

Impact:

- `--cull wind ccw` style input does not parse as intended.
- Some culling options are silently ignored.

Priority: high

### 3.8 Unsigned uniforms are uploaded with the signed API in one overload

Code: `src/gl/program.cpp:88-103`

- `GLProgramUniform::set(GLuint value)` uses `glUniform1i()` instead of `glUniform1ui()`.

Impact:

- Wrong API call for `uint`, `uimage2D`, and unsigned sampler/image scalar bindings.
- May work accidentally for small values but is not correct OpenGL usage.

Priority: high

### 3.9 `Pass::GenericObject` leaks memory

Code: `src/runtime/sequence.h:247-278`

- `GenericObject` allocates `value` with `new T[size]`.
- The destructor is empty.

Impact:

- All `BObject` and `SSBObject` instances leak host memory.
- The current debug SSBO path makes this leak more visible.

Priority: high

### 3.10 Parser error handling can leave construction in a partially valid state

Code: `src/runtime/sequence.cpp:1041-1045`

- A missing numeric field inside the constructor logs an error and does `return;`.
- This returns from `Sequence::Sequence()` rather than aborting consistently.

Impact:

- The object may be left partially initialized.
- `main()` still calls `sequence.run()` afterward.

Priority: high

### 3.11 Output and readback path is fully synchronous and allocates per save

Code: `src/runtime/pass_output.cpp:31-143` and `src/gl/texture.cpp:172-208`

- `saveTexture()` always performs GPU-to-CPU readback through `Texture::getData()`.
- `getData()` allocates a fresh heap buffer with `malloc()`.
- Readback is synchronous via `glGetTexImage()`.

Impact:

- Export cost scales badly with pass count and output size.
- No staging reuse, no pipelining, no separation between GPU work and file IO.
- This is a major barrier to any HPC-style throughput design.

Priority: high

### 3.12 Execution logic is still mixed with diagnostics and process termination

Code: many locations, especially `src/runtime/sequence.cpp` and `src/gl/program.cpp`

- The hot path still logs heavily, prints directly to stdout, and uses `exit()` deeply inside helper logic.
- Several helper functions also throw exceptions while others call `exit()`.

Impact:

- No consistent error propagation model.
- Hard to test, hard to batch, hard to embed, hard to parallelize.

Priority: high

## 4. Unresolved issues in shader parsing and shader-facing resource logic

### 4.1 Combined `vertfrag` loading is not a real parser

Code: `src/gl/program_manager.cpp`

- Single-file vertex+fragment mode now strips `RAWGL_VERTEX_SHADER` / `RAWGL_FRAGMENT_SHADER` guarded sections into stage-specific source before compilation.

Current limitations:

- The source split is still guard-based rather than a full GLSL parser.
- `#extension` handling is only preserved as-written; there is still no include/dependency tracking layer around GLSL source loading.
- There is no `#line` remapping for error reporting.
- There is no include or dependency tracking.

Practical effect:

- Real-world GLSL that begins with `#version` is fragile in single-file mode.
- Diagnostics are harder to map back to the original source.

### 4.2 Shader reflection mixes atomic counters and SSBO reflection into one path

Code: `src/gl/program.cpp:484-742`

- `compileBuffersList()` tries to build atomic counter metadata and SSBO metadata in one function.
- Early return semantics and map-index reconstruction make the behavior fragile.

Specific weaknesses:

- The function returns early if either resource class is absent.
- Atomic counter matching relies on reconstructed multimap ordering instead of a direct resource model.
- Buffer layout assumptions are derived from sorted maps and offsets rather than a stable program resource representation.

Practical effect:

- Reflection is difficult to trust.
- Any future parser refactor should separate:
  - uniform reflection
  - framebuffer outputs
  - image uniforms
  - atomic counters
  - shader storage blocks

### 4.3 The CLI/resource layer only partially models the GLSL resource model

Code: mainly `src/runtime/sequence.cpp:781-1379`

Supported or semi-supported:

- scalar/vector/matrix uniforms
- `sampler2D`
- `image2D`
- fragment outputs
- compute output images wired through uniforms
- partial atomic counter initialization

Missing or incomplete:

- uniform arrays
- sampler arrays
- image arrays
- 3D textures and cube textures
- integer sampler families beyond the scalar upload path
- structured SSBO input/output declaration from CLI
- compute z-dimension workgroup size
- explicit read/write access modes for image uniforms

Practical effect:

- Shader authors are forced into a narrow subset of GLSL.
- The parser looks more complete than the actual runtime contract.

### 4.4 Pass dependency handling is implicit rather than validated

Code: `src/runtime/sequence.cpp:826-873`, `src/runtime/sequence.cpp:1496-1527`, `src/runtime/sequence.cpp:1611-1631`

- Pass-to-pass references are string-based and are resolved opportunistically.
- There is no explicit dependency graph or validation pass.

Risks:

- Resource resolution depends on construction order.
- Cycles are not modeled explicitly.
- Size references, output references, and texture references do not share one dependency system.

Practical effect:

- Refactoring is harder because behavior is encoded in string conventions and parse order.

### 4.5 Several shader-facing limitations are still visible in code comments or hardcoded behavior

Examples:

- `src/runtime/sequence.cpp:579-581`
  Pass size scaling and margin handling are still TODO.
- `src/runtime/sequence.cpp:1121-1126`
  `atomic buff` parsing is stubbed.
- `src/runtime/pass_input.cpp:452-484`
  Counter parameter parsing remains commented-out legacy code.
- `README.md:84-86`
  The project itself documents atomic/buffer support and uniform-array support as unfinished or weak.

## 5. HPC-C++ gap analysis

The repository already aims at GPU processing and some asynchronous preparation, but it is still far from the C++ HPC discipline described by the `cpp` skill.

### 5.1 What currently aligns with the target

- There is a usable split between setup-like work in `initCommon()` and execution work in `run()`.
- Passes and resources are already explicit enough that a staged refactor can preserve behavior.
- Texture loading is already moved out of the draw loop.
- The code uses direct OpenGL APIs rather than a high-level framework that would hide GPU work.

### 5.2 Main mismatches with the HPC-C++ target

- Exceptions are still used in parsing and initialization.
- `exit()` is used widely instead of explicit error returns.
- `std::shared_ptr`, `std::map`, `std::unordered_map`, `std::function`, `std::future`, and dynamic `std::vector` growth are common.
- The execution path still allocates and frees resources.
- Diagnostics are mixed into hot execution.
- Resource lifetime is not explicit enough to guarantee deterministic execution and teardown.
- Global mutable state remains, for example `g_glslProgramManager`.
- There is no clean immutable execution plan object.
- CPU-side readback/export is synchronous and directly coupled to pass execution.

### 5.3 What “HPC-C++” should mean here in practice

For this codebase, moving toward the HPC-C++ style should mean:

- Separate the lifecycle into:
  - parse
  - validate
  - build execution plan
  - allocate GPU resources
  - execute
  - export
  - teardown
- Make execution read-only with respect to pipeline topology.
- Move all GL object creation out of `run()`.
- Replace stringly-typed pass references with validated IDs and handles.
- Replace exception/`exit()` control flow with explicit result types.
- Bound or eliminate dynamic allocation in the hot path.
- Remove debug-only readback and logging from the steady-state execution path.
- Make resource descriptors explicit enough that later CPU-side parallel preparation is deterministic.

## 6. Recommended refactor direction

### Phase 1: stabilize correctness

- Split atomic counter reflection from SSBO reflection.
- Remove the hardcoded `AtBuf` path from graphics execution.
- Fix compute output reference validation.
- Fix `GL_IMAGE_2D` binding semantics.
- Fix the `cull` parser loop.
- Fix `GLuint` uniform upload to use the unsigned API.
- Replace constructor `return` error paths with explicit failure propagation.

### Phase 2: make the pipeline explicit

- Introduce a parsed command model that is independent of GL.
- Introduce an explicit shader interface model extracted from program reflection.
- Make shader-declared uniforms, images, outputs, atomic counters, and SSBOs the source of truth for CLI validation.
- Model built-in system uniforms explicitly as part of the engine contract rather than as scattered hardcoded special cases.
- Limit CLI declarations to external data sources, explicit overrides, and pass-level policy that cannot be inferred from shader code alone.
- Introduce a validated pass graph with typed resource references.
- Introduce a compiled execution-plan layer that resolves:
  - pass order
  - texture dependencies
  - output formats
  - mesh ownership
  - buffer bindings

### Phase 3: separate initialization from execution

- Build programs and reflect resources once.
- Allocate textures, FBOs, counter buffers, and mesh buffers once.
- Precompute all internal uniform locations once.
- Ensure `run()` performs only binding and dispatch/draw.

### Phase 4: extract a reusable engine library

- Move orchestration out of the process-oriented `main()` path and into a reusable core API.
- Make CLI a thin frontend over that core.
- Design the engine API so future GUI and Python `nanobind` bindings use the same build/execute path.
- Treat interface inspection, pass-graph build, execution, and export requests as explicit library operations.
- Ensure headless batch processing is a first-class use case of the same core engine.
- Model processing graphs explicitly rather than treating a single `Sequence` instance as the only engine unit.
- Support both:
  - independent graphs with isolated state
  - graphs that share an explicit resource context for cached programs, textures, meshes, and reusable allocations
- Keep resource sharing explicit in the API so graph lifetime and ownership remain deterministic.
- Carry animation-oriented system state such as time, frame number, pass index, and framebuffer data through explicit engine-side uniform/state objects.
- Keep the engine contract backend-neutral where practical so future non-OpenGL backends remain possible.

### Phase 4.1: preserve a path to future backends

- Continue shipping OpenGL as the active backend during the current refactor.
- Do not attempt a Vulkan rewrite during the current cleanup phase.
- Keep internal concepts such as shader source, shader interface, resource descriptors, execution plans, and context ownership backend-neutral where practical.
- Treat Vulkan as the primary future backend candidate for explicit device selection, stronger shader/pipeline cache control, and better long-term batch execution structure.
- Treat Metal as a possible later target through cross-compilation paths, not as an immediate implementation commitment.

### Phase 4.2: define the shared-resource model before async work

- Treat the following as first-class shared execution cases:
  - same graph, different uniforms, textures, or system uniform state across many runs
  - same graph, animated frame sequences driven by time, frame, and related built-ins
  - multi-pass graphs where one pass output becomes a later pass input
  - multiple different graphs sharing static assets such as LUTs, textures, meshes, and shader sources
  - repeated interactive rebuilds where only one shader or one pass changes
  - persistent graph-local state such as feedback textures, history buffers, or counters
- Make resource lifetime explicit:
  - per dispatch
  - per graph run
  - persistent per graph
  - shared per context
- Keep atomic counters separate from image load/store resources with atomic operations. They are different resource classes and should not collapse into one vague "atomic texture" abstraction.
- Separate reusable cache layers explicitly:
  - shader source cache
  - preprocessed shader cache
  - shader interface cache
  - compiled program cache
  - host-side decoded image cache
  - host-side parsed mesh cache
  - GPU resource cache
  - execution-plan cache
- Use content-sensitive cache keys where practical. Path-only cache keys are acceptable as a transitional step, but they are not the long-term batch model.
- Do not add async or parallel execution until these ownership, lifetime, and cache boundaries are explicit.

### Phase 5: make output and IO scalable

- Decouple GPU execution from file export.
- Introduce staging buffers or batched readback if export remains synchronous.
- Keep output requests as data, not side effects embedded in the pass loop.

### Phase 6: move closer to the HPC-C++ style

- Replace exception-based numeric parsing with `from_chars`-style parsing everywhere.
- Reduce `shared_ptr` and `map` usage in hot paths.
- Replace runtime string dispatch with indexed descriptors created during validation.
- Convert pass execution state into compact, immutable arrays of descriptors where practical.

## 7. Suggested refactor order

If the goal is safe modernization without losing current features, the best order is:

1. Fix the direct correctness bugs listed in section 3.
2. Replace the current parser state machine with a typed parse/validate layer.
3. Extract an explicit shader interface model from reflection and make it the primary validation contract.
4. Rebuild shader reflection around explicit resource classes that feed that interface model.
5. Remove all GL allocations from `run()`.
6. Extract a reusable core library and move CLI to a thin adapter layer.
7. Introduce explicit graph objects plus optional shared-resource contexts in that core API.
8. Define the shared-resource model, cache layers, and resource lifetimes explicitly.
9. Revisit async/parallel preparation only after the execution plan becomes deterministic and library boundaries are stable.
10. Keep the internal engine contract backend-neutral enough that Vulkan later, and possibly Metal after that, stay viable without rewriting the public API shape.

## 8. Short conclusion

RawGL already contains the foundations of a useful multi-pass GPU tool, but the current implementation mixes parsing, validation, resource creation, execution, debug instrumentation, and file export too tightly. The largest technical debt sits in `Sequence.cpp` and in the shader reflection/resource model in `GLProgram.cpp` and `GLProgramManager.cpp`.

The immediate refactor target should not be “make it parallel” first. It should be “make the pipeline explicit and deterministic first.” Once parsing, reflection, allocation, execution, and shared-resource ownership are separated, the code will be in a position where HPC-style C++ rules can be applied without fighting the current architecture. The same discipline also keeps a future Vulkan backend realistic, while avoiding a premature rewrite away from the currently working OpenGL path.
