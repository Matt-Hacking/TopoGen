/**
 * @file OutputTracker.cpp
 * @brief Implementation of object-oriented debugging and output tracking system
 */

#include "OutputTracker.hpp"
#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace topo {

OutputTracker::OutputTracker()
    : verbose_(false), tracking_start_time_(std::chrono::system_clock::now()), logger_("OutputTracker") {
}

OutputTracker::OutputTracker(bool verbose)
    : verbose_(verbose), tracking_start_time_(std::chrono::system_clock::now()), logger_("OutputTracker") {
}

void OutputTracker::outputFunctionState() const {
    if (verbose_ && !functionState.empty()) {
        logMessage("[FUNCTION STATE] " + functionState);
    }
}

void OutputTracker::output(const std::string& info) const {
    if (verbose_) {
        logMessage("[OBJECT STATE] " + info);
    }
}

void OutputTracker::output(int value, const std::string& description) const {
    if (verbose_) {
        std::string message = "[OBJECT STATE] " + std::to_string(value);
        if (!description.empty()) {
            message += " (" + description + ")";
        }
        logMessage(message);
    }
}

void OutputTracker::output(size_t value, const std::string& description) const {
    if (verbose_) {
        std::string message = "[OBJECT STATE] " + std::to_string(value);
        if (!description.empty()) {
            message += " (" + description + ")";
        }
        logMessage(message);
    }
}

void OutputTracker::output(double value, const std::string& description) const {
    if (verbose_) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << value;
        std::string message = "[OBJECT STATE] " + oss.str();
        if (!description.empty()) {
            message += " (" + description + ")";
        }
        logMessage(message);
    }
}

void OutputTracker::trackGeneratedFile(const OutputFileInfo& file_info) {
    tracked_files_.push_back(file_info);

    if (verbose_) {
        std::string message = "[FILE TRACKED] " + file_info.filename +
                             " (format: " + file_info.format +
                             ", type: " + file_info.type + ")";
        if (file_info.layer_number >= 0) {
            message += " [Layer " + std::to_string(file_info.layer_number) + "]";
        }
        logMessage(message);
    }
}

void OutputTracker::trackGeneratedFile(const std::string& filename, const std::string& format,
                                      const std::string& type, int layer, double elevation) {
    OutputFileInfo info(filename, format, type);
    info.layer_number = layer;
    info.elevation = elevation;

    // Try to get file size if file exists
    if (std::filesystem::exists(filename)) {
        try {
            info.file_size_bytes = std::filesystem::file_size(filename);
            info.generation_successful = true;
        } catch (const std::exception& e) {
            info.error_message = "Could not get file size: " + std::string(e.what());
        }
    }

    trackGeneratedFile(info);
}

void OutputTracker::startStage(const std::string& stage_name) {
    stages_.emplace_back(stage_name);

    if (verbose_) {
        logMessage("[STAGE START] " + stage_name);
    }
}

void OutputTracker::completeStage(const std::string& stage_name, bool successful, const std::string& error) {
    GenerationStage* stage = findStage(stage_name);
    if (stage) {
        stage->complete(successful, error);

        if (verbose_) {
            std::string message = "[STAGE COMPLETE] " + stage_name +
                                 " (" + formatDuration(stage->duration()) + ")";
            if (!successful) {
                message += " [FAILED: " + error + "]";
            }
            logMessage(message);
        }
    }
}

void OutputTracker::addStageData(const std::string& stage_name, const std::string& key, const std::string& value) {
    GenerationStage* stage = findStage(stage_name);
    if (stage) {
        stage->stage_data[key] = value;

        if (verbose_) {
            logMessage("[STAGE DATA] " + stage_name + ": " + key + " = " + value);
        }
    }
}

std::string OutputTracker::getFileTrackingSummary() const {
    std::ostringstream oss;
    size_t successful = 0;
    size_t total_size = 0;

    for (const auto& file : tracked_files_) {
        if (file.generation_successful) {
            successful++;
            total_size += file.file_size_bytes;
        }
    }

    oss << "Files: " << successful << "/" << tracked_files_.size()
        << " successful, " << formatFileSize(total_size) << " total";

    return oss.str();
}

