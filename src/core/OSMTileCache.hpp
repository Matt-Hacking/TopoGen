/**
 * @file OSMTileCache.hpp
 * @brief OSM tile caching system for improved performance and reduced server load
 *
 * Implements a disk-based caching system for OpenStreetMap tiles with configurable
 * cache directory, size limits, and expiration policies. Supports HTTP caching headers
 * and provides cache statistics for debugging.
 *
 * Copyright (c) 2025 Matthew Block
 * Enhanced by Claude (Anthropic AI Assistant)
 * Licensed under the MIT License.
 */

#pragma once

#include "Logger.hpp"
#include <string>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <optional>

namespace topo {

/**
 * @brief Cache entry metadata for OSM tiles
 */
struct OSMTileEntry {
    std::string tile_url;
    std::string local_path;
    std::chrono::system_clock::time_point cache_time;
    std::chrono::system_clock::time_point expire_time;
    size_t file_size_bytes = 0;
    std::string etag;
    std::string last_modified;
    bool is_valid = true;

    OSMTileEntry() = default;
    OSMTileEntry(const std::string& url, const std::string& path)
        : tile_url(url), local_path(path),
          cache_time(std::chrono::system_clock::now()),
          expire_time(std::chrono::system_clock::now() + std::chrono::hours(24)) {}
};

/**
 * @brief Cache statistics for monitoring and debugging
 */
struct OSMCacheStats {
    size_t total_requests = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    size_t downloads = 0;
    size_t expired_entries = 0;
    size_t total_cache_size_bytes = 0;
    size_t total_cached_files = 0;
    std::chrono::milliseconds total_download_time{0};
    std::chrono::milliseconds total_cache_lookup_time{0};

    double hit_rate() const {
        return total_requests > 0 ? static_cast<double>(cache_hits) / total_requests : 0.0;
    }

    double average_download_time_ms() const {
        return downloads > 0 ? static_cast<double>(total_download_time.count()) / downloads : 0.0;
    }
};

/**
 * @brief Configuration for OSM tile caching
 */
struct OSMCacheConfig {
    std::string cache_directory = "cache/osm_tiles";
    size_t max_cache_size_mb = 500;  // 500 MB default
    std::chrono::hours default_expiry{24};  // 24 hours default
    bool enable_etag_validation = true;
    bool enable_cache = true;
    std::string user_agent = "TopographicGenerator/2.0";
    int connection_timeout_seconds = 30;
    int max_retries = 3;
};

/**
 * @brief High-performance OSM tile caching system
 *
 * Provides disk-based caching for OpenStreetMap tiles with:
 * - HTTP caching headers support (ETag, Last-Modified)
 * - Configurable cache size and expiration
 * - Thread-safe operations
 * - Cache statistics and monitoring
 * - Automatic cache cleanup and maintenance
 */
class OSMTileCache {
public:
    explicit OSMTileCache(const OSMCacheConfig& config = OSMCacheConfig{});
    ~OSMTileCache();

    /**
     * @brief Retrieve a tile from cache or download if not available
     * @param tile_url The URL of the OSM tile to retrieve
     * @param output_path Where to save the tile data
     * @return True if successful, false otherwise
     */
    bool get_tile(const std::string& tile_url, const std::string& output_path);

    /**
     * @brief Check if a tile exists in cache and is valid
     * @param tile_url The URL of the OSM tile to check
     * @return True if tile is cached and valid
     */
    bool is_cached(const std::string& tile_url) const;

    /**
     * @brief Get cached tile path without downloading
     * @param tile_url The URL of the OSM tile
     * @return Path to cached file if available, empty if not cached
     */
    std::optional<std::string> get_cached_path(const std::string& tile_url) const;

    /**
     * @brief Manually cache a tile from local data
     * @param tile_url The URL this tile represents
     * @param data_path Path to the tile data to cache
     * @return True if successful
     */
    bool cache_tile(const std::string& tile_url, const std::string& data_path);

    /**
     * @brief Remove expired entries from cache
     * @return Number of entries removed
     */
    size_t cleanup_expired();

    /**
     * @brief Remove least recently used entries to fit within size limit
     * @return Number of entries removed
     */
    size_t cleanup_lru();

    /**
     * @brief Clear all cached tiles
     */
    void clear_cache();

    /**
     * @brief Get current cache statistics
     */
    OSMCacheStats get_stats() const;

    /**
     * @brief Print cache statistics to console
     */
    void print_stats() const;

    /**
     * @brief Enable or disable caching
     */
    void set_cache_enabled(bool enabled) { config_.enable_cache = enabled; }

    /**
     * @brief Check if caching is enabled
     */
    bool is_cache_enabled() const { return config_.enable_cache; }

    /**
     * @brief Update cache configuration
     */
    void update_config(const OSMCacheConfig& config);

    /**
     * @brief Get cache directory path
     */
    std::string get_cache_directory() const { return config_.cache_directory; }

    /**
     * @brief Set the logger for this cache
     *
     * @param logger Logger instance to use
     */
    void set_logger(const Logger& logger);

private:
    OSMCacheConfig config_;
    std::unordered_map<std::string, OSMTileEntry> cache_index_;
    mutable std::mutex cache_mutex_;
    OSMCacheStats stats_;
    Logger logger_;  // Centralized logging system

    // Helper methods
    std::string url_to_cache_path(const std::string& url) const;
    std::string compute_cache_key(const std::string& url) const;
    bool download_tile(const std::string& url, const std::string& output_path);
    bool validate_cached_entry(const OSMTileEntry& entry) const;
    void load_cache_index();
    void save_cache_index();
    void update_stats_on_hit();
    void update_stats_on_miss();
    void update_stats_on_download(std::chrono::milliseconds download_time);
    bool ensure_cache_directory() const;
    size_t get_current_cache_size() const;
    bool is_cache_size_exceeded() const;
};

} // namespace topo