cmake_minimum_required(VERSION 3.20)

# Set CMake policies for compatibility
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
endif()
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)  # ExternalProject download methods
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)  # FindBoost module removal in CMake 3.30+
endif()

# Suppress developer warnings from third-party packages
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "Suppress developer warnings" FORCE)

project(TopographicGenerator VERSION 0.25.002 LANGUAGES CXX)

# Fix for ranlib "Resource busy" issues on mounted volumes
if(APPLE)
    # Override AR to include symbol table creation and disable separate ranlib call
    set(CMAKE_AR "/usr/bin/ar")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
    # Create a no-op ranlib since ar with 's' flag already adds the symbol table
    set(RANLIB_NOOP "${CMAKE_BINARY_DIR}/ranlib_noop.sh")
    file(WRITE "${RANLIB_NOOP}" "#!/bin/bash\n# No-op ranlib replacement - ar qcs already handles symbol table\nexit 0\n")
    execute_process(COMMAND chmod +x "${RANLIB_NOOP}")
    set(CMAKE_RANLIB "${RANLIB_NOOP}")
    message(STATUS "Using ar qcs with no-op ranlib to avoid mounted volume issues")
endif()

# Modern C++ standard for latest parallel algorithms and concepts
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Use -march=native only for native builds, not cross-compilation
    # For cross-compilation (like QEMU ARM64), use generic optimization
    if(CMAKE_CROSSCOMPILING)
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    else()
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -gdwarf-4 -Wall -Wextra -Wpedantic")
    
    # macOS-specific settings
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
        # Disable adhoc code signing for network-mounted volumes
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-no_adhoc_codesign")
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Project version: ${PROJECT_VERSION}")

# Generate version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

# Options
option(ENABLE_PARALLEL "Enable parallel processing" ON)
option(ENABLE_NURBS "Enable NURBS surface generation" ON)
option(ENABLE_TESTING "Enable unit tests" OFF)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(BUILD_GUI "Build Qt GUI application" ON)

# Find optional libraries (not needed on Windows with vcpkg)
find_package(PkgConfig QUIET)

# CGAL completely removed - using custom geometry types and GDAL/OGR for all geometry operations
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ============================================================================
# Phase 1: Smart Dependency Discovery
# ============================================================================

# Check for build cache tools first
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Found ccache: ${CCACHE_PROGRAM} - enabling build caching")
endif()

# Enable vendored dependencies first (massive performance boost)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/vendors/CMakeLists.txt)
    message(STATUS "Loading vendored dependencies...")
    add_subdirectory(external/vendors)
    vendor_summary()
endif()

