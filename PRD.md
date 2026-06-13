# ContourCAM — Product Requirements Document

> A 2.5D CAM toolpath & G-code generator with a layered C++ / C# / Python architecture.
> Personal portfolio project by Bhargav Hirpara. Status: Approved PRD — pre-implementation.

---

## 0. Meta
| | |
|---|---|
| **Product name** | ContourCAM |
| **Author / Owner** | Bhargav Hirpara |
| **Type** | Personal portfolio project (open-source, public GitHub) |
| **Location** | `C:\Users\fk6147\ContourCAM` (outside OneDrive) |
| **Date** | 2026-06-13 |
| **Document version** | v2 (expanded) |
| **Status** | Approved PRD — not yet implemented |

**Naming rationale.** "Contour" captures the 2D geometry/toolpath work; "CAM" anchors it in the
manufacturing domain — together it reads instantly as "a CAM tool" to anyone in the field.

**Location rationale.** The project lives under the local user profile (`C:\Users\fk6147\ContourCAM`),
deliberately **outside OneDrive**. Syncing a C++/vcpkg/.NET build tree through OneDrive causes
file-lock build failures, possible `.git` corruption, sync-conflict duplicate files, slow builds and
wasted quota, and Windows `MAX_PATH` errors — and it would mix a personal project into corporate
(Sandvik) cloud storage. The project is versioned with Git and pushed to GitHub instead.

---

## 1. Executive Summary
ContourCAM is a desktop **CAM (computer-aided manufacturing)** tool that converts a 2D engineering
drawing of a part into **CNC machine instructions (G-code)**. The user opens a DXF/STEP profile,
selects a cutting tool and parameters, and the application computes the tool's travel path
(contour, pocket clearing, drilling) and exports ISO G-code, verified in an independent CNC simulator.

The architecture is intentionally **layered to mirror real CAD/manufacturing software**:
- **C++20 geometry/CAM core** (OpenCASCADE) — the compute engine, no UI.
- **C#/.NET 8 WPF desktop app** — the operator-facing product.
- **Python automation layer** — headless batch processing.

All three layers talk to the core through **one flat C ABI**.

---

## 2. Motivation / Career Goal
- The author's CV lists C++ as a skill but currently shows **no C++ evidence**; the only C# is from
  his master's thesis, and every project is Python. This project converts the C++ claim into proof.
