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

FRAGMENT_SHADER = """#version 450 core
layout(location = 0) flat in uint v_id0;
layout(location = 0) out uint TriangleId;
void main()
{
    TriangleId = v_id0;
}
"""


def _make_mesh(positions, value):
    indices = np.array([[0, 1, 2]], dtype=np.uint32)
    return rawgl.make_host_mesh(
        positions=np.asarray(positions, dtype=np.float32),
        indices=indices,
        uint_attrs={"source_triangle_id": np.array([value, value, value], dtype=np.uint32)},
    )


def _max_left(image):
    return int(image[:, :16].max())


def _max_right(image):
    return int(image[:, 16:].max())


def main() -> int:
    original = _make_mesh(
        [
            [0.10, -0.75, 0.0],
            [0.95, -0.75, 0.0],
            [0.55, 0.75, 0.0],
        ],
        3,
    )
    replacement = _make_mesh(
        [
            [-0.95, -0.75, 0.0],
            [-0.10, -0.75, 0.0],
            [-0.55, 0.75, 0.0],
        ],
        11,
    )

    pass0 = rawgl.render_pass(
        FRAGMENT_SHADER,
        vertex_shader=VERTEX_SHADER,
        size=(32, 32),
        meshes={"target": original},
        outputs={"TriangleId": {"format": "r32ui", "channels": 1, "capture_to_host": True}},
        cull_parameters={"enable": "false"},
    )
    pass1 = rawgl.render_pass(
        FRAGMENT_SHADER,
        vertex_shader=VERTEX_SHADER,
        size=(32, 32),
        meshes={"target": original},
        outputs={"TriangleId": {"format": "r32ui", "channels": 1, "capture_to_host": True}},
        cull_parameters={"enable": "false"},
    )
    prepared = rawgl.prepare_workflow(rawgl.build_workflow(pass0, pass1, verbosity=0))

    scoped = prepared.run(meshes={(0, "target"): replacement})
    scoped_pass0 = scoped.output_array("TriangleId::0")
    scoped_pass1 = scoped.output_array("TriangleId::1")
    if scoped_pass0.dtype != np.uint32 or scoped_pass1.dtype != np.uint32:
        raise RuntimeError("r32ui captured outputs did not round-trip as numpy.uint32")
    if _max_left(scoped_pass0) < 11 or _max_right(scoped_pass0) != 0:
        raise RuntimeError("pass-index mesh override did not affect pass 0")
    if _max_left(scoped_pass1) != 0 or _max_right(scoped_pass1) < 3:
        raise RuntimeError("pass-index mesh override leaked into pass 1")

    shared = prepared.run(meshes={"target": replacement})
    shared_pass0 = shared.output_array("TriangleId::0")
    shared_pass1 = shared.output_array("TriangleId::1")
    if _max_left(shared_pass0) < 11 or _max_right(shared_pass0) != 0:
        raise RuntimeError("unscoped mesh override did not affect pass 0")
    if _max_left(shared_pass1) < 11 or _max_right(shared_pass1) != 0:
        raise RuntimeError("unscoped mesh override did not affect pass 1")

    restored = prepared.run()
    restored_pass0 = restored.output_array("TriangleId::0")
    restored_pass1 = restored.output_array("TriangleId::1")
    if _max_left(restored_pass0) != 0 or _max_right(restored_pass0) < 3:
        raise RuntimeError("pass 0 was not restored after mesh override")
    if _max_left(restored_pass1) != 0 or _max_right(restored_pass1) < 3:
        raise RuntimeError("pass 1 was not restored after mesh override")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
