# Core library for topographic generation

add_library(TopoCore STATIC
    TopographicMesh.cpp
    ElevationProcessor.cpp
    # TrianglePlaneIntersector.cpp  # REMOVED: Dead code - never called in production
    TopographicGenerator.cpp
    SRTMDownloader.cpp
    ContourGenerator.cpp
    HeightmapTriangulator.cpp
    OutputTracker.cpp
    OSMTileCache.cpp
    Logger.cpp
    # GeometryDebugger.cpp  # DISABLED: CGAL-dependent
    MemoryMonitor.cpp
    StackGuard.cpp
    # CGALWrapper.cpp  # DISABLED: CGAL removed from codebase
    CrashHandler.cpp
    # IncrementalProcessor.cpp  # DISABLED: CGAL-dependent (uses CGALWrapper)
    InputValidator.cpp
    ScalingCalculator.cpp
    GeocodeService.cpp
    UnitParser.cpp
    LabelRenderer.cpp
    TextFitter.cpp
    TextPathGenerator.cpp
)

target_include_directories(TopoCore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(TopoCore
    PUBLIC
        # All geometry operations now use custom types and GDAL/OGR (CGAL completely removed)
        Eigen3::Eigen
        GDAL::GDAL
        Freetype::Freetype  # For text rendering in raster exports (headers included transitively)
        CURL::libcurl
        ZLIB::ZLIB
        nlohmann_json::nlohmann_json
        OpenSSL::Crypto
)

# PROJ library removed - using WGS84 coordinates throughout

# Optional libraries
if(TBB_FOUND)
    if(TARGET TBB::tbb)
        target_link_libraries(TopoCore PUBLIC TBB::tbb)
    elseif(TARGET tbb)
        target_link_libraries(TopoCore PUBLIC tbb)
    else()
        # Fallback to variables
        target_include_directories(TopoCore PRIVATE ${TBB_INCLUDE_DIRS})
        target_link_libraries(TopoCore PUBLIC ${TBB_LIBRARIES})
    endif()
endif()

if(OpenMP_FOUND)
    if(TARGET OpenMP::OpenMP_CXX)
        target_link_libraries(TopoCore PUBLIC OpenMP::OpenMP_CXX)
        message(STATUS "Linking TopoCore with OpenMP::OpenMP_CXX target")
    else()
        # Fallback manual OpenMP configuration
        if(DEFINED OpenMP_CXX_INCLUDE_DIRS)
            target_include_directories(TopoCore PRIVATE ${OpenMP_CXX_INCLUDE_DIRS})
            message(STATUS "Added OpenMP include dirs: ${OpenMP_CXX_INCLUDE_DIRS}")
        endif()
        if(DEFINED OpenMP_CXX_FLAGS)
            target_compile_options(TopoCore PRIVATE ${OpenMP_CXX_FLAGS})
            message(STATUS "Added OpenMP compile flags: ${OpenMP_CXX_FLAGS}")
        endif()
        if(DEFINED OpenMP_CXX_LIBRARIES)
            target_link_libraries(TopoCore PUBLIC ${OpenMP_CXX_LIBRARIES})
            message(STATUS "Added OpenMP libraries: ${OpenMP_CXX_LIBRARIES}")
        endif()
    endif()
else()
    message(STATUS "OpenMP not found - TopoCore will use sequential processing")
endif()

# HDF5 support temporarily disabled
# if(HDF5_FOUND)
#     target_link_libraries(TopoCore PUBLIC HDF5::HDF5_CXX)
# endif()

if(libigl_FOUND)
    if(TARGET igl::core)
        target_link_libraries(TopoCore PUBLIC igl::core)
    elseif(TARGET igl)
        target_link_libraries(TopoCore PUBLIC igl)
    endif()
    # Link CGAL-dependent libigl functions for mesh repair
    if(TARGET igl::cgal)
        target_link_libraries(TopoCore PUBLIC igl::cgal)
        message(STATUS "Linked igl::cgal for mesh repair operations")
    elseif(TARGET igl_copyleft_cgal)
        target_link_libraries(TopoCore PUBLIC igl_copyleft_cgal)
        message(STATUS "Linked igl_copyleft_cgal for mesh repair operations")
    endif()
endif()

# CGAL and dependencies for robust triangulation
if(CGAL_FOUND)
    target_link_libraries(TopoCore PUBLIC CGAL::CGAL)
    if(GMP_FOUND)
        target_include_directories(TopoCore PRIVATE ${GMP_INCLUDE_DIR})
        target_link_libraries(TopoCore PUBLIC ${GMP_LIBRARIES})
    endif()
    if(MPFR_FOUND)
        target_include_directories(TopoCore PRIVATE ${MPFR_INCLUDE_DIR})
        target_link_libraries(TopoCore PUBLIC ${MPFR_LIBRARIES})
    endif()
    message(STATUS "Linked CGAL, GMP, and MPFR to TopoCore")
endif()

# Compiler-specific optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(TopoCore PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native -mtune=native>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
    )
endif()

# Add subdirectories for implementations
# (These would contain the actual .cpp implementations)
# add_subdirectory(impl)