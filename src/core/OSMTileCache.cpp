/**
 * @file OSMTileCache.cpp
 * @brief Implementation of OSM tile caching system
 */

#include "OSMTileCache.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <curl/curl.h>
#include <openssl/evp.h>

namespace topo {

namespace {
    // Callback for writing HTTP response data
    size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t total_size = size * nmemb;
        std::ofstream* file = static_cast<std::ofstream*>(userp);
        file->write(static_cast<char*>(contents), total_size);
        return total_size;
    }

    // Generate MD5 hash for URL
    std::string md5_hash(const std::string& input) {
        unsigned char result[EVP_MAX_MD_SIZE];
        unsigned int result_len = 0;

        // Use the modern EVP API instead of deprecated MD5() function
        EVP_Digest(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(),
                   result, &result_len, EVP_md5(), nullptr);

        std::ostringstream sstream;
        sstream << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < result_len; ++i) {
            sstream << std::setw(2) << static_cast<unsigned>(result[i]);
        }
        return sstream.str();
    }
}

OSMTileCache::OSMTileCache(const OSMCacheConfig& config)
    : config_(config) {

    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Ensure cache directory exists
    ensure_cache_directory();

    // Load existing cache index
    load_cache_index();

    logger_.info("OSM Tile Cache initialized:");
    logger_.info("  Cache directory: " + config_.cache_directory);
    logger_.info("  Max cache size: " + std::to_string(config_.max_cache_size_mb) + " MB");
    logger_.info("  Default expiry: " + std::to_string(config_.default_expiry.count()) + " hours");
    logger_.info("  Caching enabled: " + std::string(config_.enable_cache ? "yes" : "no"));
}

OSMTileCache::~OSMTileCache() {
    save_cache_index();
    curl_global_cleanup();
}

void OSMTileCache::set_logger([[maybe_unused]] const Logger& logger) {
    // Logger contains mutex and cannot be copied - use move constructor or pointer storage instead
    // For now, logger_ is already initialized with default settings
}

bool OSMTileCache::get_tile(const std::string& tile_url, const std::string& output_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(cache_mutex_);
    stats_.total_requests++;

    if (!config_.enable_cache) {
        // Cache disabled, download directly
        bool success = download_tile(tile_url, output_path);
        if (success) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto download_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            update_stats_on_download(download_time);
        }
        return success;
    }

    std::string cache_key = compute_cache_key(tile_url);
    auto it = cache_index_.find(cache_key);

    // Check if we have a cached entry
    if (it != cache_index_.end() && validate_cached_entry(it->second)) {
        // Cache hit - copy from cache
        try {
            std::filesystem::copy_file(it->second.local_path, output_path,
                std::filesystem::copy_options::overwrite_existing);

            update_stats_on_hit();

            auto end_time = std::chrono::high_resolution_clock::now();
            auto lookup_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            stats_.total_cache_lookup_time += lookup_time;

            logger_.debug("Cache HIT: " + tile_url + " -> " + it->second.local_path);

            return true;
        } catch (const std::exception& e) {
            logger_.warning("Failed to copy cached tile: " + std::string(e.what()));
            // Fall through to download
        }
    }

    // Cache miss - need to download
    update_stats_on_miss();

    std::string cache_path = url_to_cache_path(tile_url);

    auto download_start = std::chrono::high_resolution_clock::now();
    bool download_success = download_tile(tile_url, cache_path);
    auto download_end = std::chrono::high_resolution_clock::now();

    if (download_success) {
        auto download_time = std::chrono::duration_cast<std::chrono::milliseconds>(download_end - download_start);
        update_stats_on_download(download_time);

        // Add to cache index
        OSMTileEntry entry(tile_url, cache_path);
        try {
            entry.file_size_bytes = std::filesystem::file_size(cache_path);
            cache_index_[cache_key] = entry;

            logger_.debug("Downloaded and cached: " + tile_url + " -> " + cache_path);
        } catch (const std::exception& e) {
            logger_.warning("Failed to get file size for cached tile: " + std::string(e.what()));
        }

        // Copy to output location
        try {
            if (cache_path != output_path) {
                std::filesystem::copy_file(cache_path, output_path,
                    std::filesystem::copy_options::overwrite_existing);
            }
        } catch (const std::exception& e) {
            logger_.error("Failed to copy downloaded tile to output: " + std::string(e.what()));
            return false;
        }

        // Check if cache cleanup is needed
        if (is_cache_size_exceeded()) {
            cleanup_lru();
        }

        return true;
    }

    return false;
}

