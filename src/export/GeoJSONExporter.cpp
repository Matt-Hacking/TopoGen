/**
 * @file GeoJSONExporter.cpp
 * @brief Implementation of GeoJSON export
 */

#include "GeoJSONExporter.hpp"
#include "../core/Logger.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace topo {

GeoJSONExporter::GeoJSONExporter()
    : options_() {}

GeoJSONExporter::GeoJSONExporter(const Options& options)
    : options_(options) {}

bool GeoJSONExporter::export_geojson(const std::vector<ContourLayer>& layers,
                                    const std::string& filename) {
    Logger logger("GeoJSONExporter");

    if (layers.empty()) {
        logger.error("No layers to export");
        return false;
    }

    std::string geojson = to_geojson_string(layers);

    std::ofstream file(filename);
    if (!file.is_open()) {
        logger.error("Failed to create GeoJSON file: " + filename);
        return false;
    }

    file << geojson;
    file.close();

    logger.info("Exported GeoJSON: " + filename);
    return true;
}

bool GeoJSONExporter::export_layer(const ContourLayer& layer,
                                   const std::string& filename,
                                   [[maybe_unused]] int layer_number) {
    std::vector<ContourLayer> single_layer = {layer};
    return export_geojson(single_layer, filename);
}

std::string GeoJSONExporter::to_geojson_string(const std::vector<ContourLayer>& layers) {
    std::ostringstream json;

    // Set precision for coordinates
    json << std::fixed << std::setprecision(options_.precision);

    // Start FeatureCollection
    json << "{";
    if (options_.pretty_print) json << "\n  ";

    json << "\"type\": \"FeatureCollection\",";
    if (options_.pretty_print) json << "\n  ";

    // Add CRS if requested
    if (options_.include_crs) {
        json << "\"crs\": {";
        if (options_.pretty_print) json << "\n    ";
        json << "\"type\": \"name\",";
        if (options_.pretty_print) json << "\n    ";
        json << "\"properties\": {";
        if (options_.pretty_print) json << "\n      ";
        json << "\"name\": \"" << options_.crs << "\"";
        if (options_.pretty_print) json << "\n    ";
        json << "}";
        if (options_.pretty_print) json << "\n  ";
        json << "},";
        if (options_.pretty_print) json << "\n  ";
    }

    // Start features array
    json << "\"features\": [";

    bool first_feature = true;
    for (size_t layer_idx = 0; layer_idx < layers.size(); ++layer_idx) {
        const auto& layer = layers[layer_idx];

        for (const auto& poly : layer.polygons) {
            if (!first_feature) {
                json << ",";
            }
            if (options_.pretty_print) json << "\n    ";

            json << polygon_to_geojson(poly, layer.elevation, static_cast<int>(layer_idx + 1));

            first_feature = false;
        }
    }

    if (options_.pretty_print) json << "\n  ";
    json << "]";

    if (options_.pretty_print) json << "\n";
    json << "}";

    return json.str();
}

std::string GeoJSONExporter::polygon_to_geojson(const ContourLayer::PolygonData& poly,
                                                double elevation,
                                                int layer_number) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(options_.precision);

    json << "{";
    if (options_.pretty_print) json << "\n      ";

    json << "\"type\": \"Feature\",";
    if (options_.pretty_print) json << "\n      ";

    // Add properties
    if (options_.include_properties) {
        json << "\"properties\": ";
        json << create_properties(elevation, layer_number, poly);
        json << ",";
        if (options_.pretty_print) json << "\n      ";
    }

    // Add geometry
    json << "\"geometry\": {";
    if (options_.pretty_print) json << "\n        ";

    json << "\"type\": \"Polygon\",";
    if (options_.pretty_print) json << "\n        ";

    json << "\"coordinates\": [";
    if (options_.pretty_print) json << "\n          ";

    // Outer boundary (first ring)
    if (!poly.rings.empty()) {
        json << coordinates_to_json(poly.rings[0]);

        // Holes (subsequent rings)
        if (options_.include_holes && poly.rings.size() > 1) {
            for (size_t i = 1; i < poly.rings.size(); ++i) {
                json << ",";
                if (options_.pretty_print) json << "\n          ";
                json << coordinates_to_json(poly.rings[i]);
            }
        }
    }

    if (options_.pretty_print) json << "\n        ";
    json << "]";
    if (options_.pretty_print) json << "\n      ";
    json << "}";
    if (options_.pretty_print) json << "\n    ";
    json << "}";

    return json.str();
}

std::string GeoJSONExporter::coordinates_to_json(const std::vector<std::pair<double, double>>& ring) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(options_.precision);

    json << "[";

    bool first_point = true;
    for (const auto& point : ring) {
        if (!first_point) {
            json << ", ";
        }
        json << point_to_json(point);
        first_point = false;
    }

    // Close the polygon by repeating first point (GeoJSON requirement)
    if (!ring.empty()) {
        json << ", " << point_to_json(ring[0]);
    }

    json << "]";
    return json.str();
}

std::string GeoJSONExporter::point_to_json(const std::pair<double, double>& point) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(options_.precision);

    json << "[" << format_coordinate(point.first) << ", "
         << format_coordinate(point.second) << "]";

    return json.str();
}

std::string GeoJSONExporter::create_properties(double elevation, int layer_number,
                                               const ContourLayer::PolygonData& poly) {
    std::ostringstream json;
    json << "{";

    bool first_prop = true;

    if (options_.add_elevation) {
        json << "\"elevation\": " << elevation;
        first_prop = false;
    }

    if (options_.add_layer_number) {
        if (!first_prop) json << ", ";
        json << "\"layer\": " << layer_number;
        first_prop = false;
    }

    if (options_.add_area && !poly.rings.empty()) {
        if (!first_prop) json << ", ";
        // Calculate area using shoelace formula (in map units squared)
        double area = 0.0;
        const auto& ring = poly.rings[0]; // Exterior ring
        for (size_t i = 0; i < ring.size(); ++i) {
            size_t j = (i + 1) % ring.size();
            area += ring[i].first * ring[j].second;
            area -= ring[j].first * ring[i].second;
        }
        area = std::abs(area) / 2.0;
        json << "\"area\": " << area;
        first_prop = false;
    }

    if (options_.add_style) {
        if (!first_prop) json << ", ";
        // Add basic Mapbox/Leaflet style hints
        json << "\"stroke\": \"#0033ff\", ";
        json << "\"stroke-width\": 2, ";
        json << "\"fill\": \"#0099ff\", ";
        json << "\"fill-opacity\": 0.3";
        first_prop = false;
    }

    json << "}";
    return json.str();
}

std::string GeoJSONExporter::format_coordinate(double value) const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(options_.precision) << value;
    return ss.str();
}

std::string GeoJSONExporter::escape_json_string(const std::string& str) const {
    std::ostringstream escaped;
    for (char c : str) {
        switch (c) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 32) {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    escaped << c;
                }
        }
    }
    return escaped.str();
}

} // namespace topo
