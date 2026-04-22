#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path

import rawgl


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def _find_entry(entries, *names):
    for entry in entries:
        if entry.name in names:
            return entry
    return None


def main() -> int:
    input_path = Path("tests/inputs/sky.jpg")
    output_path = Path("tests/outputs/rawgl_python_metadata_smoke.tif")
    output_path.unlink(missing_ok=True)

    image = rawgl.io.load_image(input_path)
    rawgl.io.save_image(image, output_path, bits=16)

    entries = rawgl.io.read_metadata(
        output_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    if not entries:
        return fail("rawgl.io.read_metadata() returned no metadata entries")

    width_entry = _find_entry(entries, "ImageWidth")
    if width_entry is None or width_entry.value_text != str(image.width):
        return fail("rawgl.io.read_metadata() did not export ImageWidth correctly")

    height_entry = _find_entry(entries, "ImageLength", "ImageHeight")
    if height_entry is None or height_entry.value_text != str(image.height):
        return fail("rawgl.io.read_metadata() did not export image height correctly")

    datetime_entry = _find_entry(entries, "DateTime", "ModifyDate")
    if datetime_entry is None or not datetime_entry.value_text:
        return fail("rawgl.io.read_metadata() did not export a date/time entry")

    document = rawgl.io.read_metadata_document(
        output_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    if not document.fields:
        return fail("rawgl.io.read_metadata_document() returned no fields")

    io_runtime = rawgl.IoRuntime()
    request = rawgl.MetadataReadRequest()
    request.path = str(output_path)
    request.name_style = rawgl.MetadataNameStyle.oiio
    request.name_policy = rawgl.MetadataNamePolicy.exif_tool_alias
    result = io_runtime.read_metadata_file(request)
    if not result.success:
        return fail(f"IoRuntime.read_metadata_file() failed: {result.error_message}")
    if not result.entries:
        return fail("IoRuntime.read_metadata_file() returned no metadata entries")

    if getattr(rawgl.io, "MetadataReadRequest", None) is None:
        return fail("rawgl.io does not expose MetadataReadRequest")
    if getattr(rawgl.io, "MetadataDocumentReadRequest", None) is None:
        return fail("rawgl.io does not expose MetadataDocumentReadRequest")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
