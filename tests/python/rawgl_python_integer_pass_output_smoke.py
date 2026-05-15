from __future__ import annotations

import numpy as np

import rawgl


VERTEX_SHADER = """#version 450 core
layout(location = 0) in vec3 position;
layout(location = 4) in uint id0;
layout(location = 0) flat out uint v_id0;
void main()
{
    v_id0 = id0;
    gl_Position = vec4(position, 1.0);
}
"""

WRITE_ID_FRAGMENT = """#version 450 core
layout(location = 0) flat in uint v_id0;
layout(location = 0) out uint TriangleId;
void main()
{
    TriangleId = v_id0;
}
"""

SAMPLE_ID_FRAGMENT = """#version 450 core
layout(location = 0) in vec2 UV;
uniform usampler2D tri_id;
layout(location = 0) out vec4 OutColor;
void main()
{
    uint id_value = texture(tri_id, UV).r;
    OutColor = id_value == 7u ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
}
"""


def main() -> int:
    positions = np.array(
        [
            [-0.75, -0.75, 0.0],
            [0.75, -0.75, 0.0],
            [0.00, 0.75, 0.0],
        ],
        dtype=np.float32,
    )
    indices = np.array([[0, 1, 2]], dtype=np.uint32)
    mesh = rawgl.make_host_mesh(
        positions=positions,
        indices=indices,
        uint_attrs={"source_triangle_id": np.array([7, 7, 7], dtype=np.uint32)},
    )

    workflow = rawgl.build_workflow(
        rawgl.render_pass(
            WRITE_ID_FRAGMENT,
            vertex_shader=VERTEX_SHADER,
            size=(32, 32),
            meshes={"triangle": mesh},
            outputs={"TriangleId": {"format": "r32ui", "channels": 1, "capture_to_host": True}},
            cull_parameters={"enable": "false"},
        ),
        rawgl.image_pass(
            SAMPLE_ID_FRAGMENT,
            size=(32, 32),
            inputs={
                "tri_id": {
                    "pass_output": ("TriangleId", 0),
                    "min_filter": "nearest",
                    "mag_filter": "nearest",
                    "wrap_s": "clamp_to_edge",
                    "wrap_t": "clamp_to_edge",
                },
            },
            outputs={"OutColor": {"format": "rgba32f", "channels": 4, "capture_to_host": True}},
        ),
        verbosity=0,
    )

    result = rawgl.run_workflow(workflow)
    id_buffer = result.output_array("TriangleId::0")
    if id_buffer.dtype != np.uint32 or id_buffer.shape != (32, 32):
        raise RuntimeError(f"unexpected integer capture shape or dtype: {id_buffer.shape} {id_buffer.dtype}")
    if id_buffer.max() != 7:
        raise RuntimeError("integer pass output capture did not preserve the written ID value")

    image = result.output_array("OutColor::1")
    if image.shape != (32, 32, 4):
        raise RuntimeError(f"unexpected captured output shape: {image.shape}")
    if image[:, :, 1].max() <= 0.5:
        raise RuntimeError("integer pass output was not sampled by the next pass")
    if image[:, :, 0].max() > 0.0:
        raise RuntimeError("integer pass output sampled an unexpected ID value")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