std::string OutputTracker::getPipelineStatus() const {
    std::ostringstream oss;
    size_t completed = getCompletedStageCount();
    size_t total = stages_.size();

    oss << "Pipeline: " << completed << "/" << total << " stages completed";

    if (!stages_.empty()) {
        const auto& current = stages_.back();
        if (!current.completed) {
            oss << " (current: " << current.stage_name << ")";
        }
    }

    return oss.str();
}

std::string OutputTracker::getTimingReport() const {
    std::ostringstream oss;
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - tracking_start_time_);

    oss << "Total time: " << formatDuration(total_time);

    if (!stages_.empty()) {
        std::chrono::milliseconds stage_time(0);
        for (const auto& stage : stages_) {
            if (stage.completed) {
                stage_time += stage.duration();
            }
        }
        oss << ", Stage time: " << formatDuration(stage_time);
    }

    return oss.str();
}

std::string OutputTracker::getCurrentStage() const {
    if (stages_.empty()) {
        return "No stages";
    }

    const auto& current = stages_.back();
    if (current.completed) {
        return "All stages completed";
    } else {
        return "Current stage: " + current.stage_name;
    }
}

void OutputTracker::updateFileSizes() {
    for (auto& file : tracked_files_) {
        if (std::filesystem::exists(file.filename)) {
            try {
                file.file_size_bytes = std::filesystem::file_size(file.filename);
                file.generation_successful = true;
                file.error_message.clear();
            } catch (const std::exception& e) {
                file.error_message = "Could not update file size: " + std::string(e.what());
            }
        } else {
            file.generation_successful = false;
            file.error_message = "File does not exist";
        }
    }
}

void OutputTracker::validateTrackedFiles() {
    for (auto& file : tracked_files_) {
        if (!std::filesystem::exists(file.filename)) {
            file.generation_successful = false;
            file.error_message = "File not found";

            if (verbose_) {
                logMessage("[FILE MISSING] " + file.filename);
            }
        }
    }
}

void OutputTracker::printSummary() const {
    std::ostringstream summary;
    summary << "\n=== Output Tracking Summary ===\n";
    summary << getFileTrackingSummary() << "\n";
    summary << getPipelineStatus() << "\n";
    summary << getTimingReport() << "\n";
    summary << "================================";
    logger_.info(summary.str());
}

void OutputTracker::printDetailedReport() const {
    std::ostringstream report;
    report << "\n=== Detailed Output Report ===\n";

    // Pipeline stages
    report << "\nPipeline Stages:\n";
    for (const auto& stage : stages_) {
        report << "  " << stage.stage_name;
        if (stage.completed) {
            report << " [" << formatDuration(stage.duration()) << "]";
            if (!stage.successful) {
                report << " FAILED: " << stage.error_message;
            }
        } else {
            report << " [IN PROGRESS]";
        }
        report << "\n";

        // Stage data
        for (const auto& [key, value] : stage.stage_data) {
            report << "    " << key << ": " << value << "\n";
        }
    }

    // File details
    report << "\nGenerated Files:\n";
    for (const auto& file : tracked_files_) {
        report << "  " << file.filename;
        report << " (" << file.format << ", " << file.type << ")";

        if (file.layer_number >= 0) {
            report << " [Layer " << file.layer_number << "]";
        }

        if (file.generation_successful) {
            report << " [" << formatFileSize(file.file_size_bytes) << "]";
        } else {
            report << " [FAILED: " << file.error_message << "]";
        }
        report << "\n";
    }

    report << "==============================";
    logger_.info(report.str());
}