# Smart nlohmann/json detection with vendor priority
if(NOT nlohmann_json_FOUND)
    find_package(nlohmann_json QUIET)
    if(NOT nlohmann_json_FOUND)
    # Check for header-only installation
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
        PATHS /usr/include /usr/local/include /opt/homebrew/include
              $ENV{CONDA_PREFIX}/include $ENV{HOMEBREW_PREFIX}/include)

    if(NLOHMANN_JSON_INCLUDE_DIR)
        message(STATUS "Found nlohmann/json headers: ${NLOHMANN_JSON_INCLUDE_DIR}")
        add_library(nlohmann_json INTERFACE)
        target_include_directories(nlohmann_json INTERFACE ${NLOHMANN_JSON_INCLUDE_DIR})
        add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
        set(nlohmann_json_FOUND TRUE)
    else()
        # Fall back to FetchContent only if not found
        include(FetchContent)
        message(STATUS "nlohmann/json not found locally - downloading...")

        FetchContent_Declare(
            nlohmann_json
            GIT_REPOSITORY https://github.com/nlohmann/json.git
            GIT_TAG v3.11.3
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(nlohmann_json)
        set(nlohmann_json_FOUND TRUE)
    endif()
    endif()
else()
    message(STATUS "Found nlohmann/json: using system installation")
endif()

# Eigen for linear algebra
find_package(Eigen3 3.4 REQUIRED NO_MODULE)
if(Eigen3_FOUND)
    message(STATUS "Found Eigen3: ${Eigen3_VERSION}")
else()
    message(FATAL_ERROR "Eigen3 not found - required for linear algebra")
endif()

# GDAL for GIS data
find_package(GDAL 3.0 REQUIRED)
if(GDAL_FOUND)
    message(STATUS "Found GDAL: ${GDAL_VERSION}")
else()
    message(FATAL_ERROR "GDAL not found - required for elevation data")
endif()

# FreeType for text rendering in raster outputs
find_package(Freetype REQUIRED)
if(Freetype_FOUND)
    message(STATUS "Found FreeType: ${FREETYPE_VERSION_STRING}")
else()
    message(FATAL_ERROR "FreeType not found - required for PNG text rendering")
endif()

# CGAL for robust polygon triangulation
find_package(CGAL QUIET COMPONENTS Core)
if(CGAL_FOUND)
    message(STATUS "Found CGAL: ${CGAL_VERSION}")
    # CGAL requires GMP and MPFR
    find_package(GMP REQUIRED)
    find_package(MPFR REQUIRED)
    if(GMP_FOUND)
        message(STATUS "Found GMP: ${GMP_LIBRARIES}")
    endif()
    if(MPFR_FOUND)
        message(STATUS "Found MPFR: ${MPFR_LIBRARIES}")
    endif()
else()
    message(WARNING "CGAL not found - advanced triangulation disabled, will use fallback")
endif()

# libcurl for HTTP downloads
find_package(CURL REQUIRED)
if(CURL_FOUND)
    message(STATUS "Found libcurl: ${CURL_VERSION_STRING}")
else()
    message(FATAL_ERROR "libcurl not found - required for SRTM tile downloads")
endif()

# OpenSSL for cryptographic functions (MD5 hashing in OSM cache)
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
else()
    message(FATAL_ERROR "OpenSSL not found - required for OSM tile caching")
endif()

# zlib for gzip decompression
find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
    message(STATUS "Found zlib: ${ZLIB_VERSION_STRING}")
else()
    message(FATAL_ERROR "zlib not found - required for SRTM tile decompression")
endif()

# Qt6 for GUI application (optional)
if(BUILD_GUI)
    find_package(Qt6 6.5 QUIET COMPONENTS Core Widgets Svg SvgWidgets Network PrintSupport WebEngineWidgets WebChannel)
    if(Qt6_FOUND)
        message(STATUS "Found Qt6: ${Qt6_VERSION}")
        message(STATUS "Qt6 Components: Core Widgets Svg SvgWidgets Network PrintSupport WebEngineWidgets WebChannel")
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)
    else()
        message(WARNING "Qt6 not found - GUI will not be built. Install Qt6 or set BUILD_GUI=OFF")
        set(BUILD_GUI OFF)
    endif()
else()
    message(STATUS "GUI build disabled (BUILD_GUI=OFF)")
endif()

# PROJ coordinate transformations disabled - using WGS84 coordinates throughout
message(STATUS "PROJ coordinate transformations disabled - using WGS84 coordinates")
add_definitions(-DNO_PROJ)

