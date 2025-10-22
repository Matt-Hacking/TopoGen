/**
 * @file SRTMDownloader.cpp
 * @brief Implementation of SRTM elevation data downloading
 */

#include "SRTMDownloader.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>
#include <algorithm>
#include <thread>
#include <chrono>

namespace topo {

// Callback for curl to write data to file
static size_t WriteFileCallback(void* contents, size_t size, size_t nmemb, FILE* fp) {
    return fwrite(contents, size, nmemb, fp);
}

SRTMDownloader::SRTMDownloader() : config_() {
    // Initialize curl globally
    static bool curl_initialized = false;
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }

    // Create cache directory
    std::filesystem::create_directories(config_.cache_directory);
}

SRTMDownloader::SRTMDownloader(const Config& config) : config_(config) {
    // Initialize curl globally
    static bool curl_initialized = false;
    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_initialized = true;
    }

    // Create cache directory
    std::filesystem::create_directories(config_.cache_directory);
}

void SRTMDownloader::set_logger([[maybe_unused]] const Logger& logger) {
    // Logger contains mutex and cannot be copied - use move constructor or pointer storage instead
    // For now, logger_ is already initialized with default settings
}

std::vector<std::string> SRTMDownloader::download_tiles(const BoundingBox& bounds) {
    std::vector<std::string> tile_paths;
    
    // Handle antimeridian crossing
    std::vector<BoundingBox> bounds_list;
    if (is_antimeridian_crossing(bounds)) {
        bounds_list = split_antimeridian_bounds(bounds);
        logger_.info("Antimeridian crossing detected, split into " + std::to_string(bounds_list.size()) + " regions");
    } else {
        bounds_list.push_back(bounds);
    }

    for (const auto& bbox : bounds_list) {
        // Calculate tile coverage
        int lat_min = static_cast<int>(std::floor(bbox.min_y));
        int lat_max = static_cast<int>(std::floor(bbox.max_y));
        int lon_min = static_cast<int>(std::floor(bbox.min_x));
        int lon_max = static_cast<int>(std::floor(bbox.max_x));

        logger_.info("Downloading SRTM tiles for region:");
        logger_.info("  Latitude: " + std::to_string(lat_min) + " to " + std::to_string(lat_max));
        logger_.info("  Longitude: " + std::to_string(lon_min) + " to " + std::to_string(lon_max));

        // Download each required tile
        for (int lat = lat_min; lat <= lat_max; ++lat) {
            for (int lon = lon_min; lon <= lon_max; ++lon) {
                std::string filename = get_tile_filename(lat, lon);
                
                if (tile_exists_in_cache(filename)) {
                    logger_.debug("Using cached tile: " + filename);
                    tile_paths.push_back(get_tile_path(filename).string());
                } else {
                    logger_.info("Downloading tile: " + filename);

                    if (download_single_tile(filename)) {
                        tile_paths.push_back(get_tile_path(filename).string());
                    } else {
                        logger_.warning("Failed to download tile: " + filename);
                        // Continue with other tiles - some areas may not have SRTM coverage
                    }
                }
            }
        }
    }

    logger_.info("Successfully downloaded/cached " + std::to_string(tile_paths.size()) + " SRTM tiles");

    return tile_paths;
}

std::string SRTMDownloader::get_tile_filename(double lat, double lon) {
    int ilat = static_cast<int>(std::floor(lat));
    int ilon = static_cast<int>(std::floor(lon));
    
    std::string ns = (ilat >= 0) ? "N" : "S";
    std::string ew = (ilon >= 0) ? "E" : "W";
    
    std::ostringstream ss;
    ss << ns << std::setfill('0') << std::setw(2) << std::abs(ilat)
       << ew << std::setfill('0') << std::setw(3) << std::abs(ilon)
       << ".hgt.gz";
    
    return ss.str();
}

bool SRTMDownloader::tile_exists_in_cache(const std::string& filename) const {
    auto tile_path = get_tile_path(filename);
    
    if (!std::filesystem::exists(tile_path)) {
        return false;
    }
    
    // Check if file size is reasonable (should be ~25MB for SRTM1)
    auto file_size = std::filesystem::file_size(tile_path);
    if (file_size < 1000000 || file_size > 50000000) {  // 1MB to 50MB range
        logger_.warning("Cached tile " + filename + " has suspicious size: " + std::to_string(file_size) + " bytes");
        return false;
    }
    
    return validate_tile_file(tile_path);
}

