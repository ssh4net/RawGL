# RawGL Type-Support and Python Binding Plan

Date: 2026-04-15

Purpose: define the concrete API and model changes needed for:

- Wave 1: array-aware and Python-useful growth on top of the current stable core
- Wave 2: general resource-type expansion for non-`2D` textures/images and buffer-backed resources

This document is a planning note only. It does not change the current supported feature set described in [glsl_interface_support_2.0.0.md](/mnt/w/VisualStudio/RawGL/docs/glsl_interface_support_2.0.0.md).

Related architectural note:

- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)
- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_batch_design_2026-04-18.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_batch_design_2026-04-18.md)

## 1. Current status

Current overall progress is good enough to start planning bindings, but the public API still needs one more boundary cleanup before broad binding exposure:

- The public API exists in [rawgl_core.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl_core.h), but it is still partially shaped like internal graph/runtime plumbing.
- The active CLI path already runs through the core graph API in [context.cpp](/mnt/w/VisualStudio/RawGL/src/core/context.cpp), [cli_graph.cpp](/mnt/w/VisualStudio/RawGL/src/cli/cli_graph.cpp), and [sequence.cpp](/mnt/w/VisualStudio/RawGL/src/runtime/sequence.cpp).
- `Sequence` is runtime-only, which means Python and GUI frontends no longer have to inherit CLI construction logic.

What is still missing for a useful binding and for broader GLSL type support:

- no array-aware public model for uniforms, outputs, or atomic counters
- no public in-memory image upload/download path
- no public source-module model for file-backed GLSL, in-memory GLSL text, and optional SPIR-V byte blobs
- no public model for sampler/image dimensions beyond `2D`
- no public model for SSBO or UBO binding
- no pass-to-pass subresource model for arrays, layers, mip levels, or slices

## 2. Current API limits that matter

The current public API in [rawgl_core.h](/mnt/w/VisualStudio/RawGL/include/rawgl/rawgl_core.h) has three main limitations:

1. `GraphInputSourceKind` only distinguishes numeric values, file-backed textures, pass-output references, and persistent graph textures.
   It does not describe texture kind, image kind, buffer kind, or host-memory uploads.

2. `ShaderResourceInfo` reports names, GL types, and a small amount of binding metadata.
   It does not explicitly model:
   - resource class
   - array length
   - texture/image dimensionality
   - image access mode
   - writable vs sampled usage

3. Pass-to-pass transfer is name-only.
   The current model can reference `name::passIndex`, but it cannot identify:
   - array element
   - texture layer
   - mip level
   - 3D slice / z range

4. Shader program input is path-oriented.
   That is adequate for CLI, but not for Python or generated pipelines.
   The public model should also allow:
   - GLSL source strings supplied from memory
   - optional SPIR-V bytecode blobs supplied from memory

## 3. Design rules

The next two waves should follow these rules:

- Keep the current stable `sampler2D` / `image2D` path working without forcing new syntax.
- Extend the public API by adding explicit descriptors, not by adding more string conventions.
- Do not make Python depend on CLI parsing.
- Allow Python and other frontends to provide optional data-lifetime hints so the core can reuse stable inputs across repeated executions and batches.
- Make source ownership explicit: file path, GLSL text, and optional SPIR-V bytes should be represented structurally rather than hidden behind temporary files.
- Make Wave 1 small enough that it can land before broad backend-neutral resource work.
- Make Wave 2 the point where sampler/image/buffer resource classes become truly general.

## 4. Wave 1: array-aware and Python-useful expansion

Wave 1 should be the smallest feature set that:

- fixes the most visible “missing types” pain
- makes a first Python binding meaningfully useful
- does not require a full rewrite of the resource model

### 4.1 Public API changes

Add an explicit shader source/module input record for inspection and pass definitions:

- `enum class ShaderModuleSourceKind`
  Values for the first step:
  `file_path`, `glsl_text`, `spirv_binary`
- `struct ShaderModuleDefinition`
  Fields:
  - stage/program role
  - source kind
  - file path
  - in-memory GLSL text
  - owned byte blob for SPIR-V
  - optional debug label / virtual name

Wave 1 should fully support:

- file-backed GLSL
- in-memory GLSL source text

Wave 1 may expose SPIR-V byte blobs in the public API, but execution support should remain conditional on backend/driver capability.

Rule:

