/**
 * @file OutputTracker.hpp
 * @brief Object-oriented debugging and output tracking system
 *
 * Implements comprehensive state tracking for debugging using object-oriented
 * pattern where debug objects contain their own state information and provide
 * query methods for logging and troubleshooting.
 *
 * Copyright (c) 2025 Matthew Block
 * Enhanced by Claude (Anthropic AI Assistant)
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "Logger.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace topo {

/**
 * @brief Information about a generated output file
 */
struct OutputFileInfo {
    std::string filename;
    std::string format;
    std::string type;  // "layer", "stacked", "combined", "svg"
    size_t file_size_bytes = 0;
    int layer_number = -1;  // -1 for non-layer files
    double elevation = 0.0;  // For layer files
    std::chrono::system_clock::time_point creation_time;
    bool generation_successful = false;
    std::string error_message;

    OutputFileInfo(const std::string& fname, const std::string& fmt, const std::string& t = "unknown")
        : filename(fname), format(fmt), type(t), creation_time(std::chrono::system_clock::now()) {}
};

/**
 * @brief Generation stage tracking for pipeline debugging
 */
struct GenerationStage {
    std::string stage_name;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    bool completed = false;
    bool successful = false;
    std::string error_message;
    std::unordered_map<std::string, std::string> stage_data;  // Key-value pairs for stage-specific info

    GenerationStage(const std::string& name)
        : stage_name(name), start_time(std::chrono::system_clock::now()) {}

    void complete(bool success = true, const std::string& error = "") {
        end_time = std::chrono::system_clock::now();
        completed = true;
        successful = success;
        error_message = error;
    }

    std::chrono::milliseconds duration() const {
        if (!completed) return std::chrono::milliseconds(0);
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
};

/**
 * @brief Object-oriented debug and output tracking system
 *
 * Following user's guidance for OOP debugging pattern:
 * - Objects contain debugging information about themselves
 * - Debug messaging objects query information from tracked objects
 * - Provides functionState tracking and object query methods
 *
 * Usage pattern:
 *   log.functionState = "Calling contour.getPoly()";
 *   contour.getPoly();
 *   log.functionState = "Exited contour.getPoly() with " + contour.lastPolyCount();
 *   log.output(contour.polygonCount());
 *   log.outputFunctionState();
 */
class OutputTracker {
public:
    OutputTracker();
    explicit OutputTracker(bool verbose);

    // Object-oriented debugging pattern
    std::string functionState;  // Current function/operation state

    /**
     * @brief Output current function state to console/log
     */
    void outputFunctionState() const;

    /**
     * @brief Output arbitrary object state information
     * @param info Information to output (from object query methods)
     */
    void output(const std::string& info) const;
    void output(int value, const std::string& description = "") const;
    void output(size_t value, const std::string& description = "") const;
    void output(double value, const std::string& description = "") const;

    // File tracking
    void trackGeneratedFile(const OutputFileInfo& file_info);
    void trackGeneratedFile(const std::string& filename, const std::string& format,
                           const std::string& type = "unknown", int layer = -1, double elevation = 0.0);

    // Stage tracking for pipeline debugging
    void startStage(const std::string& stage_name);
    void completeStage(const std::string& stage_name, bool successful = true, const std::string& error = "");
    void addStageData(const std::string& stage_name, const std::string& key, const std::string& value);

    // Object state queries for logging
    std::string getFileTrackingSummary() const;
    std::string getPipelineStatus() const;
    std::string getTimingReport() const;
    std::string getCurrentStage() const;

    // File system integration
    void updateFileSizes();  // Update file sizes for tracked files
    void validateTrackedFiles();  // Check if tracked files actually exist

    // Output methods
    void printSummary() const;
    void printDetailedReport() const;
    void printFileList() const;

    // Export tracking data
    void exportTrackingData(const std::string& filename) const;

    // Configuration
    void setVerbose(bool verbose) { verbose_ = verbose; }
    bool isVerbose() const { return verbose_; }

    // Statistics
    size_t getTrackedFileCount() const { return tracked_files_.size(); }
    size_t getCompletedStageCount() const;
    size_t getTotalFileSize() const;

    // File list access
    std::vector<std::string> getOutputFiles() const;
    const std::vector<OutputFileInfo>& getTrackedFiles() const { return tracked_files_; }

    // Clear tracking data
    void clear();

private:
    bool verbose_;
    std::vector<OutputFileInfo> tracked_files_;
    std::vector<GenerationStage> stages_;
    std::chrono::system_clock::time_point tracking_start_time_;
    mutable Logger logger_;  // mutable for const methods

    // Helper methods
    std::string formatDuration(std::chrono::milliseconds duration) const;
    std::string formatFileSize(size_t bytes) const;
    void logMessage(const std::string& message) const;

    // Find stage by name
    GenerationStage* findStage(const std::string& stage_name);
    const GenerationStage* findStage(const std::string& stage_name) const;
};

} // namespace topo