- It targets **both** audiences he is applying to: **CAD-software vendors** (C++ / OpenCASCADE /
  geometry-kernel signal) **and manufacturing/automation companies** (CAM domain + C#/.NET signal).
- It reuses his real edge — DXF, engineering drawings, and cutting tools (he works at Walter / Sandvik).
- The layered design lets **one** project speak to both audiences with **genuine, non-overlapping
  roles per language** — not resume padding.

---

## 3. Goals & Non-Goals

### Goals
- **G1.** Demonstrate real, authored modern C++ (geometry + CAM algorithms), not just library glue.
- **G2.** Demonstrate professional C++/C# interop over a clean C ABI (a production CAD pattern).
- **G3.** Produce a visually demonstrable, working desktop product (drawing in → toolpath + G-code out).
- **G4.** Externally verifiable correctness (independent CNC simulator).
- **G5.** Reuse and extend the author's manufacturing + DXF domain knowledge.

### Non-Goals (explicitly out of scope)
- **NG1.** Full 3D-surface or 5-axis machining. (2.5D only.)
- **NG2.** Vendor-certified / machine-specific post-processors. (Generic ISO G-code only.)
- **NG3.** Production- or safety-grade CAM. (Portfolio demonstration, stated honestly.)
- **NG4.** A full parametric CAD modeler. (It consumes drawings; it does not author 3D models.)

---

## 4. Target Users
- **P1 — Manufacturing/CAM engineer:** loads a part drawing, generates and exports toolpaths.
- **P2 — Automation engineer:** batch-processes many drawings headlessly via Python.
- **P3 — (the real audience) Recruiters/interviewers** evaluating the author's C++/C#/CAD skills.

---

## 5. Success Metrics
- **M1.** C++ core builds in CI on Windows + Linux; C# app builds on Windows; all green.
- **M2.** GoogleTest suite passes (parsing, offset correctness, pocket coverage, drilling, G-code parse-back).
- **M3.** Exported G-code runs without error and yields the correct shape in CAMotics (independent sim).
- **M4.** Same input → identical toolpath from **both** the C# app and the Python layer (proves a shared core).
- **M5.** README with architecture diagram + GIF of the C# app; clean public repo with a CI badge.
- **M6 (ultimate).** Usable as a concrete talking point / artifact in interviews.

---

## 6. Scope — Prioritized (Definition of Done)

### MVP (must finish — the resume-worthy core)
- Read the MVP sample DXF (see §7): closed outer contour + 1 rectangular pocket + 4 holes.
- Compute: radius-compensated outer contour path; pocket clearing (concentric offsets); drilling at holes.
- Emit valid ISO G-code (G20/G21, G0/G1/G2/G3, spindle, feeds, tool change).
- C# WPF app: load drawing, set tool/params, render geometry + colored toolpath overlay, export G-code.
- Verify G-code in CAMotics. GoogleTest on core. CI builds core (Win+Linux) + app (Win).

### Secondary (after MVP is solid)
- STEP import; multiple pockets + island avoidance; Python batch layer + parity test; tool library;
  save/load job (JSON); multi-depth Z step-downs; lead-in/out arcs.

### Stretch (nice-to-have)
- Toolpath animation / raster material-removal simulation; feeds & speeds from material CSV; blog post.

---

## 7. MVP Sample Part Specification (makes the Definition of Done objectively testable)
A single, fully dimensioned reference part used to define and verify the MVP.

| Attribute | Value |
|---|---|
| **Stock** | 100 mm × 80 mm plate, 6 mm thick |
| **Units** | millimetres (G21) |
| **Origin (WCS)** | part lower-left corner; X→right, Y→up, Z=0 at top of stock |
| **Outer contour** | full 100 × 80 rectangle, profiled on the **outside** |
| **Pocket** | rectangular, 40 mm × 30 mm, centred; 5 mm deep |
| **Holes** | 4 × Ø6 mm through-holes, centres 10 mm in from each corner |
| **End mill** | Ø6 mm, 2-flute (used for contour + pocket) |
| **Drill** | Ø6 mm (used for holes) |
| **Default cut params** | step-down 2 mm, stepover 45% of Ø, feed 600 mm/min, plunge 200 mm/min, spindle 10 000 rpm, safe Z +5 mm |

**Acceptance:** generating against this part yields an outer contour pass, a cleared 40×30 pocket
(no gouging into walls, full floor coverage), and 4 drilled holes; the exported G-code reproduces
exactly this part in CAMotics.

---

## 8. System Architecture

```
[ C# WPF App ]          [ Python CLI / notebook ]
       \                          /
        \  P/Invoke              /  ctypes
         v                      v
        [   Flat C ABI  (contourcam_c_api.h)   ]
                        |
                 [  C++20 Core lib  ]
        DXF/STEP import (OCCT)  ->  geometry model
        ->  offset / contour / pocket / drill algorithms
        ->  toolpath model  ->  G-code post-processor
                        |
                [  OpenCASCADE 8.0  ]
```

**Principles**
- The core is GUI-free, deterministic, and unit-tested — the single source of truth for all
  geometry/CAM logic.
- C# and Python are thin consumers; they **never** reimplement geometry — they call the core.
- The C ABI is the contract; designed once and frozen for consumers.

---

## 9. Functional Requirements

### Core (C++)
- **FR-C1.** Parse DXF entities: LINE, ARC, CIRCLE, LWPOLYLINE, POLYLINE (documented subset).
- **FR-C2.** Import STEP (2D profile faces) via OCCT (secondary).
- **FR-C3.** Assemble connected edges into closed wires; heal gaps/dups (OCCT ShapeFix); detect islands.
- **FR-C4.** Tool model: diameter, flute count, type (end mill / drill); stock/material params.
- **FR-C5.** Contour toolpath with cutter-radius compensation (inside/outside/on); climb vs conventional.
- **FR-C6.** Pocket clearing via inward concentric offsets with island avoidance + stepover.
- **FR-C7.** Drilling cycles at detected circle centers (peck optional, stretch).
- **FR-C8.** 2.5D depth passes: repeat XY paths at descending Z by step-down.
- **FR-C9.** Toolpath model: ordered segments {rapid | feed | arcCW | arcCCW} with X/Y/Z, arc I/J, feed.
- **FR-C10.** G-code post: configurable header/footer, units, spindle, feeds/speeds, tool change, coolant.

### C ABI
- **FR-A1.** `load_dxf` / `load_step` → opaque document handle + status code.
- **FR-A2.** Set tool/job/post params via blittable POD structs.
- **FR-A3.** `generate_toolpath(doc, params)` → opaque toolpath handle.
- **FR-A4.** Query `segment_count`, then copy segments into a caller buffer (two-call pattern).
- **FR-A5.** `export_gcode(toolpath, path, post_params)`.
- **FR-A6.** Explicit `free` for each handle; `last_error_message()` for diagnostics.

### C# WPF App
- **FR-U1.** Open DXF/STEP via file dialog; show errors gracefully.
- **FR-U2.** 2D viewport (Canvas/DrawingVisual): pan/zoom, render geometry.
- **FR-U3.** Tool & job parameter panel (diameter, depth, stepover, feeds, climb direction).
- **FR-U4.** Generate → overlay toolpath (distinct colors: rapids vs cuts vs drill).
- **FR-U5.** Export G-code; save/load job as JSON (secondary).
- **FR-U6.** Simple tool library (secondary).

### Python
- **FR-P1.** ctypes wrapper exposing the same operations.
- **FR-P2.** Batch CLI: process a folder of DXFs → G-code files.
- **FR-P3.** Example notebook; parity assertion vs the C# app output.

---

## 10. Non-Functional Requirements
- **NFR-1 Performance.** Toolpath generation for the MVP sample part completes in **< 2 s** on a
  typical developer laptop; app cold-start **< 3 s**; the core handles drawings of up to ~5,000
  entities without freezing the UI (generation runs off the UI thread).
- **NFR-2 Reliability.** Malformed, empty, or unsupported input yields a clear error message and
  **never crashes** the app; every ABI call returns a status code.
- **NFR-3 Usability.** The primary workflow (open → set tool → generate → export) is reachable from a
  single window with sensible defaults pre-filled (the §7 params).
- **NFR-4 Maintainability.** Modular core (separate parsing / geometry / toolpath / post modules); the
  C ABI is documented; code is clang-format-clean; CI enforces build + tests.
- **NFR-5 Portability.** The C++ core is cross-platform (Windows + Linux); the WPF app is Windows-only
  (accepted, documented).
- **NFR-6 Determinism.** For identical input + params the core produces byte-identical G-code — this
  is what makes the C#↔Python parity test (M4) meaningful.
- **NFR-7 Security.** Treat DXF/STEP files as untrusted input: validate entity fields, bound all
  buffers marshaled across the C ABI, avoid unbounded reads, no network access (a local desktop tool).
- **NFR-8 Documentation.** README + architecture diagram + build steps + honest scope statement;
  public-repo quality.

---

## 11. Technical Requirements / Stack
- **C++20** (C++17 minimum for OCCT 8.0). Compiled as a native shared lib (`.dll` / `.so`).
- **OpenCASCADE Technology 8.0:** import (`STEPControl_Reader`), 2D offsets
  (`BRepOffsetAPI_MakeOffset` / Geom2d offsets), healing (`ShapeFix`), topology (`TopoDS`/`BRep`).
- **DXF:** hand-rolled minimal reader (shows authored C++ + reuses DXF knowledge; also avoids
  libdxfrw's GPL license — see §20); `libdxfrw` only as an optional fallback.
- **C ABI:** flat `extern "C"` header, POD structs only, opaque handles.
- **C# / .NET 8 WPF;** interop via **P/Invoke** (`DllImport`). (C++/CLI is an alternative; P/Invoke preferred — simpler.)
- **Python 3.11+** via **ctypes** (thin pybind11 optional).
- **Build:** CMake + vcpkg manifest (occt, gtest) for the core; dotnet SDK solution for the app.
- **CI:** GitHub Actions — core on `windows-latest` + `ubuntu-latest`; app on `windows-latest`.
- **Tests:** GoogleTest (core), small xUnit/console smoke (C#), pytest parity (Python).
- **Format/quality:** clang-format, .editorconfig, warnings-as-errors where sane.

---

## 12. Build Prerequisites (developer setup)
**Windows (full project):**
- Windows 10/11 x64.
- Visual Studio 2022 (or Build Tools) with **"Desktop development with C++"** (MSVC v143) and
  **".NET desktop development"** workloads.
- CMake **≥ 3.21** (presets + vcpkg toolchain).
- vcpkg (manifest mode) for `occt` and `gtest`.
- .NET 8 SDK.
- Python 3.11+ (for the automation layer).
- Git.
- *(Optional, for verification)* CAMotics — independent CNC simulator.

**Linux (core + tests only):**
- GCC **≥ 11** or Clang **≥ 14** (C++20), CMake ≥ 3.21, vcpkg, Python 3.11+.
- The WPF app does not build on Linux (Windows-only).

> First build note: vcpkg builds OCCT from source the first time — expect a long initial build;
> enable vcpkg binary caching to avoid repeating it (see §19 R3).

---

## 13. Interop Hard Rules (the #1 correctness risk — must follow)
- **No C++/OCCT exception may cross the C ABI:** wrap **every** `extern "C"` function in try/catch → status code.
- **Ownership:** every returned handle/buffer has an explicit free function; document who owns what.
- **Variable-length data (segments):** two-call pattern (get count, then fill caller buffer) **or**
  core-allocates + dedicated free function. Never return raw `std::` containers across the boundary.
- **ABI stability:** only POD/blittable structs, fixed-width types, explicit calling convention; x64 only.
- **Native lib resolution:** ensure the `.dll` is copied next to the C# app / resolved via `NativeLibrary`;
  match bitness (x64 on both sides).

---

## 14. Key Algorithms (the authored "heart")
- **2D offset / cutter compensation:** offset a closed wire by the tool radius; handle direction
  (climb/conventional); prune self-intersections that appear on concave regions.
- **Pocket clearing:** repeatedly offset the boundary inward by the stepover (≤ tool diameter) until
  the area is exhausted; subtract islands; order rings inside-out or outside-in; link rings with feed moves.
- **Drilling:** detect CIRCLE entities ≈ tool diameter → point cycle at centers.
- **Depth stepping (2.5D):** loop the XY toolpath over Z levels from top to target depth by step-down.
- **Lead-in/out:** tangent arc onto the contour to avoid dwell marks (secondary).
- **Path linking/ordering:** minimize rapids; retract to safe Z between disjoint regions.

> **Risk:** robust offsetting/pocketing is hard (islands merge/split). **Mitigate:** lean on OCCT
> offset APIs, constrain sample geometry, and add strong unit tests on simple shapes first.

---

## 15. Data Model (illustrative)
- `ToolParams { double diameter_mm; int flutes; enum type; }`
- `JobParams { double target_depth_mm; double step_down_mm; double stepover_frac; double feed; double plunge_feed; double spindle_rpm; enum direction; enum strategy; }`
- `Segment { enum kind; double x, y, z; double i, j; double feed; }`  // POD, crosses the ABI
- `Toolpath { vector<Segment>; }`  // internal; exposed via count + fill
- `PostParams { enum units (G20/G21); bool coolant; char header[]; ... }`

---

## 16. G-code Dialect & Coordinate Conventions
- **Standard:** a widely compatible **RS-274 / ISO 6983** subset, targeting **LinuxCNC / GRBL**-style
  interpreters (and loadable in CAMotics). Not a vendor-specific post (see NG2).
- **Modal defaults emitted:** `G21` (mm), `G90` (absolute), `G17` (XY arc plane), `G54` (work offset).
- **Coordinate system:** WCS origin at the part **lower-left corner**; **Z = 0 at top of stock**;
  positive Z up; **safe/retract Z = +5 mm**.
- **Motion:** `G0` rapids, `G1` feed, `G2`/`G3` clockwise/counter-clockwise arcs using **I/J** centers.
- **Spindle/tool/coolant:** `M3`/`M5` spindle on-CW/off, `M6` tool change (with `T`), `M8`/`M9`
  coolant on/off; program end `M30`.
- **Feeds:** cutting feed and plunge feed from `JobParams`; spindle rpm from `JobParams`.
- **Units handling:** if a DXF declares inches, convert to mm (default) or emit `G20` accordingly;
  the assumed/used units are documented in the output header.

---

## 17. Implementation Phases (dependency-ordered; sizes S/M/L, not calendar)
- **Phase 0 — Setup & PROVE THE BRIDGE [M] (foundational):** repo, CMake + vcpkg (occt, gtest),
  .NET 8 WPF solution, CI, clang-format, LICENSE, README skeleton. Hello-world: C++ exports an
  `extern "C"` function; C# P/Invokes it **and** Python ctypes calls it. De-risks interop before
  building the core.
- **Phase 1 — Core: geometry ingestion [M] (after P0):** DXF reader → OCCT wires; (STEP optional);
  contour assembly + ShapeFix healing; island detection; GoogleTest with sample DXFs.
- **Phase 2 — Core: toolpath algorithms [L] (after P1; the heart):** tool/stock model; offset engine;
  contour with radius comp; pocket clearing + island avoidance; drilling; toolpath model; tests
  (offset distance/closure, pocket coverage, no-gouge).
- **Phase 3 — Core: G-code + finalize C ABI [M] (after P2):** configurable ISO post (G0/1/2/3, feeds,
  tool change, coolant); 2.5D step-downs; path ordering/linking, retracts, lead-in/out; **freeze** the
  flat C ABI for consumers.
- **Phase 4 — C# WPF app [L] (after P3; the product):** file open, viewport (geometry + colored
  toolpath overlay), tool/job panel, generate, export G-code (save/load job, tool library = secondary).
  P/Invoke marshaling of segment arrays.
- **Phase 5 — Python automation + polish [M] (after P3; parallelizable with P4):** ctypes batch CLI +
  notebook + parity test; docs (README GIFs, architecture diagram, samples, build steps, ISO/G-code
  refs); CI badge; stretch (sim/animation, material CSV, blog post).

> **MVP milestone = Phases 0–4** on the single sample part. Phase 5 + secondary/stretch = bonus.

---

## 18. Testing & Verification
- **Unit (GoogleTest):** DXF parse correctness; wire assembly; offset distance + closure; pocket-area
  coverage; drilling-center detection; G-code lexable/parse-back.
- **Interop:** a C# smoke test + a Python ctypes test call the core and validate identical results.
- **External:** export G-code → load in **CAMotics** (or NC Viewer) → visually confirm the correct
  part = the strongest evidence it actually works.
- **Parity (M4):** the C# app vs Python produce identical toolpaths/G-code for the same input.
- **CI gates:** build + unit tests on every push; artifacts (sample G-code) uploaded.

---

## 19. Risks & Mitigations
- **R1 — C++/C# interop bugs (crashes/leaks) — HIGHEST.** Mitigate: interop hard rules (§13) + prove
  the bridge in Phase 0.
- **R2 — Robust 2D offsetting/pocketing.** Mitigate: use OCCT offset APIs, simple sample geometry
  first, prune self-intersections, focused tests.
- **R3 — OCCT via vcpkg builds from source = slow/large CI.** Mitigate: vcpkg binary caching or prebuilt OCCT.
- **R4 — WPF is Windows-only.** Accept: app CI is Windows-only; Linux CI builds the core only.
- **R5 — Units (mm/inch) mismatch.** Mitigate: detect/assume + emit G20/G21; document.
- **R6 — Scope creep / not finishing (would HURT the CV).** Mitigate: strict MVP cut-line;
  secondary/stretch clearly optional.
- **R7 — Timeline: 1–2 months solo while working is aggressive.** Mitigate: MVP-first; treat ~3 months
  as the comfortable full-scope target.
- **R8 — Sample data.** Mitigate: hand-make simple DXFs / export from FreeCAD.

---

## 20. License & Third-Party Dependencies
- **Own code license:** **MIT** (or Apache-2.0) — permissive, recruiter-friendly, fine for a portfolio.
- **OpenCASCADE 8.0:** **LGPL-2.1 with exception.** Use it via **dynamic linking** so the project's own
  code can remain MIT/Apache-2.0; ship/credit OCCT per its license.
- **libdxfrw:** **GPLv2** — copyleft. Using it would force the whole project to GPL, so it is **avoided**;
  the hand-rolled DXF reader sidesteps this entirely (a real reason behind decision A3).
- **GoogleTest:** BSD-3-Clause. **.NET / WPF:** MIT. **pybind11 (if used):** BSD. — all permissive.
- **CAMotics:** GPL, but used only as an **external** verification tool (not linked/distributed), so it
  imposes no license obligation on this project.
- **Action:** add a `LICENSE` file (MIT) + a `THIRD_PARTY_NOTICES` note crediting OCCT and others.

---

## 21. Repository Structure
```
ContourCAM/
├─ core/
│  ├─ include/contourcam_c_api.h     # the single C ABI boundary
│  ├─ src/                           # geometry, dxf_reader, step_reader, offset,
│  │                                 # toolpath{contour,pocket,drill}, postproc
│  └─ tests/                         # gtest + sample-based
├─ app-csharp/                       # .NET 8 WPF: P/Invoke wrappers, viewport, viewmodels
├─ automation-python/                # ctypes wrapper, batch CLI, notebook, parity tests
├─ samples/                          # plate+pocket+holes.dxf, gasket.dxf, optional .step
├─ docs/                             # architecture diagram, screenshots/GIFs
├─ cmake/
├─ CMakeLists.txt
├─ vcpkg.json
├─ ContourCAM.sln
├─ .github/workflows/ci.yml
├─ README.md
├─ LICENSE
├─ THIRD_PARTY_NOTICES.md
├─ .clang-format
└─ .editorconfig
```

---

## 22. Final Outcome / Deliverables
- A public GitHub repo with: the native C++ CAM core, the C# WPF desktop app, the Python automation,
  and full CI.
- A working demo: load a sample drawing → generate a toolpath → export G-code → validated in CAMotics.
- A README with an architecture diagram, an app GIF, build instructions, and an honest scope statement.
- A CV bullet (§23) + an interview-ready artifact demonstrating C++ + C#/.NET interop + CAD/CAM domain.

---

## 23. Résumé Framing
> Add to the CV after the MVP is done; keep the PDF in sync.

"Built ContourCAM, a 2.5D CAM toolpath generator with a layered architecture: a native C++20
geometry/CAM core on OpenCASCADE (DXF/STEP import, radius-compensated contour & pocket-clearing
toolpaths with island avoidance, configurable ISO G-code) exposed via a flat C ABI; a C#/.NET 8 WPF
desktop app (drawing + live toolpath viewport, G-code export) consuming the core via P/Invoke; and a
Python automation layer for batch processing. CMake + vcpkg + GitHub Actions CI, GoogleTest; output
verified in an independent CNC simulator (CAMotics)."

**Framing rule:** present as a demonstration of C++/CAD-kernel + C#/.NET interop + manufacturing
domain — **not** production/vendor-grade CAM.

---

## 24. Assumptions & Decisions
- **A1.** Keep all three layers; Python strictly secondary (Phase 5).
- **A2.** MVP-first; full scope ≈ 3 months part-time, or MVP-only in 1–2 months. *(timeline still open)*
- **A3.** Hand-rolled DXF reader (documented subset) over libdxfrw — also avoids GPL (see §20).
- **A4.** P/Invoke over the flat C API (not C++/CLI).
- **A5.** Project name: ContourCAM.
- **A6.** Units default to mm (G21).
- **A7.** Location: `C:\Users\fk6147\ContourCAM`, outside OneDrive.
- **A8.** Own-code license: MIT; OCCT used via dynamic linking (see §20).

---

## 25. Glossary
- **CAM** — Computer-Aided Manufacturing: software that produces machining instructions from geometry.
- **CNC** — Computer Numerical Control: the controller that drives a machine tool.
- **G-code (RS-274 / ISO 6983)** — the text instruction language CNC machines execute.
- **DXF** — Drawing Exchange Format: a common 2D CAD vector format (entities like LINE/ARC/CIRCLE).
- **STEP (ISO 10303)** — a neutral 3D/2D CAD exchange format.
- **B-rep** — Boundary Representation: how solids/faces/edges are modeled in a CAD kernel.
- **OCCT** — OpenCASCADE Technology, the open-source C++ CAD geometry kernel used here.
- **ABI** — Application Binary Interface: the binary contract between the core and its callers.
- **C ABI / `extern "C"`** — a flat, language-neutral binary interface callable from C#, Python, etc.
- **P/Invoke** — .NET's mechanism to call native C functions from C#.
- **ctypes** — Python's standard library for calling native C functions.
- **WPF** — Windows Presentation Foundation: the .NET desktop UI framework for the app.
- **Toolpath** — the ordered sequence of moves the cutting tool follows.
- **Contour (profile)** — cutting along a part's outline.
- **Pocket clearing** — removing all material inside a closed region.
- **Stepover** — sideways distance between adjacent cutting passes.
- **Step-down** — vertical depth removed per Z level (the "2.5D" in the project).
- **Climb vs conventional milling** — the two cutter rotation directions relative to feed.
- **Cutter-radius compensation** — offsetting the path by the tool radius so the cut edge is correct.
- **Lead-in / lead-out** — short approach/exit moves (often arcs) onto/off the cut.
- **Rapid vs feed** — fast non-cutting moves (G0) vs controlled cutting moves (G1).
- **Island** — a region left uncut inside a pocket (the tool must avoid it).

---

## 26. Document History
- **v1 (2026-06-13):** Initial 19-section PRD.
- **v2 (2026-06-13):** Added MVP Sample Part Specification (§7), Non-Functional Requirements (§10),
  Build Prerequisites (§12), G-code Dialect & Coordinate Conventions (§16), License & Third-Party
  Dependencies (§20), Glossary (§25), Document History (§26); added naming & location rationale (§0);
  renumbered to 26 sections and updated cross-references.
