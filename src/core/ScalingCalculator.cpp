/**
 * @file ScalingCalculator.cpp
 * @brief Implementation of scaling calculations
 */

#include "ScalingCalculator.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace topo {

ScalingCalculator::ScalingCalculator(const TopographicConfig& config)
    : config_(config), logger_("ScalingCalculator") {}

ScalingResult ScalingCalculator::calculate_2d_scale(double xy_extent_meters,
                                                     std::optional<double> z_extent_meters) {
    // Check for cross-mode override
    if (config_.use_3d_scaling_for_2d) {
        logger_.info("Using 3D scaling method for 2D output (user override)");
        return calculate_3d_scale(xy_extent_meters, z_extent_meters.value_or(0.0));
    }

    // Route to appropriate method
    switch (config_.scaling_2d_method) {
        case TopographicConfig::ScalingMethod::AUTO:
            return scale_2d_auto(xy_extent_meters, z_extent_meters);
        case TopographicConfig::ScalingMethod::BED_SIZE:
            return scale_2d_bed_size(xy_extent_meters);
        case TopographicConfig::ScalingMethod::MATERIAL_THICKNESS:
            if (!z_extent_meters.has_value()) {
                logger_.warning("MATERIAL_THICKNESS method requires elevation data, falling back to BED_SIZE");
                return scale_2d_bed_size(xy_extent_meters);
            }
            return scale_2d_material_thickness(xy_extent_meters, z_extent_meters.value());
        case TopographicConfig::ScalingMethod::LAYERS:
            if (!z_extent_meters.has_value()) {
                logger_.warning("LAYERS method requires elevation data, falling back to BED_SIZE");
                return scale_2d_bed_size(xy_extent_meters);
            }
            return scale_2d_layers(xy_extent_meters, z_extent_meters.value());
        case TopographicConfig::ScalingMethod::EXPLICIT:
            return scale_2d_explicit();
        case TopographicConfig::ScalingMethod::PRINT_HEIGHT:
        case TopographicConfig::ScalingMethod::UNIFORM_XYZ:
            logger_.warning("Method " + std::to_string(static_cast<int>(config_.scaling_2d_method)) +
                          " is 3D-only, falling back to BED_SIZE");
            return scale_2d_bed_size(xy_extent_meters);
        default:
            logger_.warning("Unknown 2D scaling method, falling back to BED_SIZE");
            return scale_2d_bed_size(xy_extent_meters);
    }
}

ScalingResult ScalingCalculator::calculate_3d_scale(double xy_extent_meters, double z_extent_meters) {
    // Check for cross-mode override
    if (config_.use_2d_scaling_for_3d) {
        logger_.info("Using 2D scaling method for 3D output (user override)");
        return calculate_2d_scale(xy_extent_meters, z_extent_meters);
    }

    // Route to appropriate method
    switch (config_.scaling_3d_method) {
        case TopographicConfig::ScalingMethod::AUTO:
            return scale_3d_auto(xy_extent_meters, z_extent_meters);
        case TopographicConfig::ScalingMethod::BED_SIZE:
            return scale_3d_bed_size(xy_extent_meters);
        case TopographicConfig::ScalingMethod::PRINT_HEIGHT:
            return scale_3d_print_height(xy_extent_meters, z_extent_meters);
        case TopographicConfig::ScalingMethod::UNIFORM_XYZ:
            return scale_3d_uniform_xyz(xy_extent_meters, z_extent_meters);
        case TopographicConfig::ScalingMethod::LAYERS:
            return scale_3d_layers(xy_extent_meters, z_extent_meters);
        case TopographicConfig::ScalingMethod::EXPLICIT:
            return scale_3d_explicit();
        case TopographicConfig::ScalingMethod::MATERIAL_THICKNESS:
            logger_.warning("MATERIAL_THICKNESS is 2D-only, falling back to BED_SIZE");
            return scale_3d_bed_size(xy_extent_meters);
        default:
            logger_.warning("Unknown 3D scaling method, falling back to BED_SIZE");
            return scale_3d_bed_size(xy_extent_meters);
    }
}

// ============================================================================
// 2D Scaling Methods
// ============================================================================

ScalingResult ScalingCalculator::scale_2d_auto(double xy_extent, std::optional<double> z_extent) {
    // Choose best method based on available parameters
    if (config_.explicit_2d_scale_factor.has_value()) {
        logger_.info("AUTO mode: Using EXPLICIT method (user provided scale factor)");
        return scale_2d_explicit();
    }

    if (z_extent.has_value() && config_.substrate_depth_mm.has_value()) {
        logger_.info("AUTO mode: Using LAYERS method (substrate depth and elevation data available)");
        return scale_2d_layers(xy_extent, z_extent.value());
    }

    if (z_extent.has_value()) {
        logger_.info("AUTO mode: Using MATERIAL_THICKNESS method (elevation data available)");
        return scale_2d_material_thickness(xy_extent, z_extent.value());
    }

    logger_.info("AUTO mode: Using BED_SIZE method (default)");
    return scale_2d_bed_size(xy_extent);
}

