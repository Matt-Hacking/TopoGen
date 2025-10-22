/**
 * @file SRTMDownloader.hpp
 * @brief SRTM elevation data downloading and caching system
 * 
 * Port of Python download_clip_elevation_tiles.py functionality
 * Downloads SRTM tiles from OpenTopography AWS S3 with local caching
 * 
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "Logger.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <cmath>

namespace topo {

/**
 * @brief SRTM tile downloader and cache manager
 * 
 * Handles downloading 1-arc-second SRTM elevation tiles from
 * OpenTopography's AWS S3 bucket with local file caching and
 * atomic download operations using file locks.
 */
class SRTMDownloader {
public:
    /**
     * @brief Download configuration
     */
    struct Config {
        std::string cache_directory = "cache/tiles";
        std::string base_url = "https://s3.amazonaws.com/elevation-tiles-prod/skadi";
        int timeout_seconds = 30;
        int max_retries = 3;
    };

    SRTMDownloader();
    explicit SRTMDownloader(const Config& config);

    /**
     * @brief Download SRTM tiles for bounding box
     * 
     * @param bounds Geographic bounding box (min_lon, min_lat, max_lon, max_lat)
     * @return Vector of paths to downloaded .hgt.gz files
     */
    std::vector<std::string> download_tiles(const BoundingBox& bounds);

    /**
     * @brief Get tile filename for coordinates
     * 
     * @param lat Latitude (will be floored)
     * @param lon Longitude (will be floored) 
     * @return SRTM tile filename (e.g., "N63W152.hgt.gz")
     */
    static std::string get_tile_filename(double lat, double lon);

    /**
     * @brief Check if tile exists in cache
     * 
     * @param filename Tile filename
     * @return true if tile exists and is valid
     */
    bool tile_exists_in_cache(const std::string& filename) const;

    /**
     * @brief Get full path to cached tile
     *
     * @param filename Tile filename
     * @return Full filesystem path to cached tile
     */
    std::filesystem::path get_tile_path(const std::string& filename) const;

    /**
     * @brief Set the logger for this downloader
     *
     * @param logger Logger instance to use
     */
    void set_logger(const Logger& logger);

private:
    Config config_;
    Logger logger_;  // Centralized logging system

    /**
     * @brief Download single tile with retries
     * 
     * @param filename Tile filename to download
     * @return true if download successful
     */
    bool download_single_tile(const std::string& filename);

    /**
     * @brief Download file from URL with HTTP client
     * 
     * @param url Source URL
     * @param output_path Destination file path
     * @return true if download successful
     */
    bool download_file(const std::string& url, const std::filesystem::path& output_path);

    /**
     * @brief Validate downloaded tile file
     * 
     * @param file_path Path to tile file
     * @return true if file appears to be valid SRTM data
     */
    bool validate_tile_file(const std::filesystem::path& file_path) const;

    /**
     * @brief Check for antimeridian crossing
     * 
     * @param bounds Bounding box to check
     * @return true if bounding box crosses 180° longitude
     */
    static bool is_antimeridian_crossing(const BoundingBox& bounds);

    /**
     * @brief Split bounds that cross antimeridian
     * 
     * @param bounds Original bounding box
     * @return Vector of bounding boxes (1 or 2 elements)
     */
    static std::vector<BoundingBox> split_antimeridian_bounds(const BoundingBox& bounds);
};

} // namespace topo