/**
 * @file StackGuard.cpp
 * @brief Implementation of stack overflow detection for segmentation fault prevention
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "StackGuard.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef __APPLE__
#include <pthread.h>
#include <sys/resource.h>
#elif __linux__
#include <pthread.h>
#include <sys/resource.h>
#elif _WIN32
#include <windows.h>
#endif

namespace topo {

StackGuard::StackGuard()
    : stack_base_(nullptr)
    , stack_size_(0)
    , warning_threshold_(75.0)
    , critical_threshold_(90.0)
    , auto_monitoring_(true)
    , recursion_counter_(0)
    , logger_("StackGuard") {
    initialize_stack_detection();
}

StackGuard::~StackGuard() {
    if (auto_monitoring_) {
        auto final_stats = get_current_stats();
        if (final_stats.is_warning_level()) {
            logger_.warning("Final stack usage: " + format_stack_usage(final_stats));
        }
    }
    logger_.flush();
}

StackStats StackGuard::get_current_stats() const {
    void* current_sp = get_current_stack_pointer();
    return calculate_stack_usage(current_sp);
}

void StackGuard::check_stack_safety(const std::string& operation_name) const {
    auto stats = get_current_stats();

    if (stats.is_critical_level()) {
        throw StackOverflowException(operation_name, stats);
    }

    if (auto_monitoring_ && stats.is_warning_level()) {
        logger_.warning(operation_name + ": " + format_stack_usage(stats));
    }
}

std::string StackGuard::format_stack_usage(const StackStats& stats) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << stats.usage_percentage << "% used ("
        << stats.stack_used_kb << "/" << stats.stack_size_kb << " KB)";

    if (stats.is_critical_level()) {
        oss << " [CRITICAL]";
    } else if (stats.is_warning_level()) {
        oss << " [WARNING]";
    }

    if (stats.recursion_depth > 0) {
        oss << ", depth: " << stats.recursion_depth;
    }

    return oss.str();
}

void StackGuard::log_stack_usage(const std::string& operation_name) const {
    auto stats = get_current_stats();
    logger_.info(operation_name + ": " + format_stack_usage(stats));
}

void StackGuard::set_warning_thresholds(double warning_percentage, double critical_percentage) {
    warning_threshold_ = warning_percentage;
    critical_threshold_ = critical_percentage;
}

size_t StackGuard::get_max_safe_recursion_depth(size_t bytes_per_frame) const {
    auto stats = get_current_stats();
    size_t available_bytes = stats.stack_available_kb * 1024;

    // Reserve 25% of available stack for safety
    available_bytes = static_cast<size_t>(available_bytes * 0.75);

    return available_bytes / bytes_per_frame;
}

void* StackGuard::get_current_stack_pointer() const {
    void* sp;
#ifdef __GNUC__
    sp = __builtin_frame_address(0);
#elif _MSC_VER
    sp = _AddressOfReturnAddress();
#else
    // Fallback: use address of local variable
    volatile char dummy;
    sp = (void*)&dummy;
#endif
    return sp;
}

StackStats StackGuard::calculate_stack_usage(void* current_sp) const {
    StackStats stats;
    stats.stack_size_kb = stack_size_ / 1024;
    stats.recursion_depth = recursion_counter_;

    if (stack_base_ && current_sp) {
        // Calculate stack usage based on direction of stack growth
        ptrdiff_t used_bytes;

#ifdef __APPLE__
        // Stack grows downward on most systems
        if (current_sp < stack_base_) {
            used_bytes = static_cast<char*>(stack_base_) - static_cast<char*>(current_sp);
        } else {
            used_bytes = static_cast<char*>(current_sp) - static_cast<char*>(stack_base_);
        }
#else
        // Assume stack grows downward
        used_bytes = static_cast<char*>(stack_base_) - static_cast<char*>(current_sp);
        if (used_bytes < 0) {
            used_bytes = -used_bytes; // Handle upward growing stacks
        }
#endif

        stats.stack_used_kb = static_cast<size_t>(used_bytes) / 1024;

        if (stats.stack_used_kb > stats.stack_size_kb) {
            stats.stack_used_kb = stats.stack_size_kb; // Cap at total size
        }

        stats.stack_available_kb = stats.stack_size_kb - stats.stack_used_kb;
        stats.usage_percentage = (static_cast<double>(stats.stack_used_kb) / stats.stack_size_kb) * 100.0;
    }

    return stats;
}

void StackGuard::initialize_stack_detection() {
#if defined(__linux__)
    pthread_t self = pthread_self();
    void* stack_addr;
    size_t stack_size;

    pthread_attr_t attr;
    if (pthread_getattr_np(self, &attr) == 0) {
        if (pthread_attr_getstack(&attr, &stack_addr, &stack_size) == 0) {
            stack_base_ = static_cast<char*>(stack_addr) + stack_size;
            stack_size_ = stack_size;
        }
        pthread_attr_destroy(&attr);
    }

#elif defined(__APPLE__)
    // macOS-specific stack detection using pthread_get_stackaddr_np
    stack_base_ = pthread_get_stackaddr_np(pthread_self());
    stack_size_ = pthread_get_stacksize_np(pthread_self());

#endif

    // Fallback: use resource limits if no platform-specific method worked
    if (stack_size_ == 0) {
        struct rlimit rl;
        if (getrlimit(RLIMIT_STACK, &rl) == 0) {
            stack_size_ = rl.rlim_cur;
            // Estimate stack base from current position
            void* current_sp = get_current_stack_pointer();
            stack_base_ = static_cast<char*>(current_sp) + (stack_size_ / 2);
        }
    }

#ifdef _WIN32
    // Windows implementation
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(get_current_stack_pointer(), &mbi, sizeof(mbi));

    stack_base_ = static_cast<char*>(mbi.BaseAddress) + mbi.RegionSize;
    stack_size_ = mbi.RegionSize;
#endif

    // Default fallback
    if (stack_size_ == 0) {
        stack_size_ = 8 * 1024 * 1024; // 8MB default
        void* current_sp = get_current_stack_pointer();
        stack_base_ = static_cast<char*>(current_sp) + (stack_size_ / 2);
    }
}

// StackScope implementation
StackScope::StackScope(StackGuard& guard, const std::string& operation_name)
    : guard_(guard), operation_name_(operation_name) {
    initial_stats_ = guard_.get_current_stats();
    guard_.check_stack_safety(operation_name);
}

StackScope::~StackScope() {
    auto final_stats = guard_.get_current_stats();

    // Check if significant stack usage increase occurred
    if (final_stats.stack_used_kb > initial_stats_.stack_used_kb + 100) { // 100KB threshold
        guard_.get_logger().info(operation_name_ + " used " + std::to_string(final_stats.stack_used_kb - initial_stats_.stack_used_kb) + " KB additional stack");
    }
}

// RecursionGuard implementation
thread_local size_t RecursionGuard::global_recursion_depth_ = 0;

RecursionGuard::RecursionGuard(const std::string& function_name, size_t max_depth)
    : function_name_(function_name), max_depth_(max_depth), depth_(++global_recursion_depth_) {
    if (depth_ > max_depth_) {
        --global_recursion_depth_; // Rollback
        throw StackOverflowException("Recursion depth exceeded in " + function_name,
                                   StackStats{0, 0, 0, 0.0, depth_});
    }
}

RecursionGuard::~RecursionGuard() {
    --global_recursion_depth_;
}

} // namespace topo