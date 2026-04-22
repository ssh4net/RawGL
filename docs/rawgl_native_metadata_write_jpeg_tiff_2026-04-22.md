# RawGL Native Metadata Write Plan for JPEG and TIFF

Date: 2026-04-22

Purpose: define the first usable native metadata write path in `rawgl_io`
without reintroducing the removed OIIO-attribute bridge.

Related notes:

- [rawgl_metadata_write_and_transfer_2026-04-22.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_metadata_write_and_transfer_2026-04-22.md)
- [rawgl_module_split_2026-04-17.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_module_split_2026-04-17.md)
- [rawgl_public_api_redesign_2026-04-16.md](/mnt/w/VisualStudio/RawGL/docs/rawgl_public_api_redesign_2026-04-16.md)

## 1. Scope

This plan is intentionally narrow.

First native metadata write support should target:

- JPEG
- TIFF

First metadata families to care about:

- EXIF
- XMP
- ICC

IPTC may be included only if the implementation path is clean inside the same
family-specific writer. It is not required for the first landing.

Not in first scope:

- PNG metadata write
- OpenEXR attribute write
- BMFF / JUMBF / HEIF / JPEG XL metadata write
- public metadata mutation helpers
- a generic cross-format metadata transfer feature

## 2. Principle

The first usable path is:

- preserve native packets when possible
- rebuild native packets only when needed

It is not:

- flatten to generic attributes
- ask the image writer to reinterpret those attributes

So the internal write pipeline should stay packet- and family-aware until the
final format-specific writer step.

## 3. Public API rule

Do not reintroduce save-side metadata transfer into the public API yet.

For now:

- keep `MetadataEntry`
- keep `MetadataValue`
- keep `MetadataField`
- keep `MetadataDocument`
- keep metadata reads public
- keep metadata writes internal-only until JPEG/TIFF native write is proven

Reason:

- the public API should not claim metadata-preserving save before the native
  write path actually exists
- a second rollback would be unnecessary churn

## 4. Internal model needed

`MetadataDocumentStorage` should stop being thought of as “OpenMeta ownership”
and instead become the internal carrier for source-native packets and future
write planning.

Recommended internal concepts:

- `NativeMetadataFamily`
  - `Exif`
  - `Xmp`
  - `Iptc`
  - `Icc`

- `NativeMetadataPacket`
  - `family`
  - `sourceFormat`
  - `bytes`
  - `isTrustedNativePacket`

- `NativeMetadataSnapshot`
  - ordered set of packets captured from the source file
  - optional normalized index for quick family lookup

- `MetadataDocumentStorage`
  - source `MetaStore` when useful for inspection
  - optional `NativeMetadataSnapshot`

This stays internal to `rawgl_io`.

## 5. First write policy model

The first native writer needs explicit loss policy even before it becomes
public.

Recommended internal enum:

- `MetadataLossPolicy::BestEffort`
- `MetadataLossPolicy::WarnOnDrop`
- `MetadataLossPolicy::FailOnDrop`

Meaning:

- `BestEffort`
  - write whatever supported native families can be written
  - silently skip unsupported families internally unless logging/debug hooks say otherwise

- `WarnOnDrop`
  - same write behavior, but return a structured warning list

- `FailOnDrop`
  - reject the save if a requested native family cannot be preserved or rebuilt

This should be internal at first. Public exposure can come later.

## 6. First writer modes

The first implementation should support two internal modes only:

### 6.1 Native passthrough

Use when:

- source file was read through `rawgl_io`
- source-native packets were captured
- destination is JPEG or TIFF
- no explicit metadata edits were applied

Behavior:

- copy EXIF/XMP/ICC packets forward where legal for the destination format
- preserve packet bytes as much as possible

This is the highest-value first step.

### 6.2 Rebuild from semantic model

Use later when:

- metadata was edited
- metadata was merged
- source and destination require container rebuild

This should not be the first implementation target.

## 7. Minimal internal API shape

Keep this inside `src/io` for now.

Recommended minimal API:

```cpp
enum class NativeMetadataTargetFormat : uint8_t {
    Jpeg,
    Tiff,
};

enum class MetadataLossPolicy : uint8_t {
    BestEffort,
    WarnOnDrop,
    FailOnDrop,
};

struct NativeMetadataWriteRequest {
    NativeMetadataTargetFormat targetFormat;
    const MetadataDocument* document = nullptr;
    const MetadataDocumentStorage* storage = nullptr;
    MetadataLossPolicy lossPolicy = MetadataLossPolicy::BestEffort;
};

struct NativeMetadataWriteResult {
    bool success = false;
    std::string errorMessage;
    std::vector<std::string> warnings;
    std::vector<NativeMetadataPacket> packets;
};

NativeMetadataWriteResult
build_native_metadata_for_jpeg_or_tiff(const NativeMetadataWriteRequest& request);
```

Notes:

- `document` is the semantic view
- `storage` is the source-native cache
- result packets stay family-tagged until the final JPEG/TIFF writer step

No virtual backend layer is needed yet for the first landing.
One direct JPEG/TIFF path is enough.

## 8. JPEG/TIFF writer boundary

The actual image writer step should receive:

- pixel payload
- output format
- native metadata packets by family

Not:

- generic attribute strings

Recommended writer-facing shape:

```cpp
struct JpegTiffWriteRequest {
    std::string path;
    const HostImageData* image = nullptr;
    int bits = 16;
    int alphaChannel = -1;
    std::vector<Attribute> attributes;
    std::span<const NativeMetadataPacket> metadataPackets;
};
```

This keeps packet writing a final format-specific step instead of a mid-pipeline
flattening step.

## 9. Read-side prerequisite

The first write-capable JPEG/TIFF path depends on source-native packet capture
during metadata read.

That means the next read-side extension should be:

- when reading JPEG/TIFF metadata, capture native EXIF/XMP/ICC packets into
  `MetadataDocumentStorage`
- keep the public read API unchanged

This is the real first implementation task.

## 10. Acceptance for the first landing

The first landing is good enough when:

- metadata read still works exactly as today
- no public save-side metadata API is reintroduced yet
- JPEG/TIFF read captures native EXIF/XMP/ICC packets internally
- one internal JPEG/TIFF write helper can emit source-native packets without
  flattening to OIIO attributes
- the implementation clearly reports packet families that could not be preserved

## 11. Priority after this note

Near-term priority:

1. internal native packet capture on JPEG/TIFF read
2. internal JPEG/TIFF native packet write helper
3. validation fixtures for passthrough preservation
4. only then consider reintroducing a public metadata-preserving save API

OpenMeta remains useful for:

- metadata inspection
- typed document extraction
- future native-adapter integration

But it should not drive the immediate roadmap ahead of the native JPEG/TIFF
write boundary.