# Threading libraries
if(ENABLE_PARALLEL)
    find_package(TBB QUIET)
    
    # Only call find_package(OpenMP) on non-Apple systems  
    if(NOT APPLE)
        find_package(OpenMP QUIET)
    endif()
    
    # Force OpenMP on macOS using LLVM libomp (bypass find_package issues)
    if(APPLE AND NOT OpenMP_FOUND)
        # Check for LLVM's OpenMP first, then fallback to standalone libomp
        set(LLVM_OPENMP_LIB "/opt/homebrew/Cellar/llvm/21.1.2/lib/libomp.dylib")
        set(LLVM_OPENMP_INC "/opt/homebrew/Cellar/llvm/21.1.2/include")
        set(STANDALONE_OPENMP_LIB "/opt/homebrew/Cellar/libomp/21.1.2/lib/libomp.dylib")
        set(STANDALONE_OPENMP_INC "/opt/homebrew/Cellar/libomp/21.1.2/include")
        
        if(EXISTS "${LLVM_OPENMP_LIB}" AND EXISTS "${LLVM_OPENMP_INC}/omp.h")
            message(STATUS "Using LLVM OpenMP for macOS")
            set(CHOSEN_OPENMP_LIB "${LLVM_OPENMP_LIB}")
            set(CHOSEN_OPENMP_INC "${LLVM_OPENMP_INC}")
        elseif(EXISTS "${STANDALONE_OPENMP_LIB}" AND EXISTS "${STANDALONE_OPENMP_INC}/omp.h")
            message(STATUS "Using standalone OpenMP for macOS")
            set(CHOSEN_OPENMP_LIB "${STANDALONE_OPENMP_LIB}")
            set(CHOSEN_OPENMP_INC "${STANDALONE_OPENMP_INC}")
        endif()
        
        if(CHOSEN_OPENMP_LIB)
            set(OpenMP_FOUND TRUE)
            set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
            set(OpenMP_CXX_LIB_NAMES "omp")
            set(OpenMP_CXX_LIBRARIES "${CHOSEN_OPENMP_LIB}")
            set(OpenMP_CXX_INCLUDE_DIRS "${CHOSEN_OPENMP_INC}")
            
            # Force override cached variables that might have incorrect paths
            set(OpenMP_omp_LIBRARY "${CHOSEN_OPENMP_LIB}" CACHE FILEPATH "OpenMP omp library" FORCE)
            set(OpenMP_CXX_LIB_NAMES "omp" CACHE STRING "OpenMP library names" FORCE)
            
            add_definitions(-DHAVE_OPENMP)
            
            # Create the OpenMP target manually
            if(NOT TARGET OpenMP::OpenMP_CXX)
                add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
                set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_COMPILE_OPTIONS 
                    "-Xpreprocessor" "-fopenmp")
                set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_INCLUDE_DIRECTORIES
                    "${CHOSEN_OPENMP_INC}")
                set_property(TARGET OpenMP::OpenMP_CXX PROPERTY INTERFACE_LINK_LIBRARIES
                    "${CHOSEN_OPENMP_LIB}")
            endif()
        endif()
    endif()
    
    if(TBB_FOUND)
        message(STATUS "Found Intel TBB: ${TBB_VERSION}")
        add_definitions(-DHAVE_TBB)
    else()
        message(WARNING "Intel TBB not found - falling back to OpenMP")
    endif()
    
    if(OpenMP_FOUND)
        message(STATUS "Found OpenMP: ${OpenMP_VERSION}")
        add_definitions(-DHAVE_OPENMP)
    else()
        message(STATUS "OpenMP not found - using sequential processing")
        
        # On macOS, try to find libomp manually in known Cellar location
        if(APPLE)
            # Check the exact version path first
            if(EXISTS "/opt/homebrew/Cellar/libomp/21.1.2/include/omp.h")
                set(LIBOMP_INCLUDE_DIR "/opt/homebrew/Cellar/libomp/21.1.2/include")
                set(LIBOMP_LIBRARY "/opt/homebrew/Cellar/libomp/21.1.2/lib/libomp.dylib")
            else()
                # Fallback to wildcard search
                find_path(LIBOMP_INCLUDE_DIR omp.h
                    PATHS 
                        /opt/homebrew/Cellar/libomp/*/include
                        /usr/local/Cellar/libomp/*/include
                    NO_DEFAULT_PATH)
                find_library(LIBOMP_LIBRARY NAMES omp
                    PATHS 
                        /opt/homebrew/Cellar/libomp/*/lib
                        /usr/local/Cellar/libomp/*/lib
                    NO_DEFAULT_PATH)
            endif()
            
            if(LIBOMP_INCLUDE_DIR AND LIBOMP_LIBRARY AND EXISTS "${LIBOMP_INCLUDE_DIR}/omp.h")
                message(STATUS "Found libomp manually:")
                message(STATUS "  Include: ${LIBOMP_INCLUDE_DIR}")
                message(STATUS "  Library: ${LIBOMP_LIBRARY}")
                set(OpenMP_FOUND TRUE)
                add_definitions(-DHAVE_OPENMP)
                set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
                set(OpenMP_CXX_LIBRARIES "${LIBOMP_LIBRARY}")
                set(OpenMP_CXX_INCLUDE_DIRS "${LIBOMP_INCLUDE_DIR}")
                
                # Create OpenMP targets for compatibility
                if(NOT TARGET OpenMP::OpenMP_CXX)
                    add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
                    target_compile_options(OpenMP::OpenMP_CXX INTERFACE "-Xpreprocessor" "-fopenmp")
                    target_include_directories(OpenMP::OpenMP_CXX INTERFACE "${LIBOMP_INCLUDE_DIR}")
                    target_link_libraries(OpenMP::OpenMP_CXX INTERFACE "${LIBOMP_LIBRARY}")
                endif()
            else()
                message(STATUS "libomp paths not found. Searched:")
                message(STATUS "  /opt/homebrew/Cellar/libomp/21.1.2/include")
                message(STATUS "  /opt/homebrew/Cellar/libomp/*/include")
            endif()
        endif()
    endif()
