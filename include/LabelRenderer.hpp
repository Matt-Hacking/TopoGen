/**
 * @file LabelRenderer.hpp
 * @brief Label rendering and pattern substitution for 2D outputs
 *
 * Handles text label generation with pattern substitution, placement logic,
 * and adaptive text fitting for SVG and raster exports.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include <string>
#include <optional>
#include <array>
#include <vector>
#include <utility>

namespace topo {

/**
 * @brief Configuration for label rendering
 */
struct LabelConfig {
    // Label text patterns (with %substitution patterns)
    std::string base_label_visible = "";    ///< Visible text on base layer
    std::string base_label_hidden = "";     ///< Hidden text on base layer (covered by layer 1)
    std::string layer_label_visible = "";   ///< Visible text on each layer
    std::string layer_label_hidden = "";    ///< Hidden text on each layer (covered by next layer)

    // Unit preferences for pattern substitution
    TopographicConfig::Units label_units = TopographicConfig::Units::METRIC;
    TopographicConfig::PrintUnits print_units = TopographicConfig::PrintUnits::MM;
    TopographicConfig::LandUnits land_units = TopographicConfig::LandUnits::METERS;

    // Visual styling
    std::string visible_label_color = "#000000";  ///< Black for visible labels
    std::string hidden_label_color = "#666666";   ///< Gray for hidden labels (will be covered anyway)
    double base_font_size_mm = 4.0;              ///< Base font size in mm
    double layer_font_size_mm = 3.0;             ///< Layer font size in mm

    // Adaptive fitting parameters
    double max_bend_angle_deg = 15.0;            ///< Maximum text bend angle before scaling
    double min_scale_factor = 0.5;               ///< Minimum scale before splitting
    int max_split_parts = 3;                     ///< Maximum parts to split text into
    double min_legible_size_mm = 1.5;            ///< Minimum size before truncation

    // Curved text path parameters
    bool enable_curved_text = true;              ///< Enable curved text following polygon contours
    double text_path_inset_ratio = 0.75;         ///< Inset distance as multiple of font height
    double min_path_length_ratio = 0.8;          ///< Minimum path length as multiple of text width
    int spline_sample_points = 50;               ///< Number of points to sample along spline
};

/**
 * @brief Render context for a specific layer
 */
struct LabelContext {
    int layer_number = 0;                        ///< Layer number (0 for base)
    double elevation_m = 0.0;                    ///< Elevation in meters
    double scale_ratio = 1.0;                    ///< Map scale (e.g., 5000 for 1:5000)
    double contour_height_m = 0.0;               ///< Height between contours
    BoundingBox geographic_bounds;               ///< Geographic extent
    double substrate_size_mm = 0.0;              ///< Substrate dimension

    // Geometry for placement
    BoundingBox content_bbox;                    ///< Visible content area (not covered)
    BoundingBox hidden_bbox;                     ///< Hidden area (will be covered)

    // Polygon geometry for curved text paths (optional, for next layer)
    // Using void* to avoid circular dependency - cast to vector<ContourLayer::PolygonData>* when using
    const void* next_layer_polygons = nullptr;
};

/**
 * @brief Placed label result
 */
struct PlacedLabel {
    std::string text;                            ///< Final text after substitution
    double x = 0.0;                              ///< X position in output coordinates
    double y = 0.0;                              ///< Y position in output coordinates
    double font_size_mm = 0.0;                   ///< Font size in millimeters
    std::string color = "#000000";               ///< Text color
    std::string anchor = "start";                ///< Text anchor: start, middle, end
    bool is_hidden = false;                      ///< True if label will be covered

    // Adaptive fitting metadata
    bool was_bent = false;                       ///< Text path was bent
    bool was_scaled = false;                     ///< Text was scaled down
    bool was_split = false;                      ///< Text was split into parts
    bool was_truncated = false;                  ///< Text was truncated
    std::string modification_warning = "";       ///< Warning message if modified

    // Curved path data (for actual curved text rendering)
    bool has_curved_path = false;                ///< True if curved path is available
    std::vector<std::pair<double, double>> path_points;        ///< Spline control points
    std::vector<std::pair<double, double>> char_positions;     ///< Per-character positions
    std::vector<double> char_rotations;                        ///< Per-character rotation angles (degrees)
    std::string svg_path_d;                                    ///< SVG path 'd' attribute for textPath
    std::string svg_path_id;                                   ///< Unique ID for SVG path element
};

