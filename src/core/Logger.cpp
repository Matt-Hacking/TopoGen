/**
 * @file Logger.cpp
 * @brief Implementation of centralized logging system
 */

#include "Logger.hpp"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace topo {

// Initialize static members for facility-based logging
std::unordered_map<std::string, LogLevel> Logger::facility_levels_;
LogLevel Logger::default_level_ = LogLevel::INFO;  // Default to Level 3 (INFO)
std::mutex Logger::registry_mutex_;

Logger::Logger() : current_level_(LogLevel::WARNING), component_name_(""),
                   repeat_count_(0), has_last_message_(false) {
}

Logger::Logger(const std::string& component_name)
    : current_level_(LogLevel::WARNING), component_name_(component_name),
      repeat_count_(0), has_last_message_(false) {
}

Logger::Logger(LogLevel level, const std::optional<std::string>& log_file)
    : current_level_(level), component_name_(""), log_file_path_(log_file),
      repeat_count_(0), has_last_message_(false) {
    if (log_file.has_value()) {
        initializeFileStream();
    }
}

Logger::~Logger() {
    // Output any pending duplicate message summary before destruction
    std::lock_guard<std::mutex> lock(output_mutex_);
    if (has_last_message_ && repeat_count_ > 0) {
        doOutput(last_level_, "The previous message occurred " + std::to_string(repeat_count_ + 1) + " times.");
        repeat_count_ = 0;  // Reset after output
    }

    // Flush without locking (we already hold the lock)
    std::cout.flush();
    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->flush();
    }
}

void Logger::outputMessage(LogLevel level, const std::string& message) const {
    // THIS IS THE SINGLE POINT OF LOGGING CONTROL
    // Exactly one verbosity check and exactly one std::cout call (in doOutput)

    std::lock_guard<std::mutex> lock(output_mutex_);

    // Single verbosity check using facility-aware effective level
    if (static_cast<int>(level) <= static_cast<int>(getEffectiveLevel())) {

        // Check if this is a duplicate of the last message
        if (has_last_message_ && message == last_message_ && level == last_level_) {
            // Duplicate detected - increment count and return (don't output yet)
            repeat_count_++;
            return;
        }

        // This is a new message - handle any pending duplicates first
        if (has_last_message_ && repeat_count_ > 0) {
            // Output the duplicate count summary for the previous message
            doOutput(last_level_, "The previous message occurred " + std::to_string(repeat_count_ + 1) + " times.");
        }

        // Output the new message
        doOutput(level, message);

        // Update state for next comparison
        last_message_ = message;
        last_level_ = level;
        repeat_count_ = 0;
        has_last_message_ = true;
    }
}

void Logger::setLogFile(const std::optional<std::string>& log_file) {
    std::lock_guard<std::mutex> lock(output_mutex_);

    log_file_path_ = log_file;

    // Close existing stream
    if (file_stream_) {
        file_stream_->close();
        file_stream_.reset();
    }

    // Initialize new stream if path provided
    if (log_file.has_value()) {
        initializeFileStream();
    }
}

void Logger::initializeFileStream() {
    try {
        // Create directory if it doesn't exist
        if (log_file_path_.has_value()) {
            std::filesystem::path log_path(log_file_path_.value());
            if (log_path.has_parent_path()) {
                std::filesystem::create_directories(log_path.parent_path());
            }

            // Open file in append mode (as specified in CLI help)
            file_stream_ = std::make_shared<std::ofstream>(
                log_file_path_.value(),
                std::ios::app  // Append mode - "append if exists" per CLI help
            );

            if (!file_stream_->is_open()) {
                // Don't use outputMessage here to avoid recursion
                std::cerr << "Warning: Failed to open log file: " << log_file_path_.value() << std::endl;
                file_stream_.reset();
            }
        }
    } catch (const std::exception& e) {
        // Don't use outputMessage here to avoid recursion
        std::cerr << "Warning: Exception opening log file: " << e.what() << std::endl;
        file_stream_.reset();
    }
}

