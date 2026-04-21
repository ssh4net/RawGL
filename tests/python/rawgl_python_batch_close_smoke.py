#!/usr/bin/env python3

from __future__ import annotations

import math

import rawgl

try:
    import numpy as np
except ImportError:  # pragma: no cover - optional dependency
    np = None


FRAGMENT_SHADER = """#version 450 core
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 out_color;
uniform sampler2D u_src0;
void main()
{
    out_color = texture(u_src0, UV);
}
"""


if np is not None:
    source = np.asarray([[[0.25, 0.50, 0.75, 1.0]]], dtype=np.float32)
else:
    source = rawgl.make_rgba32f_host_image(1, 1, [0.25, 0.50, 0.75, 1.0])

session = rawgl.Session()
with session.batch() as runner:
    with runner.prepare_image(
        FRAGMENT_SHADER,
        size=(1, 1),
        inputs={"u_src0": source},
        outputs={
            "out_color": {
                "format": "rgba32f",
                "channels": 4,
                "alpha_channel": 3,
                "capture_to_host": True,
            },
        },
        verbosity=0,
    ) as prepared:
        with prepared.submit() as handle:
            result = handle.wait()

if not result.success:
    raise SystemExit(f"batch close smoke failed: {result.error_message}")

expected = [0.25, 0.50, 0.75, 1.0]
if np is not None:
    array = result.output_array()
    if array.shape != (1, 1, 4):
        raise SystemExit(f"unexpected output shape: {array.shape}")
    for index, value in enumerate(expected):
        if not math.isclose(float(array[0, 0, index]), value, rel_tol=0.0, abs_tol=1e-6):
            raise SystemExit(
                f"unexpected output at channel {index}: expected {value}, got {float(array[0, 0, index])}"
            )
else:
    values = rawgl.host_image_to_rgba32f(result.captured_outputs["out_color::0"])
    for index, value in enumerate(expected):
        if not math.isclose(float(values[index]), value, rel_tol=0.0, abs_tol=1e-6):
            raise SystemExit(
                f"unexpected output at channel {index}: expected {value}, got {float(values[index])}"
            )
