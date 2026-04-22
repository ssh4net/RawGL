#!/usr/bin/env python3

from pathlib import Path

import rawgl


output_path = Path(__file__).with_name("RenderMeshOverBackground_python.png")
mesh_path = Path(__file__).resolve().parents[2] / "tests/inputs/sponge.ply"
image_size = 512


background_fragment = """#version 450 core
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 Background;

void main()
{
    vec3 top = vec3(0.10, 0.14, 0.22);
    vec3 bottom = vec3(0.88, 0.92, 0.98);
    vec3 gradient = mix(bottom, top, UV.y);
    float band = 0.5 + 0.5 * sin(UV.x * 28.0) * sin(UV.y * 20.0);
    gradient *= 0.92 + 0.08 * band;
    Background = vec4(gradient, 1.0);
}
"""


mesh_vertex = """#version 450 core
layout(location = 0) in vec3 pos;
layout(location = 3) in uvec4 color_rgba;
layout(location = 0) out float AO;

void main()
{
    AO = float(color_rgba.r) / 255.0;
    gl_Position = vec4(pos, 1.0);
}
"""


mesh_fragment = """#version 450 core
layout(location = 0) in float AO;
layout(location = 0) out vec4 MeshOverlay;

void main()
{
    vec3 dark = vec3(0.08, 0.14, 0.28);
    vec3 light = vec3(0.80, 0.88, 1.00);
    vec3 mesh_color = mix(dark, light, AO);
    MeshOverlay = vec4(mesh_color, 0.82);
}
"""


composite_fragment = """#version 450 core
layout(binding = 0) uniform sampler2D u_bg0;
layout(binding = 1) uniform sampler2D u_mesh0;
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 Composite;

void main()
{
    vec4 background = texture(u_bg0, UV);
    vec4 mesh = texture(u_mesh0, UV);
    Composite = vec4(mix(background.rgb, mesh.rgb, mesh.a), 1.0);
}
"""


workflow = rawgl.build_workflow(
    rawgl.image_pass(
        background_fragment,
        size=image_size,
        outputs={
            "Background": {
                "format": "rgba32f",
                "channels": 4,
                "alpha_channel": 3,
                "bits": 16,
            }
        },
    ),
    rawgl.render_pass(
        mesh_fragment,
        vertex_shader=mesh_vertex,
        size=image_size,
        clear_color=(0.0, 0.0, 0.0, 0.0),
        meshes=[
            {
                "path": str(mesh_path),
                "parameters": {
                    "tris": "true",
                    "rend": "tr",
                },
            }
        ],
        outputs={
            "MeshOverlay": {
                "format": "rgba32f",
                "channels": 4,
                "alpha_channel": 3,
                "bits": 16,
            }
        },
    ),
    rawgl.image_pass(
        composite_fragment,
        size=image_size,
        inputs={
            "u_bg0": rawgl.pass_output("Background", 0),
            "u_mesh0": rawgl.pass_output("MeshOverlay", 1),
        },
        outputs={
            "Composite": {
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

image = result.output_array("Composite::2")
rawgl.io.save_image(
    result.captured_outputs["Composite::2"],
    output_path,
    bits=16,
    attributes={
        "png:compressionLevel": 0,
    },
)
print(f"composited image shape: {image.shape}")
print(output_path.resolve())
