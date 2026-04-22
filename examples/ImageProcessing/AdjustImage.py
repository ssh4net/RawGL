#!/usr/bin/env python3

from pathlib import Path

import rawgl


input_path = Path(__file__).resolve().parents[2] / "tests/inputs/sky.jpg"
output_path = Path(__file__).with_name("AdjustImage_python.png")
gain = 1.20
gamma_value = 1.10


fragment_shader = """#version 450 core
layout(binding = 0) uniform sampler2D u_src0;
layout(location = 0) uniform float gain;
layout(location = 1) uniform float gamma_value;
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 OutImage;

void main()
{
    vec4 source = texture(u_src0, UV);
    vec3 adjusted = clamp(source.rgb * gain, 0.0, 1.0);
    adjusted = pow(adjusted, vec3(1.0 / max(gamma_value, 1e-6)));
    OutImage = vec4(adjusted, source.a);
}
"""


loaded = rawgl.io.load_image(input_path)
width = loaded.width
height = loaded.height

result = rawgl.io.image(
    fragment_shader,
    size=(width, height),
    inputs={
        "u_src0": str(input_path),
        "gain": gain,
        "gamma_value": gamma_value,
    },
    output={
        "path": str(output_path),
        "format": "rgba32f",
        "channels": 4,
        "alpha_channel": 3,
        "bits": 16,
        "capture_to_host": True,
        "attributes": {
            "png:compressionLevel": 0,
        },
    },
)
if not result.success:
    raise RuntimeError(result.error_message)

image = result.captured_outputs["OutImage::0"]
values = rawgl.host_image_to_rgba32f(image)
red_values = values[0::4]
green_values = values[1::4]
blue_values = values[2::4]

print(f"input image: {input_path}")
print(f"image size: {width} x {height}")
print(f"gain: {gain}")
print(f"gamma: {gamma_value}")
print(
    "output rgb range: "
    f"{min(min(red_values), min(green_values), min(blue_values)):.4f} .. "
    f"{max(max(red_values), max(green_values), max(blue_values)):.4f}"
)
print(output_path.resolve())
