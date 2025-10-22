/**
 * @file SimpleCommandLineParser.hpp
 * @brief Lightweight command-line parser to avoid CLI11 dependency issues
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <sstream>
#include <iostream>

namespace topo {

/**
 * @brief Simple command-line argument parser
 * 
 * A lightweight alternative to CLI11 that avoids CMake compatibility issues
 * while providing the essential functionality needed for our application.
 */
class SimpleCommandLineParser {
public:
    struct Option {
        std::string long_name;
        std::string short_name;
        std::string description;
        bool required;
        bool has_value;
        std::string default_value;
        
        // Default constructor for std::map
        Option() : required(false), has_value(true) {}
        
        Option(const std::string& long_name, const std::string& short_name,
               const std::string& description, bool required = false,
               bool has_value = true, const std::string& default_value = "")
            : long_name(long_name), short_name(short_name), description(description),
              required(required), has_value(has_value), default_value(default_value) {}
    };
    
    SimpleCommandLineParser(const std::string& program_name, const std::string& description)
        : program_name_(program_name), description_(description) {}
    
    // Add command line options
    void add_option(const std::string& long_name, const std::string& short_name,
                   const std::string& description, bool required = false,
                   const std::string& default_value = "") {
        options_[long_name] = Option(long_name, short_name, description, required, true, default_value);
        if (!short_name.empty()) {
            short_to_long_[short_name] = long_name;
        }
    }
    
    void add_flag(const std::string& long_name, const std::string& short_name,
                  const std::string& description) {
        options_[long_name] = Option(long_name, short_name, description, false, false);
        if (!short_name.empty()) {
            short_to_long_[short_name] = long_name;
        }
    }
    
    // Parse command line arguments
    bool parse(int argc, char* argv[]) {
        args_.clear();
        parsed_values_.clear();
        
        // Store all arguments
        for (int i = 1; i < argc; ++i) {
            args_.push_back(argv[i]);
        }
        
        // Check for help request
        for (const auto& arg : args_) {
            if (arg == "--help" || arg == "-h") {
                show_help();
                return false;
            }
        }
        
        // Parse arguments
        for (size_t i = 0; i < args_.size(); ++i) {
            const std::string& arg = args_[i];
            
            if (arg.starts_with("--")) {
                std::string option_name = arg.substr(2);
                
                // Handle --option=value format
                size_t eq_pos = option_name.find('=');
                std::string value;
                if (eq_pos != std::string::npos) {
                    value = option_name.substr(eq_pos + 1);
                    option_name = option_name.substr(0, eq_pos);
                }
                
                if (options_.find(option_name) == options_.end()) {
                    std::cerr << "Unknown option: --" << option_name << std::endl;
                    return false;
                }
                
                const auto& option = options_[option_name];
                if (option.has_value) {
                    if (value.empty()) {
                        if (i + 1 >= args_.size() || args_[i + 1].starts_with("-")) {
                            std::cerr << "Option --" << option_name << " requires a value" << std::endl;
                            return false;
                        }
                        value = args_[++i];
                    }
                    parsed_values_[option_name] = value;
                } else {
                    parsed_values_[option_name] = "true";
                }
                
            } else if (arg.starts_with("-") && arg.size() > 1) {
                std::string short_name = arg.substr(1);
                
                if (short_to_long_.find(short_name) == short_to_long_.end()) {
                    std::cerr << "Unknown option: -" << short_name << std::endl;
                    return false;
                }
                
                std::string option_name = short_to_long_[short_name];
                const auto& option = options_[option_name];
                
                if (option.has_value) {
                    if (i + 1 >= args_.size() || args_[i + 1].starts_with("-")) {
                        std::cerr << "Option -" << short_name << " requires a value" << std::endl;
                        return false;
                    }
                    parsed_values_[option_name] = args_[++i];
                } else {
                    parsed_values_[option_name] = "true";
                }
            } else {
                // Positional argument
                positional_args_.push_back(arg);
            }
        }
        
        // Check required options
        for (const auto& [name, option] : options_) {
            if (option.required && parsed_values_.find(name) == parsed_values_.end()) {
                std::cerr << "Required option --" << name << " not provided" << std::endl;
                return false;
            }
        }
        
        // Set default values
        for (const auto& [name, option] : options_) {
            if (parsed_values_.find(name) == parsed_values_.end() && !option.default_value.empty()) {
                parsed_values_[name] = option.default_value;
            }
        }
        
        return true;
    }
    
