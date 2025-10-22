/**
 * @file ShapefileExporter.hpp
 * @brief ESRI Shapefile export for contour polygons
 *
 * Exports topographic contour polygons as ESRI Shapefiles (.shp + .shx + .dbf + .prj)
 * for use in desktop GIS applications like ArcGIS and QGIS.
 */

#pragma once

#include "topographic_generator.hpp"
#include "../core/ContourGenerator.hpp"
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_geometry.h>
#include <string>
#include <vector>

namespace topo {

/**
 * @brief Exports contour polygons as ESRI Shapefiles
 *
 * Creates complete Shapefile datasets with spatial and attribute data,
 * including elevation values and layer metadata. Uses GDAL/OGR for
 * robust Shapefile generation.
 */
class ShapefileExporter {
public:
    struct Options {
        enum class GeometryType {
            POLYGON,
            POLYGON25D
        };

        GeometryType geometry_type;
        std::string projection_wkt;
        bool create_prj_file;
        bool add_elevation_field;
        bool add_layer_field;
        bool add_area_field;
        bool add_perimeter_field;
        std::string elevation_field_name;
        std::string layer_field_name;
        std::string area_field_name;
        std::string perimeter_field_name;
        bool create_spatial_index;

        Options()
            : geometry_type(GeometryType::POLYGON),
              projection_wkt(""),
              create_prj_file(true),
              add_elevation_field(true),
              add_layer_field(true),
              add_area_field(false),
              add_perimeter_field(false),
              elevation_field_name("ELEVATION"),
              layer_field_name("LAYER"),
              area_field_name("AREA"),
              perimeter_field_name("PERIMETER"),
              create_spatial_index(true) {}
    };

    ShapefileExporter();
    explicit ShapefileExporter(const Options& options);

    /**
     * @brief Export contour layers as Shapefile
     * @param layers Contour layers to export
     * @param filename Output Shapefile base name (without .shp extension)
     * @return true if export succeeded
     */
    bool export_shapefile(const std::vector<ContourLayer>& layers,
                         const std::string& filename);

    /**
     * @brief Export single layer as Shapefile
     * @param layer Single contour layer
     * @param filename Output Shapefile base name
     * @param layer_number Optional layer number for attributes
     * @return true if export succeeded
     */
    bool export_layer(const ContourLayer& layer,
                      const std::string& filename,
                      int layer_number = 0);

private:
    Options options_;

    // OGR helper methods
    OGRGeometry* polygon_to_ogr(const ContourLayer::PolygonData& poly) const;

    // Attribute creation
    void create_attribute_fields(OGRLayer* layer);
    void set_feature_attributes(OGRFeature* feature,
                                double elevation,
                                int layer_number,
                                const ContourLayer::PolygonData& poly);

    // Projection helpers
    std::string get_wgs84_wkt() const;
    bool write_prj_file(const std::string& base_filename, const std::string& wkt);
};

} // namespace topo