/**
 * @brief Label renderer with pattern substitution and placement
 *
 * Handles conversion of label templates (e.g., "Layer %{n}: %{c} elevation")
 * to actual text with appropriate values substituted.
 *
 * Pattern substitutions supported (use %{identifier} syntax):
 * - %{s}: Scale ratio (e.g., "5000" for 1:5000 scale)
 * - %{c}: Contour height with units (e.g., "21.4m" or "70.2ft")
 * - %{n}: Layer number
 * - %{x}: Center longitude (decimal degrees)
 * - %{y}: Center latitude (decimal degrees)
 * - %{w}: Geographic area width with units
 * - %{h}: Geographic area height with units
 * - %{W}: Substrate width with units
 * - %{H}: Substrate height with units
 * - %{C}: Center coordinates (lat, lon)
 * - %{UL}, %{UR}, %{LL}, %{LR}: Corner coordinates (upper-left, upper-right, lower-left, lower-right)
 *
 * Escaping: Use %%{identifier} to produce literal %{identifier} in output.
 * Example: "%%{s}" renders as "%{s}" instead of being substituted.
 */
class LabelRenderer {
public:
    explicit LabelRenderer(const LabelConfig& config);

    /**
     * @brief Substitute patterns in label text
     * @param template_text Text with %patterns
     * @param context Rendering context for this layer
     * @return Text with patterns replaced by actual values
     */
    std::string substitute_patterns(const std::string& template_text, const LabelContext& context) const;

    /**
     * @brief Substitute filename patterns
     * @param pattern Filename pattern (e.g., "%{b}-%{l}")
     * @param basename Base name for output files
     * @param layer_number Layer number (1-indexed)
     * @param elevation_m Elevation in meters
     * @return Filename with patterns substituted (without extension)
     *
     * Filename-specific patterns:
     * - %{b}: basename
     * - %{l}: layer number (zero-padded to 2 digits)
     * - %{e}: elevation in meters (integer)
     * - %{n}: layer number (same as %{l})
     */
    static std::string substitute_filename_pattern(
        const std::string& pattern,
        const std::string& basename,
        int layer_number,
        double elevation_m);

    /**
     * @brief Generate visible label for base layer
     * @param context Rendering context
     * @return Placed label, or empty optional if no label configured
     */
    std::optional<PlacedLabel> generate_base_visible_label(const LabelContext& context) const;

    /**
     * @brief Generate hidden label for base layer
     * @param context Rendering context
     * @return Placed label, or empty optional if no label configured
     */
    std::optional<PlacedLabel> generate_base_hidden_label(const LabelContext& context) const;

    /**
     * @brief Generate visible label for layer
     * @param context Rendering context
     * @return Placed label, or empty optional if no label configured
     */
    std::optional<PlacedLabel> generate_layer_visible_label(const LabelContext& context) const;

    /**
     * @brief Generate hidden label for layer
     * @param context Rendering context
     * @return Placed label, or empty optional if no label configured
     */
    std::optional<PlacedLabel> generate_layer_hidden_label(const LabelContext& context) const;

    /**
     * @brief Get current configuration
     */
    const LabelConfig& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const LabelConfig& config) { config_ = config; }

private:
    LabelConfig config_;

    /**
     * @brief Format geographic coordinate
     * @param lat Latitude in decimal degrees
     * @param lon Longitude in decimal degrees
     * @return Formatted coordinate string
     */
    std::string format_coordinate(double lat, double lon) const;

    /**
     * @brief Convert decimal degrees to DMS format
     * @param decimal Decimal degrees
     * @return Tuple of (degrees, minutes, seconds)
     */
    std::tuple<int, int, double> to_dms(double decimal) const;

    /**
     * @brief Format distance value with appropriate units
     * @param value_meters Value in meters
     * @param is_print_units True for print units (mm/inches), false for land units (meters/feet)
     * @return Formatted string with units
     */
    std::string format_distance(double value_meters, bool is_print_units = false) const;

    /**
     * @brief Place label in visible area (corner not covered by next layer)
     * @param text Label text
     * @param context Rendering context
     * @param is_base True if base layer
     * @return Placed label
     */
    PlacedLabel place_visible_label(const std::string& text, const LabelContext& context, bool is_base) const;

    /**
     * @brief Place label in hidden area (will be covered by next layer)
     * @param text Label text
     * @param context Rendering context
     * @param is_base True if base layer
     * @return Placed label
     */
    PlacedLabel place_hidden_label(const std::string& text, const LabelContext& context, bool is_base) const;
};

} // namespace topo
