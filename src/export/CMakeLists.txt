# Export library for multi-format output

add_library(TopoExport STATIC
    MultiFormatExporter.cpp
    OBJExporter.cpp
    PLYExporter.cpp
    NURBSExporter.cpp
    SVGExporter.cpp
    RasterBuilder.cpp
    RasterAnnotator.cpp
    PNGExporter.cpp
    GeoTIFFExporter.cpp
    GeoJSONExporter.cpp
    ShapefileExporter.cpp
)

target_include_directories(TopoExport
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(TopoExport
    PRIVATE
        TopoCore
    PUBLIC
        # All geometry now uses custom types and GDAL/OGR (CGAL completely removed)
        Eigen3::Eigen
        GDAL::GDAL
        Freetype::Freetype  # For text rendering in raster outputs
)

# Optional libigl for advanced mesh I/O
if(libigl_FOUND)
    if(TARGET igl::core)
        target_link_libraries(TopoExport PUBLIC igl::core)
    elseif(TARGET igl)
        target_link_libraries(TopoExport PUBLIC igl)
    endif()
endif()

# NURBS libraries (if available)
# These would need to be found or implemented
# find_package(OpenCASCADE QUIET)
# if(OpenCASCADE_FOUND)
#     target_link_libraries(TopoExport PUBLIC ${OpenCASCADE_LIBRARIES})
#     target_compile_definitions(TopoExport PRIVATE HAVE_OPENCASCADE)
# endif()

target_compile_features(TopoExport PUBLIC cxx_std_20)