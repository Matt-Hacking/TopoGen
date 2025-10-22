/**
 * @file Logger.hpp
 * @brief Centralized logging system with verbosity control
 *
 * Provides a single point of logging control to replace scattered std::cout
 * calls throughout the codebase. Implements exactly one outputMessage() method
 * with one verbosity check and one std::cout call.
 */

#pragma once

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <optional>
#include <mutex>
#include <cstdlib>
#include <unordered_map>

namespace topo {

/**
 * @brief Log levels matching CLAUDE.md specification
 *
 * Semantic levels from CLAUDE.md:
 * Level 1: Errors (disrupts execution)
 * Level 2: Warnings (disables functionality)
 * Level 3: Information (high-level)
 * Level 4: Detailed information (codepath execution)
 * Level 5: Basic debugging (objects, methods)
 * Level 6: Detailed debugging (variable values)
 */
enum class LogLevel {
    ERROR = 1,     // 1=ERROR - Errors that disrupt execution
    WARNING = 2,   // 2=WARNING - Warnings that disable functionality
    INFO = 3,      // 3=INFO - High-level information (default)
    DETAILED = 4,  // 4=DETAILED - Codepath execution details
    DEBUG = 5,     // 5=DEBUG - Basic debugging (objects, methods)
    TRACE = 6      // 6=TRACE - Detailed debugging (variable values)
};

/**
 * @brief Centralized logger with single point of output control
 *
 * This class implements the requirement that "there should only be one
 * if statement (in outputMessage()) and one call to std::cout (same),
 * not several in each method of every object in the project."
 */
class Logger {
public:
    /**
     * @brief Default constructor with WARNING level
     */
    Logger();

    /**
     * @brief Constructor with component name (uses default WARNING level)
     * @param component_name Name of the component for logging identification
     */
    Logger(const std::string& component_name);

    /**
     * @brief Constructor with specified log level and optional file output
     * @param level Initial log level
     * @param log_file Optional path to log file (appends if exists)
     */
    Logger(LogLevel level, const std::optional<std::string>& log_file = std::nullopt);

    /**
     * @brief Destructor - flushes all buffers
     */
    ~Logger();

    /**
     * @brief Output a message if it meets the current verbosity level
     *
     * THIS IS THE SINGLE POINT OF LOGGING CONTROL
     * Contains exactly one verbosity check and one std::cout call
     *
     * @param level Level of this message
     * @param message Message to output
     */
    void outputMessage(LogLevel level, const std::string& message) const;

    /**
     * @brief Set the current log level threshold
     * @param level New threshold level
     */
    void setLogLevel(LogLevel level) { current_level_ = level; }

    /**
     * @brief Get the current log level threshold
     * @return Current threshold level
     */
    LogLevel getLogLevel() const { return current_level_; }

    /**
     * @brief Set or change the log file
     * @param log_file Path to log file, or nullopt to disable file logging
     */
    void setLogFile(const std::optional<std::string>& log_file);

    /**
     * @brief Check if a message level would be output
     *
     * Uses facility-specific level if set, otherwise uses instance level.
     *
     * @param level Level to check
     * @return true if message would be output
     */
    bool shouldOutput(LogLevel level) const {
        return static_cast<int>(level) <= static_cast<int>(getEffectiveLevel());
    }

    /**
     * @brief Convenience method for error messages (Level 1)
     * @param message Message to output
     */
    void error(const std::string& message) const {
        outputMessage(LogLevel::ERROR, message);
    }

    /**
     * @brief Fatal error - outputs message and exits program
     * @param message Message to output before exit
     * @param exit_code Exit code to use (default 1)
     */
    [[noreturn]] void fatal(const std::string& message, int exit_code = 1) const {
        outputMessage(LogLevel::ERROR, "FATAL: " + message);
        std::cerr.flush();
        std::cout.flush();
        std::exit(exit_code);
    }

    /**
     * @brief Convenience method for warning messages (Level 2)
     * @param message Message to output
     */
    void warning(const std::string& message) const {
        outputMessage(LogLevel::WARNING, message);
    }

    /**
     * @brief Alias for warning() - short form
     * @param message Message to output
     */
    void warn(const std::string& message) const {
        warning(message);
    }

    /**
     * @brief Convenience method for info messages (Level 3)
     * @param message Message to output
     */
    void info(const std::string& message) const {
        outputMessage(LogLevel::INFO, message);
    }

