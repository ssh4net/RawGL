# RawGL Metadata Read and Native-Write Plan

Date: 2026-04-22

Purpose: define the current metadata boundary in `rawgl_io` after removing the
OIIO-attribute-based transfer path, and describe the native-write direction
needed later.

Status update, 2026-04-27:

- RawGL now exposes explicit OpenMeta-backed source metadata transfer through
  `TransferImageMetadataFile(...)` and Python `save_image(...,
  source_metadata=document)`.
- Current target coverage is JPEG, TIFF, PNG, and EXR.
- OpenMeta types still do not leak into RawGL public headers.
- The older statement that save helpers do not preserve source metadata is now
  superseded for the explicit `source_metadata` path.

Related notes:

- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)
- [rawgl_native_metadata_write_jpeg_tiff_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_native_metadata_write_jpeg_tiff_2026-04-22.md)

## 1. Current state

Current public metadata support is file-oriented:

- `rawgl::io::ReadMetadataFile(...)`
- `rawgl::io::IoRuntime::readMetadataFile(...)`
- `rawgl::io::ReadMetadataDocumentFile(...)`
- `rawgl::io::IoRuntime::readMetadataDocumentFile(...)`
- `rawgl::io::TransferImageMetadataFile(...)`
- `rawgl::io::IoRuntime::transferImageMetadataFile(...)`
- Python:
  - `rawgl.io.read_metadata(...)`
  - `rawgl.io.read_metadata_document(...)`
  - `rawgl.io.transfer_image_metadata(...)`
  - `rawgl.io.save_image(..., source_metadata=document)`
  - `rawgl.MetadataReadRequest`
  - `rawgl.IoRuntime.read_metadata_file(...)`
  - `rawgl.MetadataDocumentReadRequest`
  - `rawgl.IoRuntime.read_metadata_document_file(...)`

Current exported types:

- `MetadataEntry`
- `MetadataValue`
- `MetadataField`
- `MetadataDocument`
Current strengths:

- RawGL owns the public type names
- OpenMeta types do not leak into public headers
- the read path is already useful for EXIF/XMP/IPTC/ICC/BMFF/JUMBF-style inspection

Current limitation:

- save helpers preserve source metadata only when explicitly passed a
  `MetadataDocument`
- there are no native format-family metadata writers yet
- there are no helper APIs for in-place metadata mutation
- there are no selective removal helpers yet

## 2. Boundary rule

This should stay true:

- `rawgl_core` remains metadata-free
- `HostImageData` remains image-payload-only
- metadata lives in `rawgl_io`

Reason:

- metadata is file/container state, not shader execution state
- keeping metadata out of `HostImageData` avoids dragging file semantics into the memory-first execution layer
- Vulkan and future backends do not benefit from mixing metadata into the execution payload model

## 3. Public model kept for future native writers

Before adding write support again, RawGL needs to keep a typed metadata model
that is separate from the current read-preview type.

Recommended split:

- `MetadataEntry`
  - keep as the current read-preview/export type
  - useful for diagnostics, printing, scripting, and light inspection

- `MetadataValue`
  - new typed write-capable payload
  - owns one of:
    - scalar
    - scalar array
    - bytes
    - text

- `MetadataField`
  - one editable metadata item
  - fields:
    - `keyKind`
    - `name`
    - `elementType`
    - `textEncoding`
    - `value`

- `MetadataDocument`
  - ordered collection of `MetadataField`
  - represents one transferable metadata set

This keeps the read-preview surface simple while preserving a RawGL-owned typed
model for future native writers.

## 4. Why the transfer path was removed

The removed transfer path flattened metadata into OIIO-style string
attributes before write.

That was not a usable long-term model because it:

- lost native container structure too early
- made EXIF/IPTC/XMP/ICC all look like one flat attribute map
- depended on writer-specific reinterpretation of those attributes
- produced misleading results for metadata preservation

So the current API no longer claims metadata-preserving save.

## 5. Load-side API shape

Do not force every `LoadImageFile(...)` call to decode metadata by default.

Image load and metadata inspection should stay separable.

Reason:

- some pipelines only need pixels
- some metadata families are expensive or irrelevant for the main path
- batch workloads may want explicit control over when metadata is touched

Recommended future option:

- optional metadata capture on image load through a separate request flag or a parallel helper result

## 6. Name model

For writes, names should not rely on the current preview/export alias settings.

Recommended rule:

- write-side `MetadataField::name` uses one stable canonical form
- export/inspection helpers can still present aliases such as OIIO-style names

This avoids ambiguity where two aliasing schemes refer to the same underlying field.

So:

- read preview:
  - configurable name style and alias policy
- write/edit:
  - canonical field identity

## 7. Phase plan

### Phase 1

Already done:

- read-only metadata inspection
- RawGL-owned preview types
- Python binding for metadata read

### Phase 2

Done:

- add `MetadataValue`
- add `MetadataField`
- add `MetadataDocument`
- keep typed metadata reads in `rawgl_io`
- remove the OIIO-attribute-based metadata transfer path

### Phase 3

After that:

- native metadata write per format family
- passthrough of native blocks where source and destination support them
- explicit metadata mutation helpers
- selective removal helpers

## 8. Non-goals for the next step

The next metadata step should not try to do all of these at once:

- full metadata editor UI model
- schema validation for every metadata family
- write support for every container family on day one
- embedding metadata into `rawgl_core`
- exposing OpenMeta-native objects in the public API

The next step should not reintroduce metadata transfer through generic OIIO
attributes. The next usable write path has to be native per format family.
