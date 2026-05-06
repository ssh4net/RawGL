#!/usr/bin/env python3

from pathlib import Path
import os

import numpy as np
import rawgl


asset_dir = Path(__file__).resolve().parent
obj_path = asset_dir / "sketchfab_2021_08_02_12_42_08.obj"
bunny_base_color_path = asset_dir / "skate_bunny_basecolor.jpg"
skateboard_base_color_path = asset_dir / "skateboard_basecolor.jpg"
image_size = 640


def output_path() -> Path:
    override = os.environ.get("RAWGL_OBJ_PERSPECTIVE_OUTPUT_PATH")
    path = Path(override) if override else asset_dir / "RenderObjPerspectiveBaseColor_python.jpg"
    path.parent.mkdir(parents=True, exist_ok=True)
    return path


def normalize_vector(value: np.ndarray) -> np.ndarray:
    length = float(np.linalg.norm(value))
    if length <= 0.0:
        raise ValueError("cannot normalize a zero-length vector")
    return value / length


def look_at(eye: np.ndarray, target: np.ndarray, up: np.ndarray) -> np.ndarray:
    forward = normalize_vector(target - eye)
    side = normalize_vector(np.cross(forward, up))
    corrected_up = np.cross(side, forward)

    matrix = np.eye(4, dtype=np.float32)
    matrix[0, 0:3] = side
    matrix[1, 0:3] = corrected_up
    matrix[2, 0:3] = -forward
    matrix[0, 3] = -float(np.dot(side, eye))
    matrix[1, 3] = -float(np.dot(corrected_up, eye))
    matrix[2, 3] = float(np.dot(forward, eye))
    return matrix


def perspective(fov_y_degrees: float, aspect: float, near_plane: float, far_plane: float) -> np.ndarray:
    f = 1.0 / np.tan(np.radians(fov_y_degrees) * 0.5)
    matrix = np.zeros((4, 4), dtype=np.float32)
    matrix[0, 0] = f / aspect
    matrix[1, 1] = f
    matrix[2, 2] = (far_plane + near_plane) / (near_plane - far_plane)
    matrix[2, 3] = (2.0 * far_plane * near_plane) / (near_plane - far_plane)
    matrix[3, 2] = -1.0
    return matrix


def upload_matrix_values(matrix: np.ndarray) -> list[float]:
    return matrix.astype(np.float32).T.reshape(16).tolist()


mesh_vertex = """#version 450 core
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 uv_co;
layout(location = 2) in vec3 normal;
layout(location = 4) in uint material_id;

uniform mat4 u_mvp;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec3 v_normal;
layout(location = 2) flat out uint v_material_id;

void main()
{
    v_uv = uv_co;
    v_normal = normalize(normal);
    v_material_id = material_id;
    gl_Position = u_mvp * vec4(pos, 1.0);
}
"""


mesh_fragment = """#version 450 core
layout(binding = 0) uniform sampler2D u_bunny_base_color;
layout(binding = 1) uniform sampler2D u_skateboard_base_color;

uniform uint u_bunny_material_id;
uniform uint u_skateboard_material_id;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_normal;
layout(location = 2) flat in uint v_material_id;

layout(location = 0) out vec4 ObjPreview;

void main()
{
    vec2 uv = vec2(fract(v_uv.x), 1.0 - fract(v_uv.y));
    bool is_skateboard = v_material_id == u_skateboard_material_id;
    bool is_bunny = v_material_id == u_bunny_material_id;
    vec3 bunny_color = texture(u_bunny_base_color, uv).rgb;
    vec3 skateboard_color = texture(u_skateboard_base_color, uv).rgb;
    vec3 base_color = is_skateboard ? skateboard_color : (is_bunny ? bunny_color : vec3(1.0, 0.0, 1.0));

    vec3 normal_dir = normalize(v_normal);
    vec3 light_dir = normalize(vec3(0.35, -0.45, 0.82));
    float diffuse = max(dot(normal_dir, light_dir), 0.0);
    float rim = pow(1.0 - max(normal_dir.z, 0.0), 2.0) * 0.10;
    vec3 lit = base_color * (0.28 + diffuse * 0.72) + rim;
    ObjPreview = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
"""


mesh_info = rawgl.inspect_mesh_file(obj_path)
if not mesh_info.success:
    raise RuntimeError(mesh_info.error_message)
if not mesh_info.has_bounds:
    raise RuntimeError(f"OBJ file has no vertex positions: {obj_path}")

material_ids = {material.name: int(material.id) for material in mesh_info.materials}
bunny_material_id = material_ids.get("Bunny")
skateboard_material_id = material_ids.get("Skateboard")
if bunny_material_id is None or skateboard_material_id is None:
    raise RuntimeError(f"OBJ material names not found: {material_ids}")

min_corner = np.array(mesh_info.bounds_min, dtype=np.float32)
max_corner = np.array(mesh_info.bounds_max, dtype=np.float32)
center = (min_corner + max_corner) * 0.5
extent = max_corner - min_corner
radius = float(np.linalg.norm(extent) * 0.5)
fov_y = 42.0
camera_distance = radius / np.sin(np.radians(fov_y) * 0.5) * 1.15
camera_direction = normalize_vector(np.array([-0.72, 1.0, 0.42], dtype=np.float32))
eye = center + camera_direction * camera_distance

view = look_at(eye, center, np.array([0.0, 0.0, 1.0], dtype=np.float32))
projection = perspective(fov_y, 1.0, max(radius * 0.02, 0.05), radius * 6.0)
mvp = projection @ view

workflow = rawgl.build_workflow(
    rawgl.render_pass(
        mesh_fragment,
        vertex_shader=mesh_vertex,
        size=image_size,
        clear_color=(0.018, 0.020, 0.023, 1.0),
        inputs={
            "u_mvp": upload_matrix_values(mvp),
            "u_bunny_material_id": {"uint_values": [bunny_material_id]},
            "u_skateboard_material_id": {"uint_values": [skateboard_material_id]},
            "u_bunny_base_color": rawgl.io.load_image(bunny_base_color_path),
            "u_skateboard_base_color": rawgl.io.load_image(skateboard_base_color_path),
        },
        meshes=[
            {
                "path": str(obj_path),
                "parameters": {
                    "tris": "false",
                    "rend": "tr",
                },
            }
        ],
        cull_parameters={
            "enable": "false",
        },
        outputs={
            "ObjPreview": {
                "format": "rgba32f",
                "channels": 4,
                "alpha_channel": 3,
                "bits": 16,
                "capture_to_host": True,
            }
        },
    ),
    verbosity=0,
)

session = rawgl.Session()
prepared = rawgl.prepare_workflow(workflow, session=session)
result = prepared.run()
if not result.success:
    raise RuntimeError(result.error_message)

preview = rawgl.host_image_to_array(result.captured_outputs["ObjPreview::0"])
preview = np.ascontiguousarray(np.flipud(preview))

rawgl.io.save_image(
    preview,
    output_path(),
    bits=8,
    alpha_channel=3,
    attributes={
        "jpeg:quality": 94,
    },
)

print(f"OBJ bounds min={min_corner.tolist()} max={max_corner.tolist()}")
print(f"OBJ material IDs: Bunny={bunny_material_id} Skateboard={skateboard_material_id}")
print(f"Rendered preview: {output_path().resolve()}")
