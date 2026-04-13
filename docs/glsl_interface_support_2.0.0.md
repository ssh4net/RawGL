# RawGL 2.0.0 GLSL Interface Support

This matrix describes what RawGL 2.0.0 currently supports in the shader-driven core API and CLI path.

It is based on the current implementation in:

- [rawgl_core.cpp](/mnt/w/VisualStudio/RawGL/SRC/RawGL/src/rawgl_core.cpp)
- [sequence.cpp](/mnt/w/VisualStudio/RawGL/SRC/RawGL/src/sequence.cpp)
- [gl_program.cpp](/mnt/w/VisualStudio/RawGL/SRC/RawGL/src/intern/gl_program.cpp)

## Summary

| Type / structure | Current status | Notes | Gap closure path |
| --- | --- | --- | --- |
| Scalar numeric uniforms | Supported | `bool`, `int`, `uint`, `float`, `double` | N/A |
| Vector numeric uniforms | Supported | `bvec*`, `ivec*`, `uvec*`, `vec*`, `dvec*` | N/A |
| Matrix numeric uniforms | Supported | `mat*`, `mat*x*`, `dmat*`, `dmat*x*` | N/A |
| `sampler2D` inputs | Supported | File-backed or pass-output-backed | N/A |
| `image2D` inputs | Supported | Read-only image binding for compute inputs | N/A |
| Fragment outputs | Supported | Named `out` variables | N/A |
| Compute image outputs | Supported | `image2D` outputs matched through uniforms | N/A |
| Atomic counters | Supported | Reflection, initialization, execution | N/A |
| Engine system uniforms | Supported | Engine-owned, not user-bindable | N/A |
| SSBO/buffer variables reflection | Reflected only | No public binding path yet | Core/runtime work required |
| UBO binding | Not supported | No public binding path | Core/runtime work required |
| Sampler types other than `sampler2D` | Not supported | Reflected type names may exist, but binding path rejects them | Core/runtime work required |
| Image types other than `image2D` | Not supported | Same limitation as samplers | Core/runtime work required |
| Uniform arrays | Partially / effectively unsupported | Reflection exists, but typed input path does not model array element counts cleanly | Small core/model work plus CLI/front-end support |
| Sampler arrays / image arrays | Not supported | No binding path | Core/runtime work required |
| Mesh attributes as shader-driven inputs | Partially supported | Mesh data uploads exist, but attribute binding is fixed-function, not interface-driven | Core/runtime work required |

## Detailed Matrix

