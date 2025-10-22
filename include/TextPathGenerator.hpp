/**
 * @file TextPathGenerator.hpp
 * @brief Generate curved text paths from polygon contours
 *
 * Creates smooth spline paths that follow polygon contours for curved text rendering.
 * Supports automatic segment selection, polygon insetting, and path sampling for
 * character placement along curves.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "ContourGenerator.hpp"
#include <vector>
#include <string>
#include <optional>

namespace topo {

/**
 * @brief Spline path for curved text rendering
 */
struct TextPath {
    std::vector<std::pair<double, double>> control_points;  ///< Spline control points
    std::vector<std::pair<double, double>> sample_points;   ///< Evenly-spaced sample points
    std::vector<double> tangent_angles;                     ///< Rotation angles at each sample point (degrees)
    double total_length = 0.0;                              ///< Total path length
    std::string svg_path_d;                                 ///< SVG path 'd' attribute

    bool empty() const { return control_points.empty(); }
};

/**
 * @brief Configuration for text path generation
 */
struct TextPathConfig {
    double inset_ratio = 0.75;                  ///< Inset distance as multiple of font height
    double min_path_length_ratio = 0.8;         ///< Minimum path length as multiple of text width
    int spline_sample_points = 50;              ///< Number of points to sample along spline
    double segment_angle_threshold_deg = 90.0;  ///< Max angle change for segment continuity (increased for irregular polygons)
    double min_segment_length_ratio = 0.6;      ///< Minimum segment length as ratio of text width
};

/**
 * @brief Generate curved text paths from polygon contours
 *
 * This class takes polygon geometry and produces smooth spline paths suitable
 * for curved text rendering. It handles:
 * - Finding the largest polygon by area
 * - Insetting polygons to keep text inside bounds
 * - Selecting the best contour segment for text
 * - Fitting smooth splines to contour vertices
 * - Sampling paths for character placement
 */
class TextPathGenerator {
public:
    explicit TextPathGenerator(const TextPathConfig& config = TextPathConfig());

    /**
     * @brief Generate curved text path from polygon layer
     * @param polygons Vector of polygons (typically from next layer)
     * @param text_width Estimated text width
     * @param font_height Font height for inset calculation
     * @param center_x Preferred horizontal center position
     * @param center_y Preferred vertical center position
     * @return TextPath if generation succeeded, empty optional otherwise
     */
    std::optional<TextPath> generate_path_from_polygons(
        const std::vector<ContourLayer::PolygonData>& polygons,
        double text_width,
        double font_height,
        double center_x,
        double center_y
    ) const;

    /**
     * @brief Find largest polygon by area
     * @param polygons Vector of polygons
     * @return Index of largest polygon, or -1 if none found
     */
    int find_largest_polygon(const std::vector<ContourLayer::PolygonData>& polygons) const;

    /**
     * @brief Calculate polygon area
     * @param ring Polygon ring (closed loop of vertices)
     * @return Signed area (positive for CCW, negative for CW)
     */
    double calculate_polygon_area(const std::vector<std::pair<double, double>>& ring) const;

    /**
     * @brief Inset (offset) a polygon ring
     * @param ring Original polygon ring
     * @param inset_distance Distance to inset (negative = shrink)
     * @return Inset polygon ring, or empty if collapsed
     */
    std::vector<std::pair<double, double>> inset_polygon(
        const std::vector<std::pair<double, double>>& ring,
        double inset_distance
    ) const;

    /**
     * @brief Extract longest suitable segment from polygon
     * @param ring Polygon ring vertices
     * @param min_length Minimum segment length
     * @param center_x Preferred horizontal center
     * @param center_y Preferred vertical center
     * @return Start and end indices of best segment
     */
    std::pair<int, int> extract_best_segment(
        const std::vector<std::pair<double, double>>& ring,
        double min_length,
        double center_x,
        double center_y
    ) const;

    /**
     * @brief Fit Catmull-Rom spline to polygon segment
     * @param ring Polygon ring
     * @param start_idx Start vertex index
     * @param end_idx End vertex index
     * @return Spline control points
     */
    std::vector<std::pair<double, double>> fit_spline_to_segment(
        const std::vector<std::pair<double, double>>& ring,
        int start_idx,
        int end_idx
    ) const;

    /**
     * @brief Sample points evenly along spline path
     * @param control_points Spline control points
     * @param num_samples Number of points to sample
     * @return Sampled points and tangent angles
     */
    std::pair<std::vector<std::pair<double, double>>, std::vector<double>> sample_spline(
        const std::vector<std::pair<double, double>>& control_points,
        int num_samples
    ) const;

    /**
     * @brief Calculate total path length
     * @param points Path points
     * @return Total length
     */
    double calculate_path_length(const std::vector<std::pair<double, double>>& points) const;

    /**
     * @brief Generate SVG path 'd' attribute from control points
     * @param control_points Spline control points
     * @return SVG path string
     */
    std::string generate_svg_path_d(const std::vector<std::pair<double, double>>& control_points) const;

    /**
     * @brief Get current configuration
     */
    const TextPathConfig& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const TextPathConfig& config) { config_ = config; }

private:
    TextPathConfig config_;

    /**
     * @brief Evaluate Catmull-Rom spline at parameter t
     * @param p0, p1, p2, p3 Four consecutive control points
     * @param t Parameter in [0, 1]
     * @return Interpolated point
     */
    std::pair<double, double> evaluate_catmull_rom(
        const std::pair<double, double>& p0,
        const std::pair<double, double>& p1,
        const std::pair<double, double>& p2,
        const std::pair<double, double>& p3,
        double t
    ) const;

    /**
     * @brief Calculate tangent vector at spline point
     * @param p0, p1, p2, p3 Four consecutive control points
     * @param t Parameter in [0, 1]
     * @return Tangent vector (normalized)
     */
    std::pair<double, double> evaluate_catmull_rom_tangent(
        const std::pair<double, double>& p0,
        const std::pair<double, double>& p1,
        const std::pair<double, double>& p2,
        const std::pair<double, double>& p3,
        double t
    ) const;

    /**
     * @brief Calculate segment length along polygon
     * @param ring Polygon ring
     * @param start_idx Start index
     * @param end_idx End index
     * @return Total segment length
     */
    double calculate_segment_length(
        const std::vector<std::pair<double, double>>& ring,
        int start_idx,
        int end_idx
    ) const;

    /**
     * @brief Check if segment has acceptable curvature
     * @param ring Polygon ring
     * @param start_idx Start index
     * @param end_idx End index
     * @return True if curvature is acceptable for text
     */
    bool check_segment_curvature(
        const std::vector<std::pair<double, double>>& ring,
        int start_idx,
        int end_idx
    ) const;
};

} // namespace topo
