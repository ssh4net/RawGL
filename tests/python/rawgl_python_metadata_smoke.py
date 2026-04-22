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
    input_path = Path("tests/inputs/EmptyPresetLUT.png")
    output_path = Path("tests/outputs/rawgl_python_metadata_smoke.tif")
    copied_path = Path("tests/outputs/rawgl_python_metadata_copy_smoke.tif")
    explicit_path = Path("tests/outputs/rawgl_python_metadata_explicit_smoke.tif")
    output_path.unlink(missing_ok=True)
    copied_path.unlink(missing_ok=True)
    explicit_path.unlink(missing_ok=True)

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

    rawgl.io.save_image(
        image,
        copied_path,
        bits=16,
        metadata_mode=rawgl.MetadataTransferMode.copy_source,
        source_metadata=document,
    )
    copied_entries = rawgl.io.read_metadata(
        copied_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    copied_datetime = _find_entry(copied_entries, "DateTime", "ModifyDate")
    if copied_datetime is None or not copied_datetime.value_text:
        return fail("copy-source metadata save did not preserve a date/time entry")

    explicit_document = rawgl.MetadataDocument()
    software_field = rawgl.MetadataField()
    software_field.key_kind = rawgl.MetadataKeyKind.exif_tag
    software_field.name = "Software"
    software_field.value.kind = rawgl.MetadataValueKind.text
    software_field.value.text_encoding = rawgl.MetadataTextEncoding.utf8
    software_field.value.count = len(b"RawGL python metadata smoke")
    software_field.value.bytes = b"RawGL python metadata smoke"
    explicit_document.fields = [software_field]

    rawgl.io.save_image(
        image,
        explicit_path,
        bits=16,
        metadata_mode=rawgl.MetadataTransferMode.explicit_only,
        explicit_metadata=explicit_document,
    )
    explicit_entries = rawgl.io.read_metadata(
        explicit_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    software_entry = _find_entry(explicit_entries, "Software")
    if software_entry is None or software_entry.value_text != "RawGL python metadata smoke":
        return fail("explicit metadata save did not export Software correctly")

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
    if getattr(rawgl.io, "MetadataTransferMode", None) is None:
        return fail("rawgl.io does not expose MetadataTransferMode")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
