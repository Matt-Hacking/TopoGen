/**
 * @file GeoJSONExporter.hpp
 * @brief GeoJSON vector export for contour polygons
 *
 * Exports topographic contour polygons as GeoJSON FeatureCollection
 * for use in web mapping applications and GIS software.
 */

#pragma once

#include "topographic_generator.hpp"
#include "../core/ContourGenerator.hpp"
#include <string>
#include <vector>

namespace topo {

/**
 * @brief Exports contour polygons as GeoJSON
 *
 * Creates GeoJSON FeatureCollection with contour polygons as features,
 * including elevation metadata and optional styling properties.
 */
class GeoJSONExporter {
public:
    struct Options {
        bool pretty_print;
        bool include_properties;
        bool include_holes;
        int precision;
        bool add_elevation;
        bool add_layer_number;
        bool add_area;
        bool add_style;
        std::string crs;
        bool include_crs;

        Options()
            : pretty_print(true),
              include_properties(true),
              include_holes(true),
              precision(6),
              add_elevation(true),
              add_layer_number(true),
              add_area(false),
              add_style(false),
              crs("EPSG:4326"),
              include_crs(true) {}
    };

    GeoJSONExporter();
    explicit GeoJSONExporter(const Options& options);

    /**
     * @brief Export contour layers as GeoJSON FeatureCollection
     * @param layers Contour layers to export
     * @param filename Output GeoJSON filename
     * @return true if export succeeded
     */
    bool export_geojson(const std::vector<ContourLayer>& layers,
                       const std::string& filename);

    /**
     * @brief Export single layer as GeoJSON
     * @param layer Single contour layer
     * @param filename Output GeoJSON filename
     * @param layer_number Optional layer number for properties
     * @return true if export succeeded
     */
    bool export_layer(const ContourLayer& layer,
                      const std::string& filename,
                      int layer_number = 0);

    /**
     * @brief Generate GeoJSON string (without writing to file)
     * @param layers Contour layers to convert
     * @return GeoJSON string
     */
    std::string to_geojson_string(const std::vector<ContourLayer>& layers);

private:
    Options options_;

    // JSON generation methods
    std::string polygon_to_geojson(const ContourLayer::PolygonData& poly,
                                   double elevation,
                                   int layer_number);
    std::string coordinates_to_json(const std::vector<std::pair<double, double>>& ring);
    std::string point_to_json(const std::pair<double, double>& point);

    // Feature properties
    std::string create_properties(double elevation, int layer_number,
                                  const ContourLayer::PolygonData& poly);

    // Formatting helpers
    std::string format_coordinate(double value) const;
    std::string escape_json_string(const std::string& str) const;
};

} // namespace topo
