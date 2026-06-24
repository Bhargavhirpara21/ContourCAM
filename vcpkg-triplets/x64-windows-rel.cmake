# Release-only x64 Windows triplet: halves the (large) OpenCASCADE build by
# skipping the Debug variant, and keeps dynamic linking so OCCT stays a separate
# DLL (LGPL-2.1 dynamic-link compliance, PRD section 20).
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)
