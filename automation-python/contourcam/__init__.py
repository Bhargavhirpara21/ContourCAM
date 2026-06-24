"""ctypes bindings for the ContourCAM native core (flat C ABI).

The same compiled C++ core that the C# app uses, driven from Python. Geometry and
toolpath logic are never reimplemented here -- every call goes through the core.
"""
from __future__ import annotations

import ctypes
import sys
import weakref
from pathlib import Path

__all__ = [
    "Circle", "Segment", "ToolParams", "JobParams", "PostParams",
    "Core", "Document", "Toolpath", "library_filename", "load",
    "CC_OK", "END_MILL", "DRILL", "CLIMB", "CONVENTIONAL",
]

CC_OK = 0
END_MILL, DRILL = 0, 1
CLIMB, CONVENTIONAL = 0, 1


class Circle(ctypes.Structure):
    """Mirrors cc_circle (centre + radius, millimetres)."""

    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double), ("radius", ctypes.c_double)]


class Segment(ctypes.Structure):
    """Mirrors cc_segment (one ordered move)."""

    _fields_ = [
        ("kind", ctypes.c_int32),
        ("x", ctypes.c_double), ("y", ctypes.c_double), ("z", ctypes.c_double),
        ("i", ctypes.c_double), ("j", ctypes.c_double),
        ("feed", ctypes.c_double),
    ]


class ToolParams(ctypes.Structure):
    """Mirrors cc_tool_params."""

    _fields_ = [("diameter_mm", ctypes.c_double), ("flutes", ctypes.c_int32), ("type", ctypes.c_int32)]


class JobParams(ctypes.Structure):
    """Mirrors cc_job_params."""

    _fields_ = [
        ("target_depth_mm", ctypes.c_double), ("step_down_mm", ctypes.c_double),
        ("stepover_frac", ctypes.c_double), ("feed", ctypes.c_double),
        ("plunge_feed", ctypes.c_double), ("spindle_rpm", ctypes.c_double),
        ("safe_z_mm", ctypes.c_double), ("direction", ctypes.c_int32),
    ]


class PostParams(ctypes.Structure):
    """Mirrors cc_post_params."""

    _fields_ = [("metric", ctypes.c_int32), ("coolant", ctypes.c_int32), ("tool_number", ctypes.c_int32)]


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


def _free_document(lib: ctypes.CDLL, handle: ctypes.c_void_p) -> None:
    if handle:
        if lib.cc_free_document(handle) != CC_OK:
            print("[contourcam] cc_free_document failed", file=sys.stderr)


def _free_toolpath(lib: ctypes.CDLL, handle: ctypes.c_void_p) -> None:
    if handle:
        if lib.cc_free_toolpath(handle) != CC_OK:
            print("[contourcam] cc_free_toolpath failed", file=sys.stderr)


class _Handle:
    """Base for opaque handles freed exactly once (explicitly or by the GC)."""

    def __init__(self, core: "Core", handle: ctypes.c_void_p, freer) -> None:
        self._core = core
        self._handle = handle
        self._finalizer = weakref.finalize(self, freer, core._lib, handle)

    def __enter__(self):
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def _check(self) -> ctypes.c_void_p:
        if not self._handle:
            raise RuntimeError("handle is closed")
        return self._handle

    def close(self) -> None:
        if self._finalizer.alive:
            self._finalizer()
        self._handle = ctypes.c_void_p()


class Toolpath(_Handle):
    def __init__(self, core: "Core", handle: ctypes.c_void_p) -> None:
        super().__init__(core, handle, _free_toolpath)

    def segment_count(self) -> int:
        out = ctypes.c_int()
        self._core._call("cc_toolpath_segment_count", self._check(), ctypes.byref(out))
        return out.value

    def segments(self) -> list[Segment]:
        n = self.segment_count()
        buf = (Segment * n)()
        written = ctypes.c_int()
        self._core._call("cc_toolpath_get_segments", self._check(), buf, n, ctypes.byref(written))
        return list(buf[: written.value])

    def export_gcode(self, path: str, post: PostParams | None = None) -> None:
        p = post if post is not None else PostParams(1, 0, 1)
        self._core._call("cc_export_gcode", self._check(), path.encode("utf-8"), ctypes.byref(p))


