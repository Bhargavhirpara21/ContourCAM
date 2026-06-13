"""ctypes bindings for the ContourCAM native core (flat C ABI).

Phase 0: only the bridge-proving functions are wrapped. The same module will
grow to cover the full ABI (load_dxf, generate_toolpath, export_gcode, ...) in
later phases -- always calling the C++ core, never reimplementing geometry.
"""
from __future__ import annotations

import ctypes
import sys
from pathlib import Path

__all__ = ["Core", "library_filename", "load"]


def library_filename() -> str:
    """Return the platform-specific shared-library file name."""
    if sys.platform == "win32":
        return "contourcam_core.dll"
    if sys.platform == "darwin":
        return "libcontourcam_core.dylib"
    return "libcontourcam_core.so"


def load(lib_dir: str | Path | None = None) -> "Core":
    """Load the native core from ``lib_dir`` (or the default search path)."""
    name = library_filename()
    path = str(Path(lib_dir) / name) if lib_dir is not None else name
    return Core(ctypes.CDLL(path))


class Core:
    """Thin object wrapper around the loaded ``ctypes.CDLL``."""

    def __init__(self, lib: ctypes.CDLL) -> None:
        self._lib = lib

        lib.cc_version.restype = ctypes.c_char_p
        lib.cc_version.argtypes = []

        lib.cc_add.restype = ctypes.c_int
        lib.cc_add.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]

        lib.cc_last_error.restype = ctypes.c_char_p
        lib.cc_last_error.argtypes = []

    def version(self) -> str:
        return self._lib.cc_version().decode("utf-8")

    def last_error(self) -> str:
        return self._lib.cc_last_error().decode("utf-8")

    def add(self, a: int, b: int) -> int:
        out = ctypes.c_int()
        status = self._lib.cc_add(a, b, ctypes.byref(out))
        if status != 0:
            raise RuntimeError(f"cc_add failed (status {status}): {self.last_error()}")
        return out.value
