/**
 * @file TextFitter.cpp
 * @brief Implementation of adaptive text fitting algorithms
 */

#include "TextFitter.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace topo {

TextFitter::TextFitter(const Config& config)
    : config_(config) {
}

FittedText TextFitter::fit_text(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    const BoundingBox& available_bbox,
    const std::string& anchor) const {

    // First, check if text fits as-is
    if (check_fit(text, x, y, font_size_mm, 0.0, available_bbox, anchor)) {
        FittedText result;
        result.text = text;
        result.x = x;
        result.y = y;
        result.font_size_mm = font_size_mm;
        result.bend_angle_deg = 0.0;
        return result;
    }

    // Stage 1: Try bending
    auto bent_result = try_bend(text, x, y, font_size_mm, available_bbox, anchor);
    if (bent_result) {
        return *bent_result;
    }

    // Stage 2: Try scaling
    auto scaled_result = try_scale(text, x, y, font_size_mm, available_bbox, anchor);
    if (scaled_result) {
        return *scaled_result;
    }

    // Stage 3: Try splitting
    auto split_result = try_split(text, x, y, font_size_mm, available_bbox, anchor);
    if (split_result) {
        return *split_result;
    }

    // Stage 4: Truncate (last resort)
    return truncate(text, x, y, font_size_mm, available_bbox, anchor);
}

bool TextFitter::check_fit(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    double bend_angle_deg,
    const BoundingBox& available_bbox,
    const std::string& anchor) const {

    BoundingBox text_bbox = estimate_text_bbox(text, x, y, font_size_mm, bend_angle_deg, anchor);

    // Check if text bounding box fits within available bbox
    return text_bbox.min_x >= available_bbox.min_x &&
           text_bbox.max_x <= available_bbox.max_x &&
           text_bbox.min_y >= available_bbox.min_y &&
           text_bbox.max_y <= available_bbox.max_y;
}

BoundingBox TextFitter::estimate_text_bbox(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    double bend_angle_deg,
    const std::string& anchor) const {

    double text_width = estimate_text_width(text, font_size_mm);
    double text_height = font_size_mm;

    // Account for bending (bent text takes up more vertical space)
    if (bend_angle_deg > 0) {
        double bend_rad = bend_angle_deg * M_PI / 180.0;
        text_height += text_width * std::sin(bend_rad) * 0.5;  // Approximate
    }

    // Account for anchor
    double left_offset = 0.0;
    if (anchor == "middle") {
        left_offset = -text_width / 2.0;
    } else if (anchor == "end") {
        left_offset = -text_width;
    }
    // "start" anchor has no offset

    BoundingBox bbox;
    bbox.min_x = x + left_offset - config_.margin_mm;
    bbox.max_x = x + left_offset + text_width + config_.margin_mm;
    bbox.min_y = y - text_height / 2.0 - config_.margin_mm;
    bbox.max_y = y + text_height / 2.0 + config_.margin_mm;

    return bbox;
}

double TextFitter::estimate_text_width(const std::string& text, double font_size_mm) const {
    // Approximate: each character is roughly char_width_ratio * font_size wide
    return text.length() * font_size_mm * config_.char_width_ratio;
}

std::optional<FittedText> TextFitter::try_bend(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    const BoundingBox& available_bbox,
    const std::string& anchor) const {

    // Try progressively larger bend angles
    const int steps = 5;
    for (int i = 1; i <= steps; ++i) {
        double bend_angle = (config_.max_bend_angle_deg / steps) * i;

        if (check_fit(text, x, y, font_size_mm, bend_angle, available_bbox, anchor)) {
            FittedText result;
            result.text = text;
            result.x = x;
            result.y = y;
            result.font_size_mm = font_size_mm;
            result.bend_angle_deg = bend_angle;
            result.was_bent = true;
            result.warning = "Text bent " + std::to_string(static_cast<int>(bend_angle)) + "° to fit available space";
            return result;
        }
    }

    return std::nullopt;  // Bending insufficient
}

std::optional<FittedText> TextFitter::try_scale(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    const BoundingBox& available_bbox,
    const std::string& anchor) const {

    double min_size = font_size_mm * config_.min_scale_factor;
    if (min_size < config_.min_legible_size_mm) {
        return std::nullopt;  // Would be too small
    }

    // Try progressively smaller sizes
    const int steps = 10;
    double size_step = (font_size_mm - min_size) / steps;

    for (int i = 1; i <= steps; ++i) {
        double scaled_size = font_size_mm - (size_step * i);

        if (check_fit(text, x, y, scaled_size, 0.0, available_bbox, anchor)) {
            FittedText result;
            result.text = text;
            result.x = x;
            result.y = y;
            result.font_size_mm = scaled_size;
            result.was_scaled = true;

            int scale_percent = static_cast<int>((scaled_size / font_size_mm) * 100);
            result.warning = "Text scaled to " + std::to_string(scale_percent) + "% of original size to fit";
            return result;
        }
    }

    return std::nullopt;  // Scaling insufficient
}

std::optional<FittedText> TextFitter::try_split(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    const BoundingBox& available_bbox,
    const std::string& anchor) const {

    // Try splitting text into 2 or 3 parts
    for (int num_parts = 2; num_parts <= config_.max_split_parts; ++num_parts) {
        size_t chars_per_part = (text.length() + num_parts - 1) / num_parts;

        std::vector<std::string> parts;
        std::vector<std::pair<double, double>> positions;

        bool all_parts_fit = true;
        for (int i = 0; i < num_parts; ++i) {
            size_t start = i * chars_per_part;
            if (start >= text.length()) break;

            size_t end = std::min(start + chars_per_part, text.length());
            std::string part = text.substr(start, end - start);

            // Position parts vertically stacked
            double part_y = y + (i - num_parts / 2.0) * (font_size_mm * 1.2);

            if (!check_fit(part, x, part_y, font_size_mm, 0.0, available_bbox, anchor)) {
                all_parts_fit = false;
                break;
            }

            parts.push_back(part);
            positions.push_back({x, part_y});
        }

        if (all_parts_fit && !parts.empty()) {
            FittedText result;
            result.text = text;  // Keep original for reference
            result.split_parts = parts;
            result.split_positions = positions;
            result.x = x;
            result.y = y;
            result.font_size_mm = font_size_mm;
            result.was_split = true;
            result.warning = "Text split into " + std::to_string(num_parts) + " parts to fit available space";
            return result;
        }
    }

    return std::nullopt;  // Splitting insufficient
}

FittedText TextFitter::truncate(
    const std::string& text,
    double x, double y,
    double font_size_mm,
    const BoundingBox& available_bbox,
    const std::string& anchor) const {

    FittedText result;
    result.x = x;
    result.y = y;
    result.font_size_mm = font_size_mm;
    result.was_truncated = true;

    // Calculate maximum characters that fit
    double available_width = available_bbox.max_x - available_bbox.min_x - 2 * config_.margin_mm;
    double char_width = font_size_mm * config_.char_width_ratio;
    size_t max_chars = static_cast<size_t>(available_width / char_width);

    if (max_chars < 3) {
        // Can't even fit ellipsis
        result.text = "…";
        result.warning = "Text severely truncated (area too small)";
    } else if (max_chars >= text.length()) {
        // Actually fits (shouldn't happen, but handle gracefully)
        result.text = text;
        result.warning = "Text marked for truncation but actually fits";
    } else {
        // Truncate and add ellipsis
        result.text = text.substr(0, max_chars - 1) + "…";
        result.warning = "Text truncated from " + std::to_string(text.length()) +
                        " to " + std::to_string(max_chars) + " characters";
    }

    return result;
}

} // namespace topo
