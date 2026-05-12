#!/usr/bin/env python3

from __future__ import annotations

import math
import sys
from pathlib import Path

import rawgl

try:
    import numpy as np
except ImportError:  # pragma: no cover - optional dependency
    np = None


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


INLINE_LUT_FRAGMENT = """#version 450 core
layout(location = 0) in vec2 UV;
layout(location = 0) uniform int img_size;
layout(location = 1) uniform int lut_size;
layout(location = 0) out vec3 EmptyLUT;
void main()
{
    float pixel_size = 1.0 / img_size;
    float pixel_offset = pixel_size / 2.0;
    EmptyLUT.r = mod((UV.x - pixel_offset) * lut_size, 1.0) * (img_size + lut_size) / img_size;
    EmptyLUT.g = mod((UV.y - 1.0 - pixel_offset) * lut_size, 1.0) * (img_size + lut_size) / img_size;
    float gv = floor(lut_size + lut_size * (UV.y - 1.0)) / (lut_size - 1);
    float gh = floor(lut_size + lut_size * (UV.x - 1.0)) / (lut_size - 1);
    EmptyLUT.b = (lut_size * gv + gh) / (lut_size + 1);
}
"""

def verify_inline_lut(session: rawgl.Session) -> int:
    result = rawgl.image(
        INLINE_LUT_FRAGMENT,
        session=session,
        size=8,
        verbosity=0,
        inputs={
            "img_size": 8,
            "lut_size": 4,
        },
        output={
            "format": "rgba32f",
            "channels": 4,
            "alpha_channel": 3,
            "bits": 16,
            "capture_to_host": True,
        },
    )
    if not result.success:
        return fail(f"inline LUT workflow execution failed: {result.error_message}")

    image = result.captured_outputs.get("EmptyLUT::0")
    if image is None:
        return fail("inline LUT workflow did not capture EmptyLUT::0")
    if image.width != 8 or image.height != 8 or image.channels != 4:
        return fail(f"unexpected LUT image shape: {image.width}x{image.height}x{image.channels}")

    values = rawgl.host_image_to_rgba32f(image)
    if len(values) != 8 * 8 * 4:
        return fail(f"unexpected LUT float payload length: {len(values)}")

    red_values = values[0::4]
    green_values = values[1::4]
    blue_values = values[2::4]
    if min(red_values) < -1e-6 or max(red_values) < 0.5:
        return fail("inline LUT workflow red channel range looks invalid")
    if min(green_values) < -1e-6 or max(green_values) < 0.5:
        return fail("inline LUT workflow green channel range looks invalid")
    if min(blue_values) < -1e-6 or max(blue_values) <= min(blue_values):
        return fail("inline LUT workflow blue channel range looks invalid")

    if np is not None:
        output_arrays = rawgl.captured_output_arrays(result)
        lut_array = output_arrays.get("EmptyLUT::0")
        if lut_array is None:
            return fail("captured_output_arrays() did not return EmptyLUT::0")
        if lut_array.shape != (8, 8, 4):
            return fail(f"unexpected NumPy LUT array shape: {lut_array.shape}")
        if lut_array.dtype != np.float32:
            return fail(f"unexpected NumPy LUT array dtype: {lut_array.dtype}")
        default_lut_array = result.output_array()
        if default_lut_array.shape != (8, 8, 4):
            return fail(f"unexpected RunResultView.output_array() shape: {default_lut_array.shape}")

    return 0


INLINE_HOST_COUNTER_COMPUTE = """#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 0, rgba32f) uniform readonly image2D u_src0;
layout(binding = 1, offset = 0) uniform atomic_uint counter0;
layout(binding = 2, rgba32f) uniform writeonly image2D o_out0;
void main()
{
    vec4 value = imageLoad(u_src0, ivec2(0, 0));
    imageStore(o_out0, ivec2(0, 0), value);
    atomicCounterIncrement(counter0);
}
"""


INLINE_PREPARED_OVERRIDE_COMPUTE = """#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 0, rgba32f) uniform writeonly image2D o_out0;

uniform float gain;
uniform vec3 bias;
uniform mat3 transform;

void main()
{
    vec3 value = transform * vec3(gain, gain, gain) + bias;
    imageStore(o_out0, ivec2(0, 0), vec4(value, 1.0));
}
"""


def verify_inline_host_counter(session: rawgl.Session) -> int:
    source_input = rawgl.make_rgba32f_host_image(1, 1, [0.25, 0.5, 0.75, 1.0])
    if np is not None:
        source_input = np.array([[[0.25, 0.5, 0.75, 1.0]]], dtype=np.float32)

    result = rawgl.compute(
        INLINE_HOST_COUNTER_COMPUTE,
        session=session,
        size=(1, 1),
        workgroup_size=(1, 1),
        verbosity=0,
        inputs={
            "u_src0": source_input,
        },
        counters={
            "counter0": 7,
        },
        output={
            "format": "rgba32f",
            "channels": 4,
            "alpha_channel": 3,
            "bits": 16,
            "capture_to_host": True,
        },
    )
    if not result.success:
        return fail(f"inline host/counter workflow execution failed: {result.error_message}")

    image = result.captured_outputs.get("o_out0::0")
    if image is None:
        return fail("inline host/counter workflow did not capture o_out0::0")
    values = rawgl.host_image_to_rgba32f(image)
    expected = [0.25, 0.5, 0.75, 1.0]
    if len(values) != 4 or any(not math.isclose(values[i], expected[i], rel_tol=0.0, abs_tol=1e-6) for i in range(4)):
        return fail(f"inline host/counter output mismatch: {values}")

    counter_values = result.captured_counters.get("counter0::0")
    if counter_values != [8]:
        return fail(f"inline host/counter values mismatch: {counter_values}")

    return 0


