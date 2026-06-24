# Third-Party Notices

ContourCAM's own source code is licensed under the MIT License (see `LICENSE`).
It uses the following third-party components. Each remains under its own license.

## GoogleTest
- **Used for:** the C++ unit-test suite only (not shipped in the runtime library).
- **License:** BSD 3-Clause.
- **Source:** https://github.com/google/googletest
- Fetched at configure time via CMake `FetchContent`; not distributed with the
  ContourCAM binaries.

## OpenCASCADE Technology (OCCT)
- **Status:** *not yet integrated.* Planned for Phase 2 (robust 2D offsetting /
  pocketing and STEP import). This entry will be completed when OCCT is added.
- **Expected license:** LGPL-2.1 with the Open CASCADE exception. OCCT will be
  used via **dynamic linking** so ContourCAM's own code can remain MIT, and the
  OCCT license texts will be reproduced here at that time.