endif()

# ============================================================================
# Smart libigl Discovery and Configuration
# ============================================================================

# Vendor priority: libigl is already loaded if vendored
if(NOT libigl_FOUND)
    # Check for precompiled libigl first (Homebrew, system packages, conda)
    find_package(libigl QUIET)

    if(NOT libigl_FOUND)
        # Check for header-only libigl installation
        find_path(LIBIGL_INCLUDE_DIR igl/igl_inline.h
        PATHS /usr/include /usr/local/include /opt/homebrew/include
              $ENV{CONDA_PREFIX}/include $ENV{HOMEBREW_PREFIX}/include
        PATH_SUFFIXES libigl)

        if(LIBIGL_INCLUDE_DIR)
            message(STATUS "Found libigl headers: ${LIBIGL_INCLUDE_DIR}")
            # Create header-only target
            add_library(igl_core INTERFACE)
            target_include_directories(igl_core INTERFACE ${LIBIGL_INCLUDE_DIR})
            add_library(igl::core ALIAS igl_core)
            set(libigl_FOUND TRUE)
        else()
            # Only download if absolutely necessary and no precompiled version available
            message(STATUS "libigl not found locally - checking if download is needed...")

            # Check if this project actually needs libigl features (header analysis)
            file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp" "src/*.hpp" "include/*.hpp")
            set(NEEDS_LIBIGL FALSE)
            foreach(SOURCE_FILE ${PROJECT_SOURCES})
                file(READ ${SOURCE_FILE} FILE_CONTENT)
                if(FILE_CONTENT MATCHES "#include.*igl/")
                    set(NEEDS_LIBIGL TRUE)
                    break()
                endif()
            endforeach()

            if(NEEDS_LIBIGL)
                include(FetchContent)
                message(STATUS "Project requires libigl - downloading optimized configuration...")

                # Optimized libigl configuration - header-only with existing dependencies
                set(LIBIGL_RESTRICTED_PYTHON OFF CACHE BOOL "Build libigl python bindings" FORCE)
                # Enable CGAL for mesh repair operations (remesh_self_intersections)
                set(LIBIGL_WITH_CGAL ON CACHE BOOL "CGAL integration for mesh repair" FORCE)
                set(LIBIGL_WITH_OPENGL OFF CACHE BOOL "Use OpenGL in libigl" FORCE)
                set(LIBIGL_WITH_OPENGL_GLFW OFF CACHE BOOL "Use GLFW in libigl" FORCE)
                set(LIBIGL_WITH_PNG OFF CACHE BOOL "Use PNG in libigl" FORCE)
                set(LIBIGL_WITH_TETGEN OFF CACHE BOOL "Use TetGen in libigl" FORCE)
                set(LIBIGL_WITH_TRIANGLE OFF CACHE BOOL "Use Triangle in libigl" FORCE)
                set(LIBIGL_WITH_PREDICATES OFF CACHE BOOL "Use Predicates in libigl" FORCE)
                set(LIBIGL_WITH_XML OFF CACHE BOOL "Use XML in libigl" FORCE)
                set(LIBIGL_INSTALL OFF CACHE BOOL "Install libigl" FORCE)
                set(LIBIGL_USE_STATIC_LIBRARY OFF CACHE BOOL "Use header-only libigl" FORCE)

                FetchContent_Declare(
                    libigl
                    GIT_REPOSITORY https://github.com/libigl/libigl.git
                    GIT_TAG v2.5.0
                    GIT_SHALLOW TRUE
                )
                FetchContent_MakeAvailable(libigl)
                set(libigl_FOUND TRUE)
                message(STATUS "Downloaded libigl with optimized configuration")
            else()
                message(STATUS "Project doesn't use libigl features - skipping download")
                set(libigl_FOUND FALSE)
            endif()
        endif()
    endif()
else()
    message(STATUS "Found libigl: using system installation")
endif()

if(libigl_FOUND)
    message(STATUS "libigl enabled - advanced mesh operations available")
    add_definitions(-DHAVE_LIBIGL)
else()
    message(STATUS "libigl disabled - using basic mesh operations")
endif()

# Optional: HDF5 for high-performance data storage (disabled for now)
# find_package(HDF5 QUIET COMPONENTS CXX)
# if(HDF5_FOUND)
#     message(STATUS "Found HDF5: ${HDF5_VERSION}")
#     add_definitions(-DHAVE_HDF5)
# else()
    message(STATUS "HDF5 support disabled - using standard file I/O")
# endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# External dependencies (disabled - using built-in alternatives)
# add_subdirectory(external)

# Core library
add_subdirectory(src/core)

# Export modules
add_subdirectory(src/export)

# CLI interface
add_subdirectory(src/cli)

# GUI interface (if Qt6 is available)
if(BUILD_GUI AND Qt6_FOUND)
    add_subdirectory(src/gui)
endif()

# Main executable
add_executable(topo-gen
    src/main.cpp
)

target_link_libraries(topo-gen
    PRIVATE
    TopoCLI
    TopoCore
    TopoExport
)

# Link OpenMP if available
if(OpenMP_FOUND AND TARGET OpenMP::OpenMP_CXX)
    target_link_libraries(topo-gen PRIVATE OpenMP::OpenMP_CXX)
endif()

# macOS: Remove extended attributes and code sign executable after build
# This prevents "Code Signature Invalid" errors when building on network volumes
# Strategy: Copy to /tmp (local volume), clean and sign there, then copy back
if(APPLE)
    add_custom_command(TARGET topo-gen POST_BUILD
        COMMAND cp $<TARGET_FILE:topo-gen> /tmp/topo-gen-temp
        COMMAND xattr -cr /tmp/topo-gen-temp
        COMMAND codesign -s - -f /tmp/topo-gen-temp
        COMMAND cp /tmp/topo-gen-temp $<TARGET_FILE:topo-gen>
        COMMAND rm /tmp/topo-gen-temp
        COMMENT "Code-signing executable (via /tmp to avoid network volume issues)"
        VERBATIM
    )
endif()

# Set installation targets
install(TARGETS topo-gen
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/topographic-generator
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Testing (only if tests directory exists)
if(ENABLE_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
elseif(ENABLE_TESTING)
    message(STATUS "Testing enabled but tests directory not found - skipping tests")
endif()

# Benchmarks (only if benchmarks directory exists)
if(BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks")
    add_subdirectory(benchmarks)
elseif(BUILD_BENCHMARKS)
    message(STATUS "Benchmarks enabled but benchmarks directory not found - skipping benchmarks")
endif()

# Documentation (only if docs directory has CMakeLists.txt and Doxygen is found)
find_package(Doxygen QUIET)
if(Doxygen_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs/CMakeLists.txt")
    add_subdirectory(docs)
endif()

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TopographicGeneratorConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Parallel processing: ${ENABLE_PARALLEL}")
message(STATUS "  NURBS support: ${ENABLE_NURBS}")
message(STATUS "  GUI application: ${BUILD_GUI}")
message(STATUS "  Testing: ${ENABLE_TESTING}")
message(STATUS "")