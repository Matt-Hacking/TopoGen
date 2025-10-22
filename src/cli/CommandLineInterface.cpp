/**
 * @file CommandLineInterface.cpp
 * @brief Command line interface implementation matching Python genContours.py
 */

#include "CommandLineInterface.hpp"
#include "SimpleCommandLineParser.hpp"
#include "topographic_generator.hpp"
#include "../core/GeocodeService.hpp"
#include "../core/Logger.hpp"
#include "version.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

using json = nlohmann::json;

namespace topo {

bool CommandLineInterface::parse_arguments(int argc, char* argv[]) {
    SimpleCommandLineParser parser("topo-gen", 
        "Generate laser-cuttable contour maps and 3D models from elevation data\n"
        "\n"
        "High-performance C++ version with SRTM data processing, CGAL geometry\n"
        "libraries, and professional export formats including:\n"
        "  • SVG - Laser-cutting ready 2D contour layers\n"
        "  • STL/OBJ/PLY - 3D models for printing/manufacturing\n"
        // "  • NURBS - CAD-compatible surface models\n"
        "\n"
        "Default area: Mount Denali, Alaska (11.1 × 11.1 mile region)\n"
        "\n"
        "Examples:\n"
        "  # Generate SVG layers for laser cutting (default)\n"
        "  topo-gen\n"
        "\n"
        "  # Generate STL for 3D printing\n"
        "  topo-gen --output-formats stl\n"
        "\n"
        "  # Multiple formats with custom area\n"
        "  topo-gen --output-formats svg,stl --upper-left 63.12,-151.10 --lower-right 63.02,-150.90\n"
        "\n"
        "  # Add labels with pattern substitution (see --help for full list of variables)\n"
        "  topo-gen --base-label-visible \"Scale 1:%{s}\" --layer-label-visible \"Layer %{n}, Elev %{e}m\"\n"
        "\n"
        "Pattern Substitution Variables:\n"
        "  Label patterns (for --base-label-visible, --layer-label-visible, etc.):\n"
        "    %{s}  = Scale ratio (e.g., \"25000\" for 1:25000)\n"
        "    %{c}  = Contour height in meters\n"
        "    %{n}  = Layer number\n"
        "    %{l}  = Layer number (zero-padded to 2 digits)\n"
        "    %{e}  = Elevation in meters\n"
        "    %{x}  = Center longitude\n"
        "    %{y}  = Center latitude\n"
        "    %{w}  = Area width (formatted)\n"
        "    %{h}  = Area height (formatted)\n"
        "    %{W}  = Substrate width (formatted)\n"
        "    %{H}  = Substrate height (formatted)\n"
        "    %{C}  = Center coordinate (lat, lon)\n"
        "    %{UL} = Upper-left coordinate\n"
        "    %{UR} = Upper-right coordinate\n"
        "    %{LL} = Lower-left coordinate\n"
        "    %{LR} = Lower-right coordinate\n"
        "\n"
        "  Filename patterns (for --filename):\n"
        "    %{b}  = Base name\n"
        "    %{l}  = Layer number (zero-padded)\n"
        "    %{e}  = Elevation in meters\n"
        "    %{n}  = Layer number (alias for %{l})\n");
    
    // Configuration file option
    parser.add_option("config", "c", "Path to JSON configuration file");
    
    // Coordinate options
    parser.add_option("upper-left", "", "Upper left corner coordinates (lat,lon)");
    parser.add_option("lower-right", "", "Lower right corner coordinates (lat,lon)");
    parser.add_option("query", "q", "Place name or location query (uses OpenStreetMap Nominatim)");

    // Output options
    parser.add_option("base-name", "", "Base name for output files", false, "mount_denali");
    parser.add_option("filename", "", "Filename pattern (supports %{b}, %{l}, %{e}, %{n} - see pattern list above)", false, "%{b}-%{l}");
    parser.add_option("output-dir", "", "Output directory", false, "output");
    parser.add_option("output-path", "", "Output directory path (alias for --output-dir)", false, "output");
    
    // Layer options
    parser.add_option("num-layers", "", "Number of contour layers to generate", false, "7");
    parser.add_option("height-per-layer", "", "Height of each layer in meters", false, "21.43");
    parser.add_option("contour-interval", "", "Contour spacing for SVG export in meters", false, "100.0");
    parser.add_option("layer-thickness", "", "Thickness of each layer in mm", false, "3.0");
    parser.add_option("substrate-size", "", "Size of the substrate in mm", false, "200.0");
    parser.add_option("substrate-depth", "", "Total depth/height of substrate material stack in mm");
    parser.add_option("cutting-bed-size", "", "Size of cutting bed: single value or X,Y dimensions");
    parser.add_option("cutting-bed-x", "", "X-axis dimension of cutting bed in mm");
    parser.add_option("cutting-bed-y", "", "Y-axis dimension of cutting bed in mm");
    
    // Processing options
    parser.add_option("simplify", "", "Simplification tolerance in meters", false, "1.0");
    parser.add_option("smoothing", "", "Smoothing iterations", false, "1");
    parser.add_option("min-area", "", "Minimum area for features in m²", false, "100.0");
    parser.add_option("min-feature-width", "", "Minimum feature width in mm", false, "2.0");
    parser.add_option("print-resolution", "", "Print resolution in DPI", false, "600.0");

    // Color options for 2D output
    parser.add_option("stroke-color", "", "Stroke/border color as RGB hex (e.g., FF0000 for red)", false, "FF0000");
    parser.add_option("background-color", "", "Background color as RGB hex (e.g., FFFFFF for white)", false, "FFFFFF");
    parser.add_option("stroke-width", "", "Stroke width: unitless=pixels (converted via DPI), or with unit (e.g., 0.2mm, 3px)", false, "3px");

    // Elevation options
    parser.add_option("elevation-threshold", "", "Elevation threshold in meters");
    parser.add_option("min-elevation", "", "Minimum elevation to include in meters");
    parser.add_option("max-elevation", "", "Maximum elevation to include in meters");
    
    // Print direction options
    parser.add_flag("upside-down", "", "Print/cut layers upside down");
    parser.add_flag("no-upside-down", "", "Print/cut layers right-side up (default)");
    
    // Contour method options
    parser.add_flag("vertical-contour-relief", "", "Create layers with vertical sides (default)");
    parser.add_flag("no-vertical-contour-relief", "", "Create layers following terrain shapes");
    
    // Boundary options
    parser.add_flag("outer-boundaries-only", "", "Generate only outer boundaries");
    parser.add_flag("no-outer-boundaries-only", "", "Generate full contours including holes (default)");
    parser.add_flag("remove-holes", "", "Remove polygon holes from SVG output (simpler laser cutting, default)");
    parser.add_flag("keep-holes", "", "Keep polygon holes in SVG output (shows valleys/depressions)");
    
    // Feature inclusion options
    parser.add_flag("include-roads", "", "Include road features");
    parser.add_flag("no-include-roads", "", "Do not include roads");
    parser.add_flag("include-buildings", "", "Include building features");
    parser.add_flag("no-include-buildings", "", "Do not include buildings");
    parser.add_flag("include-waterways", "", "Include waterway features");
    parser.add_flag("no-include-waterways", "", "Do not include waterways");
    
    // Layer generation options
    parser.add_flag("force-all-layers", "", "Generate all layers even if empty");
    parser.add_flag("no-force-all-layers", "", "Skip empty layers (default)");
    parser.add_flag("inset-upper-layers", "", "Cut holes where next layer sits (reduces material, creates nesting lips)");
    parser.add_flag("no-inset-upper-layers", "", "Solid elevation bands, no holes (default)");
    parser.add_option("inset-offset", "", "Size of nesting lip in mm (default: 1.0)", false, "1.0");
    parser.add_flag("include-layer-numbers", "", "Add layer numbers (default)");
    parser.add_flag("no-include-layer-numbers", "", "Do not add layer numbers");
    parser.add_flag("add-registration-marks", "", "Add registration marks (default)");
    parser.add_flag("no-add-registration-marks", "", "Do not add registration marks");
    parser.add_flag("add-physical-registration", "", "Add physical registration holes");
    parser.add_flag("no-add-physical-registration", "", "Do not add physical holes (default)");
    parser.add_option("physical-registration-hole-diameter", "", "Hole diameter in mm", false, "1.0");
    parser.add_flag("add-base-layer", "", "Add an extra base layer");
    parser.add_flag("no-add-base-layer", "", "Do not add base layer (default)");
    
    // Labeling options
    // Visible labels: Placed in corners, remain visible after assembly
    // Hidden labels: Placed in center, covered by next layer when stacked
    parser.add_option("base-label-visible", "", "Visible label on base layer (stays visible after assembly). Supports patterns: %{s}, %{c}, %{n}, %{l}, %{e}, %{x}, %{y}, %{w}, %{h}, %{W}, %{H}, %{C}, %{UL}, %{UR}, %{LL}, %{LR}");
    parser.add_option("base-label-hidden", "", "Hidden label on base layer (covered by next layer). Supports same patterns as --base-label-visible");
    parser.add_option("layer-label-visible", "", "Visible label on each layer (stays visible after assembly). Supports same patterns as --base-label-visible");
    parser.add_option("layer-label-hidden", "", "Hidden label on each layer (covered by next layer). Supports same patterns as --base-label-visible");
    
    // Unit options
    parser.add_option("label-units", "", "Unit system: metric or imperial", false, "metric");
    parser.add_option("print-units", "", "Print units: mm or inches", false, "mm");
    parser.add_option("land-units", "", "Land units: meters or feet", false, "meters");
    
    // Output format options
    parser.add_flag("output-layers", "", "Generate individual layer files (default)");
    parser.add_flag("no-output-layers", "", "Do not generate layer files");
    parser.add_option("output-formats", "", "Output formats: svg,stl,obj,ply (comma-separated)", false, "svg");
    parser.add_option("output-format", "", "Output format: svg,stl,obj,ply (alias for --output-formats)", false, "svg");
    parser.add_flag("output-stacked", "", "Generate single 3D stacked model");
    parser.add_flag("no-output-stacked", "", "Do not generate stacked model (default)");

    // Scaling control options
    parser.add_option("2d-scaling-method", "", "2D scaling method: auto,bed-size,material-thickness,layers,explicit", false, "auto");
    parser.add_option("3d-scaling-method", "", "3D scaling method: auto,bed-size,print-height,uniform-xyz,explicit", false, "auto");
    parser.add_flag("use-2d-scaling-for-3d", "", "Force 2D scaling method for 3D outputs");
    parser.add_flag("use-3d-scaling-for-2d", "", "Force 3D scaling method for 2D outputs");
    parser.add_option("explicit-2d-scale-factor", "", "Explicit 2D scale factor in mm/m");
    parser.add_option("explicit-3d-scale-factor", "", "Explicit 3D scale factor in mm/m");

    // Logging and utility options
    parser.add_flag("silent", "s", "Suppress all output (same as --log-level 0)");
    parser.add_flag("verbose", "v", "Enable verbose logging (same as --log-level 6)");
    parser.add_option("log-level", "", "Logging level: 1=ERROR, 2=WARNING, 3=INFO (default), 4=DETAILED, 5=DEBUG, 6=TRACE\n"
                                       "                Supports facility-specific: \"3,PNGExporter=6\" or \"PNGExporter=6,ContourGenerator=4\"", false, "3");
    parser.add_option("log-file", "", "Log to file (append if exists)");
    parser.add_option("layer-list", "", "Generate only specified layers (e.g. 3,4,5-7,11)");
    parser.add_option("create-config", "", "Create default configuration file at path");

    // Geometry debugging options
    parser.add_flag("debug-geometry", "", "Enable geometry pathology detection and SVG debugging");
    parser.add_option("debug-output-dir", "", "Directory for debug SVG files", false, "debug");
    parser.add_flag("debug-show-legend", "", "Include pathology legend in debug SVGs");
    
    // Additional options
    parser.add_option("quality", "q", "Mesh quality: draft,medium,high,ultra", false, "medium");
    parser.add_option("color-scheme", "", "Color scheme: grayscale,terrain,rainbow,topographic,hypsometric", false, "terrain");
    parser.add_option("render-mode", "", "Render mode for 2D outputs: full-color,grayscale,monochrome", false, "full-color");
    parser.add_flag("terrain-following", "t", "Generate terrain-following surface");
    parser.add_flag("obj-materials", "", "Include materials in OBJ export");
    parser.add_flag("obj-colors", "", "Use elevation-based coloring in OBJ");
    parser.add_flag("dry-run", "", "Parse arguments and validate without processing");
    parser.add_flag("version", "", "Show version information");
    
    if (!parser.parse(argc, argv)) {
        return false;
    }
    
    // Handle version flag
    if (parser.get_flag("version")) {
        std::cout << "Topographic Generator v" << TOPO_VERSION_STRING << " (Public BETA)" << std::endl;
        std::cout << "High-performance topographic modeling based on Bambu Slicer algorithms" << std::endl;
        std::cout << "Built with CGAL, Eigen, GDAL, TBB, OpenMP" << std::endl;
        std::cout << "Copyright (c) 2025 Matthew Block" << std::endl;
        return false;
    }
    
    // Handle create-config flag
    if (auto config_path = parser.get("create-config")) {
        create_default_config_file(config_path.value());
        std::cout << "Created default configuration file: " << config_path.value() << std::endl;
        return false;
    }
    
    // Load configuration file if specified
    if (auto config_file = parser.get("config")) {
        if (!load_config_file(config_file.value())) {
            std::cerr << "Failed to load configuration file: " << config_file.value() << std::endl;
            return false;
        }
        config_.config_file = config_file.value();
    }
    
    // Parse coordinate options (required if no config file)
    auto upper_left = parser.get("upper-left");
    auto lower_right = parser.get("lower-right");
    auto query = parser.get("query");

    if (query.has_value()) {
        // Use geocoding to resolve place name
        if (!geocode_query(query.value())) {
            std::cerr << "Failed to geocode query: " << query.value() << std::endl;
            std::cerr << "Try using explicit coordinates with --upper-left and --lower-right" << std::endl;
            return false;
        }
    } else if (upper_left.has_value() && lower_right.has_value()) {
        if (!parse_coordinates(upper_left.value(), lower_right.value())) {
            std::cerr << "Invalid coordinate format. Use: lat,lon" << std::endl;
            return false;
        }
    } else if (!config_.config_file.has_value()) {
        // Use default Mount Denali coordinates - bounds are initialized in TopographicConfig header
        // No additional setup needed - bounds are already properly initialized
    }
    
    // Parse all other options with Python defaults
    parse_all_options(parser);

    // Validate output formats
    if (!config_.output_formats.empty()) {
        if (!validate_output_formats(config_.output_formats)) {
            return false;
        }
    }

    return true;
}

bool CommandLineInterface::parse_coordinates(const std::string& upper_left_str, const std::string& lower_right_str) {
    try {
        // Parse upper-left coordinates (supports decimal degrees and DMS format)
        auto [upper_lat, left_lon] = unit_parser_.parse_coordinate_pair(upper_left_str);

        // Parse lower-right coordinates (supports decimal degrees and DMS format)
        auto [lower_lat, right_lon] = unit_parser_.parse_coordinate_pair(lower_right_str);
        
        // Validate coordinates
        if (upper_lat <= lower_lat || left_lon >= right_lon) {
            std::cerr << "Invalid coordinate bounds: upper-left must be northwest of lower-right" << std::endl;
            return false;
        }
        
        // Store in config
        config_.upper_left_lat = upper_lat;
        config_.upper_left_lon = left_lon;
        config_.lower_right_lat = lower_lat;
        config_.lower_right_lon = right_lon;
        
        // Create bounding box (lon, lat, lon, lat)
        config_.bounds = BoundingBox(left_lon, lower_lat, right_lon, upper_lat);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CommandLineInterface::parse_bounds(const std::string& bounds_str) {
    std::istringstream iss(bounds_str);
    std::string token;
    std::vector<double> coords;
    
    while (std::getline(iss, token, ',')) {
        try {
            coords.push_back(std::stod(token));
        } catch (const std::exception&) {
            return false;
        }
    }
    
    if (coords.size() != 4) {
        return false;
    }
    
    // Create bounding box (lat1,lon1,lat2,lon2)
    config_.bounds = BoundingBox(coords[1], coords[0], coords[3], coords[2]); // lon,lat,lon,lat
    return true;
}

void CommandLineInterface::parse_all_options(const SimpleCommandLineParser& parser) {
    // Output options
    if (auto value = parser.get("base-name")) config_.base_name = value.value();
    if (auto value = parser.get("filename")) config_.filename_pattern = value.value();
    if (auto value = parser.get("output-dir")) config_.output_directory = value.value();
    if (auto value = parser.get("output-path")) {
        // --output-path is an alias for --output-dir (takes precedence)
        std::string path = value.value();
        // Remove trailing slash if present for consistency
        if (!path.empty() && path.back() == '/') {
            path.pop_back();
        }
        config_.output_directory = path;
    }
    
    // Layer options
    if (auto value = parser.get_as<int>("num-layers")) config_.num_layers = value.value();

    // Parse height-per-layer with unit suffix support (e.g., "200ft", "50m", "30km")
    if (auto value = parser.get("height-per-layer")) {
        auto parsed = unit_parser_.parse_land_distance(value.value());
        config_.height_per_layer = parsed.value;
    }

    if (auto value = parser.get_as<double>("contour-interval")) config_.contour_interval = value.value();

    // Parse layer-thickness with unit suffix support (e.g., "3mm", "0.125in")
    if (auto value = parser.get("layer-thickness")) {
        auto parsed = unit_parser_.parse_print_distance(value.value());
        config_.layer_thickness_mm = parsed.value;
    }

    // Parse substrate-size with unit suffix support (e.g., "200mm", "8in")
    if (auto value = parser.get("substrate-size")) {
        auto parsed = unit_parser_.parse_print_distance(value.value());
        config_.substrate_size_mm = parsed.value;
    }
    if (auto value = parser.get_as<double>("substrate-depth")) config_.substrate_depth_mm = value.value();
    if (auto value = parser.get_as<double>("cutting-bed-x")) config_.cutting_bed_x_mm = value.value();
    if (auto value = parser.get_as<double>("cutting-bed-y")) config_.cutting_bed_y_mm = value.value();
    
    // Handle cutting-bed-size (can be single value or X,Y)
    if (auto value = parser.get("cutting-bed-size")) {
        parse_cutting_bed_size(value.value());
    }
    
    // Processing options
    // Parse simplify tolerance with unit suffix support (e.g., "5m", "10ft")
    if (auto value = parser.get("simplify")) {
        auto parsed = unit_parser_.parse_land_distance(value.value());
        config_.simplification_tolerance = parsed.value;
    }

    if (auto value = parser.get_as<int>("smoothing")) config_.smoothing_iterations = value.value();
    if (auto value = parser.get_as<double>("min-area")) config_.min_feature_area = value.value();

    // Parse min-feature-width with unit suffix support (e.g., "2mm", "0.1in")
    if (auto value = parser.get("min-feature-width")) {
        auto parsed = unit_parser_.parse_print_distance(value.value());
        config_.min_feature_width_mm = parsed.value;
    }
    if (auto value = parser.get_as<double>("print-resolution")) config_.print_resolution_dpi = value.value();

    // Color options
    if (auto value = parser.get("stroke-color")) config_.stroke_color = value.value();
    if (auto value = parser.get("background-color")) config_.background_color = value.value();

    // Parse stroke width with unit support (pixels default, converted to mm via DPI)
    if (auto value = parser.get("stroke-width")) {
        auto parsed = unit_parser_.parse_stroke_width(value.value(), config_.print_resolution_dpi);
        config_.stroke_width = parsed.value;  // Now in millimeters
    }

    // Elevation options
    if (auto value = parser.get_as<double>("elevation-threshold")) config_.elevation_threshold = value.value();
    if (auto value = parser.get_as<double>("min-elevation")) config_.min_elevation = value.value();
    if (auto value = parser.get_as<double>("max-elevation")) config_.max_elevation = value.value();
    
    // Boolean options with --no- variants
    parse_boolean_option(parser, "upside-down", "no-upside-down", config_.upside_down_printing);
    parse_boolean_option(parser, "vertical-contour-relief", "no-vertical-contour-relief", config_.vertical_contour_relief);
    parse_boolean_option(parser, "outer-boundaries-only", "no-outer-boundaries-only", config_.outer_boundaries_only);
    parse_boolean_option(parser, "remove-holes", "keep-holes", config_.remove_holes);
    parse_boolean_option(parser, "include-roads", "no-include-roads", config_.include_roads);
    parse_boolean_option(parser, "include-buildings", "no-include-buildings", config_.include_buildings);
    parse_boolean_option(parser, "include-waterways", "no-include-waterways", config_.include_waterways);
    parse_boolean_option(parser, "force-all-layers", "no-force-all-layers", config_.force_all_layers);
    parse_boolean_option(parser, "inset-upper-layers", "no-inset-upper-layers", config_.inset_upper_layers);
    parse_boolean_option(parser, "include-layer-numbers", "no-include-layer-numbers", config_.include_layer_numbers);
    parse_boolean_option(parser, "add-registration-marks", "no-add-registration-marks", config_.add_registration_marks);
    parse_boolean_option(parser, "add-physical-registration", "no-add-physical-registration", config_.add_physical_registration);
    parse_boolean_option(parser, "add-base-layer", "no-add-base-layer", config_.add_base_layer);
    parse_boolean_option(parser, "output-layers", "no-output-layers", config_.output_layers);
    parse_boolean_option(parser, "output-stacked", "no-output-stacked", config_.output_stacked);
    
    // Physical registration hole diameter
    if (auto value = parser.get_as<double>("physical-registration-hole-diameter")) {
        config_.physical_registration_hole_diameter = value.value();
    }

    // Inset offset for layer stacking
    if (auto value = parser.get_as<double>("inset-offset")) {
        config_.inset_offset_mm = value.value();
    }

    // Labeling options
    if (auto value = parser.get("base-label-visible")) config_.base_label_visible = value.value();
    if (auto value = parser.get("base-label-hidden")) config_.base_label_hidden = value.value();
    if (auto value = parser.get("layer-label-visible")) config_.layer_label_visible = value.value();
    if (auto value = parser.get("layer-label-hidden")) config_.layer_label_hidden = value.value();
    
    // Unit options
    if (auto value = parser.get("label-units")) parse_label_units(value.value());
    if (auto value = parser.get("print-units")) parse_print_units(value.value());
    if (auto value = parser.get("land-units")) parse_land_units(value.value());
    
    // Output formats (support both singular and plural forms)
    if (auto value = parser.get("output-formats")) {
        config_.output_formats = parse_formats(value.value());
    } else if (auto value = parser.get("output-format")) {
        config_.output_formats = parse_formats(value.value());
    }
    
    // Logging options with priority: CLI > ENV > config file > defaults
    // 1. Check environment variable first
    const char* env_log_level = std::getenv("TOPO_LOG_LEVEL");
    if (env_log_level) {
        std::string env_config(env_log_level);
        Logger::parseLogConfig(env_config);
        // Extract default level for config
        if (env_config.find('=') == std::string::npos) {
            // Simple number, use as default
            config_.log_level = std::stoi(env_config);
        } else {
            // Facility-specific, set to default
            config_.log_level = 3;  // INFO default
        }
    }

    // 2. CLI arguments override environment
    if (auto value = parser.get("log-level")) {
        std::string log_config = value.value();
        Logger::parseLogConfig(log_config);
        // Extract default level for config
        if (log_config.find('=') == std::string::npos) {
            // Simple number
            config_.log_level = std::stoi(log_config);
        } else {
            // Facility-specific, extract default if provided
            size_t comma_pos = log_config.find(',');
            if (comma_pos != std::string::npos) {
                std::string first_part = log_config.substr(0, comma_pos);
                if (first_part.find('=') == std::string::npos) {
                    config_.log_level = std::stoi(first_part);
                }
            }
        }
    }

    // 3. Flags override everything
    if (parser.get_flag("silent")) {
        config_.log_level = 0; // Silent: no output
        Logger::setDefaultLevel(static_cast<LogLevel>(1));  // ERROR level minimum
    }
    if (parser.get_flag("verbose")) {
        config_.log_level = 6; // Verbose: full debugging
        Logger::setDefaultLevel(LogLevel::TRACE);
    }

    // 4. Log file configuration
    const char* env_log_file = std::getenv("TOPO_LOG_FILE");
    if (env_log_file) {
        config_.log_file = std::string(env_log_file);
    }
    if (auto value = parser.get("log-file")) {
        config_.log_file = value.value();  // CLI overrides environment
    }

    // Geometry debugging options
    config_.debug_geometry = parser.get_flag("debug-geometry");
    if (auto value = parser.get("debug-output-dir")) config_.debug_image_path = value.value();
    config_.debug_show_legend = parser.get_flag("debug-show-legend");
    
    // Layer list
    if (auto value = parser.get("layer-list")) {
        config_.layer_list = value.value();
        config_.specific_layers = parse_layer_list(value.value());
    }
    
    // C++ specific options
    if (auto value = parser.get("quality")) config_.quality = parse_quality(value.value());
    if (auto value = parser.get("color-scheme")) config_.color_scheme = parse_color_scheme(value.value());
    if (auto value = parser.get("render-mode")) config_.render_mode = parse_render_mode(value.value());
    config_.terrain_following = parser.get_flag("terrain-following");
    config_.obj_include_materials = parser.get_flag("obj-materials");
    config_.obj_elevation_colors = parser.get_flag("obj-colors");
    
    // Scaling method options
    if (auto value = parser.get("2d-scaling-method")) {
        config_.scaling_2d_method = parse_scaling_method(value.value(), true);
    }
    if (auto value = parser.get("3d-scaling-method")) {
        config_.scaling_3d_method = parse_scaling_method(value.value(), false);
    }
    config_.use_2d_scaling_for_3d = parser.get_flag("use-2d-scaling-for-3d");
    config_.use_3d_scaling_for_2d = parser.get_flag("use-3d-scaling-for-2d");
    if (auto value = parser.get_as<double>("explicit-2d-scale-factor")) {
        config_.explicit_2d_scale_factor = value.value();
    }
    if (auto value = parser.get_as<double>("explicit-3d-scale-factor")) {
        config_.explicit_3d_scale_factor = value.value();
    }

    // Utility flags
    dry_run_ = parser.get_flag("dry-run");
}

void CommandLineInterface::parse_boolean_option(const SimpleCommandLineParser& parser,
                                               const std::string& positive_flag,
                                               const std::string& negative_flag,
                                               bool& config_value) {
    if (parser.get_flag(positive_flag)) {
        config_value = true;
    } else if (parser.get_flag(negative_flag)) {
        config_value = false;
    }
    // If neither is specified, keep default value
}

void CommandLineInterface::parse_cutting_bed_size(const std::string& size_str) {
    auto comma_pos = size_str.find(',');
    if (comma_pos != std::string::npos) {
        // X,Y format
        try {
            config_.cutting_bed_x_mm = std::stod(size_str.substr(0, comma_pos));
            config_.cutting_bed_y_mm = std::stod(size_str.substr(comma_pos + 1));
        } catch (const std::exception&) {
            std::cerr << "Warning: Invalid cutting bed size format: " << size_str << std::endl;
        }
    } else {
        // Single value (square)
        try {
            double size = std::stod(size_str);
            config_.cutting_bed_x_mm = size;
            config_.cutting_bed_y_mm = size;
            config_.cutting_bed_size_mm = size;
        } catch (const std::exception&) {
            std::cerr << "Warning: Invalid cutting bed size: " << size_str << std::endl;
        }
    }
}

std::vector<int> CommandLineInterface::parse_layer_list(const std::string& layer_str) {
    std::vector<int> layers;
    std::istringstream iss(layer_str);
    std::string part;
    
    while (std::getline(iss, part, ',')) {
        // Trim whitespace
        part.erase(0, part.find_first_not_of(" \t"));
        part.erase(part.find_last_not_of(" \t") + 1);
        
        auto dash_pos = part.find('-');
        if (dash_pos != std::string::npos) {
            // Range like "5-7"
            try {
                int start = std::stoi(part.substr(0, dash_pos));
                int end = std::stoi(part.substr(dash_pos + 1));
                if (start <= end) {
                    for (int i = start; i <= end; ++i) {
                        layers.push_back(i);
                    }
                }
            } catch (const std::exception&) {
                std::cerr << "Warning: Invalid layer range: " << part << std::endl;
            }
        } else {
            // Single layer
            try {
                layers.push_back(std::stoi(part));
            } catch (const std::exception&) {
                std::cerr << "Warning: Invalid layer number: " << part << std::endl;
            }
        }
    }
    
    // Remove duplicates and sort
    std::sort(layers.begin(), layers.end());
    layers.erase(std::unique(layers.begin(), layers.end()), layers.end());
    
    return layers;
}

void CommandLineInterface::parse_label_units(const std::string& units_str) {
    if (units_str == "metric") {
        config_.label_units = TopographicConfig::Units::METRIC;
    } else if (units_str == "imperial") {
        config_.label_units = TopographicConfig::Units::IMPERIAL;
    } else {
        std::cerr << "Warning: Unknown label units '" << units_str << "', using 'metric'" << std::endl;
    }
}

void CommandLineInterface::parse_print_units(const std::string& units_str) {
    if (units_str == "mm") {
        config_.print_units = TopographicConfig::PrintUnits::MM;
    } else if (units_str == "inches") {
        config_.print_units = TopographicConfig::PrintUnits::INCHES;
    } else {
        std::cerr << "Warning: Unknown print units '" << units_str << "', using 'mm'" << std::endl;
    }

    // Also update unit parser preferences for flexible parsing
    try {
        auto prefs = unit_parser_.get_preferences();
        prefs.set_print_units(units_str);
        unit_parser_.set_preferences(prefs);
    } catch (const UnitParseError& e) {
        // Warning already printed above
    }
}

void CommandLineInterface::parse_land_units(const std::string& units_str) {
    if (units_str == "meters") {
        config_.land_units = TopographicConfig::LandUnits::METERS;
    } else if (units_str == "feet") {
        config_.land_units = TopographicConfig::LandUnits::FEET;
    } else {
        std::cerr << "Warning: Unknown land units '" << units_str << "', using 'meters'" << std::endl;
    }

    // Also update unit parser preferences for flexible parsing
    try {
        auto prefs = unit_parser_.get_preferences();
        prefs.set_land_units(units_str);
        unit_parser_.set_preferences(prefs);
    } catch (const UnitParseError& e) {
        // Warning already printed above
    }
}

bool CommandLineInterface::create_default_config_file(const std::string& filename) {
    // Create default JSON configuration matching Python version
    const std::string default_config = R"({
  "upper_left_lat": 63.1497,
  "upper_left_lon": -151.1847,
  "lower_right_lat": 62.9887,
  "lower_right_lon": -150.8293,
  "base_name": "mount_denali",
  "num_layers": 7,
  "height_per_layer": 21.43,
  "substrate_size_mm": 200.0,
  "layer_thickness_mm": 3.0,
  "substrate_depth_mm": null,
  "cutting_bed_size_mm": null,
  "cutting_bed_x_mm": null,
  "cutting_bed_y_mm": null,
  "simplify_tolerance": 5.0,
  "smoothing": 1,
  "min_area": 100.0,
  "min_feature_width_mm": 2.0,
  "print_resolution_dpi": 600.0,
  "elevation_threshold": null,
  "min_elevation": null,
  "max_elevation": null,
  "upside_down_printing": false,
  "vertical_contour_relief": true,
  "include_roads": false,
  "include_buildings": false,
  "include_waterways": false,
  "force_all_layers": false,
  "inset_upper_layers": false,
  "inset_offset_mm": 1.0,
  "include_layer_numbers": true,
  "add_registration_marks": true,
  "add_physical_registration": false,
  "physical_registration_hole_diameter": 1.0,
  "add_base_layer": false,
  "base_label_visible": "",
  "base_label_hidden": "",
  "layer_label_visible": "",
  "layer_label_hidden": "",
  "output_dir": "output",
  "output_formats": "svg",
  "output_stacked": false,
  "label_units": "metric",
  "print_units": "mm",
  "land_units": "meters"
})";

    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << default_config;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool CommandLineInterface::load_config_file(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open config file: " << filename << std::endl;
            return false;
        }

        json config;
        file >> config;

        // Helper lambda to parse value with optional unit suffix
        auto parse_distance_value = [this](const json& j, const std::string& key, auto& target, bool is_land_distance) {
            if (!j.contains(key) || j[key].is_null()) return;

            if (j[key].is_string()) {
                // String value - parse with unit suffix support
                std::string value_str = j[key].get<std::string>();
                try {
                    if (is_land_distance) {
                        auto parsed = unit_parser_.parse_land_distance(value_str);
                        target = parsed.value;
                    } else {
                        auto parsed = unit_parser_.parse_print_distance(value_str);
                        target = parsed.value;
                    }
                } catch (const UnitParseError& e) {
                    std::cerr << "Warning: " << e.what() << " for key '" << key << "', using default" << std::endl;
                }
            } else if (j[key].is_number()) {
                // Numeric value - use directly (already in correct units)
                target = j[key].get<double>();
            }
        };

        // Parse coordinates (supports DMS format in strings)
        if (config.contains("upper_left_lat") && config.contains("upper_left_lon")) {
            try {
                std::string lat_str = config["upper_left_lat"].is_string() ?
                    config["upper_left_lat"].get<std::string>() :
                    std::to_string(config["upper_left_lat"].get<double>());
                std::string lon_str = config["upper_left_lon"].is_string() ?
                    config["upper_left_lon"].get<std::string>() :
                    std::to_string(config["upper_left_lon"].get<double>());

                config_.upper_left_lat = unit_parser_.parse_latitude(lat_str);
                config_.upper_left_lon = unit_parser_.parse_longitude(lon_str);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error parsing upper-left coordinates: " << e.what() << std::endl;
            }
        }

        if (config.contains("lower_right_lat") && config.contains("lower_right_lon")) {
            try {
                std::string lat_str = config["lower_right_lat"].is_string() ?
                    config["lower_right_lat"].get<std::string>() :
                    std::to_string(config["lower_right_lat"].get<double>());
                std::string lon_str = config["lower_right_lon"].is_string() ?
                    config["lower_right_lon"].get<std::string>() :
                    std::to_string(config["lower_right_lon"].get<double>());

                config_.lower_right_lat = unit_parser_.parse_latitude(lat_str);
                config_.lower_right_lon = unit_parser_.parse_longitude(lon_str);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error parsing lower-right coordinates: " << e.what() << std::endl;
            }
        }

        // Update bounding box if coordinates are set
        if (config.contains("upper_left_lat") && config.contains("lower_right_lat")) {
            config_.bounds = BoundingBox(config_.upper_left_lon, config_.lower_right_lat,
                                        config_.lower_right_lon, config_.upper_left_lat);
        }

        // Parse basic string/int parameters
        if (config.contains("base_name")) config_.base_name = config["base_name"];
        if (config.contains("num_layers") && config["num_layers"].is_number())
            config_.num_layers = config["num_layers"];
        if (config.contains("output_dir")) config_.output_directory = config["output_dir"];

        // Parse distance parameters with unit suffix support
        parse_distance_value(config, "height_per_layer", config_.height_per_layer, true);
        parse_distance_value(config, "layer_thickness_mm", config_.layer_thickness_mm, false);
        parse_distance_value(config, "substrate_size_mm", config_.substrate_size_mm, false);
        parse_distance_value(config, "substrate_depth_mm", config_.substrate_depth_mm, false);
        parse_distance_value(config, "simplify_tolerance", config_.simplification_tolerance, true);
        parse_distance_value(config, "min_feature_width_mm", config_.min_feature_width_mm, false);

        // Other numeric parameters (no unit suffixes)
        if (config.contains("smoothing") && config["smoothing"].is_number())
            config_.smoothing_iterations = config["smoothing"];
        if (config.contains("min_area") && config["min_area"].is_number())
            config_.min_feature_area = config["min_area"];

        // Boolean parameters
        if (config.contains("include_roads")) config_.include_roads = config["include_roads"];
        if (config.contains("include_buildings")) config_.include_buildings = config["include_buildings"];
        if (config.contains("include_waterways")) config_.include_waterways = config["include_waterways"];
        if (config.contains("include_layer_numbers")) config_.include_layer_numbers = config["include_layer_numbers"];
        if (config.contains("add_registration_marks")) config_.add_registration_marks = config["add_registration_marks"];

        // Output formats
        if (config.contains("output_formats")) {
            std::string formats_str = config["output_formats"];
            config_.output_formats = parse_formats(formats_str);
        }

        // Unit preferences (update parser for subsequent values)
        if (config.contains("land_units")) {
            std::string land_units = config["land_units"];
            parse_land_units(land_units);
        }
        if (config.contains("print_units")) {
            std::string print_units = config["print_units"];
            parse_print_units(print_units);
        }

        return true;

    } catch (const json::exception& e) {
        std::cerr << "Error parsing JSON config file: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error loading config file: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> CommandLineInterface::parse_formats(const std::string& formats_str) {
    std::vector<std::string> formats;
    std::istringstream iss(formats_str);
    std::string format;
    
    while (std::getline(iss, format, ',')) {
        // Trim whitespace
        format.erase(0, format.find_first_not_of(" \t"));
        format.erase(format.find_last_not_of(" \t") + 1);
        
        if (!format.empty()) {
            formats.push_back(format);
        }
    }
    
    return formats;
}

bool CommandLineInterface::validate_output_formats(const std::vector<std::string>& formats) {
    // List of all supported output formats with their exporters
    static const std::vector<std::string> supported_formats = {
        "svg",       // SVGExporter - laser-cutting ready 2D layers
        "stl",       // STLExporter - 3D printing binary/ASCII
        "obj",       // OBJExporter - 3D with materials and colors
        "ply",       // PLYExporter - research and visualization
        "nurbs",     // NURBSExporter - CAD-compatible surfaces (IGES/STEP)
        "png",       // PNGExporter - raster images
        "geotiff",   // GeoTIFFExporter - georeferenced raster
        "geojson",   // GeoJSONExporter - vector GIS format
        "shapefile"  // ShapefileExporter - ESRI Shapefile
    };

    std::vector<std::string> unsupported_formats;

    for (const auto& format : formats) {
        bool is_supported = std::find(supported_formats.begin(),
                                     supported_formats.end(),
                                     format) != supported_formats.end();
        if (!is_supported) {
            unsupported_formats.push_back(format);
        }
    }

    if (!unsupported_formats.empty()) {
        std::cerr << "\n❌ Error: Unsupported output format";
        if (unsupported_formats.size() > 1) {
            std::cerr << "s";
        }
        std::cerr << ": ";

        for (size_t i = 0; i < unsupported_formats.size(); ++i) {
            if (i > 0) std::cerr << ", ";
            std::cerr << "'" << unsupported_formats[i] << "'";
        }
        std::cerr << "\n\n";

        std::cerr << "Supported formats:\n";
        std::cerr << "  3D Mesh:  stl, obj, ply, nurbs\n";
        std::cerr << "  Raster:   png, geotiff\n";
        std::cerr << "  Vector:   svg, geojson, shapefile\n\n";
        std::cerr << "Example: --output-formats svg,stl,obj\n\n";

        return false;
    }

    return true;
}

TopographicConfig::MeshQuality CommandLineInterface::parse_quality(const std::string& quality_str) {
    if (quality_str == "draft") return TopographicConfig::MeshQuality::DRAFT;
    if (quality_str == "medium") return TopographicConfig::MeshQuality::MEDIUM;
    if (quality_str == "high") return TopographicConfig::MeshQuality::HIGH;
    if (quality_str == "ultra") return TopographicConfig::MeshQuality::ULTRA;
    
    std::cerr << "Warning: Unknown quality '" << quality_str << "', using 'medium'" << std::endl;
    return TopographicConfig::MeshQuality::MEDIUM;
}


TopographicConfig::ColorScheme CommandLineInterface::parse_color_scheme(const std::string& color_str) {
    if (color_str == "grayscale") return TopographicConfig::ColorScheme::GRAYSCALE;
    if (color_str == "terrain") return TopographicConfig::ColorScheme::TERRAIN;
    if (color_str == "rainbow") return TopographicConfig::ColorScheme::RAINBOW;
    if (color_str == "topographic") return TopographicConfig::ColorScheme::TOPOGRAPHIC;
    if (color_str == "hypsometric") return TopographicConfig::ColorScheme::HYPSOMETRIC;
    if (color_str == "custom") return TopographicConfig::ColorScheme::CUSTOM;

    std::cerr << "Warning: Unknown color scheme '" << color_str << "', using 'terrain'" << std::endl;
    return TopographicConfig::ColorScheme::TERRAIN;
}

TopographicConfig::RenderMode CommandLineInterface::parse_render_mode(const std::string& mode_str) {
    if (mode_str == "full-color" || mode_str == "full_color") return TopographicConfig::RenderMode::FULL_COLOR;
    if (mode_str == "grayscale" || mode_str == "greyscale") return TopographicConfig::RenderMode::GRAYSCALE;
    if (mode_str == "monochrome" || mode_str == "mono") return TopographicConfig::RenderMode::MONOCHROME;

    std::cerr << "Warning: Unknown render mode '" << mode_str << "', using 'full-color'" << std::endl;
    return TopographicConfig::RenderMode::FULL_COLOR;
}

TopographicConfig::ScalingMethod CommandLineInterface::parse_scaling_method(const std::string& method_str, bool is_2d) {
    if (method_str == "auto") return TopographicConfig::ScalingMethod::AUTO;
    if (method_str == "bed-size") return TopographicConfig::ScalingMethod::BED_SIZE;
    if (method_str == "explicit") return TopographicConfig::ScalingMethod::EXPLICIT;
    if (method_str == "layers") return TopographicConfig::ScalingMethod::LAYERS;

    if (is_2d) {
        // 2D-specific methods
        if (method_str == "material-thickness") return TopographicConfig::ScalingMethod::MATERIAL_THICKNESS;

        // Warn about 3D-only methods
        if (method_str == "print-height" || method_str == "uniform-xyz") {
            std::cerr << "Warning: '" << method_str << "' is 3D-only, using 'auto' for 2D" << std::endl;
            return TopographicConfig::ScalingMethod::AUTO;
        }
    } else {
        // 3D-specific methods
        if (method_str == "print-height") return TopographicConfig::ScalingMethod::PRINT_HEIGHT;
        if (method_str == "uniform-xyz") return TopographicConfig::ScalingMethod::UNIFORM_XYZ;

        // Warn about 2D-only methods
        if (method_str == "material-thickness") {
            std::cerr << "Warning: 'material-thickness' is 2D-only, using 'auto' for 3D" << std::endl;
            return TopographicConfig::ScalingMethod::AUTO;
        }
    }

    std::cerr << "Warning: Unknown scaling method '" << method_str << "', using 'auto'" << std::endl;
    return TopographicConfig::ScalingMethod::AUTO;
}

bool CommandLineInterface::geocode_query(const std::string& query) {
    GeocodeService geocoder;

    auto result = geocoder.geocode(query);

    if (!result.has_value()) {
        return false;
    }

    // Use the bounding box from the geocoding result
    config_.bounds = result->bounds;
    config_.upper_left_lat = result->bounds.max_y;
    config_.upper_left_lon = result->bounds.min_x;
    config_.lower_right_lat = result->bounds.min_y;
    config_.lower_right_lon = result->bounds.max_x;

    std::cout << "Geocoded location: " << result->display_name << "\n";
    std::cout << "Bounds: (" << result->bounds.min_x << "," << result->bounds.min_y
              << ") to (" << result->bounds.max_x << "," << result->bounds.max_y << ")\n";

    return true;
}

void CommandLineInterface::print_config() const {
    if (config_.log_level < 4) return;  // Only print at INFO level or higher

    std::cout << "\n=== Configuration ===\n";
    std::cout << "Coordinates: " << config_.upper_left_lat << "," << config_.upper_left_lon
              << " to " << config_.lower_right_lat << "," << config_.lower_right_lon << "\n";
    std::cout << "Output directory: " << config_.output_directory << "\n";
    std::cout << "Base name: " << config_.base_name << "\n";
    std::cout << "Formats: ";
    for (size_t i = 0; i < config_.output_formats.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << config_.output_formats[i];
    }
    std::cout << "\nLayers: " << config_.num_layers << "\n";
    std::cout << "Height per layer: " << config_.height_per_layer << "m\n";
    std::cout << "Layer thickness: " << config_.layer_thickness_mm << "mm\n";
    std::cout << "Substrate size: " << config_.substrate_size_mm << "mm\n";
    std::cout << "Vertical contour relief: " << (config_.vertical_contour_relief ? "yes" : "no") << "\n";
    std::cout << "Parallel processing: " << (config_.parallel_processing ? "yes" : "no") << "\n";
    std::cout << "Terrain following: " << (config_.terrain_following ? "yes" : "no") << "\n";
    std::cout << "===================\n\n";
}

} // namespace topo