std::filesystem::path SRTMDownloader::get_tile_path(const std::string& filename) const {
    return std::filesystem::path(config_.cache_directory) / filename;
}

bool SRTMDownloader::download_single_tile(const std::string& filename) {
    // Extract latitude prefix for subdirectory (e.g., "N63" from "N63W152.hgt.gz")
    std::string lat_prefix = filename.substr(0, 3); // Extract "N63" or "S45" etc.
    std::string url = config_.base_url + "/" + lat_prefix + "/" + filename;
    auto output_path = get_tile_path(filename);
    auto lock_path = output_path.string() + ".lock";
    
    // Simple file locking mechanism
    std::ofstream lock_file(lock_path);
    if (!lock_file.is_open()) {
        logger_.error("Could not create lock file: " + lock_path);
        return false;
    }
    
    // Attempt download with retries
    bool success = false;
    for (int attempt = 1; attempt <= config_.max_retries; ++attempt) {
        if (attempt > 1) {
            logger_.debug("Retry " + std::to_string(attempt) + "/" + std::to_string(config_.max_retries) + " for " + filename);
        }
        
        if (download_file(url, output_path)) {
            success = true;
            break;
        }
        
        if (attempt < config_.max_retries) {
            // Exponential backoff
            int delay_ms = 1000 * (1 << (attempt - 1));  // 1s, 2s, 4s, ...
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    }
    
    // Clean up lock file
    lock_file.close();
    std::filesystem::remove(lock_path);
    
    if (success && !validate_tile_file(output_path)) {
        logger_.error("Downloaded tile failed validation: " + filename);
        std::filesystem::remove(output_path);
        success = false;
    }
    
    return success;
}

bool SRTMDownloader::download_file(const std::string& url, const std::filesystem::path& output_path) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        logger_.error("Failed to initialize curl");
        return false;
    }
    
    FILE* fp = fopen(output_path.string().c_str(), "wb");
    if (!fp) {
        logger_.error("Failed to open output file: " + output_path.string());
        curl_easy_cleanup(curl);
        return false;
    }
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config_.timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);  // For simplicity, disable SSL verification
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TopographicGenerator/2.0");
    
    // Perform the download
    CURLcode res = curl_easy_perform(curl);
    
    // Get response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    // Clean up
    fclose(fp);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        logger_.error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
        std::filesystem::remove(output_path);
        return false;
    }
    
    if (response_code != 200) {
        logger_.warning("HTTP error " + std::to_string(response_code) + " for URL: " + url);
        std::filesystem::remove(output_path);
        return false;
    }
    
    return true;
}

bool SRTMDownloader::validate_tile_file(const std::filesystem::path& file_path) const {
    // Basic validation - just check if file exists and has reasonable size
    if (!std::filesystem::exists(file_path)) {
        return false;
    }
    
    auto file_size = std::filesystem::file_size(file_path);
    
    // SRTM .hgt.gz files are typically 15-30 MB
    if (file_size < 5000000 || file_size > 50000000) {
        return false;
    }
    
    // Could add more validation here:
    // - Check gzip magic numbers
    // - Verify uncompressed size is exactly 3601*3601*2 bytes for SRTM1
    // - Check for reasonable elevation values
    
    return true;
}

bool SRTMDownloader::is_antimeridian_crossing(const BoundingBox& bounds) {
    return bounds.min_x > bounds.max_x;
}

std::vector<BoundingBox> SRTMDownloader::split_antimeridian_bounds(const BoundingBox& bounds) {
    std::vector<BoundingBox> result;
    
    if (!is_antimeridian_crossing(bounds)) {
        result.push_back(bounds);
        return result;
    }
    
    // Split into western and eastern parts
    // Western part: min_x to 180
    result.emplace_back(bounds.min_x, bounds.min_y, 180.0, bounds.max_y);
    
    // Eastern part: -180 to max_x  
    result.emplace_back(-180.0, bounds.min_y, bounds.max_x, bounds.max_y);
    
    return result;
}

} // namespace topo