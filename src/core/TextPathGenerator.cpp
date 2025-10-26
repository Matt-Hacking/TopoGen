/**
 * @file TextPathGenerator.cpp
 * @brief Implementation of curved text path generation
 */

#include "TextPathGenerator.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <limits>

// Define M_PI if not already defined (MSVC doesn't define it by default)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace topo {

TextPathGenerator::TextPathGenerator(const TextPathConfig& config)
    : config_(config) {
}

std::optional<TextPath> TextPathGenerator::generate_path_from_polygons(
    const std::vector<ContourLayer::PolygonData>& polygons,
    double text_width,
    double font_height,
    double center_x,
    double center_y) const {

    if (polygons.empty()) {
        return std::nullopt;
    }

    // Find largest polygon by area
    int largest_idx = find_largest_polygon(polygons);
    if (largest_idx < 0) {
        return std::nullopt;
    }

    const auto& polygon = polygons[largest_idx];
    if (polygon.empty()) {
        return std::nullopt;
    }

    // Get exterior ring
    const auto& exterior = polygon.exterior();
    if (exterior.size() < 4) {  // Need at least 3 unique vertices + closing vertex
        return std::nullopt;
    }

    // Skip curved text for very simple polygons (e.g., rectangles with only 4-5 vertices)
    // These don't benefit from curved text and straight text is faster
    if (exterior.size() <= 6) {
        return std::nullopt;
    }

    // Calculate inset distance
    double inset_distance = font_height * config_.inset_ratio;

    // Inset polygon to keep text inside bounds
    auto inset_ring = inset_polygon(exterior, inset_distance);
    if (inset_ring.size() < 4) {
        // Inset collapsed polygon, try with smaller inset
        inset_distance *= 0.5;
        inset_ring = inset_polygon(exterior, inset_distance);
        if (inset_ring.size() < 4) {
            return std::nullopt;  // Still collapsed, give up
        }
    }

    // Find best segment for text
    double min_segment_length = text_width * config_.min_segment_length_ratio;
    auto [start_idx, end_idx] = extract_best_segment(inset_ring, min_segment_length, center_x, center_y);

    if (start_idx < 0 || end_idx < 0) {
        return std::nullopt;  // No suitable segment found
    }

    // Fit spline to segment
    auto control_points = fit_spline_to_segment(inset_ring, start_idx, end_idx);
    if (control_points.empty()) {
        return std::nullopt;
    }

    // Sample spline for character placement
    auto [sample_points, tangent_angles] = sample_spline(control_points, config_.spline_sample_points);

    // Calculate path length
    double path_length = calculate_path_length(sample_points);

    // Check if text can fit
    if (path_length < text_width * config_.min_path_length_ratio) {
        return std::nullopt;  // Path too short for text
    }

    // Generate SVG path
    std::string svg_path = generate_svg_path_d(control_points);

    // Build result
    TextPath result;
    result.control_points = control_points;
    result.sample_points = sample_points;
    result.tangent_angles = tangent_angles;
    result.total_length = path_length;
    result.svg_path_d = svg_path;

    return result;
}

int TextPathGenerator::find_largest_polygon(const std::vector<ContourLayer::PolygonData>& polygons) const {
    if (polygons.empty()) {
        return -1;
    }

    int largest_idx = -1;
    double largest_area = 0.0;

    for (size_t i = 0; i < polygons.size(); ++i) {
        if (polygons[i].empty()) continue;

        double area = std::abs(calculate_polygon_area(polygons[i].exterior()));
        if (area > largest_area) {
            largest_area = area;
            largest_idx = static_cast<int>(i);
        }
    }

    return largest_idx;
}

double TextPathGenerator::calculate_polygon_area(const std::vector<std::pair<double, double>>& ring) const {
    if (ring.size() < 3) {
        return 0.0;
    }

    // Shoelace formula
    double area = 0.0;
    for (size_t i = 0; i < ring.size(); ++i) {
        size_t j = (i + 1) % ring.size();
        area += ring[i].first * ring[j].second;
        area -= ring[j].first * ring[i].second;
    }

    return area * 0.5;
}

