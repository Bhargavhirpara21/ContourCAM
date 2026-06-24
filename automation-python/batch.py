"""Batch CLI: convert a folder of DXF drawings into G-code (FR-P2).

Each *.dxf in INPUT_DIR becomes <name>.gcode in OUTPUT_DIR, using one shared
native core -- the same engine the C# app uses. Currently emits the
radius-compensated outer contour (drilling/pocket selectable as the core grows).

Usage:
    python batch.py INPUT_DIR OUTPUT_DIR [--lib-dir DIR] [cut options]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from contourcam import CLIMB, END_MILL, JobParams, PostParams, ToolParams, load


def main() -> int:
    ap = argparse.ArgumentParser(description="Batch DXF -> G-code via the ContourCAM core.")
    ap.add_argument("input_dir", help="folder containing .dxf files")
    ap.add_argument("output_dir", help="folder to write .gcode files into")
    ap.add_argument("--lib-dir", default=None, help="directory containing the native core")
    ap.add_argument("--tool-diameter", type=float, default=6.0)
    ap.add_argument("--depth", type=float, default=6.0)
    ap.add_argument("--step-down", type=float, default=2.0)
    ap.add_argument("--feed", type=float, default=600.0)
    ap.add_argument("--plunge", type=float, default=200.0)
    ap.add_argument("--rpm", type=float, default=10000.0)
    ap.add_argument("--safe-z", type=float, default=5.0)
    args = ap.parse_args()

    in_dir = Path(args.input_dir)
    out_dir = Path(args.output_dir)
    dxfs = sorted(in_dir.glob("*.dxf"))
    if not dxfs:
        print(f"no .dxf files in {in_dir}", file=sys.stderr)
        return 1
    out_dir.mkdir(parents=True, exist_ok=True)

    core = load(args.lib_dir)
    tool = ToolParams(args.tool_diameter, 2, END_MILL)
    job = JobParams(args.depth, args.step_down, 0.45, args.feed, args.plunge, args.rpm,
                    args.safe_z, CLIMB)
    post = PostParams(1, 0, 1)

    ok = 0
    for dxf in dxfs:
        try:
            with core.load_dxf(str(dxf)) as doc:
                with doc.generate_toolpath(tool, job) as tp:
                    out = out_dir / (dxf.stem + ".gcode")
                    tp.export_gcode(str(out), post)
                    print(f"{dxf.name} -> {out.name} ({tp.segment_count()} segments)")
                    ok += 1
        except Exception as exc:  # noqa: BLE001 -- report and continue the batch
            print(f"{dxf.name}: ERROR {exc}", file=sys.stderr)

    print(f"done: {ok}/{len(dxfs)} files")
    return 0 if ok == len(dxfs) else 2


if __name__ == "__main__":
    raise SystemExit(main())