- Python must not need to write temporary shader files just to use generated GLSL.
- CLI may remain file-oriented, but should translate into the same structured module definition.

Add explicit reflection metadata to `ShaderResourceInfo`:

- `enum class ShaderResourceClass`
  Values:
  `uniform_numeric`, `sampler`, `image`, `output`, `atomic_counter`, `buffer_variable`, `system_uniform`
- `size_t arrayLength = 1`
- `bool isArray = false`
- `int vectorWidth = 1`
- `int matrixColumns = 1`
- `int matrixRows = 1`

Add explicit image/sampler shape metadata:

- `enum class ShaderTextureShape`
  Values for Wave 1:
  `unknown`, `tex_2d`

This keeps Wave 1 small while creating the field that Wave 2 will extend.

### 4.2 Graph input/output model changes

Keep `GraphInputDefinition` and `GraphOutputDefinition`, but add exact element addressing:

- `size_t arrayElement = 0`
- `bool usesArrayElement = false`

Meaning:

- `name = "weights", usesArrayElement = false` means whole resource or scalar resource
- `name = "weights", usesArrayElement = true, arrayElement = 3` means `weights[3]`

Do the same for `GraphAtomicCounterDefinition`:

- `size_t arrayElement = 0`
- `bool usesArrayElement = false`

This is preferable to encoding array elements into the string name because:

- reflection can keep `baseName` stable
- Python bindings can populate fields structurally
- CLI can still map `weights[3]` syntax into these fields later

### 4.3 Reflection behavior changes

Reflection in [program.cpp](/mnt/w/VisualStudio/RawGL/src/gl/program.cpp) should stop collapsing array resources into one ambiguous base-name-only path.

Wave 1 target behavior:

- scalar/vector/matrix uniforms remain unchanged
- uniform arrays report:
  - base resource name
  - `isArray = true`
  - `arrayLength`
- fragment output arrays report:
  - base output name
  - `isArray = true`
  - `arrayLength`
- atomic counter arrays report:
  - base counter name
  - `isArray = true`
  - `arrayLength`

Wave 1 does not need full sampler/image array execution support yet.

### 4.4 Runtime behavior changes

Wave 1 execution support should cover:

- numeric uniform arrays
  - provided as contiguous numeric values through the existing numeric vectors
  - validated against reflected `arrayLength`
- explicit fragment output array element selection
  - exact output element export and pass reference
- atomic counter array element initialization/readback
  - only if the shader interface can map array elements cleanly

Wave 1 should still reject:

- sampler arrays
- image arrays
- non-`2D` textures/images
- SSBO/UBO binding

### 4.5 Python usefulness requirement

A Python binding that only passes file paths is not enough. Wave 1 should therefore also add one host-memory image path:

- new `GraphInputSourceKind::hostTexture`

Add a host image payload record:

- `struct HostImageData`
  Fields:
  - `int width`
  - `int height`
  - `int channels`
  - `int alphaChannel`
  - `unsigned int glInternalFormat`
  - `unsigned int glType`
  - `std::vector<std::byte>` or equivalent owned byte storage

Then extend `GraphInputDefinition` and `GraphInputOverride` with:

- `std::shared_ptr<HostImageData> hostTexture`
- optional `ResourceLifetimeHint lifetimeHint`

Wave 1 output side should also add optional host readback:

- `bool captureToHost = false`

and a matching result payload in `GraphExecutionResult`:

- `std::map<std::string, HostImageData>` or a similarly named output-image map

That is the minimum needed for Python to pass image data in memory instead of round-tripping through disk.

Lifetime hint intent for Wave 1:

- the first Python binding should be able to mark host textures and similar stable inputs as:
  - one-shot
  - reusable for the prepared workflow
  - reusable across a larger batch/session

This does not require exposing internal cache implementation details.
It only gives the core a stronger signal that repeated uploads or rebuilds are avoidable.

### 4.6 Python binding scope after Wave 1

Once Wave 1 lands, the first Python binding can reasonably expose:

- `Session`
- shader inspection
- workflow preparation
- prepared workflow execution
- numeric inputs and overrides
- file-backed textures
- host-memory 2D textures
- pass-to-pass output references
- host-memory capture of selected outputs

This is enough for:

- scripting
- batch execution
- notebook-based experimentation
- image pipelines driven from NumPy-style host buffers

without waiting for the full generalized resource model.

