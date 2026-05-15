from __future__ import annotations

import numpy as np

import rawgl


VERTEX_SHADER = """#version 450 core
layout(location = 0) in vec3 position;
layout(location = 3) in uvec4 color_rgba;
layout(location = 4) in uint id0;
layout(location = 0) out vec4 v_color;
layout(location = 1) flat out uint v_id0;
void main()
{
    v_color = vec4(color_rgba) / 255.0;
    v_id0 = id0;
    gl_Position = vec4(position, 1.0);
}
"""

FRAGMENT_SHADER = """#version 450 core
layout(location = 0) in vec4 v_color;
layout(location = 1) flat in uint v_id0;
layout(location = 0) out vec4 OutColor;
layout(location = 1) out uint TriangleId;
void main()
{
    OutColor = v_color;
    TriangleId = v_id0;
}
"""


def _as_positions(points) -> np.ndarray:
    return np.asarray(points, dtype=np.float32)


def _make_mesh(points, value: int, rgba: tuple[int, int, int, int]) -> rawgl.HostMeshData:
    positions = _as_positions(points)
    vertex_count = positions.shape[0]
    return rawgl.make_host_mesh(
        positions=positions,
        indices=np.array([[0, 1, 2]], dtype=np.uint32),
        texcoords=np.zeros((vertex_count, 2), dtype=np.float32),
        normals=np.tile(np.array([[0.0, 0.0, 1.0]], dtype=np.float32), (vertex_count, 1)),
        colors=np.tile(np.asarray(rgba, dtype=np.uint8), (vertex_count, 1)),
        uint_attrs={"source_triangle_id": np.full(vertex_count, value, dtype=np.uint32)},
    )


def _left_max(image: np.ndarray) -> int:
    return int(image[:, :16].max())


def _right_max(image: np.ndarray) -> int:
    return int(image[:, 16:].max())


def main() -> int:
    left_triangle = [
        [-0.95, -0.75, 0.0],
        [-0.10, -0.75, 0.0],
        [-0.55, 0.75, 0.0],
    ]
    right_triangle = [
        [0.10, -0.75, 0.0],
        [0.95, -0.75, 0.0],
        [0.55, 0.75, 0.0],
    ]
    top_left_triangle = [
        [-0.95, 0.10, 0.0],
        [-0.10, 0.10, 0.0],
        [-0.55, 0.90, 0.0],
    ]
    top_right_triangle = [
        [0.10, 0.10, 0.0],
        [0.95, 0.10, 0.0],
        [0.55, 0.90, 0.0],
    ]

    head = _make_mesh(left_triangle, 3, (255, 0, 0, 255))
    eyes = _make_mesh(right_triangle, 5, (0, 255, 0, 255))
    head_on_right = _make_mesh(top_right_triangle, 13, (255, 0, 0, 255))

    workflow = rawgl.build_workflow(
        rawgl.render_pass(
            FRAGMENT_SHADER,
            vertex_shader=VERTEX_SHADER,
            size=(32, 32),
            meshes=[
                {"name": "head", "host_mesh": head},
                {"name": "eyes", "host_mesh": eyes},
            ],
            outputs={
                "OutColor": {"format": "rgba32f", "channels": 4, "capture_to_host": True},
                "TriangleId": {"format": "r32ui", "channels": 1, "capture_to_host": True},
            },
            cull_parameters={"enable": "false"},
        )
    )
    prepared = rawgl.prepare_workflow(workflow)

    baseline = prepared.run()
    baseline_color = baseline.output_array("OutColor::0")
    baseline_id = baseline.output_array("TriangleId::0")
    if baseline_id.dtype != np.uint32:
        raise RuntimeError(f"expected r32ui capture to return uint32, got {baseline_id.dtype}")
    if _left_max(baseline_id) < 3 or _right_max(baseline_id) < 5:
        raise RuntimeError("list-form host mesh bindings did not draw both named meshes")
    if float(baseline_color[:, :16, 0].max()) < 0.75 or float(baseline_color[:, 16:, 1].max()) < 0.75:
        raise RuntimeError("host mesh color attribute layout did not render expected colors")

    inline_override = prepared.run(
        meshes={
            "eyes": {
                "positions": _as_positions(top_left_triangle),
                "indices": np.array([[0, 1, 2]], dtype=np.uint32),
                "colors": np.tile(np.array([0, 0, 255, 255], dtype=np.uint8), (3, 1)),
                "uint_attrs": {"source_triangle_id": np.full(3, 9, dtype=np.uint32)},
            }
        }
    )
    inline_id = inline_override.output_array("TriangleId::0")
    if _left_max(inline_id) < 9 or _right_max(inline_id) != 0:
        raise RuntimeError("inline mesh override mapping did not replace the named mesh")

    list_override = prepared.run(mesh_overrides=[{"name": "head", "pass_index": 0, "host_mesh": head_on_right}])
    list_id = list_override.output_array("TriangleId::0")
    if _left_max(list_id) != 0 or _right_max(list_id) < 13:
        raise RuntimeError("sequence-form mesh_overrides did not apply the pass-indexed host mesh")

    updated = prepared.run(
        mesh_updates={
            "head": {
                "positions": _as_positions(top_right_triangle),
                "normals": np.tile(np.array([[0.0, 0.0, 1.0]], dtype=np.float32), (3, 1)),
            }
        }
    )
    updated_color = updated.output_array("OutColor::0")
    updated_id = updated.output_array("TriangleId::0")
    if _left_max(updated_id) != 0 or _right_max(updated_id) < 5:
        raise RuntimeError("mesh_updates did not move the fixed-topology mesh")
    if float(updated_color[:, 16:, 0].max()) < 0.75:
        raise RuntimeError("mesh_updates did not preserve the original vertex color stream")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