ScalingResult ScalingCalculator::scale_2d_bed_size(double xy_extent) {
    double target_bed_mm = get_target_bed_size_mm();

    // Use 90% of bed size to leave margin
    double target_size_mm = target_bed_mm * 0.9;
    double scale_factor = target_size_mm / xy_extent;

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "BED_SIZE method: Fitting " << xy_extent << "m extent to "
                << target_bed_mm << "mm bed\n";
    explanation << "  Calculation: (" << target_bed_mm << "mm × 0.9) / "
                << xy_extent << "m = " << scale_factor << " mm/m\n";
    explanation << "  Result: " << xy_extent << "m → " << (xy_extent * scale_factor) << "mm";

    logger_.info(explanation.str());
    return ScalingResult(scale_factor, "BED_SIZE", explanation.str());
}

ScalingResult ScalingCalculator::scale_2d_material_thickness(double xy_extent, double z_extent) {
    // Calculate scale based on layer thickness and number of layers
    double total_material_thickness_mm = config_.num_layers * config_.layer_thickness_mm;
    double scale_factor = total_material_thickness_mm / z_extent;

    // Also check if it fits on bed
    double target_bed_mm = get_target_bed_size_mm();
    double xy_size_mm = xy_extent * scale_factor;

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "MATERIAL_THICKNESS method: Scaling based on layer material\n";
    explanation << "  Calculation: (" << config_.num_layers << " layers × "
                << config_.layer_thickness_mm << "mm) / " << z_extent << "m elevation = "
                << scale_factor << " mm/m\n";
    explanation << "  XY extent: " << xy_extent << "m → " << xy_size_mm << "mm\n";
    explanation << "  Z extent: " << z_extent << "m → " << total_material_thickness_mm << "mm";

    if (xy_size_mm > target_bed_mm) {
        explanation << "\n  WARNING: XY extent (" << xy_size_mm << "mm) exceeds bed size ("
                   << target_bed_mm << "mm)";
        logger_.warning(explanation.str());
    } else {
        logger_.info(explanation.str());
    }

    return ScalingResult(scale_factor, "MATERIAL_THICKNESS", explanation.str());
}

ScalingResult ScalingCalculator::scale_2d_layers(double xy_extent, double z_extent) {
    if (!config_.substrate_depth_mm.has_value()) {
        logger_.warning("LAYERS method requires substrate_depth_mm, falling back to MATERIAL_THICKNESS");
        return scale_2d_material_thickness(xy_extent, z_extent);
    }

    double substrate_depth_mm = config_.substrate_depth_mm.value();
    double scale_factor = substrate_depth_mm / z_extent;

    // Check if it fits on bed
    double target_bed_mm = get_target_bed_size_mm();
    double xy_size_mm = xy_extent * scale_factor;

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "LAYERS method: Fitting elevation range to substrate depth\n";
    explanation << "  Calculation: " << substrate_depth_mm << "mm substrate / "
                << z_extent << "m elevation = " << scale_factor << " mm/m\n";
    explanation << "  XY extent: " << xy_extent << "m → " << xy_size_mm << "mm\n";
    explanation << "  Z extent: " << z_extent << "m → " << substrate_depth_mm << "mm";

    if (xy_size_mm > target_bed_mm) {
        explanation << "\n  WARNING: XY extent (" << xy_size_mm << "mm) exceeds bed size ("
                   << target_bed_mm << "mm)";
        logger_.warning(explanation.str());
    } else {
        logger_.info(explanation.str());
    }

    return ScalingResult(scale_factor, "LAYERS", explanation.str());
}

ScalingResult ScalingCalculator::scale_2d_explicit() {
    if (!config_.explicit_2d_scale_factor.has_value()) {
        logger_.error("EXPLICIT method requires explicit_2d_scale_factor, falling back to BED_SIZE");
        return scale_2d_bed_size(0.0); // Will be recalculated with actual extent
    }

    double scale_factor = config_.explicit_2d_scale_factor.value();

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "EXPLICIT method: Using user-provided scale factor\n";
    explanation << "  Scale factor: " << scale_factor << " mm/m";

    logger_.info(explanation.str());
    return ScalingResult(scale_factor, "EXPLICIT", explanation.str());
}

// ============================================================================
// 3D Scaling Methods
// ============================================================================

