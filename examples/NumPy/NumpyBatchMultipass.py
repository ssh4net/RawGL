#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import time

import numpy as np
import rawgl


@dataclass(frozen=True, slots=True)
class BatchConfig:
    width: int = 256
    height: int = 160
    frame_count: int = 4
    fps: float = 12.0


PASS0_FRAGMENT = """#version 450 core
layout(binding = 0) uniform sampler2D u_src0;
layout(binding = 1) uniform sampler2D u_src1;
layout(location = 0) uniform float blend_value;
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 Mix0;

void main()
{
    vec3 a = texture(u_src0, UV).rgb;
    vec3 b = texture(u_src1, UV).rgb;
    vec3 mixed = mix(a, b, clamp(blend_value, 0.0, 1.0));
    Mix0 = vec4(mixed, 1.0);
}
"""


PASS1_FRAGMENT = """#version 450 core
layout(binding = 0) uniform sampler2D u_mix0;
layout(location = 0) uniform float exposure;
layout(location = 1) uniform float frame_phase;
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 FrameOut;

void main()
{
    vec3 color = texture(u_mix0, UV).rgb;
    vec2 centered = UV * 2.0 - 1.0;
    float vignette = clamp(1.0 - dot(centered, centered) * 0.20, 0.72, 1.0);
    color *= exposure * vignette;
    color += 0.025 * vec3(sin(frame_phase), sin(frame_phase + 2.1), sin(frame_phase + 4.2));
    FrameOut = vec4(clamp(color, 0.0, 1.0), 1.0);
}
"""


def _output_dir() -> Path:
    override = os.environ.get("RAWGL_NUMPY_BATCH_OUTPUT_DIR")
    path = Path(override) if override else Path(__file__).resolve().parent / "NumpyBatchMultipass_frames"
    path.mkdir(parents=True, exist_ok=True)
    return path


def _make_png_save_options() -> rawgl.ImageCodecSaveOptions:
    png = rawgl.io.PngSaveOptions()
    png.has_compression_level = True
    png.compression_level = 2

    codec = rawgl.io.ImageCodecSaveOptions()
    codec.has_png = True
    codec.png = png
    return codec


def _make_base_fields(config: BatchConfig) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    x = np.linspace(0.0, 1.0, config.width, dtype=np.float32)
    y = np.linspace(0.0, 1.0, config.height, dtype=np.float32)
    xx, yy = np.meshgrid(x, y)
    radius = np.sqrt((xx - 0.5) * (xx - 0.5) + (yy - 0.5) * (yy - 0.5)).astype(np.float32, copy=False)
    return xx.astype(np.float32, copy=False), yy.astype(np.float32, copy=False), radius


def _frame_sources(
    xx: np.ndarray,
    yy: np.ndarray,
    radius: np.ndarray,
    frame_index: int,
    frame_count: int,
) -> tuple[np.ndarray, np.ndarray]:
    phase = np.float32(frame_index / max(frame_count - 1, 1))
    wave = np.sin((xx * 7.0 + yy * 3.0 + phase * 2.0) * np.float32(np.pi)).astype(np.float32, copy=False)
    rings = np.sin((radius * 18.0 - phase * 4.0) * np.float32(np.pi)).astype(np.float32, copy=False)

    source_a = np.empty((xx.shape[0], xx.shape[1], 3), dtype=np.float32)
    source_b = np.empty_like(source_a)

    source_a[:, :, 0] = np.clip(xx * 0.82 + 0.10 * wave, 0.0, 1.0)
    source_a[:, :, 1] = np.clip(yy * 0.78 + 0.12 * rings, 0.0, 1.0)
    source_a[:, :, 2] = np.clip(0.35 + 0.45 * (1.0 - radius), 0.0, 1.0)

    source_b[:, :, 0] = np.clip(0.18 + 0.65 * rings, 0.0, 1.0)
    source_b[:, :, 1] = np.clip(1.0 - xx * 0.72 + 0.08 * wave, 0.0, 1.0)
    source_b[:, :, 2] = np.clip(yy * 0.50 + phase * 0.35, 0.0, 1.0)

    return np.ascontiguousarray(source_a), np.ascontiguousarray(source_b)


def _build_workflow(config: BatchConfig, source_a: np.ndarray, source_b: np.ndarray) -> rawgl.Workflow:
    return rawgl.build_workflow(
        rawgl.image_pass(
            PASS0_FRAGMENT,
            size=(config.width, config.height),
            inputs={
                "u_src0": source_a,
                "u_src1": source_b,
                "blend_value": 0.45,
            },
            outputs={
                "Mix0": {
                    "format": "rgba32f",
                    "channels": 4,
                    "alpha_channel": 3,
                    "bits": 16,
                }
            },
        ),
        rawgl.image_pass(
            PASS1_FRAGMENT,
            size=(config.width, config.height),
            inputs={
                "u_mix0": rawgl.pass_output("Mix0", 0),
                "exposure": 1.05,
                "frame_phase": 0.0,
            },
            outputs={
                "FrameOut": {
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


def main() -> int:
    config = BatchConfig()
    output_dir = _output_dir()
    png_options = _make_png_save_options()

    xx, yy, radius = _make_base_fields(config)
    initial_a, initial_b = _frame_sources(xx, yy, radius, 0, config.frame_count)
    workflow = _build_workflow(config, initial_a, initial_b)

    session = rawgl.Session()
    io_runtime = rawgl.io.Runtime()
    t0 = time.perf_counter()
    with session.batch(io_runtime=io_runtime) as runner:
        prepared = rawgl.prepare_batch_workflow(runner, workflow)
        handles = []
        output_paths = []
        for frame_index in range(config.frame_count):
            source_a, source_b = _frame_sources(xx, yy, radius, frame_index, config.frame_count)
            output_path = output_dir / f"NumpyBatchMultipass_{frame_index:03d}.png"
            output_path.unlink(missing_ok=True)
            output_paths.append(output_path)

            handles.append(
                prepared.submit(
                    inputs={
                        (0, "u_src0"): source_a,
                        (0, "u_src1"): source_b,
                        (0, "blend_value"): 0.25 + 0.18 * frame_index,
                        (1, "exposure"): 1.00 + 0.03 * frame_index,
                        (1, "frame_phase"): frame_index / config.fps,
                    },
                    outputs={
                        (1, "FrameOut"): {
                            "path": output_path,
                            "format": "rgba32f",
                            "channels": 4,
                            "alpha_channel": 3,
                            "bits": 16,
                            "codec_options": png_options,
                        }
                    },
                    system_uniforms={
                        "frame": frame_index,
                        "time": frame_index / config.fps,
                    },
                )
            )

        for frame_index, handle in enumerate(handles):
            result = handle.wait()
            if not result.success:
                raise RuntimeError(result.error_message)
            if not output_paths[frame_index].exists():
                raise RuntimeError(f"missing batch output: {output_paths[frame_index]}")

    t1 = time.perf_counter()
    print(f"frames: {config.frame_count}")
    print(f"frame size: {config.width}x{config.height}")
    print(f"elapsed: {(t1 - t0) * 1000.0:.2f} ms")
    for path in output_paths:
        print(path.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