void Logger::doOutput([[maybe_unused]] LogLevel level, const std::string& message) const {
    // THE ACTUAL SINGLE POINT OF OUTPUT
    // Contains exactly one std::cout call as originally requested

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    // Format timestamp as HH:MM:SS.mmm
    std::tm* tm = std::localtime(&time_t);
    char timestamp[32];
    std::snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d.%03d",
                  tm->tm_hour, tm->tm_min, tm->tm_sec, static_cast<int>(ms.count()));

    // Single std::cout call with timestamp
    std::cout << "[" << timestamp << "] " << message << std::endl;

    // Optional file output (if enabled)
    if (file_stream_ && file_stream_->is_open()) {
        *file_stream_ << message << std::endl;
        file_stream_->flush();  // Ensure immediate write for debugging
    }
}

void Logger::flush() const {
    std::lock_guard<std::mutex> lock(output_mutex_);

    // Output any pending duplicate message summary before flush
    if (has_last_message_ && repeat_count_ > 0) {
        doOutput(last_level_, "The previous message occurred " + std::to_string(repeat_count_ + 1) + " times.");
        repeat_count_ = 0;  // Reset after output
    }

    // Flush console output
    std::cout.flush();

    // Flush file output if available
    if (file_stream_ && file_stream_->is_open()) {
        file_stream_->flush();
    }
}

// ============================================================================
// Facility-based logging implementation
// ============================================================================

void Logger::setFacilityLevel(const std::string& facility, LogLevel level) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    facility_levels_[facility] = level;
}

void Logger::setDefaultLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    default_level_ = level;
}

LogLevel Logger::getFacilityLevel(const std::string& facility) {
    std::lock_guard<std::mutex> lock(registry_mutex_);

    auto it = facility_levels_.find(facility);
    if (it != facility_levels_.end()) {
        return it->second;
    }

    return default_level_;
}

void Logger::parseLogConfig(const std::string& config) {
    if (config.empty()) return;

    std::lock_guard<std::mutex> lock(registry_mutex_);

    // Split by commas
    std::stringstream ss(config);
    std::string token;

    while (std::getline(ss, token, ',')) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);

        if (token.empty()) continue;

        // Check if this is a facility=level pair
        size_t equals_pos = token.find('=');
        if (equals_pos != std::string::npos) {
            // Facility-specific setting
            std::string facility = token.substr(0, equals_pos);
            std::string level_str = token.substr(equals_pos + 1);

            // Trim facility and level_str
            facility.erase(0, facility.find_first_not_of(" \t\n\r"));
            facility.erase(facility.find_last_not_of(" \t\n\r") + 1);
            level_str.erase(0, level_str.find_first_not_of(" \t\n\r"));
            level_str.erase(level_str.find_last_not_of(" \t\n\r") + 1);

            try {
                int level_int = std::stoi(level_str);
                LogLevel level = static_cast<LogLevel>(std::clamp(level_int, 1, 6));

                if (facility == "default") {
                    default_level_ = level;
                } else {
                    facility_levels_[facility] = level;
                }
            } catch (const std::exception&) {
                std::cerr << "Warning: Invalid log level '" << level_str << "' for facility '" << facility << "'" << std::endl;
            }
        } else {
            // Default level setting
            try {
                int level_int = std::stoi(token);
                default_level_ = static_cast<LogLevel>(std::clamp(level_int, 1, 6));
            } catch (const std::exception&) {
                std::cerr << "Warning: Invalid default log level '" << token << "'" << std::endl;
            }
        }
    }
}

void Logger::clearFacilityLevels() {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    facility_levels_.clear();
}

LogLevel Logger::getEffectiveLevel() const {
    // If component name is set, check for facility-specific level
    if (!component_name_.empty()) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        auto it = facility_levels_.find(component_name_);
        if (it != facility_levels_.end()) {
            return it->second;
        }
    }

    // Fall back to instance level if it's not the default WARNING
    if (current_level_ != LogLevel::WARNING) {
        return current_level_;
    }

    // Finally fall back to global default
    std::lock_guard<std::mutex> lock(registry_mutex_);
    return default_level_;
}

} // namespace topo