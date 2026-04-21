#!/usr/bin/env python3

from pathlib import Path

import rawgl


# Source "Bitshift" by @XorView:
# https://fragcoord.xyz/s/bbjr6uba
#
# This adapts the original FragCoord shader into standalone RawGL GLSL and
# renders a short 512x512 PNG sequence by reusing one prepared workflow.

image_size = 512
frame_count = 16
fps = 12.0
output_dir = Path(__file__).with_name("BitshiftSequence_frames")


fragment_shader = """#version 450 core
uniform vec2 u_resolution;
uniform float iTime;
layout(location = 0) out vec4 Color;

void main()
{
    vec2 p = round((2.0 * gl_FragCoord.xy - u_resolution) / u_resolution.y * 32.0) / 24.0;
    float z = 1.0 - dot(p, p);
    vec4 value =
        floor(
            z * fract(dot(p, vec2(11.0))) +
            z * 6.0 / exp(2.0 / abs(tan(iTime + p.x - p.y * z * 6.0 + vec4(0.0, 0.4, 1.0, 0.0))))
        ) / 4.0;
    Color = vec4(value.rgb, 1.0);
}
"""


output_dir.mkdir(parents=True, exist_ok=True)

session = rawgl.Session()
prepared = rawgl.prepare_image(
    fragment_shader,
    session=session,
    size=(image_size, image_size),
    inputs={
        "u_resolution": [float(image_size), float(image_size)],
    },
    outputs={
        "Color": {
            "format": "rgba32f",
            "channels": 4,
            "alpha_channel": 3,
            "bits": 16,
            "capture_to_host": True,
        }
    },
    verbosity=0,
)

for frame_index in range(frame_count):
    result = prepared.run(
        system_uniforms={
            "time": frame_index / fps,
            "frame": frame_index,
        }
    )
    if not result.success:
        raise RuntimeError(f"frame {frame_index} failed: {result.error_message}")

    frame_path = output_dir / f"Bitshift_{frame_index:03d}.png"
    rawgl.save_image(
        result.captured_outputs["Color::0"],
        frame_path,
        bits=16,
        attributes={
            "png:compressionLevel": 0,
        },
    )

print(f"frames: {frame_count}")
print(f"size: {image_size} x {image_size}")
print(output_dir.resolve())
