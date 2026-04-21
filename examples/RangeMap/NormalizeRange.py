#!/usr/bin/env python3

from pathlib import Path

import numpy as np
import rawgl


output_path = Path(__file__).with_name("NormalizeRange_python.png")
width = 256
height = 128


reduce_range_shader = """#version 450 core
layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 0) uniform sampler2D u_src0;
layout(binding = 1, rg32f) uniform writeonly image2D o_range0;

void main()
{
    ivec2 size = textureSize(u_src0, 0);
    float min_value = 3.402823466e+38;
    float max_value = -3.402823466e+38;

    for (int y = 0; y < size.y; ++y) {
        for (int x = 0; x < size.x; ++x) {
            float value = texelFetch(u_src0, ivec2(x, y), 0).r;
            min_value = min(min_value, value);
            max_value = max(max_value, value);
        }
    }

    imageStore(o_range0, ivec2(0, 0), vec4(min_value, max_value, 0.0, 1.0));
}
"""


normalize_shader = """#version 450 core
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0) uniform sampler2D u_src0;
layout(binding = 1) uniform sampler2D u_range0;
layout(binding = 2, rgba32f) uniform writeonly image2D o_out0;

void main()
{
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(o_out0);
    if (coord.x >= size.x || coord.y >= size.y) {
        return;
    }

    float source_value = texelFetch(u_src0, coord, 0).r;
    vec2 range_value = texelFetch(u_range0, ivec2(0, 0), 0).rg;
    float width_value = max(range_value.y - range_value.x, 1e-8);
    float normalized = clamp((source_value - range_value.x) / width_value, 0.0, 1.0);
    imageStore(o_out0, coord, vec4(normalized, normalized, normalized, 1.0));
}
"""


x = np.linspace(0.20, 0.85, width, dtype=np.float32)
y = np.linspace(0.0, 0.10, height, dtype=np.float32)[:, np.newaxis]
source = np.clip(x[np.newaxis, :] + y, 0.0, 1.0).astype(np.float32, copy=False)


workflow = rawgl.build_workflow(
    rawgl.compute_pass(
        reduce_range_shader,
        size=(1, 1),
        inputs={
            "u_src0": source,
        },
        outputs={
            "o_range0": {
                "format": "rg32f",
                "channels": 2,
                "bits": 32,
            }
        },
        workgroup_size=(1, 1),
    ),
    rawgl.compute_pass(
        normalize_shader,
        size=(width, height),
        inputs={
            "u_src0": source,
            "u_range0": rawgl.pass_output("o_range0", 0),
        },
        outputs={
            "o_out0": {
                "path": str(output_path),
                "format": "rgba32f",
                "channels": 4,
                "alpha_channel": 3,
                "bits": 16,
                "capture_to_host": True,
            }
        },
        workgroup_size=(16, 16),
    ),
    verbosity=0,
)


session = rawgl.Session()
prepared = rawgl.prepare_workflow(workflow, session=session)
result = prepared.run()
if not result.success:
    raise RuntimeError(result.error_message)

normalized = result.output_array("o_out0::1")
if normalized.ndim == 3:
    normalized = normalized[..., 0]

print(f"input range: {float(source.min()):.4f} .. {float(source.max()):.4f}")
print(f"normalized range: {float(normalized.min()):.4f} .. {float(normalized.max()):.4f}")
print(output_path.resolve())
