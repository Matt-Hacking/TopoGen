/**
 * @file ExportOrchestrator.hpp
 * @brief Orchestrates export operations for topographic data
 *
 * This class coordinates exporting generated topographic data
 * (meshes and contours) to various output formats. It separates
 * export orchestration from data generation, eliminating the
 * circular dependency between TopoCore and TopoExport.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "../core/Logger.hpp"
#include "../core/OutputTracker.hpp"
#include <string>
#include <vector>
#include <memory>

namespace topo {

// Forward declarations
class TopographicGenerator;
struct ContourLayer;

/**
 * @brief Orchestrates export of topographic data to various formats
 *
 * Responsibilities:
 * - Coordinate export operations based on configuration
 * - Handle format-specific export logic
 * - Manage export performance metrics
 * - Provide user feedback on export progress
 *
 * This class acts as the orchestration layer between:
 * - TopoCore (data generation)
 * - TopoExport (export implementations)
 * - TopoCLI (command-line interface)
 */
class ExportOrchestrator {
public:
    /**
     * @brief Constructor
     * @param generator Reference to the topographic generator (data source)
     */
    explicit ExportOrchestrator(const TopographicGenerator& generator);

    /**
     * @brief Destructor
     */
    ~ExportOrchestrator();

    /**
     * @brief Export data to all configured output formats
     * @return true if all exports succeeded, false if any failed
     */
    bool export_all_formats();

private:
    const TopographicGenerator& generator_;
    Logger logger_;

    // Disable copy/move since we hold a reference
    ExportOrchestrator(const ExportOrchestrator&) = delete;
    ExportOrchestrator& operator=(const ExportOrchestrator&) = delete;
    ExportOrchestrator(ExportOrchestrator&&) = delete;
    ExportOrchestrator& operator=(ExportOrchestrator&&) = delete;
};

} // namespace topo
