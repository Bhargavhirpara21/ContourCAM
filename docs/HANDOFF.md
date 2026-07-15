# ContourCAM — Development Status & Continuation Guide

A working dev guide for picking the project up cleanly. For the product overview
see [README.md](../README.md); for the full spec see [PRD.md](../PRD.md).

## Current status (MVP complete)

All phases (0–5) are done, plus the PRD "heart" (pocket clearing). Verified green:

| Suite | Result |
|---|---|
| C++ GoogleTest | **43/43** (both OCCT and OCCT-free configs) |
| .NET xUnit | **15/15** |
| Python pytest | **3/3** |
| GitHub Actions (fast `bridge` job, Win+Linux) | green |

What it does end-to-end: **DXF → wire assembly → island detection →
radius-compensated outer contour + drilling + 2.5D depth + OpenCASCADE pocket
clearing → deterministic ISO G-code**, driven identically from a **C#/.NET WPF
app** and a **Python** layer over **one flat C ABI**. The full sample part
(outer profile + cleared 40×30 pocket + 4 holes) generates end-to-end →
`contourcam_full_part.gcode` (repo root). Pocket clearing also clears **around a
standing island** (a boss) without gouging it — `samples/plate_pocket_island.dxf`
exercises it.

## Repository map

| Path | Contents |
|---|---|
| `core/include/contourcam_c_api.h` | the flat C ABI (provisional — not yet frozen) |
| `core/src/geom/` | DXF reader, wire assembly/healing, island/part model |
| `core/src/cam/` | `toolpath.cpp` (contour/drill/depth + pocket integration), `gcode.cpp`, `pocket.cpp` (OCCT pocket clearing + island avoidance) |
| `core/tests/` | GoogleTest suites |
| `app-csharp/interop/` | `ContourCam.Interop` — shared P/Invoke wrapper (SafeHandle handles) |
| `app-csharp/app-logic/` | WPF-free view-model + transform + scene (unit-testable) |
| `app-csharp/app/` | the WPF desktop app |
| `app-csharp/smoke/`, `app-csharp/tests/` | console smoke + xUnit |
| `automation-python/` | ctypes wrapper, `batch.py`, `tests/` |
| `samples/plate_pocket_holes.dxf` | the MVP sample part (§7) |
| `samples/plate_pocket_island.dxf` | island sample: 60×60 pocket cleared around a 20×20 boss |
| `vcpkg.json`, `vcpkg-triplets/x64-windows-rel.cmake` | pinned OCCT (release-only, dynamic) |
| `CMakePresets.json` | `windows-msvc`, `windows-ninja`, `windows-occt`, `linux-ninja` |
| `ContourCAM.sln` | .NET solution (interop + app-logic + app + smoke + tests) |

## Build & test

**Default (OCCT-free — the fast path, what CI gates on):**
```bash
cmake --preset windows-msvc      # local VS generator (no vcvars needed)
                                 # or windows-ninja / linux-ninja (need MSVC env on Win)
cmake --build --preset windows-msvc
ctest --test-dir build -C Release --output-on-failure
```

**OCCT / pocket clearing (optional):**
```bash
# Requires VCPKG_ROOT set and an MSVC environment (the preset uses Ninja, so cl
# must be on PATH). vcpkg.json pins OpenCASCADE; the first build is long, then
# the pocket-occt CI job caches it (vcpkg x-gha).
cmake --preset windows-occt
cmake --build --preset windows-occt
ctest --test-dir build --output-on-failure   # 43/43, incl. the Pocket.* tests
```

**.NET:** `dotnet build ContourCAM.sln -c Release` then
`CONTOURCAM_LIB_DIR=build/bin/Release dotnet test app-csharp/tests` (Linux: `build/bin`).

**Python:** `python automation-python/smoke.py build/bin/Release samples/plate_pocket_holes.dxf`
(batch: `python automation-python/batch.py <in_dir> <out_dir> --lib-dir build/bin/Release`).

## Conventions (keep these)

- **OCCT is flag-gated** (`CONTOURCAM_USE_OCCT`, default OFF). Keep the OCCT-free
  build/CI dependency-free and fast; `pocket.cpp` compiles either way (`#ifdef
  CONTOURCAM_HAVE_OCCT` with an empty fallback).
- **C ABI hard rules:** no exception crosses the boundary (every function is
  try/catch → status code), POD/blittable only, two-call buffer pattern, x64 on
  both sides, explicit `cc_free_*`.
- **Determinism:** identical input → byte-identical G-code (the M4 C#↔Python
  parity gate depends on it).
- **Honest scope:** keep README/CV claims true to what's built.
- **Git:** commit under the repo-local identity; commit/push only when asked.

## Build gotchas (environment)

- If vcpkg picks up a non-MSVC CMake (e.g. an MSYS2 one on PATH) and a port build
  fails at the resource-compiler step, set `VCPKG_FORCE_DOWNLOADED_BINARIES=1`.
- OpenCASCADE-from-source needs ~15–20 GB transient disk (already built/cached
  here; the release-only triplet halves it).
- Wipe `build/` when switching between presets with different generators/toolchains.
- `gh` CLI isn't installed; pushes go over HTTPS.

## Known limitations (honest, documented)

- Pocket entry is a **straight plunge** per Z step (no ramp/helix).
- **Island avoidance** clears *around* a solid island via per-level OCCT boolean
  region-erosion (`samples/plate_pocket_island.dxf`). v1 scope: depth-2 interior
  islands; circular voids stay drilled; deeper nesting and split-region retract
  grouping are future.
- **STEP import not implemented** (PRD FR-C2, secondary).
- The app/Python clear pockets only when loaded against an **OCCT-built** core.

## Roadmap — prioritized next steps

1. **CAMotics verification (M3)** — load `contourcam_full_part.gcode`; confirm the
   profile + cleared pocket + holes cut. (User-side; strongest cut evidence.)
2. **True island avoidance** — **DONE** (per-level boolean region-erosion clears
   around a solid island; `samples/plate_pocket_island.dxf` + 5 gtests). The
   "island avoidance" claim is now real.
3. **Ramp/helix pocket entry** — replace the straight plunge.
4. **Bundle OCCT DLLs with the app** so a packaged build clears pockets out-of-box.
5. **Freeze the C ABI** (it's marked provisional).
6. Lead-in/out arcs; STEP import (secondary); README GIF + architecture diagram.

## Health check (run before changing anything)

Build + run all three suites (commands above). Expect 43 / 15 / 3 green, and
`git status` clean on `main`.
