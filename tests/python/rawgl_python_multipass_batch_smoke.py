#!/usr/bin/env python3

from __future__ import annotations

import math
import sys

import rawgl

try:
    import numpy as np
except ImportError:  # pragma: no cover - optional dependency
    np = None


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


PASS0_COMPUTE = """#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 0, rgba32f) uniform readonly image2D u_src0;
layout(binding = 1, rgba32f) uniform writeonly image2D o_mid0;

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 value = imageLoad(u_src0, coord);
    imageStore(o_mid0, coord, vec4(value.rgb * 2.0, value.a));
}
"""


PASS1_COMPUTE = """#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 0, rgba32f) uniform readonly image2D u_mid0;
layout(location = 0) uniform float scale;
layout(binding = 1, rgba32f) uniform writeonly image2D o_out0;

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 value = imageLoad(u_mid0, coord);
    imageStore(o_out0, coord, vec4(value.rgb * scale, value.a));
}
"""


def build_source(values):
    if np is not None:
        return np.asarray([[values]], dtype=np.float32)
    return rawgl.make_rgba32f_host_image(1, 1, list(values))


def expect_rgba(result, expected) -> int:
    values = rawgl.host_image_to_rgba32f(result.captured_outputs["o_out0::1"])
    if len(values) != 4:
        return fail(f"unexpected RGBA payload length: {len(values)}")
    for index, exp in enumerate(expected):
        if not math.isclose(values[index], exp, rel_tol=0.0, abs_tol=1e-6):
            return fail(f"output mismatch at channel {index}: expected {exp}, got {values[index]}")
    if np is not None:
        array = result.output_array()
        if array.shape != (1, 1, 4):
            return fail(f"unexpected output array shape: {array.shape}")
        if array.dtype != np.float32:
            return fail(f"unexpected output array dtype: {array.dtype}")
    return 0


print("rawgl_python_multipass_batch_smoke:start", flush=True)
session = rawgl.Session()

workflow = rawgl.build_workflow(
    [
        rawgl.compute_pass(
            PASS0_COMPUTE,
            session=session,
            inputs={
                "u_src0": build_source((0.25, 0.50, 0.75, 1.0)),
            },
            outputs={
                "o_mid0": {
                    "format": "rgba32f",
                    "channels": 4,
                    "alpha_channel": 3,
                    "bits": 16,
                },
            },
            size=None,
            workgroup_size=(1, 1),
        ),
        rawgl.compute_pass(
            PASS1_COMPUTE,
            session=session,
            inputs={
                "u_mid0": rawgl.pass_output("o_mid0", 0),
                "scale": 0.5,
            },
            outputs={
                "o_out0": {
                    "format": "rgba32f",
                    "channels": 4,
                    "alpha_channel": 3,
                    "bits": 16,
                    "capture_to_host": True,
                },
            },
            size=(1, 1),
            workgroup_size=(1, 1),
        ),
    ],
    verbosity=0,
)

runner = session.batch()
print("rawgl_python_multipass_batch_smoke:runner", flush=True)
prepared = runner.prepare_workflow(workflow)
print("rawgl_python_multipass_batch_smoke:prepared", flush=True)

first_handle = prepared.submit()
print("rawgl_python_multipass_batch_smoke:submit1", flush=True)
second_handle = prepared.submit(
    inputs={
        (0, "u_src0"): build_source((0.10, 0.20, 0.30, 1.0)),
        (1, "scale"): 1.5,
    }
)
print("rawgl_python_multipass_batch_smoke:submit2", flush=True)

first_result = first_handle.wait()
if first_result.cancelled or not first_result.success:
    raise SystemExit(fail(f"first batch multipass workflow failed: {first_result.error_message}"))

status = expect_rgba(first_result.run_result, (0.25, 0.50, 0.75, 1.0))
if status != 0:
    raise SystemExit(status)
print("rawgl_python_multipass_batch_smoke:wait1", flush=True)

second_result = second_handle.wait()
if second_result.cancelled or not second_result.success:
    raise SystemExit(fail(f"second batch multipass workflow failed: {second_result.error_message}"))

status = expect_rgba(second_result.run_result, (0.30, 0.60, 0.90, 1.0))
if status != 0:
    raise SystemExit(status)
print("rawgl_python_multipass_batch_smoke:wait2", flush=True)

del first_handle
del second_handle
del prepared
del runner
del workflow
del session

print("rawgl_python_multipass_batch_smoke:done", flush=True)
