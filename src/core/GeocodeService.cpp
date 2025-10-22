/**
 * @file GeocodeService.cpp
 * @brief Implementation of geocoding service
 */

#include "GeocodeService.hpp"
#include "Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace topo {

// Callback for CURL to write response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append(static_cast<char*>(contents), total_size);
    return total_size;
}

GeocodeService::GeocodeService()
    : options_() {
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

GeocodeService::GeocodeService(const Options& options)
    : options_(options) {
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::optional<GeocodeResult> GeocodeService::geocode(const std::string& query) {
    Logger logger("GeocodeService");

    if (query.empty()) {
        logger.error("Empty geocoding query");
        return std::nullopt;
    }

    // Build Nominatim search URL
    std::string url = build_search_url(query);

    logger.info("Geocoding query: " + query);
    logger.debug("Nominatim URL: " + url);

    // Make HTTP request
    std::string response = make_http_request(url);

    if (response.empty()) {
        logger.error("Failed to get geocoding response");
        return std::nullopt;
    }

    // Parse JSON response
    auto result = parse_nominatim_response(response);

    if (result.has_value()) {
        logger.info("Geocoded '" + query + "' to: " + result->display_name);
        logger.info("Coordinates: " + std::to_string(result->lat) + ", " + std::to_string(result->lon));
    } else {
        logger.warning("No results found for: " + query);
    }

    return result;
}

std::vector<GeocodeResult> GeocodeService::geocode_multiple(const std::string& query, int max_results) {
    Logger logger("GeocodeService");

    std::string url = build_search_url(query);
    url += "&limit=" + std::to_string(max_results);

    std::string response = make_http_request(url);

    if (response.empty()) {
        logger.error("Failed to get geocoding response");
        return {};
    }

    return parse_nominatim_response_multi(response);
}

std::optional<GeocodeResult> GeocodeService::reverse_geocode(double lat, double lon) {
    Logger logger("GeocodeService");

    std::string url = build_reverse_url(lat, lon);

    logger.debug("Reverse geocoding: " + std::to_string(lat) + ", " + std::to_string(lon));

    std::string response = make_http_request(url);

    if (response.empty()) {
        return std::nullopt;
    }

    return parse_nominatim_response(response);
}

std::string GeocodeService::make_http_request(const std::string& url) {
    Logger logger("GeocodeService");
    std::string response_data;

    CURL* curl = curl_easy_init();
    if (!curl) {
        logger.error("Failed to initialize CURL");
        return "";
    }

    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, options_.timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, options_.user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        logger.error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        curl_easy_cleanup(curl);
        return "";
    }

    // Check HTTP response code
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (http_code != 200) {
        logger.error("HTTP error " + std::to_string(http_code));
        return "";
    }

    return response_data;
}

std::optional<GeocodeResult> GeocodeService::parse_nominatim_response(const std::string& json_response) {
    Logger logger("GeocodeService");

    try {
        auto j = json::parse(json_response);

        // Nominatim returns an array, we want the first result
        if (j.is_array() && !j.empty()) {
            j = j[0];
        } else if (j.is_array() && j.empty()) {
            return std::nullopt;
        }

        GeocodeResult result;

        result.display_name = j.value("display_name", "");
        result.lat = std::stod(j.value("lat", "0.0"));
        result.lon = std::stod(j.value("lon", "0.0"));

        // Parse bounding box if available
        if (j.contains("boundingbox") && j["boundingbox"].is_array() && j["boundingbox"].size() >= 4) {
            auto bbox = j["boundingbox"];
            result.bounds.min_y = std::stod(bbox[0].get<std::string>());
            result.bounds.max_y = std::stod(bbox[1].get<std::string>());
            result.bounds.min_x = std::stod(bbox[2].get<std::string>());
            result.bounds.max_x = std::stod(bbox[3].get<std::string>());
        }

        result.osm_type = j.value("osm_type", "");
        result.osm_class = j.value("class", "");
        result.type = j.value("type", "");
        result.importance = j.value("importance", 0.0);

        return result;

    } catch (const json::exception& e) {
        logger.error("JSON parsing error: " + std::string(e.what()));
        return std::nullopt;
    } catch (const std::exception& e) {
        logger.error("Error parsing geocoding response: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<GeocodeResult> GeocodeService::parse_nominatim_response_multi(const std::string& json_response) {
    Logger logger("GeocodeService");
    std::vector<GeocodeResult> results;

    try {
        auto j = json::parse(json_response);

        if (!j.is_array()) {
            return results;
        }

        for (const auto& item : j) {
            GeocodeResult result;

            result.display_name = item.value("display_name", "");
            result.lat = std::stod(item.value("lat", "0.0"));
            result.lon = std::stod(item.value("lon", "0.0"));

            if (item.contains("boundingbox") && item["boundingbox"].is_array() && item["boundingbox"].size() >= 4) {
                auto bbox = item["boundingbox"];
                result.bounds.min_y = std::stod(bbox[0].get<std::string>());
                result.bounds.max_y = std::stod(bbox[1].get<std::string>());
                result.bounds.min_x = std::stod(bbox[2].get<std::string>());
                result.bounds.max_x = std::stod(bbox[3].get<std::string>());
            }

            result.osm_type = item.value("osm_type", "");
            result.osm_class = item.value("class", "");
            result.type = item.value("type", "");
            result.importance = item.value("importance", 0.0);

            results.push_back(result);
        }

    } catch (const std::exception& e) {
        logger.error("Error parsing geocoding response: " + std::string(e.what()));
    }

    return results;
}

std::string GeocodeService::url_encode(const std::string& value) const {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        // Keep alphanumeric and other safe characters
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

std::string GeocodeService::build_search_url(const std::string& query) const {
    std::ostringstream url;

    url << options_.nominatim_url << "/search?";
    url << "q=" << url_encode(query);
    url << "&format=json";
    url << "&limit=" << options_.max_results;

    if (options_.include_bounds) {
        url << "&bounded=1";
        url << "&polygon_text=1";
    }

    if (!options_.language.empty()) {
        url << "&accept-language=" << options_.language;
    }

    return url.str();
}

std::string GeocodeService::build_reverse_url(double lat, double lon) const {
    std::ostringstream url;

    url << options_.nominatim_url << "/reverse?";
    url << "lat=" << std::fixed << std::setprecision(8) << lat;
    url << "&lon=" << std::fixed << std::setprecision(8) << lon;
    url << "&format=json";

    if (!options_.language.empty()) {
        url << "&accept-language=" << options_.language;
    }

    return url.str();
}

} // namespace topo