### 4.7 Representative Python-first atomic workloads

One good early Python-facing validation case is image statistics built around atomic counters.

Recommended examples:

- histogram accumulation into atomic counters
- min/max tracking where the shader writes integer-compatible values into dedicated counters
- average estimation through sum + count counters, or through a second host-side reduction step after histogram capture

Notes:

- Histogram is the cleanest direct atomic-counter use case for the first Python binding.
- Min/max/avg should not be described as "free" atomic-counter features.
  In practice they need either:
  - additional counters and carefully chosen integer encodings, or
  - a second reduction step on the host after GPU-side accumulation
- This makes them good integration tests for:
  - host-memory image input
  - graph execution
  - atomic counter initialization
  - atomic counter readback
  - Python-side interpretation of structured results

Rule:

- Keep the first Python atomic examples small and deterministic.
- Prefer 1D grayscale or small integer-valued inputs for the first histogram/statistics tests.

## 5. Wave 2: generalized resource model

Wave 2 is where the public API stops being “2D image pipeline with extras” and becomes a general shader-resource binding model.

### 5.1 Public API restructuring

Wave 2 should replace the current overloaded `GraphInputDefinition` with distinct binding descriptors:

- `GraphNumericInputBinding`
- `GraphSamplerBinding`
- `GraphImageBinding`
- `GraphBufferBinding`

and a pass definition that contains explicit lists of those classes.

Do the same for outputs:

- `GraphColorOutputBinding`
- `GraphImageOutputBinding`
- later, possibly `GraphBufferOutputBinding` if needed

### 5.2 Resource shape model

Extend `ShaderTextureShape` into a real resource-shape enum:

- `tex_1d`
- `tex_2d`
- `tex_3d`
- `tex_cube`
- `tex_1d_array`
- `tex_2d_array`
- `tex_2d_ms`
- `tex_2d_ms_array`
- `tex_buffer`
- `tex_rect`

Add shape fields to image/sampler bindings:

- `ShaderTextureShape shape`
- `int mipLevel`
- `int layer`
- `int layerCount`
- `int depthSlice`

### 5.3 Access model

Wave 2 must make image and buffer access explicit:

- `enum class ResourceAccessMode`
  Values:
  `read_only`, `write_only`, `read_write`

Use this for:

- image uniforms
- SSBO bindings
- UBO usage metadata where relevant

### 5.4 Pass-to-pass transfer model

Replace the current `name::passIndex`-only model with a structured reference:

- `struct GraphResourceReference`
  Fields:
  - `size_t passIndex`
  - `std::string resourceName`
  - `bool usesArrayElement`
  - `size_t arrayElement`
  - `int mipLevel`
  - `int layer`
  - `int depthSlice`

This is the key model needed for:

- image arrays
- 3D textures
- layered rendering
- more advanced compute pipelines

### 5.5 Buffer model

Wave 2 is also the right point to expose SSBO and UBO binding.

Needed public structures:

- `GraphBufferBinding`
  Fields:
  - `std::string name`
  - `ResourceAccessMode access`
  - `size_t byteOffset`
  - `size_t byteSize`
  - owned host data or shared buffer handle

Wave 2 should keep atomic counters separate from SSBO/UBO resources.

### 5.6 Python binding scope after Wave 2

After Wave 2, Python can expose:

- structured sampler/image/buffer bindings
- resource references with subresource selection
- array-aware resource addressing
- SSBO/UBO-backed compute workflows
- non-`2D` texture/image workflows

At that point the Python layer is not just a wrapper around the CLI-era image tool. It becomes a real programmable frontend over the engine core.

## 6. Recommended order

Recommended implementation order:

1. Finish current API naming cleanup and public-surface review.
2. Land Wave 1 reflection fields.
3. Land Wave 1 array-aware numeric/output support.
4. Land Wave 1 host-memory 2D input/output path.
5. Add first Python binding against that limited but useful API.
6. Then start Wave 2 generalized resource work.

## 7. Non-goals for the next immediate step

The next coding step should not try to do all of this at once.

Do not combine in one change:

- Python binding framework introduction
- host-memory image transport
- uniform array support
- sampler/image shape generalization
- SSBO/UBO public binding

The smallest useful implementation step after this design note is:

- add array-aware reflection metadata fields
- decide exact naming/addressing rules for array elements
- keep execution support limited to Wave 1 scope
