#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import time

import numpy as np
import rawgl


@dataclass(frozen=True, slots=True)
class PipelineConfig:
    max_edge: int = 640
    saturation: float = 1.08
    contrast: float = 1.06
    gamma_value: float = 1.04


FRAGMENT_SHADER = """#version 450 core
layout(binding = 0) uniform sampler2D u_src0;
layout(location = 0) uniform float saturation;
layout(location = 1) uniform float contrast;
layout(location = 2) uniform float gamma_value;
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 OutImage;

void main()
{
    vec3 source = texture(u_src0, UV).rgb;
    float luma = dot(source, vec3(0.2126, 0.7152, 0.0722));
    vec3 graded = mix(vec3(luma), source, saturation);
    graded = clamp((graded - 0.5) * contrast + 0.5, 0.0, 1.0);
    graded = pow(graded, vec3(1.0 / max(gamma_value, 1e-6)));
    OutImage = vec4(graded, 1.0);
}
"""


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _output_dir() -> Path:
    override = os.environ.get("RAWGL_NUMPY_TYPED_IO_OUTPUT_DIR")
    path = Path(override) if override else Path(__file__).resolve().parent
    path.mkdir(parents=True, exist_ok=True)
    return path


def _make_jpeg_load_options() -> rawgl.ImageCodecLoadOptions:
    jpeg = rawgl.io.JpegLoadOptions()
    jpeg.has_color_transform = True
    jpeg.color_transform = rawgl.io.JpegLoadColorTransform.rgb

    codec = rawgl.io.ImageCodecLoadOptions()
    codec.has_backend_policy = True
    codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
    codec.has_jpeg = True
    codec.jpeg = jpeg
    return codec


def _make_png_save_options() -> rawgl.ImageCodecSaveOptions:
    png = rawgl.io.PngSaveOptions()
    png.has_compression_level = True
    png.compression_level = 3

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_png = True
    codec.png = png
    return codec


def _make_tiff_save_options() -> rawgl.ImageCodecSaveOptions:
    tiff = rawgl.io.TiffSaveOptions()
    tiff.has_compression = True
    tiff.compression = rawgl.io.TiffCompressionMode.deflate
    tiff.has_predictor = True
    tiff.predictor = rawgl.io.TiffPredictorMode.horizontal
    tiff.has_layout = True
    tiff.layout = rawgl.io.TiffStorageLayout.tiled
    tiff.has_tile_width = True
    tiff.tile_width = 128
    tiff.has_tile_height = True
    tiff.tile_height = 128

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_tiff = True
    codec.tiff = tiff
    return codec


def _make_exr_save_options() -> rawgl.ImageCodecSaveOptions:
    exr = rawgl.io.OpenExrSaveOptions()
    exr.has_compression = True
    exr.compression = rawgl.io.OpenExrCompressionMode.zip
    exr.has_layout = True
    exr.layout = rawgl.io.OpenExrStorageLayout.tiled
    exr.has_tile_width = True
    exr.tile_width = 64
    exr.has_tile_height = True
    exr.tile_height = 64

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_openexr = True
    codec.openexr = exr
    return codec


def _downsample_to_max_edge(rgb: np.ndarray, max_edge: int) -> np.ndarray:
    """Return a contiguous HxWx3 view/copy with max(width, height) <= max_edge."""

    height, width = rgb.shape[:2]
    step = max(1, int(np.ceil(max(height, width) / float(max_edge))))
    return np.ascontiguousarray(rgb[::step, ::step, :])


def _host_image_to_float_rgb(image: rawgl.HostImageData, max_edge: int) -> np.ndarray:
    """Convert RawGL HostImageData to contiguous float32 RGB in the 0..1 range."""

    array = rawgl.host_image_to_array(image)
    if array.ndim == 2:
        array = array[:, :, np.newaxis]
    if array.shape[2] == 1:
        array = np.repeat(array, 3, axis=2)
    else:
        array = array[:, :, :3]

    array = _downsample_to_max_edge(array, max_edge)
    if array.dtype == np.uint8:
        rgb = array.astype(np.float32) * (1.0 / 255.0)
    elif array.dtype == np.uint16:
        rgb = array.astype(np.float32) * (1.0 / 65535.0)
    else:
        rgb = array.astype(np.float32, copy=False)
    return np.ascontiguousarray(np.clip(rgb, 0.0, 1.0))


