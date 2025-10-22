/**
 * @file MemoryMonitor.hpp
 * @brief Memory usage monitoring and tracking for segmentation fault detection
 *
 * Provides comprehensive memory monitoring to detect potential memory-related crashes
 * before they occur, including heap usage tracking, memory leak detection, and
 * automatic warnings when approaching dangerous memory levels.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include <string>
#include <chrono>
#include <map>
#include <memory>

namespace topo {

/**
 * @brief Memory usage statistics
 */
struct MemoryStats {
    size_t heap_used_mb = 0;           ///< Current heap usage in MB
    size_t heap_peak_mb = 0;           ///< Peak heap usage in MB
    size_t heap_available_mb = 0;      ///< Available heap memory in MB
    size_t virtual_memory_mb = 0;      ///< Virtual memory usage in MB
    size_t resident_memory_mb = 0;     ///< Resident memory usage in MB
    double growth_rate_mb_per_sec = 0.0; ///< Memory growth rate

    /**
     * @brief Check if memory usage is in warning zone
     */
    bool is_warning_level() const {
        return heap_used_mb > 2048; // 2GB warning threshold
    }

    /**
     * @brief Check if memory usage is in critical zone
     */
    bool is_critical_level() const {
        return heap_used_mb > 4096; // 4GB critical threshold
    }
};

/**
 * @brief Memory checkpoint for tracking allocations between stages
 */
struct MemoryCheckpoint {
    std::string stage_name;
    MemoryStats stats;
    std::chrono::steady_clock::time_point timestamp;

    MemoryCheckpoint() = default;

    MemoryCheckpoint(const std::string& name)
        : stage_name(name), timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Comprehensive memory monitoring system
 *
 * Tracks memory usage throughout the pipeline to detect potential segmentation
 * faults before they occur. Provides automatic warnings, leak detection, and
 * detailed memory usage reporting.
 */
class MemoryMonitor {
public:
    MemoryMonitor();
    ~MemoryMonitor();

    /**
     * @brief Get current memory statistics
     */
    MemoryStats get_current_stats() const;

    /**
     * @brief Create a memory checkpoint for a processing stage
     * @param stage_name Name of the processing stage
     */
    void checkpoint(const std::string& stage_name);

    /**
     * @brief Check for memory leaks since last checkpoint
     * @param threshold_mb Leak threshold in MB (default 100MB)
     * @return true if potential leak detected
     */
    bool check_for_leaks(size_t threshold_mb = 100) const;

    /**
     * @brief Check if memory usage is approaching dangerous levels
     * @return true if warning or critical levels reached
     */
    bool check_memory_pressure() const;

    /**
     * @brief Log current memory usage with stage information
     * @param stage_name Current processing stage
     * @param verbose Include detailed memory breakdown
     */
    void log_memory_usage(const std::string& stage_name, bool verbose = false) const;

    /**
     * @brief Get memory usage summary between two checkpoints
     * @param start_stage Start checkpoint name
     * @param end_stage End checkpoint name (empty for current)
     */
    std::string get_usage_summary(const std::string& start_stage,
                                 const std::string& end_stage = "") const;

    /**
     * @brief Enable/disable automatic memory pressure warnings
     * @param enabled true to enable automatic warnings
     */
    void set_auto_warnings(bool enabled) { auto_warnings_ = enabled; }

    /**
     * @brief Set memory warning thresholds
     * @param warning_mb Warning threshold in MB
     * @param critical_mb Critical threshold in MB
     */
    void set_warning_thresholds(size_t warning_mb, size_t critical_mb);

    /**
     * @brief Get formatted memory usage string
     * @param stats Memory statistics to format
     */
    static std::string format_memory_usage(const MemoryStats& stats);

private:
    std::map<std::string, MemoryCheckpoint> checkpoints_;
    std::chrono::steady_clock::time_point start_time_;
    bool auto_warnings_;
    size_t warning_threshold_mb_;
    size_t critical_threshold_mb_;

    /**
     * @brief Get system memory statistics
     */
    MemoryStats get_system_memory_stats() const;

    /**
     * @brief Calculate memory growth rate
     */
    double calculate_growth_rate(const MemoryStats& current,
                                const MemoryCheckpoint& last_checkpoint) const;

    /**
     * @brief Issue warning if memory pressure detected
     */
    void check_and_warn_memory_pressure(const MemoryStats& stats) const;
};

/**
 * @brief RAII memory monitoring scope
 *
 * Automatically creates checkpoints on construction and destruction,
 * perfect for monitoring memory usage within a specific scope.
 */
class MemoryScope {
public:
    MemoryScope(MemoryMonitor& monitor, const std::string& scope_name);
    ~MemoryScope();

private:
    MemoryMonitor& monitor_;
    std::string scope_name_;
    MemoryStats initial_stats_;
};

} // namespace topo