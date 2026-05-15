from __future__ import annotations

import numpy as np

import rawgl


VERTEX_SHADER = """#version 450 core
layout(location = 0) in vec3 position;
layout(location = 4) in uint id0;
layout(location = 5) in uint extra_id;
layout(location = 0) flat out uint v_id0;
void main()
{
    v_id0 = id0 + extra_id;
    gl_Position = vec4(position, 1.0);
}
"""

FRAGMENT_SHADER = """#version 450 core
layout(location = 0) flat in uint v_id0;
layout(location = 0) out vec4 OutColor;
layout(location = 1) out uint TriangleId;
void main()
{
    float shade = float(v_id0 + 1u) / 8.0;
    OutColor = vec4(shade, 0.25, 0.75, 1.0);
    TriangleId = v_id0;
}
"""


def main() -> int:
    left_positions = np.array(
        [
            [-0.95, -0.75, 0.0],
            [-0.10, -0.75, 0.0],
            [-0.55, 0.75, 0.0],
        ],
        dtype=np.float32,
    )
    right_positions = np.array(
        [
            [0.10, -0.75, 0.0],
            [0.95, -0.75, 0.0],
            [0.55, 0.75, 0.0],
        ],
        dtype=np.float32,
    )
    indices = np.array([[0, 1, 2]], dtype=np.uint32)

    left_mesh = rawgl.make_host_mesh(
        positions=left_positions,
        indices=indices,
        uint_attrs={"source_triangle_id": np.array([1, 1, 1], dtype=np.uint32)},
        attributes={"extra_id": rawgl.vertex_attr(np.array([0, 0, 0], dtype=np.uint32), location=5)},
    )
    right_mesh = rawgl.make_host_mesh(
        positions=right_positions,
        indices=indices,
        uint_attrs={"source_triangle_id": np.array([5, 5, 5], dtype=np.uint32)},
        attributes={"extra_id": rawgl.vertex_attr(np.array([2, 2, 2], dtype=np.uint32), location=5)},
    )
    replacement_positions = np.array(
        [
            [-0.95, -0.90, 0.0],
            [-0.10, -0.90, 0.0],
            [-0.10, -0.10, 0.0],
            [-0.95, -0.10, 0.0],
        ],
        dtype=np.float32,
    )
    replacement_indices = np.array([[0, 1, 2], [0, 2, 3]], dtype=np.uint32)
    replacement_mesh = rawgl.make_host_mesh(
        positions=replacement_positions,
        indices=replacement_indices,
        uint_attrs={"source_triangle_id": np.array([9, 9, 9, 9], dtype=np.uint32)},
        attributes={"extra_id": rawgl.vertex_attr(np.array([0, 0, 0, 0], dtype=np.uint32), location=5)},
    )
    left_replacement_positions = np.array(
        [
            [0.10, 0.10, 0.0],
            [0.95, 0.10, 0.0],
            [0.95, 0.90, 0.0],
            [0.10, 0.90, 0.0],
        ],
        dtype=np.float32,
    )
    left_replacement_mesh = rawgl.make_host_mesh(
        positions=left_replacement_positions,
        indices=replacement_indices,
        uint_attrs={"source_triangle_id": np.array([12, 12, 12, 12], dtype=np.uint32)},
        attributes={"extra_id": rawgl.vertex_attr(np.array([0, 0, 0, 0], dtype=np.uint32), location=5)},
    )
    if left_mesh.vertex_count != 3 or left_mesh.index_count != 3:
        raise RuntimeError("unexpected HostMeshData counts")

    workflow = rawgl.build_workflow(
        rawgl.render_pass(
            FRAGMENT_SHADER,
            vertex_shader=VERTEX_SHADER,
            size=(32, 32),
            meshes={"left": left_mesh, "right": right_mesh},
            outputs={
                "OutColor": {"format": "rgba32f", "channels": 4, "capture_to_host": True},
                "TriangleId": {"format": "r32ui", "channels": 1, "capture_to_host": True},
            },
            cull_parameters={"enable": "false"},
        )
    )
    prepared = rawgl.prepare_workflow(workflow)
    result = prepared.run()
    image = result.output_array("OutColor::0")
    triangle_id = result.output_array("TriangleId::0")
    if image.shape != (32, 32, 4):
        raise RuntimeError(f"unexpected captured output shape: {image.shape}")
    if triangle_id.dtype != np.uint32 or triangle_id.shape != (32, 32):
        raise RuntimeError(f"unexpected integer output shape or dtype: {triangle_id.shape} {triangle_id.dtype}")

    left_red = image[:, :16, 0].max()
    right_red = image[:, 16:, 0].max()
    if left_red <= 0.0 or right_red <= 0.0:
        raise RuntimeError("host mesh render did not draw both bound meshes")
    if right_red <= left_red:
        raise RuntimeError("host mesh render did not preserve per-mesh id attributes")
    if triangle_id[:, :16].max() < 1 or triangle_id[:, 16:].max() < 7:
        raise RuntimeError("integer output capture did not preserve mesh id values")

    replaced = prepared.run(meshes={"right": replacement_mesh})
    replaced_triangle_id = replaced.output_array("TriangleId::0")
    if replaced_triangle_id[:, :16].max() < 9:
        raise RuntimeError("mesh override did not bind the replacement topology")
    if replaced_triangle_id[:, 16:].max() > 0:
        raise RuntimeError("mesh override did not replace the named mesh binding")

    multi_replaced = prepared.run(meshes={"left": left_replacement_mesh, "right": replacement_mesh})
    multi_replaced_triangle_id = multi_replaced.output_array("TriangleId::0")
    if multi_replaced_triangle_id[:, :16].max() < 9:
        raise RuntimeError("multiple mesh overrides did not bind the right replacement mesh")
    if multi_replaced_triangle_id[:, 16:].max() < 12:
        raise RuntimeError("multiple mesh overrides did not bind the left replacement mesh")

    restored = prepared.run()
    restored_triangle_id = restored.output_array("TriangleId::0")
    if restored_triangle_id[:, 16:].max() < 7:
        raise RuntimeError("mesh override was not restored after the run")

    moved_right = right_positions.copy()
    moved_right[:, 0] -= 1.1
    updated = prepared.run(mesh_updates={"right": {"positions": moved_right}})
    updated_image = updated.output_array("OutColor::0")
    updated_triangle_id = updated.output_array("TriangleId::0")
    if updated_image[:, 16:, 0].max() > 0.0:
        raise RuntimeError("mesh position update did not move the right mesh")
    if updated_triangle_id[:, 16:].max() > 0:
        raise RuntimeError("integer output capture did not update after mesh position update")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