    /**
     * @brief Convenience method for detailed messages (Level 4 - codepath execution)
     * @param message Message to output
     */
    void detailed(const std::string& message) const {
        outputMessage(LogLevel::DETAILED, message);
    }

    /**
     * @brief Convenience method for debug messages (Level 5 - objects, methods)
     * @param message Message to output
     */
    void debug(const std::string& message) const {
        outputMessage(LogLevel::DEBUG, message);
    }

    /**
     * @brief Convenience method for trace messages (Level 6 - variable values)
     * @param message Message to output
     */
    void trace(const std::string& message) const {
        outputMessage(LogLevel::TRACE, message);
        flush();  // Flush after highest debug level messages
    }

    /**
     * @brief Alias for trace() - verbose debug
     * @param message Message to output
     */
    void verbose(const std::string& message) const {
        trace(message);
    }

    /**
     * @brief Flush all output buffers (console and file)
     *
     * Should be called in:
     * - Object destructors
     * - Try/catch blocks and error handling
     * - Logger destructor
     * - Program exit
     * - After messages to highest debug level
     */
    void flush() const;

    // ========================================================================
    // Facility-based logging control (CLAUDE.md requirement)
    // ========================================================================

    /**
     * @brief Set log level for a specific facility (component)
     *
     * Enables fine-grained control like: "level 6 for file export but level 3 for everything else"
     *
     * @param facility Facility name (e.g., "FileExport", "MeshGeneration")
     * @param level Log level for this facility
     *
     * @example
     * Logger::setFacilityLevel("PNGExporter", LogLevel::TRACE);  // Full debugging for PNG export
     * Logger::setFacilityLevel("ContourGenerator", LogLevel::INFO);  // Only info for contour gen
     */
    static void setFacilityLevel(const std::string& facility, LogLevel level);

    /**
     * @brief Set default log level for all facilities
     *
     * This is the fallback level for facilities that don't have a specific level set.
     *
     * @param level Default log level
     */
    static void setDefaultLevel(LogLevel level);

    /**
     * @brief Get log level for a specific facility
     *
     * Returns the facility-specific level if set, otherwise returns default level.
     *
     * @param facility Facility name
     * @return Log level for this facility
     */
    static LogLevel getFacilityLevel(const std::string& facility);

    /**
     * @brief Parse and apply log configuration from string
     *
     * Supports multiple formats:
     * - Simple level: "5" sets default to DEBUG
     * - Facility-specific: "PNGExporter=6,ContourGenerator=3"
     * - Mixed: "4,PNGExporter=6" sets default to DETAILED, PNGExporter to TRACE
     *
     * @param config Configuration string
     *
     * @example
     * Logger::parseLogConfig("5");  // All facilities to DEBUG
     * Logger::parseLogConfig("PNGExporter=6,default=3");  // PNG to TRACE, others to INFO
     * Logger::parseLogConfig("3,FileExport=6");  // Default INFO, FileExport TRACE
     */
    static void parseLogConfig(const std::string& config);

    /**
     * @brief Clear all facility-specific log levels
     *
     * Resets to using only the default level for all facilities.
     */
    static void clearFacilityLevels();

    /**
     * @brief Get effective log level for this logger instance
     *
     * Checks facility-specific level first, falls back to instance level,
     * then falls back to global default.
     *
     * @return Effective log level
     */
    LogLevel getEffectiveLevel() const;

private:
    LogLevel current_level_;
    std::string component_name_;
    std::optional<std::string> log_file_path_;
    std::shared_ptr<std::ofstream> file_stream_;
    mutable std::mutex output_mutex_;  // Thread safety for logging

    // Message deduplication state
    mutable std::string last_message_;
    mutable LogLevel last_level_;
    mutable int repeat_count_;
    mutable bool has_last_message_;

    // Static facility-based logging registry
    static std::unordered_map<std::string, LogLevel> facility_levels_;
    static LogLevel default_level_;
    static std::mutex registry_mutex_;

    /**
     * @brief Initialize or update file stream
     */
    void initializeFileStream();

    /**
     * @brief Perform the actual output to console and file
     * @param level Log level of the message
     * @param message Message to output
     */
    void doOutput(LogLevel level, const std::string& message) const;
};

} // namespace topo