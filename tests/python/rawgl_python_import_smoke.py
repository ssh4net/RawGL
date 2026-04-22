#!/usr/bin/env python3

from __future__ import annotations

import sys

import rawgl


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def main() -> int:
    if rawgl.__version__ != "2.0.0":
        return fail(f"unexpected rawgl.__version__: {rawgl.__version__}")

    status = rawgl.status()
    if not status.startswith("rawgl nanobind"):
        return fail(f"unexpected rawgl.status(): {status}")

    if "core bindings disabled" in status:
        if hasattr(rawgl, "Session"):
            return fail("scaffold mode unexpectedly exposes Session")
    else:
        if not hasattr(rawgl, "Session"):
            return fail("core binding mode does not expose Session")
        if not hasattr(rawgl, "IoRuntime"):
            return fail("core binding mode does not expose IoRuntime")
        if not hasattr(rawgl, "BatchRunner"):
            return fail("core binding mode does not expose BatchRunner")
        if not hasattr(rawgl, "MetadataReadRequest"):
            return fail("core binding mode does not expose MetadataReadRequest")
        if not hasattr(rawgl, "MetadataDocument"):
            return fail("core binding mode does not expose MetadataDocument")
        if not hasattr(rawgl, "io"):
            return fail("core binding mode does not expose rawgl.io")
        if getattr(rawgl.io, "Runtime", None) is None:
            return fail("rawgl.io does not expose Runtime")
        if getattr(rawgl.io, "MetadataNameStyle", None) is None:
            return fail("rawgl.io does not expose MetadataNameStyle")
        if not hasattr(rawgl.io, "read_metadata"):
            return fail("rawgl.io does not expose read_metadata()")
        if not hasattr(rawgl.io, "read_metadata_document"):
            return fail("rawgl.io does not expose read_metadata_document()")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
