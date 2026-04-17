#!/usr/bin/env python3

from pathlib import Path
import struct
import time

import numpy as np
import rawgl

try:
    import matplotlib.pyplot as plt
except ImportError:
    plt = None


input_path = Path(__file__).resolve().parents[2] / "tests/inputs/EmptyPresetLUT.png"


histogram_shader = """#version 450 core
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0) uniform sampler2D u_src0;
layout(binding = 0, offset = 0) uniform atomic_uint histogram[256];

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(u_src0, 0);
    if (coord.x >= size.x || coord.y >= size.y) {
        return;
    }

    vec3 rgb = texelFetch(u_src0, coord, 0).rgb;
    float gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    uint bin = min(uint(floor(clamp(gray, 0.0, 1.0) * 255.0 + 0.5)), 255u);
    atomicCounterIncrement(histogram[bin]);
}
"""


apply_equalization_shader = """#version 450 core
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0) uniform sampler2D u_src0;
layout(binding = 1) uniform sampler2D u_lut0;
layout(binding = 2, r32f) uniform writeonly image2D o_out0;

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(o_out0);
    if (coord.x >= size.x || coord.y >= size.y) {
        return;
    }

    vec3 rgb = texelFetch(u_src0, coord, 0).rgb;
    float gray = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    int bin = int(min(floor(clamp(gray, 0.0, 1.0) * 255.0 + 0.5), 255.0));
    float equalized = texelFetch(u_lut0, ivec2(bin, 0), 0).r;
    imageStore(o_out0, coord, vec4(equalized, 0.0, 0.0, 1.0));
}
"""


def read_png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"{path} is not a readable PNG file")
    width, height = struct.unpack(">II", header[16:24])
    return int(width), int(height)


def print_timing(label: str, value: float) -> None:
    print(f"{label:>24}: {value * 1000.0:8.2f} ms")


def normalize_loaded_image(array: np.ndarray) -> np.ndarray:
    array = np.asarray(array)
    if array.dtype == np.uint8:
        return array.astype(np.float32) / 255.0
    if array.dtype == np.uint16:
        return array.astype(np.float32) / 65535.0
    if array.dtype != np.float32:
        return array.astype(np.float32)
    return array


source_input = str(input_path)
width, height = read_png_size(input_path)
load_t0 = time.perf_counter()
if plt is not None:
    loaded = normalize_loaded_image(plt.imread(input_path))
    source_input = loaded
    height, width = loaded.shape[:2]
load_t1 = time.perf_counter()

session = rawgl.Session()

hist_prepare_t0 = time.perf_counter()
prepared_histogram = session.prepare_compute(
    histogram_shader,
    size=(width, height),
    workgroup_size=(16, 16),
    inputs={
        "u_src0": source_input,
    },
    counters={
        "histogram": {"initial_value": 0, "array_elements": range(256)},
    },
    verbosity=0,
)
hist_prepare_t1 = time.perf_counter()

hist_run_t0 = time.perf_counter()
histogram_result = prepared_histogram.run()
hist_run_t1 = time.perf_counter()
if not histogram_result.success:
    raise RuntimeError(histogram_result.error_message)

lut_t0 = time.perf_counter()
histogram = np.asarray(histogram_result.counter_array("histogram"), dtype=np.float32)
cdf = histogram.cumsum()
non_zero = cdf[cdf > 0.0]
lut = np.zeros(256, dtype=np.float32)
if non_zero.size > 0 and float(cdf[-1]) > float(non_zero[0]):
    lut = np.clip((cdf - non_zero[0]) / (cdf[-1] - non_zero[0]), 0.0, 1.0)
lut_image = lut[np.newaxis, :]
lut_t1 = time.perf_counter()

apply_prepare_t0 = time.perf_counter()
prepared_equalization = session.prepare_compute(
    apply_equalization_shader,
    size=(width, height),
    workgroup_size=(16, 16),
    inputs={
        "u_src0": source_input,
        "u_lut0": lut_image,
    },
    output={
        "format": "r32f",
        "channels": 1,
        "bits": 32,
        "capture_to_host": True,
    },
    verbosity=0,
)
apply_prepare_t1 = time.perf_counter()

apply_run_t0 = time.perf_counter()
equalized_result = prepared_equalization.run()
apply_run_t1 = time.perf_counter()
if not equalized_result.success:
    raise RuntimeError(equalized_result.error_message)

extract_t0 = time.perf_counter()
equalized_array = equalized_result.output_array()
if equalized_array.ndim == 3:
    equalized_array = equalized_array[..., 0]
extract_t1 = time.perf_counter()

print(f"input image: {input_path}")
print(f"image size: {width} x {height}")
if plt is not None:
    print_timing("load to numpy", load_t1 - load_t0)
else:
    print("load to numpy: matplotlib not installed, using file-path input instead")
print_timing("prepare histogram", hist_prepare_t1 - hist_prepare_t0)
print_timing("run histogram", hist_run_t1 - hist_run_t0)
print_timing("build equalization lut", lut_t1 - lut_t0)
print_timing("prepare equalizer", apply_prepare_t1 - apply_prepare_t0)
print_timing("run equalizer", apply_run_t1 - apply_run_t0)
print_timing("extract numpy output", extract_t1 - extract_t0)
print_timing(
    "total",
    (hist_prepare_t1 - hist_prepare_t0)
    + (hist_run_t1 - hist_run_t0)
    + (lut_t1 - lut_t0)
    + (apply_prepare_t1 - apply_prepare_t0)
    + (apply_run_t1 - apply_run_t0)
    + (extract_t1 - extract_t0),
)
print(f"equalized array shape: {equalized_array.shape}, dtype: {equalized_array.dtype}")
print(f"equalized range: {float(equalized_array.min()):.4f} .. {float(equalized_array.max()):.4f}")
