#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys

import numpy as np
import rawgl


_GL_UNSIGNED_SHORT = 0x1403
_GL_HALF_FLOAT = 0x140B


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def make_jpeg_load_options() -> rawgl.ImageCodecLoadOptions:
    jpeg = rawgl.io.JpegLoadOptions()
    jpeg.has_color_transform = True
    jpeg.color_transform = rawgl.io.JpegLoadColorTransform.rgb

    codec = rawgl.io.ImageCodecLoadOptions()
    codec.has_backend_policy = True
    codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
    codec.has_jpeg = True
    codec.jpeg = jpeg
    return codec


def downsample_rgb_u8(image: rawgl.HostImageData, max_edge: int) -> np.ndarray:
    array = rawgl.host_image_to_array(image)
    if array.ndim != 3 or array.shape[2] < 3 or array.dtype != np.uint8:
        raise RuntimeError(f"unexpected sky.jpg host layout: shape={array.shape}, dtype={array.dtype}")
    rgb = array[:, :, :3]
    step = max(1, int(np.ceil(max(rgb.shape[:2]) / float(max_edge))))
    return np.ascontiguousarray(rgb[::step, ::step, :])


def make_png_save_options() -> rawgl.ImageCodecSaveOptions:
    png = rawgl.io.PngSaveOptions()
    png.has_compression_level = True
    png.compression_level = 1
    png.has_interlaced = True
    png.interlaced = False

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_png = True
    codec.png = png
    return codec


def make_tiff_save_options() -> rawgl.ImageCodecSaveOptions:
    tiff = rawgl.io.TiffSaveOptions()
    tiff.has_compression = True
    tiff.compression = rawgl.io.TiffCompressionMode.deflate
    tiff.has_predictor = True
    tiff.predictor = rawgl.io.TiffPredictorMode.horizontal
    tiff.has_layout = True
    tiff.layout = rawgl.io.TiffStorageLayout.tiled
    tiff.has_tile_width = True
    tiff.tile_width = 64
    tiff.has_tile_height = True
    tiff.tile_height = 64

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_tiff = True
    codec.tiff = tiff
    return codec


def make_openexr_save_options() -> rawgl.ImageCodecSaveOptions:
    openexr = rawgl.io.OpenExrSaveOptions()
    openexr.has_compression = True
    openexr.compression = rawgl.io.OpenExrCompressionMode.zip
    openexr.has_layout = True
    openexr.layout = rawgl.io.OpenExrStorageLayout.tiled
    openexr.has_tile_width = True
    openexr.tile_width = 64
    openexr.has_tile_height = True
    openexr.tile_height = 64
    openexr.has_line_order = True
    openexr.line_order = rawgl.io.OpenExrLineOrder.increasing_y

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_openexr = True
    codec.openexr = openexr
    return codec


def make_native_png_load_options() -> rawgl.ImageCodecLoadOptions:
    codec = rawgl.io.ImageCodecLoadOptions()
    codec.has_backend_policy = True
    codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
    codec.has_png = True
    return codec


def make_native_tiff_load_options() -> rawgl.ImageCodecLoadOptions:
    tiff = rawgl.io.TiffLoadOptions()
    tiff.has_directory_index = True
    tiff.directory_index = 0

    codec = rawgl.io.ImageCodecLoadOptions()
    codec.has_backend_policy = True
    codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
    codec.has_tiff = True
    codec.tiff = tiff
    return codec


def make_native_openexr_load_options() -> rawgl.ImageCodecLoadOptions:
    openexr = rawgl.io.OpenExrLoadOptions()
    openexr.has_channel_selection = True
    openexr.channel_selection = rawgl.io.OpenExrChannelSelection.rgb

    codec = rawgl.io.ImageCodecLoadOptions()
    codec.has_backend_policy = True
    codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
    codec.has_openexr = True
    codec.openexr = openexr
    return codec


def verify_output(path: Path, codec_options: rawgl.ImageCodecLoadOptions, source: np.ndarray, expected_gl_type: int) -> int:
    loaded = rawgl.io.load_image(path, codec_options=codec_options)
    if loaded.width != source.shape[1] or loaded.height != source.shape[0] or loaded.channels != source.shape[2]:
        return fail(f"{path.name} reloaded shape is unexpected: {loaded.width}x{loaded.height}x{loaded.channels}")
    if int(loaded.gl_type) != expected_gl_type:
        return fail(f"{path.name} reloaded GL type is unexpected: {loaded.gl_type}")
    if len(loaded.bytes) == 0:
        return fail(f"{path.name} reloaded payload is empty")
    return 0


def main() -> int:
    source_path = Path("tests/inputs/sky.jpg")
    source = rawgl.io.load_image(source_path, codec_options=make_jpeg_load_options())
    rgb = downsample_rgb_u8(source, 640)

    outputs = [
        (Path("tests/outputs/rawgl_python_sky_codec_conversion.png"), 16, make_png_save_options(), make_native_png_load_options(), _GL_UNSIGNED_SHORT),
        (Path("tests/outputs/rawgl_python_sky_codec_conversion.tif"), 16, make_tiff_save_options(), make_native_tiff_load_options(), _GL_UNSIGNED_SHORT),
        (Path("tests/outputs/rawgl_python_sky_codec_conversion.exr"), 16, make_openexr_save_options(), make_native_openexr_load_options(), _GL_HALF_FLOAT),
    ]

    for path, bits, save_options, load_options, expected_gl_type in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.unlink(missing_ok=True)
        rawgl.io.save_image(rgb, path, bits=bits, codec_options=save_options)
        status = verify_output(path, load_options, rgb, expected_gl_type)
        if status != 0:
            return status

    print("rawgl_python_sky_codec_conversion_smoke:done", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
