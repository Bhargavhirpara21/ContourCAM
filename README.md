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
                 [  C++20 core library  ]   ->  OpenCASCADE (pocket clearing)
```

The C++ core is the single source of truth for all geometry/CAM logic; C# and
Python are thin consumers that call it through one flat C ABI — they never
reimplement geometry.

## Repository layout

| Path | Contents |
|---|---|
| `core/` | Native C++ core: `include/contourcam_c_api.h` (the ABI) + `src/` + `tests/` |
| `app-csharp/interop/` | `ContourCam.Interop`: the shared P/Invoke wrapper over the C ABI |
| `app-csharp/app/` + `app-logic/` | .NET 8 **WPF desktop app** + its WPF-free, testable view logic |
| `app-csharp/smoke/` + `tests/` | interop console smoke + xUnit tests |
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
G-code for the same input (the M4 parity goal). A GoogleTest suite (43 tests)
runs in CI on Windows + Linux.

**Phase 4 — WPF desktop app.** A .NET 8 WPF app (built on a shared
`ContourCam.Interop` library) opens a DXF, renders the part geometry + a colored
toolpath overlay in a pan/zoom viewport, exposes the §7 cut parameters, and
generates/exports G-code (generation runs off the UI thread). The WPF-free app
logic + interop layer are covered by xUnit tests on Windows + Linux.

**Pocket clearing — the PRD "heart" — is done**, built on **OpenCASCADE**
(optional `CONTOURCAM_USE_OCCT` build): concentric inward 2D offsets clear the
pocket to its own floor depth, so the full sample part (outer profile + cleared
40×30 pocket + 4 drilled holes) generates end-to-end — verified through the
Python consumer and by GoogleTests, with a dedicated OCCT CI job (vcpkg binary
cached). It also clears **around a standing island** (a boss) without gouging it,
via per-level boolean region-erosion of the pocket against the island
(`samples/plate_pocket_island.dxf`). Underneath it all: one shared C++ core,
three consumers.

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

# 5. Run the WPF desktop app (Windows). Open samples/plate_pocket_holes.dxf,
#    then Generate and Export G-code.
dotnet run --project app-csharp/app -c Release

# 6. Run the .NET unit tests (set the core dir so the interop tests can load it)
CONTOURCAM_LIB_DIR=build/bin/Release dotnet test app-csharp/tests   # Linux: build/bin

# 7. (Optional) Pocket clearing via OpenCASCADE. Needs vcpkg with `opencascade`
#    and VCPKG_ROOT set; then the app and Python clear pockets too.
cmake --preset windows-occt
cmake --build --preset windows-occt
ctest --test-dir build --output-on-failure
```

Both smoke consumers load the sample part, generate an outer-contour toolpath,
and export **byte-identical** G-code from the one shared library. Coverage:
43 GoogleTest (C++, incl. OCCT pocket clearing + island avoidance), 15 xUnit (.NET interop + view
logic), and pytest (Python), all green in CI on Windows + Linux.

## License

MIT — see [LICENSE](LICENSE). Third-party components are credited in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md): GoogleTest (test build) and
OpenCASCADE (LGPL-2.1 + Open CASCADE exception, dynamically linked, optional
OCCT build).