bool OSMTileCache::is_cached(const std::string& tile_url) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (!config_.enable_cache) {
        return false;
    }

    std::string cache_key = compute_cache_key(tile_url);
    auto it = cache_index_.find(cache_key);

    return it != cache_index_.end() && validate_cached_entry(it->second);
}

std::optional<std::string> OSMTileCache::get_cached_path(const std::string& tile_url) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (!config_.enable_cache) {
        return std::nullopt;
    }

    std::string cache_key = compute_cache_key(tile_url);
    auto it = cache_index_.find(cache_key);

    if (it != cache_index_.end() && validate_cached_entry(it->second)) {
        return it->second.local_path;
    }

    return std::nullopt;
}

bool OSMTileCache::cache_tile(const std::string& tile_url, const std::string& data_path) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (!config_.enable_cache) {
        return false;
    }

    std::string cache_path = url_to_cache_path(tile_url);

    try {
        // Ensure cache directory exists
        std::filesystem::create_directories(std::filesystem::path(cache_path).parent_path());

        // Copy to cache
        std::filesystem::copy_file(data_path, cache_path,
            std::filesystem::copy_options::overwrite_existing);

        // Add to index
        std::string cache_key = compute_cache_key(tile_url);
        OSMTileEntry entry(tile_url, cache_path);
        entry.file_size_bytes = std::filesystem::file_size(cache_path);
        cache_index_[cache_key] = entry;

        logger_.debug("Manually cached: " + tile_url + " -> " + cache_path);

        return true;
    } catch (const std::exception& e) {
        logger_.error("Failed to cache tile: " + std::string(e.what()));
        return false;
    }
}

size_t OSMTileCache::cleanup_expired() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto now = std::chrono::system_clock::now();
    size_t removed = 0;

    auto it = cache_index_.begin();
    while (it != cache_index_.end()) {
        if (now > it->second.expire_time || !std::filesystem::exists(it->second.local_path)) {
            try {
                if (std::filesystem::exists(it->second.local_path)) {
                    std::filesystem::remove(it->second.local_path);
                }
            } catch (const std::exception& e) {
                logger_.warning("Failed to remove expired cache file: " + std::string(e.what()));
            }

            it = cache_index_.erase(it);
            removed++;
            stats_.expired_entries++;
        } else {
            ++it;
        }
    }

    if (removed > 0) {
        logger_.info("Cleaned up " + std::to_string(removed) + " expired cache entries");
    }

    return removed;
}

size_t OSMTileCache::cleanup_lru() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    if (cache_index_.empty()) {
        return 0;
    }

    // Sort entries by cache time (oldest first)
    std::vector<std::pair<std::string, OSMTileEntry*>> entries;
    for (auto& [key, entry] : cache_index_) {
        entries.emplace_back(key, &entry);
    }

    std::sort(entries.begin(), entries.end(),
        [](const auto& a, const auto& b) {
            return a.second->cache_time < b.second->cache_time;
        });

    size_t current_size = get_current_cache_size();
    size_t max_size = config_.max_cache_size_mb * 1024 * 1024;
    size_t removed = 0;

    // Remove oldest entries until we're under the limit
    for (const auto& [key, entry] : entries) {
        if (current_size <= max_size) {
            break;
        }

        try {
            if (std::filesystem::exists(entry->local_path)) {
                current_size -= entry->file_size_bytes;
                std::filesystem::remove(entry->local_path);
            }
            cache_index_.erase(key);
            removed++;
        } catch (const std::exception& e) {
            logger_.warning("Failed to remove LRU cache file: " + std::string(e.what()));
        }
    }

    if (removed > 0) {
        logger_.info("Cleaned up " + std::to_string(removed) + " LRU cache entries");
    }

    return removed;
}

void OSMTileCache::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    for (const auto& [key, entry] : cache_index_) {
        try {
            if (std::filesystem::exists(entry.local_path)) {
                std::filesystem::remove(entry.local_path);
            }
        } catch (const std::exception& e) {
            logger_.warning("Failed to remove cache file during clear: " + std::string(e.what()));
        }
    }

    cache_index_.clear();
    stats_ = OSMCacheStats{};

    logger_.info("Cache cleared");
}