def _numpy_preprocess(rgb: np.ndarray) -> np.ndarray:
    """Apply vectorized Python-side preparation before handing pixels to RawGL."""

    flat = rgb.reshape((-1, 3))
    low = np.percentile(flat, 1.0, axis=0).astype(np.float32)
    high = np.percentile(flat, 99.0, axis=0).astype(np.float32)
    scale = np.maximum(high - low, 1e-6)

    normalized = np.clip((rgb - low) / scale, 0.0, 1.0).astype(np.float32, copy=False)
    height, width = normalized.shape[:2]
    yy, xx = np.ogrid[-1.0:1.0:complex(height), -1.0:1.0:complex(width)]
    radius2 = (xx * xx + yy * yy).astype(np.float32, copy=False)
    vignette = np.clip(1.0 - 0.22 * radius2, 0.78, 1.0).astype(np.float32, copy=False)

    prepared = np.empty_like(normalized)
    np.multiply(normalized, vignette[:, :, np.newaxis], out=prepared)
    np.clip(prepared, 0.0, 1.0, out=prepared)
    return np.ascontiguousarray(prepared)


def _run_rawgl_pass(source: np.ndarray, config: PipelineConfig) -> rawgl.HostImageData:
    result = rawgl.image(
        FRAGMENT_SHADER,
        size=(int(source.shape[1]), int(source.shape[0])),
        inputs={
            "u_src0": source,
            "saturation": config.saturation,
            "contrast": config.contrast,
            "gamma_value": config.gamma_value,
        },
        output={
            "format": "rgba32f",
            "channels": 4,
            "alpha_channel": 3,
            "bits": 16,
            "capture_to_host": True,
        },
        verbosity=0,
    )
    if not result.success:
        raise RuntimeError(result.error_message)
    return result.captured_outputs["OutImage::0"]


def _save_outputs(image: rawgl.HostImageData, output_dir: Path) -> list[Path]:
    output_base = output_dir / "NumpyTypedIoPipeline_python"
    outputs = [
        (output_base.with_suffix(".png"), 16, _make_png_save_options()),
        (output_base.with_suffix(".tif"), 16, _make_tiff_save_options()),
        (output_base.with_suffix(".exr"), 16, _make_exr_save_options()),
    ]

    written: list[Path] = []
    for path, bits, codec_options in outputs:
        path.unlink(missing_ok=True)
        rawgl.io.save_image(image, path, bits=bits, codec_options=codec_options)
        written.append(path)
    return written


def main() -> int:
    config = PipelineConfig()
    input_path = _repo_root() / "tests/inputs/sky.jpg"
    output_dir = _output_dir()

    t0 = time.perf_counter()
    loaded = rawgl.io.load_image(input_path, codec_options=_make_jpeg_load_options())
    rgb = _host_image_to_float_rgb(loaded, config.max_edge)
    prepared = _numpy_preprocess(rgb)
    t1 = time.perf_counter()

    output_image = _run_rawgl_pass(prepared, config)
    output_array = rawgl.host_image_to_array(output_image)
    t2 = time.perf_counter()

    written = _save_outputs(output_image, output_dir)
    t3 = time.perf_counter()

    print(f"input: {input_path}")
    print(f"numpy source: shape={prepared.shape}, dtype={prepared.dtype}")
    print(f"rawgl output: shape={output_array.shape}, dtype={output_array.dtype}")
    print(f"output range: {float(output_array[..., :3].min()):.4f} .. {float(output_array[..., :3].max()):.4f}")
    print(f"numpy prepare: {(t1 - t0) * 1000.0:.2f} ms")
    print(f"rawgl pass: {(t2 - t1) * 1000.0:.2f} ms")
    print(f"typed saves: {(t3 - t2) * 1000.0:.2f} ms")
    for path in written:
        print(path.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
