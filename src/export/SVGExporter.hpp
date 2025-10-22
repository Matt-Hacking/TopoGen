/**
 * @file SVGExporter.hpp
 * @brief SVG export for 2D laser-cutting ready topographic layers
 *
 * Port of Python svg_export.py functionality
 * Generates laser-cutter optimized SVG files from contour polygons
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "../core/ContourGenerator.hpp"
#include "../core/Logger.hpp"
#include "MultiFormatExporter.hpp"  // For ColorMapper
#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace topo {

/**
 * @brief SVG export configuration
 */
struct SVGConfig {
    // Document settings
    double width_mm = 300.0;           ///< SVG width in millimeters
    double height_mm = 200.0;          ///< SVG height in millimeters
    double margin_mm = 10.0;           ///< Margin around content

    // Laser cutting settings
    double cut_stroke_width = 0.1;     ///< Cut line stroke width
    double alignment_stroke_width = 0.2; ///< Alignment mark stroke width
    std::string cut_color = "#FF0000";  ///< Cut line color (red)
    std::string stroke_color = "#FF0000"; ///< Stroke/border color (red)
    std::string background_color = "#FFFFFF"; ///< Background color (white)
    std::string alignment_color = "#0000FF"; ///< Alignment mark color (blue)
    std::string text_color = "#000000"; ///< Text color (black)

    // Layer organization
    bool separate_layers = true;        ///< Create separate SVG layer for each elevation
    bool force_all_layers = false;     ///< Include empty layers for consistent count
    bool include_alignment_marks = true; ///< Add corner alignment marks
    bool include_elevation_labels = true; ///< Add elevation text labels
    bool include_cut_guidelines = true; ///< Add cutting guide lines

    // Content optimization
    double simplify_tolerance = 0.1;    ///< Simplify polygons (mm)
    double min_feature_size = 1.0;     ///< Minimum feature size (mm)
    bool remove_holes = false;         ///< Remove interior holes for simpler cutting

    // Scaling and positioning
    bool auto_scale = true;            ///< Automatically scale to fit page
    double scale_factor = 1.0;         ///< Manual scale factor if auto_scale=false
    bool center_content = true;        ///< Center content on page

    // Output options
    std::string base_filename = "topo"; ///< Base filename for SVG files
    std::string filename_pattern = "";  ///< Filename pattern with %{b}, %{l}, etc. (empty = use default)
    std::string output_directory = "."; ///< Output directory
    bool verbose = false;              ///< Verbose logging

    // Render mode and color scheme
    TopographicConfig::RenderMode render_mode = TopographicConfig::RenderMode::FULL_COLOR; ///< Rendering style
    TopographicConfig::ColorScheme color_scheme = TopographicConfig::ColorScheme::TERRAIN; ///< Color scheme for fills

    // Label configuration (with pattern substitution)
    std::string base_label_visible = "";    ///< Visible label on base layer
    std::string base_label_hidden = "";     ///< Hidden label on base layer
    std::string layer_label_visible = "";   ///< Visible label on each layer
    std::string layer_label_hidden = "";    ///< Hidden label on each layer

    // Label styling
    std::string visible_label_color = "#000000";  ///< Color for visible labels
    std::string hidden_label_color = "#666666";   ///< Color for hidden labels
    double base_font_size_mm = 4.0;              ///< Font size for base labels
    double layer_font_size_mm = 3.0;             ///< Font size for layer labels

    // Geographic and scale info for label substitution
    BoundingBox geographic_bounds;       ///< Geographic extent for coordinate patterns
    double scale_ratio = 1.0;            ///< Map scale for %s pattern
    double contour_height_m = 0.0;       ///< Contour height for %c pattern
    double substrate_size_mm = 0.0;      ///< Substrate size for %sub patterns

    // Unit preferences for label substitution
    TopographicConfig::Units label_units = TopographicConfig::Units::METRIC;
    TopographicConfig::PrintUnits print_units = TopographicConfig::PrintUnits::MM;
    TopographicConfig::LandUnits land_units = TopographicConfig::LandUnits::METERS;
};

/**
 * @brief Single SVG layer representation
 */
