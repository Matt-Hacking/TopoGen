# External dependencies for Topographic Generator

# CLI11 for command line parsing
include(FetchContent)

# Set policy for CLI11 compatibility
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

# Use a more stable version of CLI11 that's compatible with our CMake setup
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.2.0
    GIT_SHALLOW TRUE
)

# Set CLI11 options before making it available
set(CLI11_SINGLE_FILE_TESTS OFF CACHE BOOL "")
set(CLI11_BUILD_TESTS OFF CACHE BOOL "")
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CLI11_BUILD_DOCS OFF CACHE BOOL "")

FetchContent_MakeAvailable(cli11)

# Optional: libslic3r integration (if available)
# This would require extracting and adapting specific components
# from Bambu Slicer's libslic3r library

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libslic3r")
    message(STATUS "Found libslic3r components")
    add_subdirectory(libslic3r)
else()
    message(STATUS "libslic3r not found - using custom triangle-plane intersection")
endif()