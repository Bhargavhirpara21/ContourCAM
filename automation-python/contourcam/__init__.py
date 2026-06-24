"""ctypes bindings for the ContourCAM native core (flat C ABI).

The same compiled C++ core that the C# app uses, driven from Python. Geometry is
never reimplemented here -- every call goes through the core. The toolpath/G-code
surface will be added as the core grows (Phases 2-3).
"""
from __future__ import annotations

import ctypes
import sys
import weakref
from pathlib import Path

__all__ = ["Circle", "Core", "Document", "library_filename", "load"]

# Status codes mirror cc_status in contourcam_c_api.h.
CC_OK = 0


class Circle(ctypes.Structure):
    """Mirrors the cc_circle POD struct (centre + radius, millimetres)."""

    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double), ("radius", ctypes.c_double)]


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


def _free_handle(lib: ctypes.CDLL, handle: ctypes.c_void_p) -> None:
    """Release a native document handle (used by close() and the GC finalizer)."""
    if handle:
        status = lib.cc_free_document(handle)
        if status != CC_OK:
            # A finalizer must not raise; report best-effort instead.
            print(f"[contourcam] cc_free_document failed (status {status})", file=sys.stderr)


class Document:
    """An opaque parsed-DXF handle; release with :meth:`close` or a ``with`` block.

    A ``weakref.finalize`` guarantees the native handle is freed exactly once even
    if the caller forgets to close it (the C# side relies on try/finally instead).
    """

    def __init__(self, core: "Core", handle: ctypes.c_void_p) -> None:
        self._core = core
        self._handle = handle
        # GC safety net -- capture lib + handle (NOT self, so it can run).
        self._finalizer = weakref.finalize(self, _free_handle, core._lib, handle)

    def __enter__(self) -> "Document":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def _check(self) -> ctypes.c_void_p:
        if not self._handle:
            raise RuntimeError("document is closed")
        return self._handle

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

    def close(self) -> None:
        # finalize() runs _free_handle exactly once and marks the finalizer dead.
        if self._finalizer.alive:
            self._finalizer()
        self._handle = ctypes.c_void_p()


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

        for name in ("cc_document_outer_count", "cc_document_circle_count", "cc_document_wire_count"):
            fn = getattr(lib, name)
            fn.restype = ctypes.c_int
            fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]

        lib.cc_document_get_circles.restype = ctypes.c_int
        lib.cc_document_get_circles.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(Circle),
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_int),
        ]

        lib.cc_free_document.restype = ctypes.c_int
        lib.cc_free_document.argtypes = [ctypes.c_void_p]

    def version(self) -> str:
        raw = self._lib.cc_version()
        return raw.decode("utf-8") if raw else ""

    def last_error(self) -> str:
        raw = self._lib.cc_last_error()
        return raw.decode("utf-8") if raw else ""

    def _call(self, name: str, *args: object) -> None:
        """Invoke an ABI function and raise if it returns a non-OK status."""
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
