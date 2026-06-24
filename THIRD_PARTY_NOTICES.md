# Third-Party Notices

ContourCAM's own source code is licensed under the MIT License (see `LICENSE`).
It uses the following third-party components. Each remains under its own license.

## GoogleTest
- **Used for:** the C++ unit-test suite only (not shipped in the runtime library).
- **License:** BSD 3-Clause.
- **Source:** https://github.com/google/googletest
- Fetched at configure time via CMake `FetchContent`; not distributed with the
  ContourCAM binaries.

## OpenCASCADE Technology (OCCT) 8.0.0
- **Used for:** robust 2D offsetting / pocket clearing (`core/src/cam/pocket.cpp`).
  Only compiled into the optional `CONTOURCAM_USE_OCCT` build; the default build
  does not depend on OCCT.
- **License:** LGPL-2.1 **with the Open CASCADE exception**.
- **Source:** https://github.com/Open-Cascade-SAS/OCCT (built via vcpkg).
- **Linking:** **dynamic** (separate, replaceable `TK*.dll` shared libraries, see
  `vcpkg-triplets/x64-windows-rel.cmake`), so ContourCAM's own MIT-licensed code
  is not subject to copyleft. The OCCT license texts (`LICENSE_LGPL_21.txt` +
  `OCCT_LGPL_EXCEPTION.txt`) ship with the OCCT distribution and are reproduced
  alongside any binary release that bundles the OCCT DLLs.
