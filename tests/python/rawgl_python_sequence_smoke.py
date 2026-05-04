#!/usr/bin/env python3

from pathlib import Path
import sys

import rawgl


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


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


def main() -> int:
    session = rawgl.Session()
    prepared = rawgl.prepare_image(
        fragment_shader,
        session=session,
        size=(96, 96),
        inputs={
            "u_resolution": [96.0, 96.0],
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

    previous_bytes = None
    output_dir = Path("tests/outputs")
    png_options = rawgl.io.PngSaveOptions()
    png_options.has_compression_level = True
    png_options.compression_level = 0
    codec_options = rawgl.io.ImageCodecSaveOptions()
    codec_options.has_png = True
    codec_options.png = png_options

    for frame_index in range(3):
        result = prepared.run(
            system_uniforms={
                "time": frame_index / 8.0,
                "frame": frame_index,
            }
        )
        if not result.success:
            return fail(f"sequence frame {frame_index} failed: {result.error_message}")

        image = result.captured_outputs.get("Color::0")
        if image is None:
            return fail("sequence workflow did not capture Color::0")
        if image.width != 96 or image.height != 96:
            return fail(f"unexpected sequence frame size: {image.width}x{image.height}")
        if previous_bytes is not None and previous_bytes == image.bytes:
            return fail("sequence workflow produced identical consecutive frames")
        previous_bytes = image.bytes

        frame_path = output_dir / f"rawgl_python_sequence_{frame_index:03d}.png"
        frame_path.unlink(missing_ok=True)
        rawgl.save_image(
            image,
            frame_path,
            bits=16,
            codec_options=codec_options,
        )
        if not frame_path.exists():
            return fail(f"sequence workflow did not write {frame_path}")

    print("rawgl_python_sequence_smoke:done", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
