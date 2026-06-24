# ContourCAM native core -- the GUI-free C++ engine behind the flat C ABI.
# Phase 1: a dependency-free C++ geometry library (DXF ingestion, wire assembly,
# island detection) built as a static lib, consumed by both the shared C ABI and
# the GoogleTest suite. OCCT arrives in Phase 2 for robust offsetting.

# --- Geometry engine (authored, no external dependencies) ------------------
add_library(contourcam_geometry STATIC
    src/geom/dxf_reader.cpp
    src/geom/wire_builder.cpp
    src/geom/part_model.cpp
    src/cam/toolpath.cpp
    src/cam/gcode.cpp
    src/cam/pocket.cpp
)

# Optional OpenCASCADE backend for robust pocket clearing (offset + islands).
# pocket.cpp always compiles; the OCCT code inside is guarded by CONTOURCAM_HAVE_OCCT.
if(CONTOURCAM_USE_OCCT)
    find_package(OpenCASCADE CONFIG REQUIRED)
    target_include_directories(contourcam_geometry PRIVATE ${OpenCASCADE_INCLUDE_DIR})
    # OCCT is an internal detail of pocket.cpp (no public header exposes it), so
    # link PRIVATE; CMake still propagates it to the final exe/dll link.
    target_link_libraries(contourcam_geometry PRIVATE ${OpenCASCADE_LIBRARIES})
    # Keep the macro PUBLIC so consumers' compiles agree on occtEnabled()'s value.
    target_compile_definitions(contourcam_geometry PUBLIC CONTOURCAM_HAVE_OCCT)
    message(STATUS "OpenCASCADE ${OpenCASCADE_VERSION} enabled (pocket clearing)")
endif()

target_include_directories(contourcam_geometry
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_compile_features(contourcam_geometry PUBLIC cxx_std_20)

# Static lib is linked into a shared library, so it must be position-independent.
set_target_properties(contourcam_geometry PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# --- Flat C ABI shared library --------------------------------------------
add_library(contourcam_core SHARED
    src/contourcam_core.cpp
    src/contourcam_abi.cpp
    src/abi_detail.hpp
    include/contourcam_c_api.h
)

target_link_libraries(contourcam_core PRIVATE contourcam_geometry)

target_include_directories(contourcam_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(contourcam_core PUBLIC cxx_std_20)

# CONTOURCAM_BUILD switches the ABI macros to "export" mode while building the
# library itself; consumers that include the header see "import".
target_compile_definitions(contourcam_core PRIVATE CONTOURCAM_BUILD)

set_target_properties(contourcam_core PROPERTIES
    OUTPUT_NAME "contourcam_core"
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
