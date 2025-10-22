/**
 * @file CrashHandler.hpp
 * @brief Signal handling and crash state capture for segmentation fault diagnosis
 *
 * Provides comprehensive crash handling with state capture, stack traces,
 * and graceful error reporting to help diagnose segmentation faults and
 * other runtime errors during complex topographic processing.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "MemoryMonitor.hpp"
#include "StackGuard.hpp"
#include <string>
#include <chrono>
#include <functional>
#include <map>
#include <vector>
#include <csignal>

namespace topo {

/**
 * @brief Crash context information
 */
struct CrashContext {
    int signal_number = 0;
    std::string signal_name;
    std::string operation_name;
    std::string current_stage;
    MemoryStats memory_stats;
    StackStats stack_stats;
    std::chrono::steady_clock::time_point crash_time;
    std::string stack_trace;
    std::map<std::string, std::string> debug_info;

    /**
     * @brief Get formatted crash report
     */
    std::string get_crash_report() const;
};

/**
 * @brief Processing stage context for crash diagnosis
 */
struct ProcessingStage {
    std::string stage_name;
    std::string operation_name;
    std::chrono::steady_clock::time_point start_time;
    MemoryStats start_memory;
    StackStats start_stack;
    std::map<std::string, std::string> stage_data;

    ProcessingStage(const std::string& name, const std::string& operation = "")
        : stage_name(name)
        , operation_name(operation)
        , start_time(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Comprehensive crash handling and state capture system
 *
 * Provides signal handling for segmentation faults and other crashes,
 * captures detailed state information including memory usage, stack status,
 * and processing context to aid in debugging complex geometry operations.
 */
class CrashHandler {
public:
    CrashHandler();
    ~CrashHandler();

    /**
     * @brief Install signal handlers for crash detection
     */
    void install_handlers();

    /**
     * @brief Remove signal handlers
     */
    void remove_handlers();

    /**
     * @brief Set memory and stack monitors for state capture
     * @param memory_monitor Memory monitor reference
     * @param stack_guard Stack guard reference
     */
    void set_monitors(MemoryMonitor* memory_monitor, StackGuard* stack_guard);

    /**
     * @brief Enter a processing stage with context capture
     * @param stage_name Name of the processing stage
     * @param operation_name Current operation being performed
     */
    void enter_stage(const std::string& stage_name, const std::string& operation_name = "");

    /**
     * @brief Exit the current processing stage
     */
    void exit_stage();

    /**
     * @brief Add debug information to current context
     * @param key Debug information key
     * @param value Debug information value
     */
    void add_debug_info(const std::string& key, const std::string& value);

    /**
     * @brief Set crash log file path
     * @param log_file Path to crash log file
     */
    void set_crash_log_file(const std::string& log_file);

    /**
     * @brief Enable/disable automatic crash reporting
     * @param enabled true to enable automatic crash reports
     */
    void set_auto_reporting(bool enabled) { auto_reporting_ = enabled; }

    /**
     * @brief Set custom crash callback
     * @param callback Function to call on crash (before default handling)
     */
    void set_crash_callback(std::function<void(const CrashContext&)> callback);

    /**
     * @brief Get current processing context
     */
    std::string get_current_context() const;

    /**
     * @brief Check if crash handlers are installed
     */
    bool handlers_installed() const { return handlers_installed_; }

    /**
     * @brief Manually trigger crash report generation (for testing)
     */
    void generate_test_crash_report() const;

private:
    static CrashHandler* instance_;
    MemoryMonitor* memory_monitor_;
    StackGuard* stack_guard_;
    std::vector<ProcessingStage> stage_stack_;
    std::map<std::string, std::string> global_debug_info_;
    std::string crash_log_file_;
    bool auto_reporting_;
    bool handlers_installed_;
    std::function<void(const CrashContext&)> crash_callback_;

    /**
     * @brief Signal handler function
     */
    static void signal_handler(int signal_number);

    /**
     * @brief Handle crash with state capture
     */
    void handle_crash(int signal_number);

    /**
     * @brief Capture current state for crash report
     */
    CrashContext capture_crash_state(int signal_number) const;

    /**
     * @brief Generate stack trace
     */
    std::string generate_stack_trace() const;

    /**
     * @brief Write crash report to log file
     */
    void write_crash_log(const CrashContext& context) const;

    /**
     * @brief Get signal name from number
     */
    static std::string get_signal_name(int signal_number);

public:
    /**
     * @brief Get formatted timestamp
     */
    static std::string get_timestamp();

private:
};

/**
 * @brief RAII processing stage scope
 *
 * Automatically enters and exits processing stages for crash context tracking.
 */
class ProcessingScope {
public:
    ProcessingScope(CrashHandler& handler, const std::string& stage_name,
                   const std::string& operation_name = "");
    ~ProcessingScope();

private:
    CrashHandler& handler_;
    std::string stage_name_;
};

/**
 * @brief Safe execution wrapper with crash handling
 *
 * Executes operations with crash protection and state capture.
 */
class SafeExecutor {
public:
    SafeExecutor(CrashHandler& handler);

    /**
     * @brief Execute function safely with crash protection
     * @tparam T Return type
     * @param operation_name Operation name for context
     * @param operation Function to execute
     * @return Result of operation
     */
    template<typename T>
    T execute_safe(const std::string& operation_name, std::function<T()> operation);

    /**
     * @brief Execute void function safely with crash protection
     * @param operation_name Operation name for context
     * @param operation Function to execute
     */
    void execute_safe_void(const std::string& operation_name, std::function<void()> operation);

private:
    CrashHandler& handler_;
};

template<typename T>
T SafeExecutor::execute_safe(const std::string& operation_name, std::function<T()> operation) {
    ProcessingScope scope(handler_, "safe_execution", operation_name);
    handler_.add_debug_info("operation_type", "templated_return");

    return operation();
}

} // namespace topo