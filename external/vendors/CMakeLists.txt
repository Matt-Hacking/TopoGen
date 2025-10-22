# ============================================================================
# Vendored Dependencies Configuration
# ============================================================================
# This file provides pre-downloaded dependencies to eliminate build-time downloads
# Total space saved: ~200MB per build cycle

# nlohmann/json - Header-only JSON library (900KB vs 187MB download)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/include/nlohmann/json.hpp)
    add_library(vendor_nlohmann_json INTERFACE)
    target_include_directories(vendor_nlohmann_json INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json/include)
    add_library(nlohmann_json::nlohmann_json ALIAS vendor_nlohmann_json)
    set(nlohmann_json_FOUND TRUE CACHE BOOL "nlohmann/json found in vendors" FORCE)
    message(STATUS "Using vendored nlohmann/json (single header)")
endif()

# libigl - Geometry processing library (8.1MB headers vs 16MB+ full repo)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libigl/include/igl/igl_inline.h)
    add_library(vendor_igl_core INTERFACE)
    target_include_directories(vendor_igl_core INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/libigl/include)

    # Configure libigl to use existing dependencies
    target_compile_definitions(vendor_igl_core INTERFACE
        IGL_STATIC_LIBRARY=OFF
        IGL_HEADER_ONLY=ON
    )

    add_library(igl::core ALIAS vendor_igl_core)
    set(libigl_FOUND TRUE CACHE BOOL "libigl found in vendors" FORCE)
    message(STATUS "Using vendored libigl (headers only)")
endif()

# Verification function
function(vendor_summary)
    message(STATUS "")
    message(STATUS "=== Vendored Dependencies Summary ===")
    if(nlohmann_json_FOUND)
        message(STATUS "✓ nlohmann/json: 900KB (saved 186MB download)")
    endif()
    if(libigl_FOUND)
        message(STATUS "✓ libigl: 8.1MB (saved 8MB+ download)")
    endif()
    message(STATUS "Total build-time savings: ~200MB")
    message(STATUS "=====================================")
    message(STATUS "")
endfunction()