/**
 * @file GeocodeService.hpp
 * @brief Geocoding service for place name to coordinate conversion
 *
 * Uses OpenStreetMap Nominatim API to convert place names, addresses,
 * and other location queries into geographic coordinates.
 */

#pragma once

#include "topographic_generator.hpp"
#include <string>
#include <optional>

namespace topo {

/**
 * @brief Result from a geocoding query
 */
struct GeocodeResult {
    std::string display_name;   // Full display name from OSM
    double lat;                 // Latitude
    double lon;                 // Longitude
    BoundingBox bounds;         // Bounding box for the location

    // Confidence and metadata
    std::string osm_type;       // node, way, or relation
    std::string osm_class;      // boundary, highway, building, etc.
    std::string type;           // administrative, residential, etc.
    double importance;          // Relevance score 0-1

    GeocodeResult() : lat(0.0), lon(0.0), importance(0.0) {}
};

/**
 * @brief Geocoding service using OSM Nominatim
 *
 * Converts place names and addresses to geographic coordinates
 * using the OpenStreetMap Nominatim geocoding API.
 */
class GeocodeService {
public:
    struct Options {
        std::string nominatim_url;
        std::string user_agent;
        int timeout_seconds;
        bool include_bounds;
        int max_results;
        std::string language;

        Options()
            : nominatim_url("https://nominatim.openstreetmap.org"),
              user_agent("TopographicGenerator/2.0"),
              timeout_seconds(10),
              include_bounds(true),
              max_results(1),
              language("en") {}
    };

    GeocodeService();
    explicit GeocodeService(const Options& options);

    /**
     * @brief Search for a location by name or address
     * @param query Place name, address, or location query
     * @return Geocoding result if found
     */
    std::optional<GeocodeResult> geocode(const std::string& query);

    /**
     * @brief Search with multiple results
     * @param query Location query
     * @param max_results Maximum number of results
     * @return Vector of geocoding results
     */
    std::vector<GeocodeResult> geocode_multiple(const std::string& query, int max_results = 5);

    /**
     * @brief Reverse geocode: convert coordinates to place name
     * @param lat Latitude
     * @param lon Longitude
     * @return Place information if found
     */
    std::optional<GeocodeResult> reverse_geocode(double lat, double lon);

private:
    Options options_;

    // HTTP request helper
    std::string make_http_request(const std::string& url);

    // JSON parsing helper
    std::optional<GeocodeResult> parse_nominatim_response(const std::string& json_response);
    std::vector<GeocodeResult> parse_nominatim_response_multi(const std::string& json_response);

    // URL encoding helper
    std::string url_encode(const std::string& value) const;

    // Build Nominatim query URL
    std::string build_search_url(const std::string& query) const;
    std::string build_reverse_url(double lat, double lon) const;
};

} // namespace topo
