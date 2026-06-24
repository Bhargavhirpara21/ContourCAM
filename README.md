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

**Phase 2 (in progress) — toolpath + G-code over the C ABI.** The native core
now turns a DXF into machine-ready G-code: it reads the supported DXF subset,
assembles + heals closed wires, classifies islands, then generates a
radius-compensated **outer contour** (profiled outside, winding-correct),
**drilling** cycles at detected holes, and **2.5D depth passes**, emitting a
deterministic ISO 6983 / RS-274 program. The same core is driven from **both**
the C# app (P/Invoke) and Python (ctypes), which produce **byte-identical**
G-code for the same input (the M4 parity goal). A GoogleTest suite (35 tests)
runs in CI on Windows + Linux.

**Pocket clearing with island avoidance** (the PRD "heart") and general/concave
offsetting need robust 2D offsets and arrive with **OpenCASCADE** (next phase).
The Phase 0 bridge still stands underneath: one shared library, three languages.

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

Both smoke consumers load the sample part, generate an outer-contour toolpath,
and export **byte-identical** G-code from the one shared library; the unit tests
cover the geometry + toolpath + G-code pipeline (35 tests).

## License

MIT — see [LICENSE](LICENSE). Third-party components are credited in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) — currently GoogleTest (used in
the test build only). OpenCASCADE will be added there when it lands in Phase 2.
