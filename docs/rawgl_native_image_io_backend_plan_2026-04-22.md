# RawGL Native Image IO Backend Plan

Date: 2026-04-22

Purpose: define the internal `rawgl_io` backend boundary for format-family image
decode and encode, and stage the move away from the current OIIO-first image
path.

Related notes:

- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)
- [rawgl_metadata_write_and_transfer_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_metadata_write_and_transfer_2026-04-22.md)
- [rawgl_native_metadata_write_jpeg_tiff_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_native_metadata_write_jpeg_tiff_2026-04-22.md)

Reference project:

- HDRView uses a central image loader plus per-format codec modules. That is a
  good structural reference for RawGL:
  <https://github.com/wkjarosz/hdrview>

## 1. Problem

The current `rawgl_io` image path still has the wrong dependency shape:

- [src/io/texture_loader.cpp](/mnt/w/VisualStudio/RawGL/src/io/texture_loader.cpp)
  depends on OIIO pixel types through [src/io/image_io.h](/mnt/w/VisualStudio/RawGL/src/io/image_io.h)
- [src/io/output_writer.cpp](/mnt/w/VisualStudio/RawGL/src/io/output_writer.cpp)
  also depends on OIIO pixel and writer types through the same layer
- [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp) still
  combines extension handling, format policy, decode, and OIIO-specific type
  decisions in one file

That blocks the next format-family work:

- JPEG via libjpeg-turbo
- TIFF via libtiff
- PNG via libpng
- OpenEXR via OpenEXR

It also keeps save/load options too generic. The current `Attribute` list is
usable for compatibility, but it is not the right long-term way to expose full
format control.

## 2. First-wave scope

The first native image IO wave should cover:

- JPEG
- TIFF
- PNG
- OpenEXR

Deferred:

- RAW / DNG
- JPEG 2000
- JPEG XL
- HTJ2K
- WebP and other lower-priority families

Rule:

- do not try to replace OIIO for every format at once
- land the seam first
- then replace one format family at a time

## 3. Design rules

1. Keep public `rawgl_io` stable while the backend cut lands.
   The active public surface remains:
   - `ImageLoadRequest`
   - `ImageLoadResult`
   - `ImageSaveRequest`
   - `ImageSaveResult`

2. Move OIIO types out of the loader/writer seam immediately.
   `texture_loader.cpp` and `output_writer.cpp` should depend only on
   RawGL-owned image types.

3. Keep OIIO as a temporary fallback backend, not the long-term source of
   truth.
   Native format-family backends should replace it incrementally.

4. Keep native metadata handling separate from generic image attributes.
   JPEG/TIFF native metadata write must target native packets, not generic OIIO
   attributes.

5. Do not expose per-format option structs publicly until at least one native
   backend is proven.
   The internal model should prepare for them now.

## 4. Internal shape

The intended internal shape is:

- central dispatch layer:
  - [src/io/image_backend.h](/mnt/w/VisualStudio/RawGL/src/io/image_backend.h)
  - [src/io/image_backend.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_backend.cpp)
- per-format-family implementations:
  - `jpeg_backend.*`
  - `png_backend.*`
  - `tiff_backend.*`
  - `exr_backend.*`
- transitional fallback implementation:
  - current OIIO-backed path through [src/io/image_io.cpp](/mnt/w/VisualStudio/RawGL/src/io/image_io.cpp)

The central dispatch layer owns RawGL-owned internal types:

- `ImageCodecFamily`
- `ImageComponentType`
- `DecodedImageData`
- `ImageEncodeSettings`

Rule:

- backend implementations may use library-native types
- code above the backend layer must not

## 5. Option model direction

Current public requests still use generic `Attribute` lists.

That remains acceptable as a compatibility layer, but it should not be the
long-term control surface for native codecs. The long-term direction is
per-format option structs, for example:

- `JpegLoadOptions`
- `JpegSaveOptions`
- `PngLoadOptions`
- `PngSaveOptions`
- `TiffLoadOptions`
- `TiffSaveOptions`
- `ExrLoadOptions`
- `ExrSaveOptions`

That lets RawGL expose real format controls directly:

- JPEG quality, progressive mode, chroma subsampling, lossless JPEG paths when
  supported, ICC packet handling
  - future extension: high-bit-depth JPEG via newer `libjpeg-turbo`
    (`12-bit` lossy/lossless and `16-bit` lossless where the deployed library
    version supports it)
- PNG bit depth, interlace, compression, filter policy, gamma/CICP/sRGB policy
- TIFF compression, tiling/strip layout, predictor, and forced BigTIFF mode
  are in. Planar layout, broader sample-format coverage, and richer TIFF
  policy are still future work.
- OpenEXR compression, half/float policy, multipart/channel handling, line
  order. One-level tiled read/write is in; mip/ripmap and multipart are still
  future work.

## 6. Migration phases

### Phase 1: land the seam

Acceptance:

- `texture_loader.cpp` no longer depends on OIIO types
- `output_writer.cpp` no longer depends on OIIO types
- central format-family dispatch exists
- OIIO remains the backend implementation for all families

### Phase 2: native readers

Priority:

1. PNG
2. JPEG
3. TIFF
4. OpenEXR

Acceptance:

- each family has a native decode implementation
- file detection and backend selection are explicit
- source-native metadata capture can hook in at the family layer

### Phase 3: native writers

Priority:

1. TIFF
2. JPEG
3. PNG
4. OpenEXR

Acceptance:

- each family has a native encode implementation
- output option resolution is RawGL-owned
- JPEG/TIFF metadata passthrough can target native packets

### Phase 4: retire OIIO fallback for covered families

Acceptance:

- JPEG/TIFF/PNG/OpenEXR paths do not require OIIO
- OIIO remains only for uncovered families or is removed entirely when the
  remaining coverage is no longer needed

### Phase 5: second-wave formats

Add:

- RAW / DNG
- JPEG 2000
- JPEG XL
- HTJ2K

## 7. Immediate next coding steps

1. Keep the new `image_backend.*` layer as the only seam used by:
   - [src/io/texture_loader.cpp](/mnt/w/VisualStudio/RawGL/src/io/texture_loader.cpp)
   - [src/io/output_writer.cpp](/mnt/w/VisualStudio/RawGL/src/io/output_writer.cpp)

2. Add native PNG read/write first.
   It is the simplest common format to validate the architecture.

3. Add native JPEG read/write next, with explicit quality and progressive
   control.
   High-bit-depth JPEG should stay a follow-up item after the baseline `8-bit`
   path is stable.

4. Add native TIFF write in parallel with the JPEG/TIFF metadata work.

5. Add OpenEXR read/write once the common pixel-type path is stable.

## 8. Success criteria

This plan is successful when:

- image IO above the backend seam uses RawGL-owned types only
- one format family can switch to a native backend without touching unrelated
  call sites
- RawGL, not OIIO, owns output-setting selection for supported families
- native metadata write for JPEG/TIFF can plug into the image writer without
  reintroducing a generic attribute bridge
