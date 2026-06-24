"""Interop smoke test for the Python (ctypes) layer.

Usage:
    python smoke.py [LIB_DIR] [DXF_PATH]

LIB_DIR   directory containing the built native core (e.g. build/bin/Release).
DXF_PATH  a DXF to load through the C ABI (default: the MVP sample part).

Proves the same compiled core that C# uses is reachable from Python and returns
the same geometry answers.
"""
from __future__ import annotations

import sys

from contourcam import load


def main() -> int:
    lib_dir = sys.argv[1] if len(sys.argv) > 1 else None
    dxf = sys.argv[2] if len(sys.argv) > 2 else "samples/plate_pocket_holes.dxf"

    core = load(lib_dir)
    print(f"[Py] core version: {core.version()}")

    # 1. Bridge liveness check.
    result = core.add(2, 3)
    print(f"[Py] cc_add(2, 3) = {result}")
    if result != 5:
        print(f"[Py] FAIL: expected 5, got {result}", file=sys.stderr)
        return 2

    # 2. Real geometry through the ABI.
    with core.load_dxf(dxf) as doc:
        outers = doc.outer_count()
        circles = doc.circles()
        print(f"[Py] {dxf}: outer={outers}, circles={len(circles)}")
        for c in circles:
            print(f"[Py]   hole @ ({c.x:g}, {c.y:g}) r={c.radius:g}")

        if outers != 1 or len(circles) != 4:
            print(f"[Py] FAIL: expected outer=1, circles=4 (got {outers}, {len(circles)})",
                  file=sys.stderr)
            return 3

    print("[Py] geometry bridge OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
