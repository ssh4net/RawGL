#!/usr/bin/env python3

from __future__ import annotations

import math
import sys

import rawgl


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def make_rgba_pixels(red_values: list[float]) -> list[float]:
    values: list[float] = []
    for red in red_values:
        values.extend([red, 0.0, 0.0, 1.0])
    return values


HISTOGRAM_COMPUTE = """#version 450 core
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


def histogram_counts(result: rawgl.RunResult) -> list[int]:
    counts = result.counter_array("histogram")
    if len(counts) != 4:
        raise RuntimeError(f"expected 4 histogram bins, got {counts}")
    return [int(value) for value in counts]


def derive_histogram_stats(counts: list[int]) -> tuple[float, float, float]:
    centers = [0.125, 0.375, 0.625, 0.875]
    total = sum(counts)
    if total <= 0:
        raise RuntimeError("histogram total is zero")

    min_value = centers[next(index for index, count in enumerate(counts) if count > 0)]
    max_value = centers[max(index for index, count in enumerate(counts) if count > 0)]
    avg_value = sum(center * count for center, count in zip(centers, counts, strict=True)) / float(total)
    return min_value, avg_value, max_value


def verify_counts(label: str, counts: list[int], expected: list[int]) -> int:
    if counts != expected:
        return fail(f"{label} histogram mismatch: expected {expected}, got {counts}")
    return 0


def main() -> int:
    session = rawgl.Session()

    base_image = rawgl.make_rgba32f_host_image(4, 1, make_rgba_pixels([0.05, 0.30, 0.55, 0.95]))
    workflow = rawgl.prepare_compute(
        HISTOGRAM_COMPUTE,
        session=session,
        size=(4, 1),
        workgroup_size=(1, 1),
        verbosity=0,
        inputs={
            "u_src0": base_image,
        },
        counters={
            "histogram": {"initial_value": 0, "array_elements": [0, 1, 2, 3]},
        },
    )
    first_result = workflow.run()
    if not first_result.success:
        return fail(f"histogram workflow first run failed: {first_result.error_message}")

    first_counts = histogram_counts(first_result)
    status = verify_counts("first run", first_counts, [1, 1, 1, 1])
    if status != 0:
        return status

    second_result = workflow.run(
        inputs={
            "u_src0": rawgl.make_rgba32f_host_image(4, 1, make_rgba_pixels([0.02, 0.24, 0.27, 0.88])),
        }
    )
    if not second_result.success:
        return fail(f"histogram workflow second run failed: {second_result.error_message}")

    second_counts = histogram_counts(second_result)
    status = verify_counts("second run", second_counts, [2, 1, 0, 1])
    if status != 0:
        return status

    min_value, avg_value, max_value = derive_histogram_stats(second_counts)
    if not math.isclose(min_value, 0.125, rel_tol=0.0, abs_tol=1e-6):
        return fail(f"unexpected derived histogram minimum: {min_value}")
    if not math.isclose(avg_value, 0.375, rel_tol=0.0, abs_tol=1e-6):
        return fail(f"unexpected derived histogram average: {avg_value}")
    if not math.isclose(max_value, 0.875, rel_tol=0.0, abs_tol=1e-6):
        return fail(f"unexpected derived histogram maximum: {max_value}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