ScalingResult ScalingCalculator::scale_3d_auto(double xy_extent, double z_extent) {
    // Choose best method based on available parameters
    if (config_.explicit_3d_scale_factor.has_value()) {
        logger_.info("AUTO mode: Using EXPLICIT method (user provided scale factor)");
        return scale_3d_explicit();
    }

    if (config_.substrate_depth_mm.has_value()) {
        logger_.info("AUTO mode: Using PRINT_HEIGHT method (substrate depth available)");
        return scale_3d_print_height(xy_extent, z_extent);
    }

    logger_.info("AUTO mode: Using BED_SIZE method (default)");
    return scale_3d_bed_size(xy_extent);
}

ScalingResult ScalingCalculator::scale_3d_bed_size(double xy_extent) {
    double target_bed_mm = get_target_bed_size_mm();

    // Use 90% of bed size to leave margin
    double target_size_mm = target_bed_mm * 0.9;
    double scale_factor = target_size_mm / xy_extent;

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "BED_SIZE method: Fitting " << xy_extent << "m extent to "
                << target_bed_mm << "mm bed\n";
    explanation << "  Calculation: (" << target_bed_mm << "mm × 0.9) / "
                << xy_extent << "m = " << scale_factor << " mm/m\n";
    explanation << "  Uniform XYZ scaling: " << xy_extent << "m → " << (xy_extent * scale_factor) << "mm";

    logger_.info(explanation.str());
    return ScalingResult(scale_factor, "BED_SIZE", explanation.str());
}

ScalingResult ScalingCalculator::scale_3d_print_height(double xy_extent, double z_extent) {
    if (!config_.substrate_depth_mm.has_value()) {
        logger_.warning("PRINT_HEIGHT method requires substrate_depth_mm, falling back to BED_SIZE");
        return scale_3d_bed_size(xy_extent);
    }

    double substrate_depth_mm = config_.substrate_depth_mm.value();
    double scale_factor = substrate_depth_mm / z_extent;

    // Check if XY fits on bed
    double target_bed_mm = get_target_bed_size_mm();
    double xy_size_mm = xy_extent * scale_factor;

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "PRINT_HEIGHT method: Fitting Z elevation to substrate depth\n";
    explanation << "  Calculation: " << substrate_depth_mm << "mm substrate / "
                << z_extent << "m elevation = " << scale_factor << " mm/m\n";
    explanation << "  XY extent: " << xy_extent << "m → " << xy_size_mm << "mm\n";
    explanation << "  Z extent: " << z_extent << "m → " << substrate_depth_mm << "mm\n";
    explanation << "  Note: Uniform XYZ scaling (no vertical exaggeration)";

    if (xy_size_mm > target_bed_mm) {
        explanation << "\n  WARNING: XY extent (" << xy_size_mm << "mm) exceeds bed size ("
                   << target_bed_mm << "mm)";
        logger_.warning(explanation.str());
    } else {
        logger_.info(explanation.str());
    }

    return ScalingResult(scale_factor, "PRINT_HEIGHT", explanation.str());
}

ScalingResult ScalingCalculator::scale_3d_uniform_xyz(double xy_extent, double z_extent) {
    // Same as PRINT_HEIGHT - ensures uniform XYZ scaling
    return scale_3d_print_height(xy_extent, z_extent);
}

ScalingResult ScalingCalculator::scale_3d_layers(double xy_extent, double z_extent) {
    // For 3D output, LAYERS method is same as PRINT_HEIGHT
    return scale_3d_print_height(xy_extent, z_extent);
}

ScalingResult ScalingCalculator::scale_3d_explicit() {
    if (!config_.explicit_3d_scale_factor.has_value()) {
        logger_.error("EXPLICIT method requires explicit_3d_scale_factor, falling back to BED_SIZE");
        return scale_3d_bed_size(0.0); // Will be recalculated with actual extent
    }

    double scale_factor = config_.explicit_3d_scale_factor.value();

    std::ostringstream explanation;
    explanation << std::fixed << std::setprecision(2);
    explanation << "EXPLICIT method: Using user-provided scale factor\n";
    explanation << "  Scale factor: " << scale_factor << " mm/m";

    logger_.info(explanation.str());
    return ScalingResult(scale_factor, "EXPLICIT", explanation.str());
}

// ============================================================================
// Helper Methods
// ============================================================================

double ScalingCalculator::get_target_bed_size_mm() const {
    if (config_.cutting_bed_size_mm.has_value()) {
        return config_.cutting_bed_size_mm.value();
    }

    if (config_.cutting_bed_x_mm.has_value() && config_.cutting_bed_y_mm.has_value()) {
        return std::min(config_.cutting_bed_x_mm.value(), config_.cutting_bed_y_mm.value());
    }

    return config_.substrate_size_mm;
}

std::string ScalingCalculator::format_calculation(const std::string& formula, double result) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << formula << " = " << result << " mm/m";
    return oss.str();
}

} // namespace topo
