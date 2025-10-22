# CMake Toolchain for Linux ARM64 (aarch64) Cross-Compilation
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake -B build-arm64

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Target environment
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Architecture-specific flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a")

# Install to aarch64-specific directory
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install/linux-aarch64" CACHE PATH "")

message(STATUS "Cross-compiling for Linux ARM64 (aarch64)")