struct SVGLayer {
    std::string layer_id;              ///< Unique layer identifier
    double elevation;                  ///< Elevation level in meters
    std::vector<ContourLayer::PolygonData> polygons; ///< Polygons for this layer (GDAL/OGR format)
    std::string fill_color = "none";   ///< Fill color
    std::string stroke_color = "#FF0000"; ///< Stroke color
    double stroke_width = 0.1;         ///< Stroke width in mm
    bool visible = true;               ///< Layer visibility

    SVGLayer(const std::string& id, double elev)
        : layer_id(id), elevation(elev) {}
};

/**
 * @brief SVG exporter for topographic layers
 * 
 * Generates laser-cutting optimized SVG files from contour polygons.
 * Supports both individual layer files and combined multi-layer files.
 */
class SVGExporter {
public:
    /**
     * @brief Constructor
     * @param config SVG export configuration
     */
    explicit SVGExporter(const SVGConfig& config = SVGConfig{});
    
    /**
     * @brief Export contour layers to SVG files
     *
     * @param layers Contour layers to export
     * @param individual_files If true, create separate file for each layer
     * @param global_min_elev Optional minimum elevation across all layers (for consistent color mapping)
     * @param global_max_elev Optional maximum elevation across all layers (for consistent color mapping)
     * @param geographic_bounds Optional geographic bounding box for consistent coordinate transformation
     * @return Vector of generated SVG file paths
     */
    std::vector<std::string> export_layers(
        const std::vector<ContourLayer>& layers,
        bool individual_files = true,
        double global_min_elev = std::numeric_limits<double>::quiet_NaN(),
        double global_max_elev = std::numeric_limits<double>::quiet_NaN(),
        const BoundingBox* geographic_bounds = nullptr
    );
    
    /**
     * @brief Export single layer to SVG file
     *
     * @param layer Single contour layer
     * @param filename Output filename (without extension)
     * @param global_min_elev Minimum elevation across all layers (for color mapping)
     * @param global_max_elev Maximum elevation across all layers (for color mapping)
     * @param geographic_bounds Optional geographic bounding box for consistent coordinate transformation
     * @param layer_index Index into svg_layers for current layer (0 for current, -1 for base)
     * @param next_layer Optional next layer for hidden label placement (nullptr if this is the top layer)
     * @param global_layer_number Actual 1-based layer number for label substitution (1, 2, 3, ...)
     * @return Path to generated SVG file
     */
    std::string export_single_layer(
        const ContourLayer& layer,
        const std::string& filename,
        double global_min_elev,
        double global_max_elev,
        const BoundingBox* geographic_bounds = nullptr,
        int layer_index = -1,
        const ContourLayer* next_layer = nullptr,
        int global_layer_number = -1
    );
    
    /**
     * @brief Export multiple layers to combined SVG file
     *
     * @param layers Multiple contour layers
     * @param filename Output filename (without extension)
     * @param geographic_bounds Optional geographic bounding box for consistent coordinate transformation
     * @return Path to generated SVG file
     */
    std::string export_combined_layers(
        const std::vector<ContourLayer>& layers,
        const std::string& filename,
        const BoundingBox* geographic_bounds = nullptr
    );
    
    /**
     * @brief Get current configuration
     */
    const SVGConfig& get_config() const { return config_; }
    
    /**
     * @brief Update configuration  
     */
    void set_config(const SVGConfig& config) { config_ = config; }

private:
    SVGConfig config_;
    mutable Logger logger_;  // mutable for const methods

    /**
     * @brief Calculate bounding box of all polygons
     */
    BoundingBox calculate_bounding_box(const std::vector<ContourLayer>& layers) const;

    /**
     * @brief Calculate bounding box from SVG-transformed polygon coordinates (in mm)
     */
    BoundingBox calculate_svg_polygon_bbox(const std::vector<ContourLayer::PolygonData>& polygons) const;

    /**
     * @brief Calculate optimal scale factor to fit content
     */
    double calculate_scale_factor(const BoundingBox& bbox) const;

    /**
     * @brief Calculate optimal scale factor to fit content with custom canvas dimensions
     */
    double calculate_scale_factor(const BoundingBox& bbox, double canvas_width_mm, double canvas_height_mm) const;
    
