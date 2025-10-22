/**
 * @file StackGuard.hpp
 * @brief Stack overflow detection and prevention for segmentation fault prevention
 *
 * Provides stack monitoring to detect potential stack overflow conditions before
 * they cause segmentation faults, especially important for recursive algorithms
 * and deep CGAL operations with complex polygons.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "Logger.hpp"
#include <string>
#include <cstddef>
#include <stdexcept>

namespace topo {

/**
 * @brief Stack usage statistics
 */
struct StackStats {
    size_t stack_size_kb = 0;          ///< Total stack size in KB
    size_t stack_used_kb = 0;          ///< Used stack space in KB
    size_t stack_available_kb = 0;     ///< Available stack space in KB
    double usage_percentage = 0.0;     ///< Stack usage as percentage
    size_t recursion_depth = 0;        ///< Current recursion depth

    /**
     * @brief Check if stack usage is approaching dangerous levels
     */
    bool is_warning_level() const {
        return usage_percentage > 75.0 || stack_available_kb < 512; // 512KB threshold
    }

    /**
     * @brief Check if stack usage is in critical zone
     */
    bool is_critical_level() const {
        return usage_percentage > 90.0 || stack_available_kb < 256; // 256KB threshold
    }
};

/**
 * @brief Stack overflow exception
 */
class StackOverflowException : public std::runtime_error {
public:
    StackOverflowException(const std::string& operation, const StackStats& stats)
        : std::runtime_error("Stack overflow detected in operation: " + operation)
        , operation_(operation), stats_(stats) {}

    const std::string& get_operation() const { return operation_; }
    const StackStats& get_stats() const { return stats_; }

private:
    std::string operation_;
    StackStats stats_;
};

/**
 * @brief Stack monitoring and overflow detection system
 *
 * Monitors stack usage to prevent stack overflow segmentation faults.
 * Particularly important for processing large polygons that may cause
 * deep recursion in CGAL algorithms.
 */
class StackGuard {
public:
    StackGuard();
    ~StackGuard();

    /**
     * @brief Get current stack usage statistics
     */
    StackStats get_current_stats() const;

    /**
     * @brief Check for potential stack overflow
     * @param operation_name Name of current operation for error reporting
     * @throws StackOverflowException if critical stack usage detected
     */
    void check_stack_safety(const std::string& operation_name) const;

    /**
     * @brief Get formatted stack usage string
     */
    std::string format_stack_usage(const StackStats& stats) const;

    /**
     * @brief Log current stack usage
     * @param operation_name Current operation name
     */
    void log_stack_usage(const std::string& operation_name) const;

    /**
     * @brief Set stack warning thresholds
     * @param warning_percentage Warning threshold (default 75%)
     * @param critical_percentage Critical threshold (default 90%)
     */
    void set_warning_thresholds(double warning_percentage, double critical_percentage);

    /**
     * @brief Enable/disable automatic stack monitoring
     * @param enabled true to enable automatic checks
     */
    void set_auto_monitoring(bool enabled) { auto_monitoring_ = enabled; }

    /**
     * @brief Get maximum safe recursion depth estimate
     * @param bytes_per_frame Estimated bytes per stack frame
     */
    size_t get_max_safe_recursion_depth(size_t bytes_per_frame = 1024) const;

    /**
     * @brief Get reference to logger for internal classes like StackScope
     * @return Reference to the logger instance
     */
    Logger& get_logger() { return logger_; }

private:
    void* stack_base_;
    size_t stack_size_;
    double warning_threshold_;
    double critical_threshold_;
    bool auto_monitoring_;
    mutable size_t recursion_counter_;
    Logger logger_;

    /**
     * @brief Get current stack pointer
     */
    void* get_current_stack_pointer() const;

    /**
     * @brief Calculate stack usage from base and current pointer
     */
    StackStats calculate_stack_usage(void* current_sp) const;

    /**
     * @brief Initialize stack base and size detection
     */
    void initialize_stack_detection();
};

/**
 * @brief RAII stack monitoring scope
 *
 * Automatically monitors stack usage within a specific scope and
 * throws exceptions if dangerous stack usage is detected.
 */
class StackScope {
public:
    StackScope(StackGuard& guard, const std::string& operation_name);
    ~StackScope();

private:
    StackGuard& guard_;
    std::string operation_name_;
    StackStats initial_stats_;
};

/**
 * @brief Recursion depth counter for detecting infinite recursion
 */
class RecursionGuard {
public:
    RecursionGuard(const std::string& function_name, size_t max_depth = 1000);
    ~RecursionGuard();

    /**
     * @brief Get current recursion depth
     */
    size_t get_depth() const { return depth_; }

    /**
     * @brief Check if recursion depth is safe
     */
    bool is_safe_depth() const { return depth_ < max_depth_; }

private:
    std::string function_name_;
    size_t max_depth_;
    size_t depth_;
    static thread_local size_t global_recursion_depth_;
};

} // namespace topo