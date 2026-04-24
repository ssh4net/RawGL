#!/usr/bin/env python3

from pathlib import Path

import numpy as np
import rawgl


asset_dir = Path(__file__).resolve().parent
obj_path = asset_dir / "sketchfab_2021_08_02_12_42_08.obj"
bunny_base_color_path = asset_dir / "skate_bunny_basecolor.jpg"
skateboard_base_color_path = asset_dir / "skateboard_basecolor.jpg"
output_path = asset_dir / "RenderObjPerspectiveBaseColor_python.jpg"
image_size = 640


def load_obj_bounds(path: Path) -> tuple[np.ndarray, np.ndarray]:
    min_corner = np.array([np.inf, np.inf, np.inf], dtype=np.float32)
    max_corner = np.array([-np.inf, -np.inf, -np.inf], dtype=np.float32)

    with path.open("r", encoding="utf-8", errors="replace") as obj_file:
        for line in obj_file:
            if not line.startswith("v "):
                continue
            fields = line.split()
            if len(fields) < 4:
                continue
            position = np.array(
                [float(fields[1]), float(fields[2]), float(fields[3])],
                dtype=np.float32,
            )
            min_corner = np.minimum(min_corner, position)
            max_corner = np.maximum(max_corner, position)

    if not np.all(np.isfinite(min_corner)) or not np.all(np.isfinite(max_corner)):
        raise RuntimeError(f"OBJ file has no vertex positions: {path}")

    return min_corner, max_corner


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

uniform mat4 u_mvp;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec3 v_normal;

void main()
{
    v_uv = uv_co;
    v_normal = normalize(normal);
    gl_Position = u_mvp * vec4(pos, 1.0);
}
"""


mesh_fragment = """#version 450 core
layout(binding = 0) uniform sampler2D u_bunny_base_color;
layout(binding = 1) uniform sampler2D u_skateboard_base_color;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec3 v_normal;

layout(location = 0) out vec4 ObjPreview;

void main()
{
    bool skateboard_tile = v_uv.x >= 1.0;
    vec2 uv = vec2(fract(v_uv.x), 1.0 - fract(v_uv.y));
    vec3 base_color = skateboard_tile
        ? texture(u_skateboard_base_color, uv).rgb
        : texture(u_bunny_base_color, uv).rgb;

    vec3 normal_dir = normalize(v_normal);
    vec3 light_dir = normalize(vec3(0.35, -0.45, 0.82));
    float diffuse = max(dot(normal_dir, light_dir), 0.0);
    float rim = pow(1.0 - max(normal_dir.z, 0.0), 2.0) * 0.10;
    vec3 lit = base_color * (0.28 + diffuse * 0.72) + rim;
    ObjPreview = vec4(clamp(lit, 0.0, 1.0), 1.0);
}
"""


min_corner, max_corner = load_obj_bounds(obj_path)
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
    output_path,
    bits=8,
    alpha_channel=3,
    attributes={
        "jpeg:quality": 94,
    },
)

print(f"OBJ bounds min={min_corner.tolist()} max={max_corner.tolist()}")
print(f"Rendered preview: {output_path.resolve()}")
