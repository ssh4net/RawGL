#!/usr/bin/env python3

import rawgl

try:
    import numpy as np
except ImportError:
    np = None


# Minimal prepared-compute example.
# It builds a 4-bin histogram from the red channel of a tiny RGBA image
# and then reuses the prepared workflow for a second run with different input.

histogram_shader = """#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 0, rgba32f) uniform readonly image2D u_src0;
layout(binding = 0, offset = 0) uniform atomic_uint histogram[4];

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 value = imageLoad(u_src0, coord);
    float red = clamp(value.r, 0.0, 0.99999994);
    uint bin = min(uint(floor(red * 4.0)), 3u);
    atomicCounterIncrement(histogram[bin]);
}
"""


def make_rgba_pixels(red_values):
    values = []
    for red in red_values:
        values.extend([red, 0.0, 0.0, 1.0])
    return values


def read_histogram(result):
    return result.counter_array("histogram")


session = rawgl.Session()

first_input = rawgl.make_rgba32f_host_image(4, 1, make_rgba_pixels([0.05, 0.30, 0.55, 0.95]))
second_input = rawgl.make_rgba32f_host_image(4, 1, make_rgba_pixels([0.02, 0.24, 0.27, 0.88]))

if np is not None:
    first_input = np.array(
        [[[0.05, 0.0, 0.0, 1.0], [0.30, 0.0, 0.0, 1.0], [0.55, 0.0, 0.0, 1.0], [0.95, 0.0, 0.0, 1.0]]],
        dtype=np.float32,
    )
    second_input = np.array(
        [[[0.02, 0.0, 0.0, 1.0], [0.24, 0.0, 0.0, 1.0], [0.27, 0.0, 0.0, 1.0], [0.88, 0.0, 0.0, 1.0]]],
        dtype=np.float32,
    )

prepared = session.prepare_compute(
    histogram_shader,
    size=(4, 1),
    workgroup_size=(1, 1),
    inputs={
        "u_src0": first_input,
    },
    counters={
        "histogram": {"initial_value": 0, "array_elements": [0, 1, 2, 3]},
    },
    verbosity=0,
)

first_result = prepared.run()
if not first_result.success:
    raise RuntimeError(first_result.error_message)
print("first histogram:", read_histogram(first_result))

second_result = prepared.run(
    inputs={
        "u_src0": second_input,
    }
)
if not second_result.success:
    raise RuntimeError(second_result.error_message)
print("second histogram:", read_histogram(second_result))
