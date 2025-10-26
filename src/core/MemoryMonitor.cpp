/**
 * @file MemoryMonitor.cpp
 * @brief Implementation of memory usage monitoring for segmentation fault detection
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "MemoryMonitor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef __APPLE__
#include <mach/mach.h>
#include <sys/resource.h>
#elif __linux__
#include <sys/resource.h>
#include <fstream>
#elif _WIN32
#define NOMINMAX  // Prevent Windows.h from defining min/max macros
#include <windows.h>
#include <psapi.h>
#endif

namespace topo {

MemoryMonitor::MemoryMonitor()
    : start_time_(std::chrono::steady_clock::now())
    , auto_warnings_(true)
    , warning_threshold_mb_(2048)  // 2GB
    , critical_threshold_mb_(4096) // 4GB
{
    checkpoint("initialization");
}

MemoryMonitor::~MemoryMonitor() {
    checkpoint("cleanup");

    if (auto_warnings_) {
        auto final_stats = get_current_stats();
        std::cout << "\n=== Final Memory Usage Summary ===" << std::endl;
        std::cout << format_memory_usage(final_stats) << std::endl;

        if (!checkpoints_.empty()) {
            auto start_it = checkpoints_.find("initialization");
            if (start_it != checkpoints_.end()) {
                std::cout << get_usage_summary("initialization", "cleanup") << std::endl;
            }
        }
        std::cout << "==================================" << std::endl;
    }
}

MemoryStats MemoryMonitor::get_current_stats() const {
    return get_system_memory_stats();
}

void MemoryMonitor::checkpoint(const std::string& stage_name) {
    MemoryCheckpoint checkpoint(stage_name);
    checkpoint.stats = get_system_memory_stats();

    // Calculate growth rate if we have a previous checkpoint
    if (!checkpoints_.empty()) {
        auto last_checkpoint = checkpoints_.rbegin()->second;
        checkpoint.stats.growth_rate_mb_per_sec =
            calculate_growth_rate(checkpoint.stats, last_checkpoint);
    }

    checkpoints_[stage_name] = checkpoint;

    if (auto_warnings_) {
        check_and_warn_memory_pressure(checkpoint.stats);
    }
}

bool MemoryMonitor::check_for_leaks(size_t threshold_mb) const {
    if (checkpoints_.size() < 2) return false;

    auto it = checkpoints_.rbegin();
    auto current = it->second;
    ++it;
    auto previous = it->second;

    size_t memory_increase = 0;
    if (current.stats.heap_used_mb > previous.stats.heap_used_mb) {
        memory_increase = current.stats.heap_used_mb - previous.stats.heap_used_mb;
    }

    return memory_increase > threshold_mb;
}

bool MemoryMonitor::check_memory_pressure() const {
    auto stats = get_current_stats();
    return stats.is_warning_level() || stats.is_critical_level();
}

void MemoryMonitor::log_memory_usage(const std::string& stage_name, bool verbose) const {
    auto stats = get_current_stats();

    std::cout << "[MEMORY] " << stage_name << ": " << format_memory_usage(stats);

    if (verbose) {
        std::cout << "\n  Detailed breakdown:";
        std::cout << "\n    Heap used: " << stats.heap_used_mb << " MB";
        std::cout << "\n    Heap peak: " << stats.heap_peak_mb << " MB";
        std::cout << "\n    Virtual: " << stats.virtual_memory_mb << " MB";
        std::cout << "\n    Resident: " << stats.resident_memory_mb << " MB";
        if (stats.growth_rate_mb_per_sec > 0) {
            std::cout << "\n    Growth rate: " << std::fixed << std::setprecision(2)
                     << stats.growth_rate_mb_per_sec << " MB/sec";
        }
    }

    std::cout << std::endl;
}

std::string MemoryMonitor::get_usage_summary(const std::string& start_stage,
                                           const std::string& end_stage) const {
    auto start_it = checkpoints_.find(start_stage);
    if (start_it == checkpoints_.end()) {
        return "Start stage '" + start_stage + "' not found";
    }

    MemoryCheckpoint end_checkpoint("current");
    if (end_stage.empty()) {
        end_checkpoint.stats = get_current_stats();
        end_checkpoint.timestamp = std::chrono::steady_clock::now();
    } else {
        auto end_it = checkpoints_.find(end_stage);
        if (end_it == checkpoints_.end()) {
            return "End stage '" + end_stage + "' not found";
        }
        end_checkpoint = end_it->second;
    }

    auto start_stats = start_it->second.stats;
    auto end_stats = end_checkpoint.stats;

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_checkpoint.timestamp - start_it->second.timestamp);

    std::ostringstream summary;
    summary << "Memory usage from '" << start_stage << "' to '"
            << (end_stage.empty() ? "current" : end_stage) << "':\n";
    summary << "  Duration: " << duration.count() << "ms\n";
    summary << "  Start: " << start_stats.heap_used_mb << " MB\n";
    summary << "  End: " << end_stats.heap_used_mb << " MB\n";

    if (end_stats.heap_used_mb > start_stats.heap_used_mb) {
        summary << "  Increase: +" << (end_stats.heap_used_mb - start_stats.heap_used_mb) << " MB\n";
    } else if (start_stats.heap_used_mb > end_stats.heap_used_mb) {
        summary << "  Decrease: -" << (start_stats.heap_used_mb - end_stats.heap_used_mb) << " MB\n";
    } else {
        summary << "  Change: 0 MB\n";
    }

    summary << "  Peak during period: " << std::max(start_stats.heap_peak_mb, end_stats.heap_peak_mb) << " MB";

    return summary.str();
}

void MemoryMonitor::set_warning_thresholds(size_t warning_mb, size_t critical_mb) {
    warning_threshold_mb_ = warning_mb;
    critical_threshold_mb_ = critical_mb;
}

std::string MemoryMonitor::format_memory_usage(const MemoryStats& stats) {
    std::ostringstream oss;
    oss << stats.heap_used_mb << " MB";

    if (stats.is_critical_level()) {
        oss << " [CRITICAL]";
    } else if (stats.is_warning_level()) {
        oss << " [WARNING]";
    }

    if (stats.heap_peak_mb > stats.heap_used_mb) {
        oss << " (peak: " << stats.heap_peak_mb << " MB)";
    }

    return oss.str();
}

MemoryStats MemoryMonitor::get_system_memory_stats() const {
    MemoryStats stats;

#ifdef __APPLE__
    // macOS implementation using mach APIs
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t info_count = MACH_TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  (task_info_t)&info, &info_count) == KERN_SUCCESS) {
        stats.virtual_memory_mb = info.virtual_size / (1024 * 1024);
        stats.resident_memory_mb = info.resident_size / (1024 * 1024);
        stats.heap_used_mb = stats.resident_memory_mb; // Approximation
    }

    // Get peak memory usage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        stats.heap_peak_mb = usage.ru_maxrss / (1024 * 1024); // macOS reports in bytes
    }

#elif __linux__
    // Linux implementation using /proc/self/status
    std::ifstream status_file("/proc/self/status");
    std::string line;

    while (std::getline(status_file, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            size_t kb = std::stoul(line.substr(7));
            stats.resident_memory_mb = kb / 1024;
            stats.heap_used_mb = stats.resident_memory_mb;
        } else if (line.substr(0, 7) == "VmSize:") {
            size_t kb = std::stoul(line.substr(8));
            stats.virtual_memory_mb = kb / 1024;
        } else if (line.substr(0, 6) == "VmPeak:") {
            size_t kb = std::stoul(line.substr(7));
            stats.heap_peak_mb = kb / 1024;
        }
    }

#elif _WIN32
    // Windows implementation using Windows APIs
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        stats.virtual_memory_mb = pmc.PrivateUsage / (1024 * 1024);
        stats.resident_memory_mb = pmc.WorkingSetSize / (1024 * 1024);
        stats.heap_used_mb = stats.resident_memory_mb;
        stats.heap_peak_mb = pmc.PeakWorkingSetSize / (1024 * 1024);
    }
#endif

    // Estimate available memory (simplified)
    stats.heap_available_mb = std::max(static_cast<size_t>(0),
                                      static_cast<size_t>(8192) - stats.heap_used_mb); // Assume 8GB system

    return stats;
}

double MemoryMonitor::calculate_growth_rate(const MemoryStats& current,
                                          const MemoryCheckpoint& last_checkpoint) const {
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_checkpoint.timestamp);

    if (time_diff.count() == 0) return 0.0;

    if (current.heap_used_mb > last_checkpoint.stats.heap_used_mb) {
        size_t memory_diff = current.heap_used_mb - last_checkpoint.stats.heap_used_mb;
        return (static_cast<double>(memory_diff) * 1000.0) / time_diff.count(); // MB per second
    }

    return 0.0;
}

void MemoryMonitor::check_and_warn_memory_pressure(const MemoryStats& stats) const {
    if (stats.is_critical_level()) {
        std::cout << "\n*** CRITICAL MEMORY WARNING ***" << std::endl;
        std::cout << "Memory usage: " << stats.heap_used_mb << " MB (>4GB critical threshold)" << std::endl;
        std::cout << "Consider reducing polygon complexity or enabling incremental processing" << std::endl;
        std::cout << "******************************\n" << std::endl;
    } else if (stats.is_warning_level()) {
        std::cout << "\n** Memory Warning: " << stats.heap_used_mb << " MB (>2GB threshold) **\n" << std::endl;
    }

    if (stats.growth_rate_mb_per_sec > 50.0) { // 50 MB/sec growth rate warning
        std::cout << "\n** Rapid Memory Growth Warning: "
                 << std::fixed << std::setprecision(1) << stats.growth_rate_mb_per_sec
                 << " MB/sec **\n" << std::endl;
    }
}

// MemoryScope implementation
MemoryScope::MemoryScope(MemoryMonitor& monitor, const std::string& scope_name)
    : monitor_(monitor), scope_name_(scope_name) {
    initial_stats_ = monitor_.get_current_stats();
    monitor_.checkpoint(scope_name + "_start");
}

MemoryScope::~MemoryScope() {
    monitor_.checkpoint(scope_name_ + "_end");

    auto final_stats = monitor_.get_current_stats();
    if (final_stats.heap_used_mb > initial_stats_.heap_used_mb + 50) { // 50MB threshold
        std::cout << "[MEMORY SCOPE] " << scope_name_ << " used "
                 << (final_stats.heap_used_mb - initial_stats_.heap_used_mb)
                 << " MB additional memory" << std::endl;
    }
}

} // namespace topo