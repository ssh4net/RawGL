#!/usr/bin/env python3

from __future__ import annotations

import os
import sys

import rawgl


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def main() -> int:
    os.environ["DISPLAY"] = ":rawgl-invalid-display"
    os.environ.pop("WAYLAND_DISPLAY", None)

    try:
        rawgl.Session()
    except RuntimeError as exc:
        message = str(exc)
        if "Failed to initialize GLFW" not in message:
            return fail(f"unexpected RuntimeError message: {message}")
        if "Failed to open display" not in message:
            return fail(f"missing display failure detail: {message}")
        return 0
    except SystemError as exc:
        return fail(f"Session() raised SystemError instead of RuntimeError: {exc}")

    return fail("Session() unexpectedly succeeded with an invalid DISPLAY")


if __name__ == "__main__":
    raise SystemExit(main())
