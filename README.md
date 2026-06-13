# ContourCAM

> A 2.5D CAM toolpath & G-code generator with a layered C++ / C# / Python architecture.

<!-- CI badge goes here once the GitHub repo + Actions are live:
[![CI](https://github.com/<user>/ContourCAM/actions/workflows/ci.yml/badge.svg)](https://github.com/<user>/ContourCAM/actions/workflows/ci.yml)
-->

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
                 [  C++20 core library  ]   ->  OpenCASCADE (from Phase 1)
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

**Phase 0 — proving the bridge.** A hello-world C++ shared library is called
from both C# (P/Invoke) and Python (ctypes), de-risking the interop before any
geometry code is written. See the build steps below.

## Build & run the Phase 0 bridge

**Prerequisites:** CMake ≥ 3.21, a C++20 compiler (MSVC v143 on Windows,
GCC ≥ 11 / Clang ≥ 14 on Linux), .NET 8 SDK, Python 3.11+.

```bash
# 1. Build the native core
cmake --preset windows-msvc        # or: cmake --preset linux-ninja
cmake --build --preset windows-msvc

# 2. Run the C# smoke (native lib must be next to the executable)
dotnet run --project app-csharp/smoke

# 3. Run the Python smoke (point it at the build output dir)
python automation-python/smoke.py build/bin
```

All three should report the same `cc_add(2, 3) = 5` from the one shared library.

## License

MIT — see [LICENSE](LICENSE). Third-party components (e.g. OpenCASCADE, from
Phase 1) are credited in `THIRD_PARTY_NOTICES.md`.
