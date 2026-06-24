"""Interop smoke test for the Python (ctypes) layer.

Usage:
    python smoke.py [LIB_DIR] [DXF_PATH] [GCODE_OUT]

Proves the same compiled core that C# uses is reachable from Python: it loads a
DXF, queries the geometry, generates an outer-contour toolpath, and exports
ISO G-code -- all through the C ABI.
"""
from __future__ import annotations

import sys

from contourcam import END_MILL, CLIMB, JobParams, PostParams, ToolParams, load


def main() -> int:
    lib_dir = sys.argv[1] if len(sys.argv) > 1 else None
    dxf = sys.argv[2] if len(sys.argv) > 2 else "samples/plate_pocket_holes.dxf"
    gcode_out = sys.argv[3] if len(sys.argv) > 3 else "contourcam_python.gcode"

    core = load(lib_dir)
    print(f"[Py] core version: {core.version()}")

    # 1. Bridge liveness check.
    if core.add(2, 3) != 5:
        print("[Py] FAIL: cc_add(2,3) != 5", file=sys.stderr)
        return 2

    # 2. Geometry + toolpath + G-code, all through the core.
    with core.load_dxf(dxf) as doc:
        outers = doc.outer_count()
        circles = doc.circles()
        print(f"[Py] {dxf}: outer={outers}, circles={len(circles)}")
        if outers != 1 or len(circles) != 4:
            print(f"[Py] FAIL: expected outer=1, circles=4 (got {outers}, {len(circles)})",
                  file=sys.stderr)
            return 3

        tool = ToolParams(6.0, 2, END_MILL)
        job = JobParams(6.0, 2.0, 0.45, 600.0, 200.0, 10000.0, 5.0, CLIMB)
        with doc.generate_toolpath(tool, job) as tp:
            n = tp.segment_count()
            tp.export_gcode(gcode_out, PostParams(1, 0, 1))
            print(f"[Py] contour toolpath: {n} segments -> {gcode_out}")
            if n <= 0:
                print("[Py] FAIL: empty toolpath", file=sys.stderr)
                return 4

    print("[Py] geometry + toolpath bridge OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