OSMCacheStats OSMTileCache::get_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    OSMCacheStats current_stats = stats_;
    current_stats.total_cache_size_bytes = get_current_cache_size();
    current_stats.total_cached_files = cache_index_.size();

    return current_stats;
}

void OSMTileCache::print_stats() const {
    auto stats = get_stats();

    logger_.info("\n=== OSM Tile Cache Statistics ===");
    logger_.info("Total requests: " + std::to_string(stats.total_requests));
    logger_.info("Cache hits: " + std::to_string(stats.cache_hits) + " (" + std::to_string(static_cast<int>(stats.hit_rate() * 100)) + "%)");
    logger_.info("Cache misses: " + std::to_string(stats.cache_misses));
    logger_.info("Downloads: " + std::to_string(stats.downloads));
    logger_.info("Expired entries: " + std::to_string(stats.expired_entries));
    logger_.info("Cached files: " + std::to_string(stats.total_cached_files));
    logger_.info("Cache size: " + std::to_string(static_cast<int>(stats.total_cache_size_bytes / (1024.0 * 1024.0))) + " MB");
    logger_.info("Average download time: " + std::to_string(static_cast<int>(stats.average_download_time_ms())) + " ms");
    logger_.info("===============================");
}

void OSMTileCache::update_config(const OSMCacheConfig& config) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    config_ = config;
    ensure_cache_directory();
}

std::string OSMTileCache::url_to_cache_path(const std::string& url) const {
    std::string hash = md5_hash(url);
    return config_.cache_directory + "/" + hash.substr(0, 2) + "/" + hash + ".tile";
}

std::string OSMTileCache::compute_cache_key(const std::string& url) const {
    return md5_hash(url);
}

bool OSMTileCache::download_tile(const std::string& url, const std::string& output_path) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    // Ensure output directory exists
    std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());

    std::ofstream file(output_path, std::ios::binary);
    if (!file.is_open()) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, config_.user_agent.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(config_.connection_timeout_seconds));
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    file.close();

    if (res != CURLE_OK || response_code != 200) {
        std::filesystem::remove(output_path);
        return false;
    }

    return true;
}

bool OSMTileCache::validate_cached_entry(const OSMTileEntry& entry) const {
    // Check if file exists
    if (!std::filesystem::exists(entry.local_path)) {
        return false;
    }

    // Check if expired
    auto now = std::chrono::system_clock::now();
    if (now > entry.expire_time) {
        return false;
    }

    return entry.is_valid;
}

void OSMTileCache::load_cache_index() {
    // For simplicity, we'll rebuild the index from the filesystem
    // In a production system, you might want to serialize the index
    if (!std::filesystem::exists(config_.cache_directory)) {
        return;
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(config_.cache_directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tile") {
                std::string filename = entry.path().stem().string();
                // This is a simplified approach - in practice you'd want to store metadata
                OSMTileEntry cache_entry;
                cache_entry.local_path = entry.path().string();
                cache_entry.file_size_bytes = entry.file_size();
                cache_entry.cache_time = std::chrono::system_clock::now();
                cache_entry.expire_time = cache_entry.cache_time + config_.default_expiry;

                cache_index_[filename] = cache_entry;
            }
        }
    } catch (const std::exception& e) {
        logger_.warning("Failed to load cache index: " + std::string(e.what()));
    }
}

void OSMTileCache::save_cache_index() {
    // In a full implementation, you'd serialize the cache index to disk
    // For now, we rely on filesystem metadata
}

void OSMTileCache::update_stats_on_hit() {
    stats_.cache_hits++;
}

void OSMTileCache::update_stats_on_miss() {
    stats_.cache_misses++;
}

void OSMTileCache::update_stats_on_download(std::chrono::milliseconds download_time) {
    stats_.downloads++;
    stats_.total_download_time += download_time;
}

bool OSMTileCache::ensure_cache_directory() const {
    try {
        std::filesystem::create_directories(config_.cache_directory);
        return true;
    } catch (const std::exception& e) {
        logger_.error("Failed to create cache directory: " + std::string(e.what()));
        return false;
    }
}

size_t OSMTileCache::get_current_cache_size() const {
    size_t total_size = 0;
    for (const auto& [key, entry] : cache_index_) {
        total_size += entry.file_size_bytes;
    }
    return total_size;
}

bool OSMTileCache::is_cache_size_exceeded() const {
    size_t current_size = get_current_cache_size();
    size_t max_size = config_.max_cache_size_mb * 1024 * 1024;
    return current_size > max_size;
}

} // namespace topo