# RawGL Metadata Write and Transfer Design

Date: 2026-04-22

Purpose: define the metadata write and transfer model in `rawgl_io`, without letting OpenMeta types become the public API.

Related notes:

- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)

## 1. Current state

Current public metadata support is file-oriented and includes both preview
inspection and typed transfer:

- `rawgl::io::ReadMetadataFile(...)`
- `rawgl::io::IoRuntime::readMetadataFile(...)`
- `rawgl::io::ReadMetadataDocumentFile(...)`
- `rawgl::io::IoRuntime::readMetadataDocumentFile(...)`
- metadata-aware `SaveImageFile(...)` through `ImageSaveRequest`
- Python:
  - `rawgl.io.read_metadata(...)`
  - `rawgl.io.read_metadata_document(...)`
  - `rawgl.io.save_image(..., metadata_mode=...)`
  - `rawgl.MetadataReadRequest`
  - `rawgl.IoRuntime.read_metadata_file(...)`
  - `rawgl.MetadataDocumentReadRequest`
  - `rawgl.IoRuntime.read_metadata_document_file(...)`

Current exported types:

- `MetadataEntry`
- `MetadataValue`
- `MetadataField`
- `MetadataDocument`
- `MetadataTransferMode`

Current strengths:

- RawGL owns the public type names
- OpenMeta types do not leak into public headers
- the read path is already useful for EXIF/XMP/IPTC/ICC/BMFF/JUMBF-style inspection

Current limitation:

- `MetadataEntry` is still a read-preview type
- there are no helper APIs for in-place metadata mutation
- there are no selective removal helpers yet
- source/load integration is still explicit instead of automatic

## 2. Boundary rule

This should stay true:

- `rawgl_core` remains metadata-free
- `HostImageData` remains image-payload-only
- metadata lives in `rawgl_io`

Reason:

- metadata is file/container state, not shader execution state
- keeping metadata out of `HostImageData` avoids dragging file semantics into the memory-first execution layer
- Vulkan and future backends do not benefit from mixing metadata into the execution payload model

## 3. Public model needed for writes

Before adding write support, RawGL needs a typed metadata model that is separate from the current read-preview type.

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

This keeps the read-preview surface simple while giving writes a non-lossy public model.

## 4. Transfer model

Write support should not be only "save these explicit fields".

Real workflows need these policies:

- `none`
  - write the image without metadata transfer

- `copy_source`
  - preserve metadata from a source document when possible

- `explicit_only`
  - write only the explicitly supplied metadata fields

- `merge_source_and_explicit`
  - start from source metadata, then override or add explicit fields

Recommended public enum:

- `MetadataTransferMode`

Recommended initial values:

- `none`
- `copy_source`
- `explicit_only`
- `merge_source_and_explicit`

## 5. Save-side API shape

Do not add a completely separate metadata writer API first.

The useful first step is to extend `ImageSaveRequest`, because metadata writes are usually part of saving an image file.

Recommended future fields on `ImageSaveRequest`:

- `MetadataTransferMode metadataMode`
- `std::shared_ptr<const MetadataDocument> sourceMetadata`
- `std::shared_ptr<const MetadataDocument> explicitMetadata`

Meaning:

- `sourceMetadata`
  - metadata carried from an earlier load or explicit read
- `explicitMetadata`
  - metadata the caller wants to add, override, or replace

This keeps the save path explicit without forcing metadata into `HostImageData`.

## 6. Load-side API shape

Do not force every `LoadImageFile(...)` call to decode metadata by default.

Image load and metadata inspection should stay separable.

Reason:

- some pipelines only need pixels
- some metadata families are expensive or irrelevant for the main path
- batch workloads may want explicit control over when metadata is touched

Recommended future option:

- optional metadata capture on image load through a separate request flag or a parallel helper result

But the first write-capable transfer path does not require that. The caller can already do:

1. `LoadImageFile(...)`
2. `ReadMetadataFile(...)`
3. `SaveImageFile(...)` with transfer policy

That is enough to productize the first round-trip path.

## 7. Name model

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

## 8. Phase plan

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
- extend `ImageSaveRequest` with transfer policy and metadata pointers
- implement metadata-preserving save in `rawgl_io`

### Phase 3

After that:

- explicit metadata mutation helpers
- selective removal helpers
- better source/load integration for round-trip workflows

## 9. Non-goals for the next step

The next metadata step should not try to do all of these at once:

- full metadata editor UI model
- schema validation for every metadata family
- write support for every container family on day one
- embedding metadata into `rawgl_core`
- exposing OpenMeta-native objects in the public API

The next step only needs to make metadata round-trip and explicit override possible through `rawgl_io`.
