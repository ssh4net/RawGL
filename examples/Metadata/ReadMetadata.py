#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

import rawgl


def main() -> None:
    source_path = Path(__file__).resolve().parents[2] / "tests/inputs/sky.jpg"
    output_dir = Path(__file__).resolve().parent

    image = rawgl.io.load_image(source_path)

    entries = rawgl.io.read_metadata(
        source_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    document = rawgl.io.read_metadata_document(
        source_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )

    outputs = [
        (output_dir / "ReadMetadata_transfer_python.tif", 16),
        (output_dir / "ReadMetadata_transfer_python.jpg", 8),
        (output_dir / "ReadMetadata_transfer_python.png", 8),
        (output_dir / "ReadMetadata_transfer_python.exr", 16),
    ]
    for path, bits in outputs:
        rawgl.io.save_image(image, path, bits=bits, source_metadata=document)

    print(f"Read {len(entries)} metadata entries from {source_path}")
    print(f"Read {len(document.fields)} typed metadata fields from {source_path}")
    for entry in entries[:12]:
        print(f"{entry.name}: {entry.value_text}")
    for path, _bits in outputs:
        transferred = rawgl.io.read_metadata(
            path,
            name_style=rawgl.MetadataNameStyle.oiio,
            name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
        )
        print(f"Wrote {path} with {len(transferred)} transferred metadata entries")


if __name__ == "__main__":
    main()