void OutputTracker::printFileList() const {
    std::ostringstream file_list;
    file_list << "\nGenerated files:\n";

    try {
        // Use index-based iteration instead of range-based to avoid potential iterator issues
        size_t file_count = tracked_files_.size();
        if (file_count == 0) {
            file_list << "  [No files tracked]";
            logger_.info(file_list.str());
            return;
        }

        for (size_t i = 0; i < file_count; ++i) {
            try {
                const auto& file = tracked_files_[i];
                if (file.generation_successful && !file.filename.empty()) {
                    // Safety check: ensure file_size_bytes is initialized (not garbage)
                    size_t safe_size = (file.file_size_bytes == SIZE_MAX || file.file_size_bytes > 1000000000) ? 0 : file.file_size_bytes;
                    file_list << "  " << file.filename
                              << " (" << formatFileSize(safe_size) << ")\n";
                }
            } catch (const std::exception& e) {
                file_list << "  [Error processing file " << i << ": " << e.what() << "]\n";
            } catch (...) {
                file_list << "  [Unknown error processing file " << i << "]\n";
            }
        }
        logger_.info(file_list.str());
    } catch (const std::exception& e) {
        file_list << "  [Error listing files: " << e.what() << "]";
        logger_.error(file_list.str());
    } catch (...) {
        file_list << "  [Unknown error while listing files]";
        logger_.error(file_list.str());
    }
}

void OutputTracker::exportTrackingData(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        if (verbose_) {
            logMessage("[ERROR] Could not open tracking export file: " + filename);
        }
        return;
    }

    file << "# Output Tracking Data Export\n";
    file << "# Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << "\n\n";

    file << "## Pipeline Stages\n";
    for (const auto& stage : stages_) {
        file << stage.stage_name << "," << stage.completed << "," << stage.successful
             << "," << stage.duration().count() << "," << stage.error_message << "\n";
    }

    file << "\n## Generated Files\n";
    for (const auto& tracked_file : tracked_files_) {
        file << tracked_file.filename << "," << tracked_file.format << ","
             << tracked_file.type << "," << tracked_file.layer_number << ","
             << tracked_file.file_size_bytes << "," << tracked_file.generation_successful
             << "," << tracked_file.error_message << "\n";
    }

    file.close();

    if (verbose_) {
        logMessage("[EXPORT] Tracking data exported to: " + filename);
    }
}

size_t OutputTracker::getCompletedStageCount() const {
    return std::count_if(stages_.begin(), stages_.end(),
                        [](const GenerationStage& stage) { return stage.completed; });
}

size_t OutputTracker::getTotalFileSize() const {
    size_t total = 0;
    for (const auto& file : tracked_files_) {
        if (file.generation_successful) {
            total += file.file_size_bytes;
        }
    }
    return total;
}

std::vector<std::string> OutputTracker::getOutputFiles() const {
    std::vector<std::string> files;
    files.reserve(tracked_files_.size());
    for (const auto& file : tracked_files_) {
        if (file.generation_successful) {
            files.push_back(file.filename);
        }
    }
    return files;
}

void OutputTracker::clear() {
    tracked_files_.clear();
    stages_.clear();
    functionState.clear();
    tracking_start_time_ = std::chrono::system_clock::now();
}

std::string OutputTracker::formatDuration(std::chrono::milliseconds duration) const {
    auto ms = duration.count();
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000.0) + "s";
    } else {
        auto minutes = ms / 60000;
        auto seconds = (ms % 60000) / 1000;
        return std::to_string(minutes) + "m" + std::to_string(seconds) + "s";
    }
}

std::string OutputTracker::formatFileSize(size_t bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB"};
    double size = static_cast<double>(bytes);
    int unit = 0;

    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

void OutputTracker::logMessage(const std::string& message) const {
    logger_.info(message);
}

GenerationStage* OutputTracker::findStage(const std::string& stage_name) {
    auto it = std::find_if(stages_.begin(), stages_.end(),
                          [&stage_name](const GenerationStage& stage) {
                              return stage.stage_name == stage_name;
                          });
    return (it != stages_.end()) ? &(*it) : nullptr;
}

const GenerationStage* OutputTracker::findStage(const std::string& stage_name) const {
    auto it = std::find_if(stages_.begin(), stages_.end(),
                          [&stage_name](const GenerationStage& stage) {
                              return stage.stage_name == stage_name;
                          });
    return (it != stages_.end()) ? &(*it) : nullptr;
}

} // namespace topo