    /**
     * @brief Transform polygon coordinates for SVG coordinate system
     */
    void transform_coordinates(
        std::vector<SVGLayer>& svg_layers,
        const BoundingBox& bbox,
        double scale_factor,
        double actual_width_mm,
        double actual_height_mm
    ) const;
    
    /**
     * @brief Simplify polygons for laser cutting
     */
    void simplify_polygons(std::vector<SVGLayer>& svg_layers) const;
    
    /**
     * @brief Write SVG file header
     */
    void write_svg_header(std::ofstream& file) const;

    /**
     * @brief Write SVG file header with custom canvas dimensions
     */
    void write_svg_header(std::ofstream& file, double width_mm, double height_mm) const;
    
    /**
     * @brief Write SVG file footer
     */
    void write_svg_footer(std::ofstream& file) const;

    /**
     * @brief Write background rectangle
     */
    void write_background(std::ofstream& file) const;

    /**
     * @brief Write background rectangle with custom dimensions
     */
    void write_background(std::ofstream& file, double width_mm, double height_mm) const;

    /**
     * @brief Write layer definitions
     */
    void write_layer_definitions(std::ofstream& file) const;
    
    /**
     * @brief Write polygon to SVG
     */
    void write_polygon(
        std::ofstream& file,
        const ContourLayer::PolygonData& polygon,
        const std::string& layer_id,
        const std::string& stroke_color,
        double stroke_width,
        double elevation = 0.0,
        double min_elevation = 0.0,
        double max_elevation = 1000.0
    ) const;

    /**
     * @brief Write alignment marks
     */
    void write_alignment_marks(std::ofstream& file) const;

    /**
     * @brief Write alignment marks with custom canvas dimensions
     */
    void write_alignment_marks(std::ofstream& file, double width_mm, double height_mm) const;

    /**
     * @brief Write elevation labels (legacy simple labels)
     */
    void write_elevation_labels(
        std::ofstream& file,
        const std::vector<SVGLayer>& svg_layers
    ) const;

    /**
     * @brief Write comprehensive labels using LabelRenderer
     * @param file SVG file stream
     * @param svg_layers All SVG layers
     * @param layer_index Current layer index (-1 for base layer)
     */
    void write_comprehensive_labels(
        std::ofstream& file,
        const std::vector<SVGLayer>& svg_layers,
        int layer_index
    ) const;

    /**
     * @brief Write comprehensive labels with custom canvas dimensions
     * @param file SVG file stream
     * @param svg_layers All SVG layers
     * @param layer_index Index into svg_layers for current layer (0 for current, -1 for base)
     * @param width_mm Custom canvas width in millimeters
     * @param height_mm Custom canvas height in millimeters
     * @param global_layer_number Actual layer number for label substitution (0 for base, 1+ for layers)
     */
    void write_comprehensive_labels(
        std::ofstream& file,
        const std::vector<SVGLayer>& svg_layers,
        int layer_index,
        double width_mm,
        double height_mm,
        int global_layer_number = -1
    ) const;

    /**
     * @brief Write cutting guidelines
     */
    void write_cutting_guidelines(std::ofstream& file) const;

    /**
     * @brief Write cutting guidelines with custom canvas dimensions
     */
    void write_cutting_guidelines(std::ofstream& file, double width_mm, double height_mm) const;

    /**
     * @brief Format SVG path data for polygon using coordinate rings
     */
    std::string polygon_to_path_data(const ContourLayer::PolygonData& polygon) const;

    /**
     * @brief Get fill color for elevation using ColorMapper
     * @return SVG color string (e.g., "#FF8800")
     */
    std::string get_fill_color(double elevation, double min_elevation, double max_elevation) const;

    /**
     * @brief Generate unique layer ID
     */
    std::string generate_layer_id(double elevation, int index) const;
    
    /**
     * @brief Ensure output directory exists
     */
    void ensure_output_directory() const;
    
    /**
     * @brief Get full output path for filename
     */
    std::string get_output_path(const std::string& filename) const;
};

} // namespace topo