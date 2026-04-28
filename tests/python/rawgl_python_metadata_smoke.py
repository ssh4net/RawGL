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
    exr_output_path = Path("tests/outputs/rawgl_python_metadata_smoke.exr")
    output_path.unlink(missing_ok=True)
    exr_output_path.unlink(missing_ok=True)

    image = rawgl.io.load_image(input_path)

    entries = rawgl.io.read_metadata(
        input_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    if not entries:
        return fail("rawgl.io.read_metadata() returned no metadata entries")

    make_entry = _find_entry(entries, "Make", "Exif:Make")
    if make_entry is None or make_entry.value_text != "Canon":
        return fail("rawgl.io.read_metadata() did not export Make correctly")

    model_entry = _find_entry(entries, "Model", "Exif:Model")
    if model_entry is None or not model_entry.value_text:
        return fail("rawgl.io.read_metadata() did not export Model correctly")

    datetime_entry = _find_entry(entries, "DateTimeOriginal", "Exif:DateTimeOriginal")
    if datetime_entry is None or not datetime_entry.value_text:
        return fail("rawgl.io.read_metadata() did not export a date/time entry")

    document = rawgl.io.read_metadata_document(
        input_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    if not document.fields:
        return fail("rawgl.io.read_metadata_document() returned no fields")

    rawgl.io.save_image(image, output_path, bits=16, source_metadata=document)
    output_entries = rawgl.io.read_metadata(
        output_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    if not output_entries:
        return fail("rawgl.io.read_metadata() returned no metadata entries for saved TIFF")

    output_width_entry = _find_entry(output_entries, "ImageWidth")
    if output_width_entry is None or output_width_entry.value_text != str(image.width):
        return fail("saved TIFF metadata did not export ImageWidth correctly")

    output_height_entry = _find_entry(output_entries, "ImageLength", "ImageHeight")
    if output_height_entry is None or output_height_entry.value_text != str(image.height):
        return fail("saved TIFF metadata did not export image height correctly")

    output_make_entry = _find_entry(output_entries, "Make", "Exif:Make")
    if output_make_entry is None or output_make_entry.value_text != make_entry.value_text:
        return fail("transferred TIFF metadata did not preserve Make")

    output_datetime_entry = _find_entry(output_entries, "DateTimeOriginal", "Exif:DateTimeOriginal")
    if output_datetime_entry is None or output_datetime_entry.value_text != datetime_entry.value_text:
        return fail("transferred TIFF metadata did not preserve DateTimeOriginal")

    rawgl.io.save_image(image, exr_output_path, bits=16, source_metadata=document)
    exr_entries = rawgl.io.read_metadata(
        exr_output_path,
        name_style=rawgl.MetadataNameStyle.oiio,
        name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
    )
    if not exr_entries:
        return fail("rawgl.io.read_metadata() returned no metadata entries for saved EXR")

    exr_line_order_entry = _find_entry(exr_entries, "openexr:lineOrder")
    if exr_line_order_entry is None or exr_line_order_entry.value_text != "0":
        return fail("saved EXR metadata did not export openexr:lineOrder correctly")

    exr_make_entry = _find_entry(exr_entries, "openexr:Make", "Make")
    if exr_make_entry is None or exr_make_entry.value_text != make_entry.value_text:
        return fail("transferred EXR metadata did not preserve Make")

    io_runtime = rawgl.IoRuntime()
    request = rawgl.MetadataReadRequest()
    request.path = str(input_path)
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
    if getattr(rawgl.io, "ImageMetadataTransferRequest", None) is None:
        return fail("rawgl.io does not expose ImageMetadataTransferRequest")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
