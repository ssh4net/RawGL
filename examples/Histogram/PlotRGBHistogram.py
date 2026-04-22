#!/usr/bin/env python3

from pathlib import Path
import time

import numpy as np
import rawgl

try:
    import matplotlib.pyplot as plt
except ImportError:
    plt = None


input_path = Path(__file__).resolve().parents[2] / "tests/inputs/sky.jpg"
plot_path = Path(__file__).with_name("sky_histogram.png")


histogram_shader = """#version 450 core
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0) uniform sampler2D u_src0;
layout(binding = 0, offset = 0) uniform atomic_uint histogram_r[256];
layout(binding = 1, offset = 0) uniform atomic_uint histogram_g[256];
layout(binding = 2, offset = 0) uniform atomic_uint histogram_b[256];

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(u_src0, 0);
    if (coord.x >= size.x || coord.y >= size.y) {
        return;
    }

    vec3 rgb = texelFetch(u_src0, coord, 0).rgb;
    uint r = min(uint(floor(clamp(rgb.r, 0.0, 1.0) * 255.0 + 0.5)), 255u);
    uint g = min(uint(floor(clamp(rgb.g, 0.0, 1.0) * 255.0 + 0.5)), 255u);
    uint b = min(uint(floor(clamp(rgb.b, 0.0, 1.0) * 255.0 + 0.5)), 255u);

    atomicCounterIncrement(histogram_r[r]);
    atomicCounterIncrement(histogram_g[g]);
    atomicCounterIncrement(histogram_b[b]);
}
"""


def counter_array(result, name: str) -> np.ndarray:
    return np.asarray(result.counter_array(name), dtype=np.int32)


def print_timing(label: str, value: float) -> None:
    print(f"{label:>24}: {value * 1000.0:8.2f} ms")


loaded = rawgl.io.load_image(input_path)
width = loaded.width
height = loaded.height
source_input = rawgl.host_image_to_array(loaded)
session = rawgl.Session()

prepare_t0 = time.perf_counter()
prepared = session.prepare_compute(
    histogram_shader,
    size=(width, height),
    workgroup_size=(16, 16),
    inputs={
        "u_src0": source_input,
    },
    counters={
        "histogram_r": {"initial_value": 0, "array_elements": range(256)},
        "histogram_g": {"initial_value": 0, "array_elements": range(256)},
        "histogram_b": {"initial_value": 0, "array_elements": range(256)},
    },
    verbosity=0,
)
prepare_t1 = time.perf_counter()

run_t0 = time.perf_counter()
result = prepared.run()
run_t1 = time.perf_counter()
if not result.success:
    raise RuntimeError(result.error_message)

extract_t0 = time.perf_counter()
hist_r = counter_array(result, "histogram_r")
hist_g = counter_array(result, "histogram_g")
hist_b = counter_array(result, "histogram_b")
extract_t1 = time.perf_counter()

print(f"input image: {input_path}")
print(f"image size: {width} x {height}")
print_timing("prepare workflow", prepare_t1 - prepare_t0)
print_timing("execute histogram", run_t1 - run_t0)
print_timing("extract counters", extract_t1 - extract_t0)
print_timing("total rawgl", (prepare_t1 - prepare_t0) + (run_t1 - run_t0) + (extract_t1 - extract_t0))

if plt is None:
    print("matplotlib is not installed; histogram plot was not generated.")
else:
    plot_t0 = time.perf_counter()
    figure, axes = plt.subplots(figsize=(10, 4))
    x = np.arange(256, dtype=np.int32)
    axes.plot(x, hist_r, color="red", linewidth=1.0, label="Red")
    axes.plot(x, hist_g, color="green", linewidth=1.0, label="Green")
    axes.plot(x, hist_b, color="blue", linewidth=1.0, label="Blue")
    axes.set_title("RawGL RGB Histogram")
    axes.set_xlabel("Bin")
    axes.set_ylabel("Count")
    axes.legend()
    axes.grid(True, alpha=0.2)
    figure.tight_layout()
    figure.savefig(plot_path, dpi=140)
    plt.close(figure)
    plot_t1 = time.perf_counter()

    print_timing("plot histogram", plot_t1 - plot_t0)
    print(f"histogram plot: {plot_path}")