    // Get parsed values
    std::optional<std::string> get(const std::string& option_name) const {
        auto it = parsed_values_.find(option_name);
        if (it != parsed_values_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    bool get_flag(const std::string& option_name) const {
        auto value = get(option_name);
        return value.has_value() && value.value() == "true";
    }
    
    template<typename T>
    std::optional<T> get_as(const std::string& option_name) const {
        auto value = get(option_name);
        if (!value.has_value()) {
            return std::nullopt;
        }
        
        std::istringstream iss(value.value());
        T result;
        if (iss >> result) {
            return result;
        }
        return std::nullopt;
    }
    
    const std::vector<std::string>& get_positional() const {
        return positional_args_;
    }
    
    void show_help() const {
        show_topographic_help();
    }
    
    void show_topographic_help() const {
        std::cout << "GENERATE CONTOUR MAPS - High-Performance Laser-Cuttable Topographic Models\n\n";
        
        std::cout << "USAGE:\n";
        std::cout << "    " << program_name_ << " [OPTIONS]\n";
        std::cout << "    " << program_name_ << " -? [OPTION]     # Get help for specific option\n\n";
        
        std::cout << "QUICK START:\n";
        std::cout << "    # Generate Mount Denali model with defaults\n";
        std::cout << "    " << program_name_ << "\n";
        std::cout << "    \n";
        std::cout << "    # Create custom configuration file\n";
        std::cout << "    " << program_name_ << " --create-config my_area.json\n";
        std::cout << "    \n";
        std::cout << "    # Generate from config file\n";
        std::cout << "    " << program_name_ << " --config my_area.json\n\n";
        
        std::cout << "COORDINATE SPECIFICATION:\n";
        std::cout << "    Use decimal degrees in lat,lon format:\n";
        std::cout << "    --upper-left 63.1200,-151.1000 --lower-right 63.0200,-150.9000\n\n";
        
        std::cout << "MAIN OPTIONS:\n";
        print_help_section("config", "Load configuration from JSON file");
        print_help_section("upper-left", "Northwest corner coordinates (lat,lon)");
        print_help_section("lower-right", "Southeast corner coordinates (lat,lon)");
        print_help_section("num-layers", "Number of cutting layers (default: 7)");
        print_help_section("base-name", "Output filename prefix (default: mount_denali)");
        print_help_section("silent", "Suppress all output (same as --log-level 0)");
        print_help_section("verbose", "Enable verbose logging (same as --log-level 6)");
        print_help_section("log-level", "Verbosity: 0=SILENT, 1=CRITICAL, 2=ERROR, 3=WARNING+ops (default), 4=INFO, 5=DEBUG, 6=ALL");
        print_help_section("log-file", "Log to specified file (append if exists)");
        print_help_section("layer-list", "Generate specific layers only (e.g., \"3,4,5-7,11\")");
        print_help_section("create-config", "Create a default configuration file at the specified path");
        std::cout << "\n";
        
        std::cout << "PROCESSING OPTIONS:\n";
        print_help_section("simplify", "Simplification tolerance in meters (default: 5.0)");
        print_help_section("smoothing", "Smoothing iterations (default: 1)");
        print_help_section("min-area", "Minimum area for features in m² (default: 100.0)");
        print_help_section("min-feature-width", "Minimum feature width in mm (default: 2.0)");
        print_help_section("print-resolution", "Print resolution in DPI for precision (default: 600.0)");
        print_help_section("vertical-contour-relief", "Create layers with vertical sides (default)");
        print_help_section("no-vertical-contour-relief", "Create layers following terrain shapes");
        print_help_section("outer-boundaries-only", "Generate only outer boundaries");
        print_help_section("no-outer-boundaries-only", "Generate full contours including holes (default)");
        std::cout << "\n";
        
        std::cout << "SCALING & LAYER OPTIONS:\n";
        print_help_section("height-per-layer", "Real elevation per layer in meters (default: 21.43)");
        print_help_section("substrate-size", "Max model size in mm (default: 200)");
        print_help_section("substrate-depth", "Total height of stacked material in mm");
        print_help_section("cutting-bed-size", "Equipment bed: square (200) or rectangular (200,300)");
        print_help_section("cutting-bed-x", "Equipment bed X dimension in mm");
        print_help_section("cutting-bed-y", "Equipment bed Y dimension in mm");
        print_help_section("layer-thickness", "Cut material thickness in mm (default: 3.0)");
        print_help_section("include-roads", "Add road features");
        print_help_section("no-include-roads", "Disable road features (default)");
        print_help_section("include-buildings", "Add building outlines");
        print_help_section("no-include-buildings", "Disable building outlines (default)");
        print_help_section("include-waterways", "Add rivers/streams");
        print_help_section("no-include-waterways", "Disable waterways (default)");
        print_help_section("force-all-layers", "Generate all layers even if empty");
        print_help_section("no-force-all-layers", "Skip empty layers (default)");
        print_help_section("inset-upper-layers", "Cut holes where next layer sits (reduces material)");
        print_help_section("no-inset-upper-layers", "Solid elevation bands, no holes (default)");
        print_help_section("inset-offset", "Nesting lip size in mm (default: 1.0)");
        print_help_section("include-layer-numbers", "Add numbered labels (default)");
        print_help_section("no-include-layer-numbers", "Disable layer numbering");
        print_help_section("add-registration-marks", "Add alignment marks (default)");
        print_help_section("no-add-registration-marks", "Disable registration marks");
        print_help_section("add-physical-registration", "Add holes for rod alignment");
        print_help_section("no-add-physical-registration", "Disable physical holes (default)");
        print_help_section("physical-registration-hole-diameter", "Hole diameter in mm (default: 1.0)");
        print_help_section("add-base-layer", "Add extra base layer");
        print_help_section("no-add-base-layer", "Skip base layer (default)");
        std::cout << "\n";
        
        std::cout << "LABELING OPTIONS:\n";
        print_help_section("base-label-visible", "Visible text on base layer");
        print_help_section("base-label-hidden", "Hidden text on base layer");
        print_help_section("layer-label-visible", "Visible text on each layer");
        print_help_section("layer-label-hidden", "Hidden text on each layer");
        print_help_section("label-units", "Unit system: metric or imperial (default: metric)");
        print_help_section("print-units", "Substrate units: mm or inches (default: mm)");
        print_help_section("land-units", "Elevation units: meters or feet (default: meters)");
        std::cout << "\n";

        std::cout << "PATTERN SUBSTITUTION VARIABLES:\n";
        std::cout << "  Label patterns (for --base-label-visible, --layer-label-visible, etc.):\n";
        std::cout << "    %{s}  = Scale ratio (e.g., \"25000\" for 1:25000)\n";
        std::cout << "    %{c}  = Contour height in meters\n";
        std::cout << "    %{n}  = Layer number\n";
        std::cout << "    %{l}  = Layer number (zero-padded to 2 digits)\n";
        std::cout << "    %{e}  = Elevation in meters\n";
        std::cout << "    %{x}  = Center longitude\n";
        std::cout << "    %{y}  = Center latitude\n";
        std::cout << "    %{w}  = Area width (formatted)\n";
        std::cout << "    %{h}  = Area height (formatted)\n";
        std::cout << "    %{W}  = Substrate width (formatted)\n";
        std::cout << "    %{H}  = Substrate height (formatted)\n";
        std::cout << "    %{C}  = Center coordinate (lat, lon)\n";
        std::cout << "    %{UL} = Upper-left coordinate\n";
        std::cout << "    %{UR} = Upper-right coordinate\n";
        std::cout << "    %{LL} = Lower-left coordinate\n";
        std::cout << "    %{LR} = Lower-right coordinate\n";
        std::cout << "\n";
        std::cout << "  Filename patterns (for --filename):\n";
        std::cout << "    %{b}  = Base name\n";
        std::cout << "    %{l}  = Layer number (zero-padded)\n";
        std::cout << "    %{e}  = Elevation in meters\n";
        std::cout << "    %{n}  = Layer number (alias for %{l})\n";
        std::cout << "\n";
        
        std::cout << "ELEVATION FILTERING OPTIONS:\n";
        print_help_section("elevation-threshold", "Height/depth threshold in meters");
        print_help_section("min-elevation", "Minimum elevation to include in meters");
        print_help_section("max-elevation", "Maximum elevation to include in meters");
        std::cout << "\n";
        
        std::cout << "PRINT/CUT DIRECTION OPTIONS:\n";
        print_help_section("upside-down", "Print/cut layers upside down");
        print_help_section("no-upside-down", "Print/cut layers right-side up (default)");
        std::cout << "\n";
        
        std::cout << "OUTPUT FORMAT OPTIONS:\n";
        print_help_section("output-layers", "Generate individual files (default)");
        print_help_section("no-output-layers", "Skip individual files");
        print_help_section("output-formats", "Output formats: svg,stl,obj,ply (default: svg)");
        print_help_section("output-stacked", "Generate single 3D stacked model");
        print_help_section("no-output-stacked", "Skip stacked model (default)");
        std::cout << "\n";
        
        std::cout << "RENDERING & EXPORT OPTIONS:\n";
        print_help_section("quality", "Mesh quality: draft,medium,high,ultra (default: medium)");
        print_help_section("color-scheme", "Color scheme: terrain,rainbow,topographic (default: terrain)");
        print_help_section("render-mode", "2D render: full-color,grayscale,monochrome (default: full-color)");
        print_help_section("terrain-following", "Generate terrain-following surface");
        print_help_section("obj-materials", "Include materials in OBJ export");
        print_help_section("obj-colors", "Use elevation-based coloring in OBJ");
        print_help_section("dry-run", "Parse arguments and validate without processing");
        print_help_section("version", "Show version information");
        std::cout << "\n";
        
        std::cout << "HELP:\n";
        std::cout << "    -h, --help, -?, --?      Show this help\n";
        std::cout << "    -? OPTION                Show detailed help for specific option\n";
        std::cout << "\n";
        
        std::cout << "EXAMPLES:\n";
        std::cout << "    " << program_name_ << " --upper-left 45.0,-120.0 --lower-right 44.9,-119.9\n";
        std::cout << "    " << program_name_ << " --config denali.json --include-waterways\n";
        std::cout << "    " << program_name_ << " --num-layers 10 --output-formats stl,svg\n\n";
        
        std::cout << "OUTPUT:\n";
        std::cout << "    By default, creates individual SVG files for each layer.\n";
        std::cout << "    Optional: ZIP archive of all layers, 3D STL file of stacked model.\n";
        std::cout << "    Files are saved to the output directory (default: ./output/)\n";
    }

private:
    void print_help_section(const std::string& option_name, const std::string& description) const {
        auto it = options_.find(option_name);
        if (it != options_.end()) {
            const auto& option = it->second;
            std::cout << "    --" << option.long_name;
            if (option.has_value) {
                std::cout << " VALUE";
            }
            std::cout << "            " << description << "\n";
        }
    }
    std::string program_name_;
    std::string description_;
    std::map<std::string, Option> options_;
    std::map<std::string, std::string> short_to_long_;
    std::vector<std::string> args_;
    std::map<std::string, std::string> parsed_values_;
    std::vector<std::string> positional_args_;
};

} // namespace topo