| Type / structure | Supported | Extent | How value is provided | Notes | Gap closure path |
| --- | --- | --- | --- | --- | --- |
| `bool` | Yes | Full scalar | CLI/core numeric input | Stored through integer path | N/A |
| `int` | Yes | Full scalar | CLI/core numeric input | | N/A |
| `uint` | Yes | Full scalar | CLI/core numeric input | | N/A |
| `float` | Yes | Full scalar | CLI/core numeric input | | N/A |
| `double` | Yes | Full scalar | CLI/core numeric input | | N/A |
| `bvec2/3/4` | Yes | Full vector | CLI/core numeric input | | N/A |
| `ivec2/3/4` | Yes | Full vector | CLI/core numeric input | | N/A |
| `uvec2/3/4` | Yes | Full vector | CLI/core numeric input | | N/A |
| `vec2/3/4` | Yes | Full vector | CLI/core numeric input | | N/A |
| `dvec2/3/4` | Yes | Full vector | CLI/core numeric input | | N/A |
| `mat2`, `mat3`, `mat4` | Yes | Full matrix | CLI/core numeric input | Flattened numeric values in column-major upload path | N/A |
| `mat2x3`, `mat2x4`, `mat3x2`, `mat3x4`, `mat4x2`, `mat4x3` | Yes | Full matrix | CLI/core numeric input | | N/A |
| `dmat2`, `dmat3`, `dmat4` | Yes | Full matrix | CLI/core numeric input | | N/A |
| `dmat2x3`, `dmat2x4`, `dmat3x2`, `dmat3x4`, `dmat4x2`, `dmat4x3` | Yes | Full matrix | CLI/core numeric input | | N/A |
| `sampler2D` | Yes | Single texture input | File path or `name::passIndex` | Texture attributes supported: filter/wrap | N/A |
| `image2D` input | Yes | Single image input | File path or `name::passIndex` | Bound as read-only image | N/A |
| Fragment `out vec*` | Yes | Single named output or reflected array base name | `--out` / graph outputs | Output arrays are only lightly supported | Small core/model work plus CLI/front-end support for full array elements |
| Compute `layout(... ) uniform image2D outName` | Yes | Single named output image | `--out` / graph outputs | Bound as write-only image | N/A |
| `atomic_uint` | Yes | Reflected and initialized | `--atomic cntr ...` / graph atomic counters | Works without SSBO presence | N/A |
| `iFBsize` | Yes | Engine-owned | Internal | Matches framebuffer size; accepted as `ivec2`, `uvec2`, `vec2`, `dvec2` | N/A |
| `iFBaspect` | Yes | Engine-owned | Internal | Accepted as `float` or `double` | N/A |
| `isQuad` | Yes | Engine-owned | Internal | Accepted as `bool`, `int`, `uint`, `float`, or `double` | N/A |
| `iTime` | Yes | Engine-owned | `GraphExecutionRequest.systemUniforms.timeSeconds` | Accepted as `float` or `double` | N/A |
| `iTimeDelta` | Yes | Engine-owned | `GraphExecutionRequest.systemUniforms.deltaTimeSeconds` | Accepted as `float` or `double` | N/A |
| `iFrame` | Yes | Engine-owned | `GraphExecutionRequest.systemUniforms.frameNumber` | Accepted as `int`, `uint`, `float`, or `double` | N/A |
| `iPassIndex` | Yes | Engine-owned | `GraphExecutionRequest.systemUniforms.passIndex` | Accepted as `int`, `uint`, `float`, or `double` | N/A |
| SSBO variables | No public binding | Reflection only | N/A | Reflected into `bufferVariables`, but there is no graph/CLI binding API | Core/runtime work required |
| UBO variables | No | N/A | N/A | Not exposed by current graph model | Core/runtime work required |
| `sampler1D`, `sampler3D`, `samplerCube`, `sampler2DArray`, `sampler2DMS`, etc. | No | Rejected by binding path | N/A | Type names are known in GL type mapping, but the input path only accepts `sampler2D` | Core/runtime work required |
| `image1D`, `image3D`, `image2DArray`, `image2DMS`, etc. | No | Rejected by binding path | N/A | Same limitation | Core/runtime work required |
| Uniform arrays | No practical full support | Reflection may expose them, but input sizing is not array-aware | N/A | Do not rely on uniform arrays yet | Small core/model work plus CLI/front-end support |
| Sampler arrays | No | N/A | N/A | Not modeled in graph input path | Core/runtime work required |
| Image arrays | No | N/A | N/A | Not modeled in graph input path | Core/runtime work required |
| Atomic counter arrays | No clear public support | Single-name counters only | N/A | Not modeled in graph API | Core/runtime work required |

## Current Behavioral Limits

1. Only `sampler2D` and `image2D` are accepted as user-provided texture/image inputs.
2. System uniforms are reserved names. User graph inputs with those names are rejected.
3. Reflection is richer than binding support. RawGL can identify more GLSL types than it can currently bind.
4. SSBOs are reflected but not yet part of the public graph input model.
5. Uniform arrays and sampler/image arrays should be treated as unsupported until a dedicated array-aware model exists.
6. Mesh data is not yet described through the same shader-interface contract as uniforms and textures.

## Recommended User Guidance

- Use scalar/vector/matrix numeric uniforms freely.
- Use `sampler2D` and `image2D` for shader-driven external image inputs today.
- Use named fragment outputs or compute `image2D` outputs for pass export and pass chaining.
- Use `atomic_uint` when needed, but avoid array-style counter designs for now.
- Do not rely on UBOs, SSBO inputs, sampler arrays, image arrays, or general uniform arrays yet.
- Reserve `iFBsize`, `iFBaspect`, `isQuad`, `iTime`, `iTimeDelta`, `iFrame`, and `iPassIndex` for engine-owned values.