std::vector<std::pair<double, double>> TextPathGenerator::inset_polygon(
    const std::vector<std::pair<double, double>>& ring,
    double inset_distance) const {

    // Simple inset using perpendicular offset
    // For production, consider using GDAL's OGR_G_Buffer() for robustness

    if (ring.size() < 3) {
        return {};
    }

    std::vector<std::pair<double, double>> inset_ring;
    inset_ring.reserve(ring.size());

    for (size_t i = 0; i < ring.size() - 1; ++i) {  // Skip closing vertex
        // Get three consecutive vertices
        size_t prev_i = (i == 0) ? ring.size() - 2 : i - 1;
        size_t next_i = (i + 1) % (ring.size() - 1);

        const auto& prev = ring[prev_i];
        const auto& curr = ring[i];
        const auto& next = ring[next_i];

        // Calculate edge vectors
        double dx1 = curr.first - prev.first;
        double dy1 = curr.second - prev.second;
        double len1 = std::sqrt(dx1*dx1 + dy1*dy1);

        double dx2 = next.first - curr.first;
        double dy2 = next.second - curr.second;
        double len2 = std::sqrt(dx2*dx2 + dy2*dy2);

        if (len1 < 1e-9 || len2 < 1e-9) {
            continue;  // Degenerate edge
        }

        // Normalize
        dx1 /= len1;
        dy1 /= len1;
        dx2 /= len2;
        dy2 /= len2;

        // Calculate perpendicular vectors (90° CCW rotation)
        double perp1_x = -dy1;
        double perp1_y = dx1;
        double perp2_x = -dy2;
        double perp2_y = dx2;

        // Average perpendicular (bisector)
        double avg_perp_x = (perp1_x + perp2_x) / 2.0;
        double avg_perp_y = (perp1_y + perp2_y) / 2.0;
        double avg_len = std::sqrt(avg_perp_x*avg_perp_x + avg_perp_y*avg_perp_y);

        if (avg_len < 1e-9) {
            // Straight line, use perpendicular
            avg_perp_x = perp1_x;
            avg_perp_y = perp1_y;
            avg_len = 1.0;
        }

        // Normalize
        avg_perp_x /= avg_len;
        avg_perp_y /= avg_len;

        // Apply inset
        double inset_x = curr.first + avg_perp_x * inset_distance;
        double inset_y = curr.second + avg_perp_y * inset_distance;

        inset_ring.push_back({inset_x, inset_y});
    }

    // Close the ring
    if (!inset_ring.empty()) {
        inset_ring.push_back(inset_ring[0]);
    }

    return inset_ring;
}

std::pair<int, int> TextPathGenerator::extract_best_segment(
    const std::vector<std::pair<double, double>>& ring,
    double min_length,
    double center_x,
    double center_y) const {

    if (ring.size() < 4) {
        return {-1, -1};
    }

    int best_start = -1;
    int best_end = -1;
    double best_score = -1.0;

    int n = static_cast<int>(ring.size()) - 1;  // Exclude closing vertex

    // Try all possible segments
    for (int start = 0; start < n; ++start) {
        for (int len = 3; len <= n - start && len <= n / 2; ++len) {  // At least 3 vertices
            int end = start + len;
            if (end >= n) break;

            // Calculate segment length
            double seg_length = calculate_segment_length(ring, start, end);
            if (seg_length < min_length) {
                continue;  // Too short
            }

            // Check curvature
            if (!check_segment_curvature(ring, start, end)) {
                continue;  // Too curved
            }

            // Calculate distance to preferred center
            double mid_idx = (start + end) / 2.0;
            size_t mid_idx_int = static_cast<size_t>(mid_idx);
            const auto& mid_point = ring[mid_idx_int];
            double dist_to_center = std::sqrt(
                std::pow(mid_point.first - center_x, 2) +
                std::pow(mid_point.second - center_y, 2)
            );

            // Score: longer is better, closer to center is better
            double score = seg_length / (1.0 + dist_to_center * 0.1);

            if (score > best_score) {
                best_score = score;
                best_start = start;
                best_end = end;
            }
        }
    }

    return {best_start, best_end};
}