def verify_prepared_numeric_overrides(session: rawgl.Session) -> int:
    prepared = rawgl.prepare_compute(
        INLINE_PREPARED_OVERRIDE_COMPUTE,
        session=session,
        size=(1, 1),
        workgroup_size=(1, 1),
        verbosity=0,
        inputs={
            "gain": 0.25,
            "bias": [0.0, 0.0, 0.0],
            "transform": [
                1.0, 0.0, 0.0,
                0.0, 1.0, 0.0,
                0.0, 0.0, 1.0,
            ],
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
    )

    def check(label: str, result: rawgl.RunResultView, expected: list[float]) -> int:
        if not result.success:
            return fail(f"{label} prepared override workflow failed: {result.error_message}")
        image = result.captured_outputs.get("o_out0::0")
        if image is None:
            return fail(f"{label} prepared override workflow did not capture o_out0::0")
        values = rawgl.host_image_to_rgba32f(image)
        if len(values) != 4 or any(
            not math.isclose(values[i], expected[i], rel_tol=0.0, abs_tol=1e-6)
            for i in range(4)
        ):
            return fail(f"{label} prepared override output mismatch: {values}")
        return 0

    status = check("default", prepared.run(), [0.25, 0.25, 0.25, 1.0])
    if status != 0:
        return status

    status = check(
        "vector",
        prepared.run(inputs={"bias": [0.0, 0.125, 0.25]}),
        [0.25, 0.375, 0.5, 1.0],
    )
    if status != 0:
        return status

    status = check(
        "matrix",
        prepared.run(
            inputs={
                "transform": [
                    1.0, 0.0, 0.0,
                    0.0, 2.0, 0.0,
                    0.0, 0.0, 3.0,
                ],
            }
        ),
        [0.25, 0.5, 0.75, 1.0],
    )
    if status != 0:
        return status

    return 0


def verify_system_uniform_mapping(session: rawgl.Session) -> int:
    result = rawgl.compute(
        "tests/shaders/system_uniforms.comp",
        session=session,
        size=(1, 1),
        workgroup_size=(1, 1),
        verbosity=0,
        output={
            "format": "rgba32f",
            "channels": 4,
            "alpha_channel": 3,
            "bits": 16,
            "capture_to_host": True,
        },
        system_uniforms={
            "time": 2.5,
            "frame": 9,
            "pass": 3,
        },
    )
    if not result.success:
        return fail(f"system-uniform workflow execution failed: {result.error_message}")

    image = result.captured_outputs.get("o_out0::0")
    if image is None:
        return fail("system-uniform workflow did not capture o_out0::0")
    values = rawgl.host_image_to_rgba32f(image)
    expected = [2.5, 9.0, 3.0, 1.0]
    if len(values) != 4 or any(not math.isclose(values[i], expected[i], rel_tol=0.0, abs_tol=1e-6) for i in range(4)):
        return fail(f"system-uniform workflow output mismatch: {values}")

    return 0


def verify_io_prepared_workflow(session: rawgl.Session) -> int:
    output_path = Path("tests/outputs/rawgl_python_io_workflow_smoke.png")
    output_path.unlink(missing_ok=True)

    prepared = rawgl.io.prepare_image(
        INLINE_LUT_FRAGMENT,
        session=session,
        size=8,
        verbosity=0,
        inputs={
            "img_size": 8,
            "lut_size": 4,
        },
        output=output_path,
    )

    if prepared.io_runtime is None:
        return fail("prepare_image() did not keep an IoRuntime for a file-backed workflow")

    result = prepared.run()
    if not result.success:
        return fail(f"IO-backed prepared workflow execution failed: {result.error_message}")

    if not output_path.exists():
        return fail("IO-backed prepared workflow did not write its output file")

    loaded = rawgl.io.load_image(output_path)
    if loaded.width != 8 or loaded.height != 8:
        return fail(f"rawgl.io.load_image() returned unexpected size: {loaded.width}x{loaded.height}")

    return 0


def main() -> int:
    session = rawgl.Session()

    status = verify_inline_lut(session)
    if status != 0:
        return status

    status = verify_inline_host_counter(session)
    if status != 0:
        return status

    status = verify_prepared_numeric_overrides(session)
    if status != 0:
        return status

    status = verify_system_uniform_mapping(session)
    if status != 0:
        return status

    status = verify_io_prepared_workflow(session)
    if status != 0:
        return status

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
