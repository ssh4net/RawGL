#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

import rawgl


def main() -> None:
    source_path = Path("tests/inputs/EmptyPresetLUT.png")
    output_path = Path("examples/Metadata/ReadMetadata_python.tif")
    copied_path = Path("examples/Metadata/ReadMetadata_copy_python.tif")
    explicit_path = Path("examples/Metadata/ReadMetadata_explicit_python.tif")

    image = rawgl.io.load_image(source_path)
    rawgl.io.save_image(image, output_path, bits=16)

    entries = rawgl.io.read_metadata(
        output_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    document = rawgl.io.read_metadata_document(
        output_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )

    rawgl.io.save_image(
        image,
        copied_path,
        bits=16,
        metadata_mode=rawgl.MetadataTransferMode.copy_source,
        source_metadata=document,
    )

    explicit_document = rawgl.MetadataDocument()
    software_field = rawgl.MetadataField()
    software_field.key_kind = rawgl.MetadataKeyKind.exif_tag
    software_field.name = "Software"
    software_field.value.kind = rawgl.MetadataValueKind.text
    software_field.value.text_encoding = rawgl.MetadataTextEncoding.utf8
    software_bytes = b"RawGL metadata example"
    software_field.value.count = len(software_bytes)
    software_field.value.bytes = software_bytes
    explicit_document.fields = [software_field]

    rawgl.io.save_image(
        image,
        explicit_path,
        bits=16,
        metadata_mode=rawgl.MetadataTransferMode.explicit_only,
        explicit_metadata=explicit_document,
    )

    print(f"Read {len(entries)} metadata entries from {output_path}")
    print(f"Read {len(document.fields)} typed metadata fields from {output_path}")
    for entry in entries[:12]:
        print(f"{entry.name}: {entry.value_text}")
    print(f"Copied metadata to {copied_path}")
    print(f"Wrote explicit metadata to {explicit_path}")


if __name__ == "__main__":
    main()
