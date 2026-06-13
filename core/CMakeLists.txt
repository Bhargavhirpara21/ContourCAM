# ContourCAM native core -- the GUI-free C++ engine behind the flat C ABI.
# Phase 0: a hello-world shared library with no external dependencies (OCCT and
# GoogleTest arrive in Phase 1).

add_library(contourcam_core SHARED
    src/contourcam_core.cpp
    include/contourcam_c_api.h
)

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
