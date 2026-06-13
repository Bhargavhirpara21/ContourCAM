"""Phase 0 interop smoke test for the Python (ctypes) layer.

Usage:
    python smoke.py [LIB_DIR]

LIB_DIR is the directory containing the built native core (e.g. ../build/bin).
If omitted, the default OS library search path is used.
"""
from __future__ import annotations

import sys

from contourcam import load


def main() -> int:
    lib_dir = sys.argv[1] if len(sys.argv) > 1 else None
    core = load(lib_dir)

    print(f"[Py] core version: {core.version()}")

    result = core.add(2, 3)
    print(f"[Py] cc_add(2, 3) = {result}")

    if result != 5:
        print(f"[Py] FAIL: expected 5, got {result}", file=sys.stderr)
        return 2

    print("[Py] bridge OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
