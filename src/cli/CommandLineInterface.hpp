/**
 * @file CommandLineInterface.hpp  
 * @brief Command line interface for the topographic generator matching Python genContours.py
 */

#pragma once

#include "topographic_generator.hpp"
#include "SimpleCommandLineParser.hpp"
#include "UnitParser.hpp"
#include <string>
#include <vector>

namespace topo {

/**
 * @brief Command line interface for parsing arguments and configuring the generator
 */
class CommandLineInterface {
public:
    CommandLineInterface() = default;
    
    /**
     * @brief Parse command line arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return true if parsing was successful, false otherwise
     */
    bool parse_arguments(int argc, char* argv[]);
    
    /**
     * @brief Get the parsed configuration
     * @return TopographicConfig object
     */
    const TopographicConfig& get_config() const { return config_; }
    
    /**
     * @brief Check if this is a dry run
     * @return true if dry run mode is enabled
     */
    bool is_dry_run() const { return dry_run_; }

    /**
     * @brief Print the current configuration
     */
    void print_config() const;

private:
    TopographicConfig config_;
    bool dry_run_ = false;
    UnitParser unit_parser_;  // Parser for flexible unit handling
    
    // Coordinate parsing methods
    bool parse_coordinates(const std::string& upper_left_str, const std::string& lower_right_str);
    bool parse_bounds(const std::string& bounds_str);
    bool geocode_query(const std::string& query);
    
    // Main parsing method
    void parse_all_options(const SimpleCommandLineParser& parser);
    
    // Boolean option parsing with --no- variants
    void parse_boolean_option(const SimpleCommandLineParser& parser,
                             const std::string& positive_flag,
                             const std::string& negative_flag, 
                             bool& config_value);
    
    // Specialized parsing methods
    void parse_cutting_bed_size(const std::string& size_str);
    std::vector<int> parse_layer_list(const std::string& layer_str);
    void parse_label_units(const std::string& units_str);
    void parse_print_units(const std::string& units_str);
    void parse_land_units(const std::string& units_str);
    
    // Configuration file methods
    bool create_default_config_file(const std::string& filename);
    bool load_config_file(const std::string& filename);
    
    // Format and enum parsing
    std::vector<std::string> parse_formats(const std::string& formats_str);
    bool validate_output_formats(const std::vector<std::string>& formats);
    TopographicConfig::MeshQuality parse_quality(const std::string& quality_str);
    TopographicConfig::ColorScheme parse_color_scheme(const std::string& color_str);
    TopographicConfig::RenderMode parse_render_mode(const std::string& mode_str);
    TopographicConfig::ScalingMethod parse_scaling_method(const std::string& method_str, bool is_2d);
};

} // namespace topo