/**
 * @file TextFitter.hpp
 * @brief Adaptive text fitting algorithms for label placement
 *
 * Implements text bending, scaling, splitting, and truncation to fit text
 * within available geometric constraints. Ensures labels stay within visible
 * or hidden areas and don't overlap into unintended regions.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "LabelRenderer.hpp"
#include <string>
#include <vector>

namespace topo {

/**
 * @brief Text fitting result
 */
struct FittedText {
    std::string text;                    ///< Final text (possibly modified)
    double x = 0.0;                      ///< X position
    double y = 0.0;                      ///< Y position
    double font_size_mm = 0.0;           ///< Final font size
    double bend_angle_deg = 0.0;         ///< Text path bend angle (0 = straight)
    std::vector<std::string> split_parts; ///< Split parts (if split)
    std::vector<std::pair<double, double>> split_positions;  ///< Positions for split parts

    // Modification flags
    bool was_bent = false;
    bool was_scaled = false;
    bool was_split = false;
    bool was_truncated = false;

    // Warnings
    std::string warning = "";
};

/**
 * @brief Adaptive text fitting engine
 *
 * Implements a four-stage fitting algorithm:
 * 1. Bend: Curve text along available space (up to max bend angle)
 * 2. Scale: Reduce font size (down to minimum legible size)
 * 3. Split: Break text into multiple parts across available polygons
 * 4. Truncate: Cut text with ellipsis if still too large
 *
 * Each stage issues warnings describing modifications made.
 */
class TextFitter {
public:
    struct Config {
        double max_bend_angle_deg = 15.0;      ///< Maximum text bend before scaling
        double min_scale_factor = 0.5;         ///< Minimum scale before splitting (50% of original)
        int max_split_parts = 3;               ///< Maximum parts to split into
        double min_legible_size_mm = 1.5;      ///< Minimum size before truncation
        double char_width_ratio = 0.6;         ///< Approximate character width/height ratio
        double margin_mm = 0.5;                ///< Margin around text
    };

    explicit TextFitter(const Config& config);

    /**
     * @brief Fit text within a bounding box
     *
     * Attempts to fit text using progressive strategies:
     * 1. Try straight text
     * 2. If too wide, try bending
     * 3. If still too wide, try scaling
     * 4. If still too wide, try splitting
     * 5. If still too wide, truncate
     *
     * @param text Text to fit
     * @param x Desired X position
     * @param y Desired Y position
     * @param font_size_mm Desired font size
     * @param available_bbox Bounding box to fit within
     * @param anchor Text anchor (start, middle, end)
     * @return Fitted text result with modifications applied
     */
    FittedText fit_text(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        const BoundingBox& available_bbox,
        const std::string& anchor = "middle"
    ) const;

    /**
     * @brief Check if text fits within bounding box
     * @param text Text to check
     * @param x Text position X
     * @param y Text position Y
     * @param font_size_mm Font size
     * @param bend_angle_deg Bend angle (0 = straight)
     * @param available_bbox Bounding box
     * @param anchor Text anchor
     * @return True if text fits
     */
    bool check_fit(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        double bend_angle_deg,
        const BoundingBox& available_bbox,
        const std::string& anchor
    ) const;

    /**
     * @brief Estimate text bounding box
     * @param text Text string
     * @param x Position X
     * @param y Position Y
     * @param font_size_mm Font size
     * @param bend_angle_deg Bend angle
     * @param anchor Text anchor
     * @return Approximate bounding box for text
     */
    BoundingBox estimate_text_bbox(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        double bend_angle_deg,
        const std::string& anchor
    ) const;

    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const Config& config) { config_ = config; }

private:
    Config config_;

    /**
     * @brief Try fitting with bending
     * @return Fitted result, or nullopt if bending insufficient
     */
    std::optional<FittedText> try_bend(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        const BoundingBox& available_bbox,
        const std::string& anchor
    ) const;

    /**
     * @brief Try fitting with scaling
     * @return Fitted result, or nullopt if scaling insufficient
     */
    std::optional<FittedText> try_scale(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        const BoundingBox& available_bbox,
        const std::string& anchor
    ) const;

    /**
     * @brief Try fitting with splitting
     * @return Fitted result, or nullopt if splitting insufficient
     */
    std::optional<FittedText> try_split(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        const BoundingBox& available_bbox,
        const std::string& anchor
    ) const;

    /**
     * @brief Truncate text to fit
     * @return Fitted result with truncated text
     */
    FittedText truncate(
        const std::string& text,
        double x, double y,
        double font_size_mm,
        const BoundingBox& available_bbox,
        const std::string& anchor
    ) const;

    /**
     * @brief Estimate text width in mm
     * @param text Text string
     * @param font_size_mm Font size
     * @return Approximate width in mm
     */
    double estimate_text_width(const std::string& text, double font_size_mm) const;
};

} // namespace topo