double TextPathGenerator::calculate_segment_length(
    const std::vector<std::pair<double, double>>& ring,
    int start_idx,
    int end_idx) const {

    double length = 0.0;
    for (int i = start_idx; i < end_idx; ++i) {
        const auto& p1 = ring[i];
        const auto& p2 = ring[i + 1];
        double dx = p2.first - p1.first;
        double dy = p2.second - p1.second;
        length += std::sqrt(dx*dx + dy*dy);
    }
    return length;
}

bool TextPathGenerator::check_segment_curvature(
    const std::vector<std::pair<double, double>>& ring,
    int start_idx,
    int end_idx) const {

    if (end_idx - start_idx < 2) {
        return true;  // Too short to have significant curvature
    }

    double max_angle_change = 0.0;

    for (int i = start_idx; i < end_idx - 1; ++i) {
        const auto& p0 = ring[i];
        const auto& p1 = ring[i + 1];
        const auto& p2 = ring[i + 2];

        // Calculate edge vectors
        double dx1 = p1.first - p0.first;
        double dy1 = p1.second - p0.second;
        double dx2 = p2.first - p1.first;
        double dy2 = p2.second - p1.second;

        // Calculate angles
        double angle1 = std::atan2(dy1, dx1);
        double angle2 = std::atan2(dy2, dx2);

        // Calculate angle change
        double angle_change = std::abs(angle2 - angle1);
        if (angle_change > M_PI) {
            angle_change = 2 * M_PI - angle_change;
        }

        max_angle_change = std::max(max_angle_change, angle_change);
    }

    // Convert to degrees
    double max_angle_deg = max_angle_change * 180.0 / M_PI;

    return max_angle_deg <= config_.segment_angle_threshold_deg;
}

std::vector<std::pair<double, double>> TextPathGenerator::fit_spline_to_segment(
    const std::vector<std::pair<double, double>>& ring,
    int start_idx,
    int end_idx) const {

    std::vector<std::pair<double, double>> control_points;

    // Extract segment vertices as control points
    for (int i = start_idx; i <= end_idx; ++i) {
        control_points.push_back(ring[i]);
    }

    return control_points;
}

std::pair<std::vector<std::pair<double, double>>, std::vector<double>> TextPathGenerator::sample_spline(
    const std::vector<std::pair<double, double>>& control_points,
    int num_samples) const {

    std::vector<std::pair<double, double>> sample_points;
    std::vector<double> tangent_angles;

    if (control_points.size() < 2) {
        return {sample_points, tangent_angles};
    }

    if (control_points.size() == 2) {
        // Linear interpolation
        const auto& p0 = control_points[0];
        const auto& p1 = control_points[1];

        for (int i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) / (num_samples - 1);
            double x = p0.first + t * (p1.first - p0.first);
            double y = p0.second + t * (p1.second - p0.second);
            sample_points.push_back({x, y});

            // Tangent angle
            double dx = p1.first - p0.first;
            double dy = p1.second - p0.second;
            double angle_rad = std::atan2(dy, dx);
            double angle_deg = angle_rad * 180.0 / M_PI;
            tangent_angles.push_back(angle_deg);
        }

        return {sample_points, tangent_angles};
    }

    // Catmull-Rom spline interpolation
    int num_segments = static_cast<int>(control_points.size()) - 1;
    int samples_per_segment = num_samples / num_segments;

    for (int seg = 0; seg < num_segments; ++seg) {
        // Get four control points for Catmull-Rom
        int i0 = std::max(0, seg - 1);
        int i1 = seg;
        int i2 = seg + 1;
        int i3 = std::min(static_cast<int>(control_points.size()) - 1, seg + 2);

        const auto& p0 = control_points[i0];
        const auto& p1 = control_points[i1];
        const auto& p2 = control_points[i2];
        const auto& p3 = control_points[i3];

        // Sample this segment
        int samples_this_seg = (seg == num_segments - 1) ?
            (num_samples - static_cast<int>(sample_points.size())) : samples_per_segment;

        for (int i = 0; i < samples_this_seg; ++i) {
            double t = static_cast<double>(i) / samples_per_segment;

            // Evaluate position
            auto point = evaluate_catmull_rom(p0, p1, p2, p3, t);
            sample_points.push_back(point);

            // Evaluate tangent
            auto tangent = evaluate_catmull_rom_tangent(p0, p1, p2, p3, t);
            double angle_rad = std::atan2(tangent.second, tangent.first);
            double angle_deg = angle_rad * 180.0 / M_PI;
            tangent_angles.push_back(angle_deg);
        }
    }

    return {sample_points, tangent_angles};
}

