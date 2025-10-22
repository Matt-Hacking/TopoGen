/**
 * @file ScalingCalculator.hpp
 * @brief Scaling calculations for 2D and 3D outputs
 *
 * Implements various scaling strategies to convert geographic elevation data
 * into appropriately sized physical models for laser cutting or 3D printing.
 */

#pragma once

#include "topographic_generator.hpp"
#include "Logger.hpp"
#include <optional>
#include <string>

namespace topo {

/**
 * @brief Result of a scaling calculation
 */
struct ScalingResult {
    double scale_factor;      // Final scale in mm/m
    std::string method_used;  // Name of method that was applied
    std::string explanation;  // Detailed explanation of calculation

    ScalingResult(double scale, const std::string& method, const std::string& explain)
        : scale_factor(scale), method_used(method), explanation(explain) {}
};

/**
 * @brief Calculates scale factors for 2D and 3D outputs based on various methods
 *
 * This class implements different scaling strategies to ensure models fit
 * properly on laser cutting beds or 3D printer build plates while maintaining
 * accurate geometric relationships.
 */
class ScalingCalculator {
public:
    explicit ScalingCalculator(const TopographicConfig& config);

    /**
     * @brief Calculate 2D scale factor for SVG/vector outputs
     * @param xy_extent_meters Maximum XY extent of the model in meters
     * @param z_extent_meters Elevation range in meters (optional, for some methods)
     * @return Scaling result with factor in mm/m and explanation
     */
    ScalingResult calculate_2d_scale(double xy_extent_meters,
                                     std::optional<double> z_extent_meters = std::nullopt);

    /**
     * @brief Calculate 3D scale factor for STL/mesh outputs
     * @param xy_extent_meters Maximum XY extent of the model in meters
     * @param z_extent_meters Elevation range in meters
     * @return Scaling result with factor in mm/m and explanation
     */
    ScalingResult calculate_3d_scale(double xy_extent_meters, double z_extent_meters);

private:
    const TopographicConfig& config_;
    Logger logger_;

    // 2D scaling methods
    ScalingResult scale_2d_auto(double xy_extent, std::optional<double> z_extent);
    ScalingResult scale_2d_bed_size(double xy_extent);
    ScalingResult scale_2d_material_thickness(double xy_extent, double z_extent);
    ScalingResult scale_2d_layers(double xy_extent, double z_extent);
    ScalingResult scale_2d_explicit();

    // 3D scaling methods
    ScalingResult scale_3d_auto(double xy_extent, double z_extent);
    ScalingResult scale_3d_bed_size(double xy_extent);
    ScalingResult scale_3d_print_height(double xy_extent, double z_extent);
    ScalingResult scale_3d_uniform_xyz(double xy_extent, double z_extent);
    ScalingResult scale_3d_layers(double xy_extent, double z_extent);
    ScalingResult scale_3d_explicit();

    // Helper methods
    double get_target_bed_size_mm() const;
    std::string format_calculation(const std::string& formula, double result) const;
};

} // namespace topo
