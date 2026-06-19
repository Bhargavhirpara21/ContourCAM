# ContourCAM -- top-level build for the native C++ core.
#
# The C# WPF app and the Python automation layer are built with their own
# toolchains (dotnet / pip); CMake only builds the native core + its tests.

cmake_minimum_required(VERSION 3.21)

project(ContourCAM
    VERSION 0.1.0
    DESCRIPTION "2.5D CAM toolpath & G-code generator (native core)"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Put all build artifacts in one predictable place so consumers (C#/Python)
# can find the shared library easily.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)

# Unit tests pull GoogleTest via FetchContent (fast, self-contained); disable
# with -DCONTOURCAM_BUILD_TESTS=OFF for a tests-free build.
option(CONTOURCAM_BUILD_TESTS "Build the C++ unit test suite" ON)
if(CONTOURCAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(core/tests)
endif()
