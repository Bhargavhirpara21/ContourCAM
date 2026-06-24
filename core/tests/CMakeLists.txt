# ContourCAM core unit tests (GoogleTest).
# GoogleTest is fetched at configure time -- no vcpkg/system install required,
# keeping the Phase 1 build self-contained and CI fast.

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
# Use the shared C runtime on MSVC to match the rest of the build.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(contourcam_tests
    test_dxf_reader.cpp
    test_wire_builder.cpp
    test_part_model.cpp
    test_c_abi.cpp
)

target_link_libraries(contourcam_tests
    PRIVATE
        contourcam_geometry   # geometry tested directly
        contourcam_core       # the flat C ABI (shared lib) tested at the boundary
        GTest::gtest_main
)

# Absolute path to the sample DXF directory, for file-based tests.
target_compile_definitions(contourcam_tests
    PRIVATE CONTOURCAM_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/samples"
)

include(GoogleTest)
gtest_discover_tests(contourcam_tests)