class Document(_Handle):
    def __init__(self, core: "Core", handle: ctypes.c_void_p) -> None:
        super().__init__(core, handle, _free_document)

    def outer_count(self) -> int:
        out = ctypes.c_int()
        self._core._call("cc_document_outer_count", self._check(), ctypes.byref(out))
        return out.value

    def circle_count(self) -> int:
        out = ctypes.c_int()
        self._core._call("cc_document_circle_count", self._check(), ctypes.byref(out))
        return out.value

    def circles(self) -> list[Circle]:
        count = self.circle_count()
        buf = (Circle * count)()
        written = ctypes.c_int()
        self._core._call("cc_document_get_circles", self._check(), buf, count, ctypes.byref(written))
        return list(buf[: written.value])

    def generate_toolpath(self, tool: ToolParams, job: JobParams) -> Toolpath:
        handle = ctypes.c_void_p()
        self._core._call("cc_generate_toolpath", self._check(), ctypes.byref(tool),
                         ctypes.byref(job), ctypes.byref(handle))
        return Toolpath(self._core, handle)


class Core:
    """Thin object wrapper around the loaded ``ctypes.CDLL``."""

    def __init__(self, lib: ctypes.CDLL) -> None:
        self._lib = lib

        lib.cc_version.restype = ctypes.c_char_p
        lib.cc_version.argtypes = []
        lib.cc_last_error.restype = ctypes.c_char_p
        lib.cc_last_error.argtypes = []

        lib.cc_add.restype = ctypes.c_int
        lib.cc_add.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]

        lib.cc_load_dxf.restype = ctypes.c_int
        lib.cc_load_dxf.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]

        for name in ("cc_document_outer_count", "cc_document_circle_count", "cc_document_wire_count",
                     "cc_toolpath_segment_count"):
            fn = getattr(lib, name)
            fn.restype = ctypes.c_int
            fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]

        lib.cc_document_get_circles.restype = ctypes.c_int
        lib.cc_document_get_circles.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(Circle), ctypes.c_int, ctypes.POINTER(ctypes.c_int)]

        lib.cc_generate_toolpath.restype = ctypes.c_int
        lib.cc_generate_toolpath.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ToolParams), ctypes.POINTER(JobParams),
            ctypes.POINTER(ctypes.c_void_p)]

        lib.cc_toolpath_get_segments.restype = ctypes.c_int
        lib.cc_toolpath_get_segments.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(Segment), ctypes.c_int, ctypes.POINTER(ctypes.c_int)]

        lib.cc_export_gcode.restype = ctypes.c_int
        lib.cc_export_gcode.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(PostParams)]

        lib.cc_free_document.restype = ctypes.c_int
        lib.cc_free_document.argtypes = [ctypes.c_void_p]
        lib.cc_free_toolpath.restype = ctypes.c_int
        lib.cc_free_toolpath.argtypes = [ctypes.c_void_p]

    def version(self) -> str:
        raw = self._lib.cc_version()
        return raw.decode("utf-8") if raw else ""

    def last_error(self) -> str:
        raw = self._lib.cc_last_error()
        return raw.decode("utf-8") if raw else ""

    def _call(self, name: str, *args: object) -> None:
        status = getattr(self._lib, name)(*args)
        if status != CC_OK:
            raise RuntimeError(f"{name} failed (status {status}): {self.last_error()}")

    def add(self, a: int, b: int) -> int:
        out = ctypes.c_int()
        self._call("cc_add", a, b, ctypes.byref(out))
        return out.value

    def load_dxf(self, path: str) -> Document:
        handle = ctypes.c_void_p()
        self._call("cc_load_dxf", path.encode("utf-8"), ctypes.byref(handle))
        return Document(self, handle)
