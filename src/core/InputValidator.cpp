/**
 * @file InputValidator.cpp
 * @brief Implementation of input validation
 */

#include "InputValidator.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace topo {

std::string ValidationResult::format_error_message() const {
    if (is_valid || conflicts.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << "\nERROR: Contradictory parameters detected:\n\n";

    for (size_t i = 0; i < conflicts.size(); ++i) {
        const auto& conflict = conflicts[i];

        oss << "Conflict " << (i + 1) << ": " << conflict.description << "\n";

        if (!conflict.involved_params.empty()) {
            oss << "  Involved parameters:\n";
            for (const auto& param : conflict.involved_params) {
                oss << "    " << param << "\n";
            }
        }

        if (!conflict.suggestions.empty()) {
            oss << "  Suggested solutions:\n";
            for (size_t j = 0; j < conflict.suggestions.size(); ++j) {
                oss << "    " << (j + 1) << ". " << conflict.suggestions[j] << "\n";
            }
        }

        if (i < conflicts.size() - 1) {
            oss << "\n";
        }
    }

    oss << "\nProgram terminated due to contradictory inputs.\n";
    return oss.str();
}

ValidationResult InputValidator::validate(const TopographicConfig& config) const {
    ValidationResult result;
    result.is_valid = true;

    // Check various constraints
    auto layer_conflict = check_layer_depth_consistency(config);
    if (layer_conflict) {
        result.conflicts.push_back(*layer_conflict);
        result.is_valid = false;
    }

    auto bed_scale_conflict = check_bed_scale_consistency(config);
    if (bed_scale_conflict) {
        result.conflicts.push_back(*bed_scale_conflict);
        result.is_valid = false;
    }

    auto scaling_method_conflict = check_scaling_method_parameters(config);
    if (scaling_method_conflict) {
        result.conflicts.push_back(*scaling_method_conflict);
        result.is_valid = false;
    }

    auto compatibility_conflict = check_2d_3d_scaling_compatibility(config);
    if (compatibility_conflict) {
        result.conflicts.push_back(*compatibility_conflict);
        result.is_valid = false;
    }

    return result;
}

std::optional<ParameterConflict> InputValidator::check_layer_depth_consistency(
    const TopographicConfig& config) const {

    // Only check if substrate_depth is explicitly set (user-provided)
    if (!config.substrate_depth_mm.has_value()) {
        return std::nullopt;
    }

    double total_thickness = config.num_layers * config.layer_thickness_mm;
    double substrate_depth = config.substrate_depth_mm.value();

    if (total_thickness > substrate_depth + 0.01) {  // Small tolerance for floating point
        ParameterConflict conflict;
        conflict.description = "Total layer thickness exceeds substrate depth";

        conflict.involved_params = {
            "--num-layers " + std::to_string(config.num_layers) + " (user-provided)",
            "--layer-thickness " + std::to_string(config.layer_thickness_mm) + "mm (user-provided)",
            "--substrate-depth " + std::to_string(substrate_depth) + "mm (user-provided)"
        };

        std::ostringstream calc;
        calc << std::fixed << std::setprecision(1);
        calc << "Calculation: " << config.num_layers << " layers × "
             << config.layer_thickness_mm << "mm = " << total_thickness
             << "mm > " << substrate_depth << "mm depth";
        conflict.involved_params.push_back(calc.str());

        // Generate suggestions
        int layers_for_depth = static_cast<int>(substrate_depth / config.layer_thickness_mm);
        double thickness_for_layers = substrate_depth / config.num_layers;
        double depth_for_layers = total_thickness;

        conflict.suggestions = {
            "Use --num-layers " + std::to_string(layers_for_depth) +
                " (" + std::to_string(layers_for_depth) + " × " +
                std::to_string(config.layer_thickness_mm) + "mm = " +
                std::to_string(layers_for_depth * config.layer_thickness_mm) + "mm)",
            "Use --layer-thickness " + std::to_string(thickness_for_layers) +
                "mm (" + std::to_string(config.num_layers) + " × " +
                std::to_string(thickness_for_layers) + "mm = " +
                std::to_string(config.num_layers * thickness_for_layers) + "mm)",
            "Use --substrate-depth " + std::to_string(depth_for_layers) + "mm"
        };

        return conflict;
    }

    return std::nullopt;
}

std::optional<ParameterConflict> InputValidator::check_bed_scale_consistency(
    const TopographicConfig& config) const {

    // Check if both bed size and explicit scale factor are provided
    // This could create a conflict if they don't produce consistent results
    // For now, we'll allow this and just use the scale factor
    // More sophisticated checks could be added here

    return std::nullopt;
}

std::optional<ParameterConflict> InputValidator::check_scaling_method_parameters(
    const TopographicConfig& config) const {

    // Check if scaling method requires parameters that aren't provided
    // This will be implemented once scaling methods are added to the config

    return std::nullopt;
}

std::optional<ParameterConflict> InputValidator::check_2d_3d_scaling_compatibility(
    const TopographicConfig& config) const {

    // Check if 2D and 3D scaling methods are compatible when both outputs are requested
    // This will be implemented once scaling methods are added to the config

    return std::nullopt;
}

} // namespace topo
