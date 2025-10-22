# CMake Toolchain for macOS Universal Binary (x86_64 + arm64)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-universal.cmake -B build-universal

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

# Build for both architectures
message(STATUS "Building macOS Universal Binary (x86_64 + arm64)")
message(STATUS "Minimum macOS version: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
