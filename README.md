# ContourCAM

> A 2.5D CAM toolpath & G-code generator with a layered C++ / C# / Python architecture.

[![CI](https://github.com/Bhargavhirpara21/ContourCAM/actions/workflows/ci.yml/badge.svg)](https://github.com/Bhargavhirpara21/ContourCAM/actions/workflows/ci.yml)

ContourCAM converts a 2D engineering drawing (DXF/STEP) into CNC machine
instructions (**G-code**): it imports a profile, computes radius-compensated
contour, pocket-clearing and drilling toolpaths, and exports ISO G-code that is
verified in an independent CNC simulator.

**Honest scope:** this is a personal **portfolio** project demonstrating a
native C++ geometry/CAM core + C++/C#/.NET interop + the manufacturing domain.
It is **not** production- or vendor-grade CAM (2.5D only; generic ISO G-code).

## Architecture

```
[ C# WPF App ]            [ Python CLI / notebook ]
        \  P/Invoke                /  ctypes
         v                        v
        [   Flat C ABI  (contourcam_c_api.h)   ]
                        |
                 [  C++20 core library  ]   ->  OpenCASCADE (Phase 2)
```

The C++ core is the single source of truth for all geometry/CAM logic; C# and
Python are thin consumers that call it through one flat C ABI — they never
reimplement geometry.

## Repository layout

| Path | Contents |
|---|---|
| `core/` | Native C++ core: `include/contourcam_c_api.h` (the ABI) + `src/` + `tests/` |
| `app-csharp/` | .NET 8 app(s): P/Invoke interop, WPF viewport (Phase 4) |
| `automation-python/` | ctypes bindings + batch CLI (Phase 5) |
| `samples/` | Sample DXF/STEP parts |
| `docs/` | Architecture diagram, screenshots/GIFs |

## Status

**Phase 1 — geometry ingestion.** The native core now reads the supported DXF
entity subset (LINE, ARC, CIRCLE, LWPOLYLINE, POLYLINE) with a hand-rolled,
tolerant reader, stitches loose edges into closed wires (healing small gaps),
and classifies those wires into an island/containment hierarchy (outer profile
vs. pockets/holes). It is covered by a GoogleTest suite that runs in CI on
Windows + Linux. OpenCASCADE is deferred to Phase 2, where it powers robust
2D offsetting/pocketing.

The Phase 0 bridge (below) still stands: one C++ shared library called from
both C# (P/Invoke) and Python (ctypes).

## Build & run

**Prerequisites:** CMake ≥ 3.21, a C++20 compiler (MSVC v143 on Windows,
GCC ≥ 11 / Clang ≥ 14 on Linux), .NET 8 SDK, Python 3.11+. GoogleTest is
fetched automatically at configure time.

```bash
# 1. Build the native core + geometry library + tests
cmake --preset windows-msvc        # or: cmake --preset linux-ninja
cmake --build --preset windows-msvc

# 2. Run the C++ unit tests
ctest --test-dir build -C Release --output-on-failure

# 3. Run the C# smoke (native lib must be next to the executable)
dotnet run --project app-csharp/smoke

# 4. Run the Python smoke (point it at the build output dir)
python automation-python/smoke.py build/bin/Release   # Linux: build/bin
```

The smoke layers should all report the same `cc_add(2, 3) = 5` from the one
shared library; the unit tests cover the Phase 1 geometry pipeline.

## License

MIT — see [LICENSE](LICENSE). Third-party components are credited in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) — currently GoogleTest (used in
the test build only). OpenCASCADE will be added there when it lands in Phase 2.