std::pair<double, double> TextPathGenerator::evaluate_catmull_rom(
    const std::pair<double, double>& p0,
    const std::pair<double, double>& p1,
    const std::pair<double, double>& p2,
    const std::pair<double, double>& p3,
    double t) const {

    double t2 = t * t;
    double t3 = t2 * t;

    // Catmull-Rom basis functions
    double b0 = -0.5 * t3 + t2 - 0.5 * t;
    double b1 = 1.5 * t3 - 2.5 * t2 + 1.0;
    double b2 = -1.5 * t3 + 2.0 * t2 + 0.5 * t;
    double b3 = 0.5 * t3 - 0.5 * t2;

    double x = b0 * p0.first + b1 * p1.first + b2 * p2.first + b3 * p3.first;
    double y = b0 * p0.second + b1 * p1.second + b2 * p2.second + b3 * p3.second;

    return {x, y};
}

std::pair<double, double> TextPathGenerator::evaluate_catmull_rom_tangent(
    const std::pair<double, double>& p0,
    const std::pair<double, double>& p1,
    const std::pair<double, double>& p2,
    const std::pair<double, double>& p3,
    double t) const {

    double t2 = t * t;

    // Derivatives of Catmull-Rom basis functions
    double db0 = -1.5 * t2 + 2.0 * t - 0.5;
    double db1 = 4.5 * t2 - 5.0 * t;
    double db2 = -4.5 * t2 + 4.0 * t + 0.5;
    double db3 = 1.5 * t2 - t;

    double dx = db0 * p0.first + db1 * p1.first + db2 * p2.first + db3 * p3.first;
    double dy = db0 * p0.second + db1 * p1.second + db2 * p2.second + db3 * p3.second;

    // Normalize
    double len = std::sqrt(dx*dx + dy*dy);
    if (len > 1e-9) {
        dx /= len;
        dy /= len;
    }

    return {dx, dy};
}

double TextPathGenerator::calculate_path_length(const std::vector<std::pair<double, double>>& points) const {
    if (points.size() < 2) {
        return 0.0;
    }

    double length = 0.0;
    for (size_t i = 1; i < points.size(); ++i) {
        double dx = points[i].first - points[i-1].first;
        double dy = points[i].second - points[i-1].second;
        length += std::sqrt(dx*dx + dy*dy);
    }

    return length;
}

std::string TextPathGenerator::generate_svg_path_d(const std::vector<std::pair<double, double>>& control_points) const {
    if (control_points.empty()) {
        return "";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    // Move to first point
    oss << "M " << control_points[0].first << " " << control_points[0].second;

    // For smooth curves, use quadratic Bezier or cubic Bezier
    // For simplicity, use linear segments (could enhance with actual Bezier control points)
    for (size_t i = 1; i < control_points.size(); ++i) {
        oss << " L " << control_points[i].first << " " << control_points[i].second;
    }

    return oss.str();
}

} // namespace topo
