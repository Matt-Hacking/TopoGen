/**
 * @file InputValidator.hpp
 * @brief Input validation for contradictory parameters
 *
 * Validates user inputs for contradictions and provides clear error messages
 * with suggested solutions when conflicts are detected.
 */

#pragma once

#include "topographic_generator.hpp"
#include <string>
#include <vector>
#include <optional>

namespace topo {

/**
 * @brief Represents a parameter conflict detected in user inputs
 */
struct ParameterConflict {
    std::string description;           // Description of the conflict
    std::vector<std::string> involved_params;  // Parameters involved
    std::vector<std::string> suggestions;      // Suggested resolutions
};

/**
 * @brief Result of input validation
 */
struct ValidationResult {
    bool is_valid;
    std::vector<ParameterConflict> conflicts;

    ValidationResult() : is_valid(true) {}

    bool has_errors() const { return !is_valid; }

    std::string format_error_message() const;
};

/**
 * @brief Validates user inputs for contradictions and conflicts
 */
class InputValidator {
public:
    InputValidator() = default;

    /**
     * @brief Validate all configuration parameters
     * @param config Configuration to validate
     * @return Validation result with any conflicts found
     */
    ValidationResult validate(const TopographicConfig& config) const;

private:
    /**
     * @brief Check if layer math is consistent
     * num_layers * layer_thickness_mm should not exceed substrate_depth_mm
     */
    std::optional<ParameterConflict> check_layer_depth_consistency(
        const TopographicConfig& config) const;

    /**
     * @brief Check if bed size and scale factor are consistent
     */
    std::optional<ParameterConflict> check_bed_scale_consistency(
        const TopographicConfig& config) const;

    /**
     * @brief Check if scaling method has required parameters
     */
    std::optional<ParameterConflict> check_scaling_method_parameters(
        const TopographicConfig& config) const;

    /**
     * @brief Check if 2D and 3D scaling methods are compatible
     */
    std::optional<ParameterConflict> check_2d_3d_scaling_compatibility(
        const TopographicConfig& config) const;
};

} // namespace topo
