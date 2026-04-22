#!/usr/bin/env python3

from pathlib import Path

import rawgl


# Minimal self-contained RawGL Python example.
# It builds one render pass from embedded GLSL strings,
# executes it once, and saves the result as a PNG image.

output_path = Path(__file__).with_name("EmptyLUT_python.png")
image_size = 512
lut_size = 8


fragment_shader = """#version 450 core
layout(location = 0) in vec2 UV;
layout(location = 0) uniform int img_size;
layout(location = 1) uniform int lut_size;
layout(location = 0) out vec3 EmptyLUT;
void main()
{
    vec2 pixel_size = 1.0 / vec2(img_size);
    vec2 pixel_offset = pixel_size / 2.0;

    ivec2 square = ivec2(img_size / lut_size);
    vec2 uv = (1 + img_size) * (UV - pixel_offset);

    EmptyLUT.rg = mod(uv, square) / (square - 1);
    vec2 g = mod(uv / square, square) / (lut_size - 1);
    EmptyLUT.b = ((lut_size - 1) * g.y + g.x) / lut_size;
}
"""

result = rawgl.io.image(
    fragment_shader,
    size=image_size,
    inputs={
        "img_size": image_size,
        "lut_size": lut_size,
    },
    output=output_path,
)
if not result.success:
    raise RuntimeError(result.error_message)

print(output_path.resolve())
