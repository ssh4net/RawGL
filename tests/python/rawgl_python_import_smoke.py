#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path

import rawgl


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def main() -> int:
    expected_version = Path("VERSION").read_text(encoding="utf-8").strip()
    if rawgl.__version__ != expected_version:
        return fail(f"unexpected rawgl.__version__: {rawgl.__version__}")

    status = rawgl.status()
    if not status.startswith("rawgl nanobind"):
        return fail(f"unexpected rawgl.status(): {status}")

    if not hasattr(rawgl, "Session"):
        return fail("core binding mode does not expose Session")
    if not hasattr(rawgl, "runtime_info"):
        return fail("core binding mode does not expose runtime_info()")
    if not hasattr(rawgl, "IoRuntime"):
        return fail("core binding mode does not expose IoRuntime")
    if not hasattr(rawgl, "BatchRunner"):
        return fail("core binding mode does not expose BatchRunner")
    if not hasattr(rawgl, "inspect_mesh_file"):
        return fail("core binding mode does not expose inspect_mesh_file()")
    mesh_info = rawgl.inspect_mesh_file("tests/inputs/fullscreen_triangle_material.obj")
    if not mesh_info.success:
        return fail(f"mesh inspection failed: {mesh_info.error_message}")
    if not hasattr(rawgl, "MetadataReadRequest"):
        return fail("core binding mode does not expose MetadataReadRequest")
    if not hasattr(rawgl, "MetadataDocument"):
        return fail("core binding mode does not expose MetadataDocument")
    if not hasattr(rawgl, "io"):
        return fail("core binding mode does not expose rawgl.io")
    if getattr(rawgl.io, "Runtime", None) is None:
        return fail("rawgl.io does not expose Runtime")
    if getattr(rawgl.io, "MetadataNameStyle", None) is None:
        return fail("rawgl.io does not expose MetadataNameStyle")
    if getattr(rawgl.io, "MetadataTransferSafety", None) is None:
        return fail("rawgl.io does not expose MetadataTransferSafety")
    if getattr(rawgl.io, "ImageIoCapabilities", None) is None:
        return fail("rawgl.io does not expose ImageIoCapabilities")
    if getattr(rawgl.io, "ImageCodecLoadOptions", None) is None:
        return fail("rawgl.io does not expose ImageCodecLoadOptions")
    load_options = rawgl.io.ImageCodecLoadOptions()
    load_options.has_backend_policy = True
    load_options.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
    jpeg_load_options = rawgl.io.JpegLoadOptions()
    jpeg_load_options.has_color_transform = True
    jpeg_load_options.color_transform = rawgl.io.JpegLoadColorTransform.rgb
    load_options.has_jpeg = True
    load_options.jpeg = jpeg_load_options
    load_request = rawgl.ImageLoadRequest()
    load_request.codec_options = load_options
    if load_request.codec_options.backend_policy != rawgl.io.ImageLoadBackendPolicy.native_only:
        return fail("ImageLoadRequest.codec_options assignment failed")
    if getattr(rawgl.io, "ImageCodecSaveOptions", None) is None:
        return fail("rawgl.io does not expose ImageCodecSaveOptions")
    codec_options = rawgl.io.ImageCodecSaveOptions()
    png_options = rawgl.io.PngSaveOptions()
    png_options.has_compression_level = True
    png_options.compression_level = 0
    codec_options.has_png = True
    codec_options.png = png_options
    request = rawgl.ImageSaveRequest()
    request.codec_options = codec_options
    if not request.codec_options.has_png:
        return fail("ImageSaveRequest.codec_options assignment failed")
    if not hasattr(rawgl.io, "read_metadata"):
        return fail("rawgl.io does not expose read_metadata()")
    if not hasattr(rawgl.io, "read_metadata_document"):
        return fail("rawgl.io does not expose read_metadata_document()")
    if not hasattr(rawgl.io, "capabilities"):
        return fail("rawgl.io does not expose capabilities()")
    capabilities = rawgl.io.capabilities()
    if not capabilities.open_image_io_fallback:
        return fail("rawgl.io.capabilities() did not report OpenImageIO fallback")
    codec_names = {codec.name for codec in capabilities.codecs}
    for codec_name in ("jpeg", "png", "tiff", "openexr", "openimageio"):
        if codec_name not in codec_names:
            return fail(f"rawgl.io.capabilities() missing codec: {codec_name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
