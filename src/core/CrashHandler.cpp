/**
 * @file CrashHandler.cpp
 * @brief Implementation of signal handling and crash state capture
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "CrashHandler.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

#ifdef __APPLE__
#include <execinfo.h>
#include <unistd.h>
#elif __linux__
#include <execinfo.h>
#include <unistd.h>
#elif _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

namespace topo {

// Static instance for signal handler
CrashHandler* CrashHandler::instance_ = nullptr;

std::string CrashContext::get_crash_report() const {
    std::ostringstream report;

    report << "=== CRASH REPORT ===" << std::endl;
    report << "Time: " << CrashHandler::get_timestamp() << std::endl;
    report << "Signal: " << signal_number << " (" << signal_name << ")" << std::endl;
    report << "Operation: " << operation_name << std::endl;
    report << "Stage: " << current_stage << std::endl;
    report << std::endl;

    report << "=== MEMORY STATE ===" << std::endl;
    report << "Heap used: " << memory_stats.heap_used_mb << " MB" << std::endl;
    report << "Heap peak: " << memory_stats.heap_peak_mb << " MB" << std::endl;
    report << "Virtual memory: " << memory_stats.virtual_memory_mb << " MB" << std::endl;
    report << "Resident memory: " << memory_stats.resident_memory_mb << " MB" << std::endl;
    if (memory_stats.is_critical_level()) {
        report << "*** MEMORY IN CRITICAL STATE ***" << std::endl;
    } else if (memory_stats.is_warning_level()) {
        report << "*** MEMORY IN WARNING STATE ***" << std::endl;
    }
    report << std::endl;

    report << "=== STACK STATE ===" << std::endl;
    report << "Stack used: " << stack_stats.stack_used_kb << " KB" << std::endl;
    report << "Stack size: " << stack_stats.stack_size_kb << " KB" << std::endl;
    report << "Stack usage: " << std::fixed << std::setprecision(1)
           << stack_stats.usage_percentage << "%" << std::endl;
    report << "Recursion depth: " << stack_stats.recursion_depth << std::endl;
    if (stack_stats.is_critical_level()) {
        report << "*** STACK IN CRITICAL STATE ***" << std::endl;
    } else if (stack_stats.is_warning_level()) {
        report << "*** STACK IN WARNING STATE ***" << std::endl;
    }
    report << std::endl;

    if (!debug_info.empty()) {
        report << "=== DEBUG INFO ===" << std::endl;
        for (const auto& [key, value] : debug_info) {
            report << key << ": " << value << std::endl;
        }
        report << std::endl;
    }

    if (!stack_trace.empty()) {
        report << "=== STACK TRACE ===" << std::endl;
        report << stack_trace << std::endl;
    }

    report << "===================" << std::endl;

    return report.str();
}

CrashHandler::CrashHandler()
    : memory_monitor_(nullptr)
    , stack_guard_(nullptr)
    , crash_log_file_("crash_report.log")
    , auto_reporting_(true)
    , handlers_installed_(false) {

    if (instance_ == nullptr) {
        instance_ = this;
    }
}

CrashHandler::~CrashHandler() {
    remove_handlers();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void CrashHandler::install_handlers() {
    if (handlers_installed_) {
        return;
    }

    // Install signal handlers for common crash signals
    std::signal(SIGSEGV, signal_handler);  // Segmentation fault
    std::signal(SIGABRT, signal_handler);  // Abort
    std::signal(SIGFPE, signal_handler);   // Floating point exception
    std::signal(SIGILL, signal_handler);   // Illegal instruction

#ifndef _WIN32
    std::signal(SIGBUS, signal_handler);   // Bus error (Unix)
#endif

    handlers_installed_ = true;

    if (auto_reporting_) {
        std::cout << "[CRASH HANDLER] Signal handlers installed" << std::endl;
    }
}

void CrashHandler::remove_handlers() {
    if (!handlers_installed_) {
        return;
    }

    // Restore default signal handlers
    std::signal(SIGSEGV, SIG_DFL);
    std::signal(SIGABRT, SIG_DFL);
    std::signal(SIGFPE, SIG_DFL);
    std::signal(SIGILL, SIG_DFL);

#ifndef _WIN32
    std::signal(SIGBUS, SIG_DFL);
#endif

    handlers_installed_ = false;

    if (auto_reporting_) {
        std::cout << "[CRASH HANDLER] Signal handlers removed" << std::endl;
    }
}

void CrashHandler::set_monitors(MemoryMonitor* memory_monitor, StackGuard* stack_guard) {
    memory_monitor_ = memory_monitor;
    stack_guard_ = stack_guard;
}

void CrashHandler::enter_stage(const std::string& stage_name, const std::string& operation_name) {
    ProcessingStage stage(stage_name, operation_name);

    if (memory_monitor_) {
        stage.start_memory = memory_monitor_->get_current_stats();
    }

    if (stack_guard_) {
        stage.start_stack = stack_guard_->get_current_stats();
    }

    stage_stack_.push_back(stage);

    if (auto_reporting_) {
        std::cout << "[CRASH HANDLER] Entered stage: " << stage_name;
        if (!operation_name.empty()) {
            std::cout << " (" << operation_name << ")";
        }
        std::cout << std::endl;
    }
}

void CrashHandler::exit_stage() {
    if (!stage_stack_.empty()) {
        auto stage = stage_stack_.back();
        stage_stack_.pop_back();

        if (auto_reporting_) {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - stage.start_time);
            std::cout << "[CRASH HANDLER] Exited stage: " << stage.stage_name
                     << " (duration: " << duration.count() << "ms)" << std::endl;
        }
    }
}

void CrashHandler::add_debug_info(const std::string& key, const std::string& value) {
    global_debug_info_[key] = value;
}

void CrashHandler::set_crash_log_file(const std::string& log_file) {
    crash_log_file_ = log_file;
}

void CrashHandler::set_crash_callback(std::function<void(const CrashContext&)> callback) {
    crash_callback_ = callback;
}

std::string CrashHandler::get_current_context() const {
    std::ostringstream context;

    if (!stage_stack_.empty()) {
        context << "Current stage: " << stage_stack_.back().stage_name;
        if (!stage_stack_.back().operation_name.empty()) {
            context << " (" << stage_stack_.back().operation_name << ")";
        }
        context << std::endl;

        if (stage_stack_.size() > 1) {
            context << "Stage stack: ";
            for (size_t i = 0; i < stage_stack_.size(); ++i) {
                if (i > 0) context << " -> ";
                context << stage_stack_[i].stage_name;
            }
            context << std::endl;
        }
    } else {
        context << "No active processing stage" << std::endl;
    }

    if (memory_monitor_) {
        auto memory_stats = memory_monitor_->get_current_stats();
        context << "Memory: " << memory_stats.heap_used_mb << " MB";
        if (memory_stats.is_warning_level()) {
            context << " [WARNING]";
        }
        context << std::endl;
    }

    if (stack_guard_) {
        auto stack_stats = stack_guard_->get_current_stats();
        context << "Stack: " << std::fixed << std::setprecision(1)
                << stack_stats.usage_percentage << "%";
        if (stack_stats.is_warning_level()) {
            context << " [WARNING]";
        }
        context << std::endl;
    }

    return context.str();
}

void CrashHandler::generate_test_crash_report() const {
    CrashContext test_context;
    test_context.signal_number = SIGUSR1;
    test_context.signal_name = "TEST_SIGNAL";
    test_context.operation_name = "test_crash_report";
    test_context.current_stage = stage_stack_.empty() ? "unknown" : stage_stack_.back().stage_name;
    test_context.crash_time = std::chrono::steady_clock::now();
    test_context.debug_info = global_debug_info_;

    if (memory_monitor_) {
        test_context.memory_stats = memory_monitor_->get_current_stats();
    }

    if (stack_guard_) {
        test_context.stack_stats = stack_guard_->get_current_stats();
    }

    test_context.stack_trace = generate_stack_trace();

    std::cout << test_context.get_crash_report() << std::endl;
}

void CrashHandler::signal_handler(int signal_number) {
    if (instance_) {
        instance_->handle_crash(signal_number);
    }

    // Re-raise the signal with default handler
    std::signal(signal_number, SIG_DFL);
    std::raise(signal_number);
}

void CrashHandler::handle_crash(int signal_number) {
    // Capture crash state
    CrashContext context = capture_crash_state(signal_number);

    // Call custom callback if set
    if (crash_callback_) {
        try {
            crash_callback_(context);
        } catch (...) {
            // Ignore callback exceptions during crash handling
        }
    }

    // Write to console
    std::cerr << std::endl << "*** FATAL ERROR ***" << std::endl;
    std::cerr << context.get_crash_report() << std::endl;

    // Write to log file
    write_crash_log(context);

    std::cerr << "Crash report written to: " << crash_log_file_ << std::endl;
}

CrashContext CrashHandler::capture_crash_state(int signal_number) const {
    CrashContext context;
    context.signal_number = signal_number;
    context.signal_name = get_signal_name(signal_number);
    context.crash_time = std::chrono::steady_clock::now();
    context.debug_info = global_debug_info_;

    if (!stage_stack_.empty()) {
        context.current_stage = stage_stack_.back().stage_name;
        context.operation_name = stage_stack_.back().operation_name;
    }

    if (memory_monitor_) {
        context.memory_stats = memory_monitor_->get_current_stats();
    }

    if (stack_guard_) {
        context.stack_stats = stack_guard_->get_current_stats();
    }

    context.stack_trace = generate_stack_trace();

    return context;
}

std::string CrashHandler::generate_stack_trace() const {
#if defined(__APPLE__) || defined(__linux__)
    // Unix stack trace using backtrace
    void* buffer[256];
    int nptrs = backtrace(buffer, 256);
    char** strings = backtrace_symbols(buffer, nptrs);

    std::ostringstream trace;
    if (strings) {
        for (int i = 0; i < nptrs; ++i) {
            trace << strings[i] << std::endl;
        }
        free(strings);
    } else {
        trace << "Failed to capture stack trace" << std::endl;
    }

    return trace.str();

#elif _WIN32
    // Windows stack trace using CaptureStackBackTrace
    void* buffer[256];
    USHORT frames = CaptureStackBackTrace(0, 256, buffer, nullptr);

    std::ostringstream trace;
    for (USHORT i = 0; i < frames; ++i) {
        trace << "Frame " << i << ": " << buffer[i] << std::endl;
    }

    return trace.str();

#else
    return "Stack trace not available on this platform";
#endif
}

void CrashHandler::write_crash_log(const CrashContext& context) const {
    try {
        std::ofstream log_file(crash_log_file_, std::ios::app);
        if (log_file.is_open()) {
            log_file << context.get_crash_report() << std::endl;
            log_file.close();
        }
    } catch (...) {
        // Ignore file writing errors during crash handling
    }
}

std::string CrashHandler::get_signal_name(int signal_number) {
    switch (signal_number) {
        case SIGSEGV: return "SIGSEGV (Segmentation fault)";
        case SIGABRT: return "SIGABRT (Abort)";
        case SIGFPE: return "SIGFPE (Floating point exception)";
        case SIGILL: return "SIGILL (Illegal instruction)";
#ifndef _WIN32
        case SIGBUS: return "SIGBUS (Bus error)";
#endif
        default: return "Unknown signal (" + std::to_string(signal_number) + ")";
    }
}

std::string CrashHandler::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);

    std::ostringstream timestamp;
    timestamp << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return timestamp.str();
}

// ProcessingScope implementation
ProcessingScope::ProcessingScope(CrashHandler& handler, const std::string& stage_name,
                               const std::string& operation_name)
    : handler_(handler), stage_name_(stage_name) {
    handler_.enter_stage(stage_name, operation_name);
}

ProcessingScope::~ProcessingScope() {
    handler_.exit_stage();
}

// SafeExecutor implementation
SafeExecutor::SafeExecutor(CrashHandler& handler) : handler_(handler) {}

void SafeExecutor::execute_safe_void(const std::string& operation_name, std::function<void()> operation) {
    ProcessingScope scope(handler_, "safe_execution", operation_name);
    handler_.add_debug_info("operation_type", "void_return");

    operation();
}

} // namespace topo