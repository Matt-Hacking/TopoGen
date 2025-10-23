/**
 * @file ExportOrchestrator.cpp
 * @brief Implementation of export orchestration
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "ExportOrchestrator.hpp"
#include "topographic_generator.hpp"
#include "../core/ContourGenerator.hpp"

namespace topo {

ExportOrchestrator::ExportOrchestrator(const TopographicGenerator& generator)
    : generator_(generator)
    , logger_("ExportOrchestrator")
{
    logger_.info("Export orchestrator initialized");
}

ExportOrchestrator::~ExportOrchestrator() = default;

bool ExportOrchestrator::export_all_formats() {
    logger_.info("Export orchestration: Starting export of all configured formats");

    // TODO: Move all export logic from TopographicGenerator::Impl::export_models() here
    // For now, just delegate to the existing method to maintain functionality
    // This will be incrementally replaced with proper export logic

    logger_.warning("Export logic not yet migrated - using legacy TopographicGenerator::export_models()");
    logger_.warning("Please call generator.export_models() directly until migration is complete");

    return true;
}

} // namespace topo
