/**
 * @file RasterAnnotator.cpp
 * @brief Implementation of raster annotation drawing
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "RasterAnnotator.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace topo {

RasterAnnotator::RasterAnnotator(const AnnotationConfig& config)
    : config_(config)
    , ft_library_(nullptr)
    , ft_face_(nullptr)
    , ft_initialized_(false)
    , loaded_font_path_("") {
}

RasterAnnotator::~RasterAnnotator() {
    cleanup_freetype();
}

bool RasterAnnotator::annotate_dataset(GDALDataset* dataset, [[maybe_unused]] double elevation) {
    if (!dataset) {
        std::cerr << "RasterAnnotator: null dataset" << std::endl;
        return false;
    }

    bool success = true;

    if (config_.add_alignment_marks) {
        if (!draw_alignment_marks(dataset)) {
            std::cerr << "RasterAnnotator: failed to draw alignment marks" << std::endl;
            success = false;
        }
    }

    if (config_.add_border) {
        if (!draw_border(dataset)) {
            std::cerr << "RasterAnnotator: failed to draw border" << std::endl;
            success = false;
        }
    }

    // TODO: Implement text rendering for elevation labels
    // This requires FreeType or similar library integration
    if (config_.add_elevation_labels) {
        std::cerr << "RasterAnnotator: text rendering not yet implemented" << std::endl;
    }

    return success;
}

bool RasterAnnotator::set_pixel(GDALDataset* dataset, int x, int y, const std::array<uint8_t, 4>& color) {
    if (!dataset) return false;

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    // Bounds check
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return false;
    }

    // Write to all 4 RGBA bands
    for (int band = 1; band <= 4; ++band) {
        GDALRasterBand* raster_band = dataset->GetRasterBand(band);
        if (!raster_band) return false;

        uint8_t value = color[band - 1];
        CPLErr err = raster_band->RasterIO(
            GF_Write,
            x, y, 1, 1,  // xoff, yoff, xsize, ysize
            &value,
            1, 1,        // buffer xsize, ysize
            GDT_Byte,
            0, 0         // pixel space, line space
        );

        if (err != CE_None) {
            return false;
        }
    }

    return true;
}

void RasterAnnotator::draw_line_bresenham(
    GDALDataset* dataset,
    int x1, int y1, int x2, int y2,
    const std::array<uint8_t, 4>& color,
    double width) {

    // Bresenham's line algorithm
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int x = x1;
    int y = y1;

    int half_width = static_cast<int>(width / 2.0);

    while (true) {
        // Draw thick line by drawing a small square at each point
        for (int dy_offset = -half_width; dy_offset <= half_width; ++dy_offset) {
            for (int dx_offset = -half_width; dx_offset <= half_width; ++dx_offset) {
                set_pixel(dataset, x + dx_offset, y + dy_offset, color);
            }
        }

        if (x == x2 && y == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

bool RasterAnnotator::draw_line(
    GDALDataset* dataset,
    int x1, int y1, int x2, int y2,
    const std::array<uint8_t, 4>& color,
    double width) {

    if (!dataset) return false;

    draw_line_bresenham(dataset, x1, y1, x2, y2, color, width);
    return true;
}

bool RasterAnnotator::draw_cross(
    GDALDataset* dataset,
    int x, int y, double size,
    const std::array<uint8_t, 4>& color,
    double width) {

    if (!dataset) return false;

    int half_size = static_cast<int>(size / 2.0);

    // Horizontal line
    draw_line(dataset, x - half_size, y, x + half_size, y, color, width);

    // Vertical line
    draw_line(dataset, x, y - half_size, x, y + half_size, color, width);

    return true;
}

bool RasterAnnotator::draw_rectangle(
    GDALDataset* dataset,
    int x, int y, int width, int height,
    const std::array<uint8_t, 4>& color,
    double stroke_width) {

    if (!dataset) return false;

    // Top edge
    draw_line(dataset, x, y, x + width, y, color, stroke_width);

    // Bottom edge
    draw_line(dataset, x, y + height, x + width, y + height, color, stroke_width);

    // Left edge
    draw_line(dataset, x, y, x, y + height, color, stroke_width);

    // Right edge
    draw_line(dataset, x + width, y, x + width, y + height, color, stroke_width);

    return true;
}

bool RasterAnnotator::draw_alignment_marks(GDALDataset* dataset) {
    if (!dataset) return false;

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    // Place marks at margin/2 to match SVG behavior
    // SVG places marks at margin_mm/2, so we follow the same pattern
    int mark_offset = static_cast<int>(config_.margin_px / 2.0);
    int mark_size = static_cast<int>(config_.mark_size_px);

    // Four corner positions (matching SVG placement at margin/2 from edges)
    std::vector<std::pair<int, int>> corners = {
        {mark_offset, mark_offset},                     // top-left
        {width - mark_offset, mark_offset},             // top-right
        {width - mark_offset, height - mark_offset},    // bottom-right
        {mark_offset, height - mark_offset}             // bottom-left
    };

    for (const auto& [cx, cy] : corners) {
        draw_cross(dataset, cx, cy, mark_size, config_.alignment_color, config_.stroke_width_px);
    }

    return true;
}

bool RasterAnnotator::draw_border(GDALDataset* dataset) {
    if (!dataset) return false;

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    int margin = static_cast<int>(config_.margin_px);

    // Draw rectangle border with margin using alignment color (matches SVG frame)
    // Terrain outline uses stroke_color (for laser cutting), border uses alignment_color (for registration)
    draw_rectangle(
        dataset,
        margin, margin,
        width - 2 * margin, height - 2 * margin,
        config_.alignment_color,
        config_.stroke_width_px
    );

    return true;
}

// FreeType initialization and management

bool RasterAnnotator::initialize_freetype() {
    if (ft_initialized_) {
        return true;  // Already initialized
    }

    FT_Error error = FT_Init_FreeType(&ft_library_);
    if (error) {
        std::cerr << "RasterAnnotator: Failed to initialize FreeType library (error " << error << ")" << std::endl;
        return false;
    }

    ft_initialized_ = true;
    return true;
}

bool RasterAnnotator::load_font(const std::string& font_path) {
    if (!ft_initialized_ && !initialize_freetype()) {
        return false;
    }

    // If this font is already loaded, skip
    if (ft_face_ != nullptr && loaded_font_path_ == font_path) {
        return true;
    }

    // Cleanup old font if any
    if (ft_face_ != nullptr) {
        FT_Done_Face(ft_face_);
        ft_face_ = nullptr;
    }

    FT_Error error = FT_New_Face(ft_library_, font_path.c_str(), 0, &ft_face_);
    if (error) {
        std::cerr << "RasterAnnotator: Failed to load font from " << font_path << " (error " << error << ")" << std::endl;
        return false;
    }

    loaded_font_path_ = font_path;
    return true;
}

void RasterAnnotator::cleanup_freetype() {
    if (ft_face_ != nullptr) {
        FT_Done_Face(ft_face_);
        ft_face_ = nullptr;
    }

    if (ft_initialized_) {
        FT_Done_FreeType(ft_library_);
        ft_library_ = nullptr;
        ft_initialized_ = false;
    }

    loaded_font_path_.clear();
}

std::string RasterAnnotator::resolve_font_path(const std::string& preferred_path,
                                                [[maybe_unused]] const std::string& face_name) const {
    // If user specified a path, try it first
    if (!preferred_path.empty()) {
        if (std::filesystem::exists(preferred_path)) {
            return preferred_path;
        } else {
            std::cerr << "WARNING: Specified font not found: " << preferred_path << std::endl;
        }
    }

    // Try system fonts for macOS
    std::vector<std::string> font_search_paths;

    // Arial (preferred)
    font_search_paths.push_back("/System/Library/Fonts/Supplemental/Arial.ttf");
    font_search_paths.push_back("/System/Library/Fonts/Supplemental/Arial Unicode.ttf");

    // Helvetica (fallback)
    font_search_paths.push_back("/System/Library/Fonts/Helvetica.ttc");

    // San Francisco (macOS system font)
    font_search_paths.push_back("/System/Library/Fonts/SFNS.ttf");

    // Check each path
    for (const auto& path : font_search_paths) {
        if (std::filesystem::exists(path)) {
            if (!preferred_path.empty()) {
                std::cerr << "WARNING: Using system font " << path << " instead of " << preferred_path << std::endl;
            }
            return path;
        }
    }

    std::cerr << "ERROR: No suitable font found. Tried:" << std::endl;
    for (const auto& path : font_search_paths) {
        std::cerr << "  - " << path << std::endl;
    }

    return "";
}

void RasterAnnotator::blend_pixel(GDALDataset* dataset, int x, int y,
                                   const std::array<uint8_t, 4>& color, uint8_t alpha) {
    if (!dataset) return;

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    // Bounds check
    if (x < 0 || x >= width || y < 0 || y >= height) {
        return;
    }

    // Read existing pixel
    std::array<uint8_t, 4> bg = {0, 0, 0, 0};
    if (dataset->GetRasterCount() >= 3) {
        CPLErr err1 = dataset->GetRasterBand(1)->RasterIO(GF_Read, x, y, 1, 1, &bg[0], 1, 1, GDT_Byte, 0, 0);
        CPLErr err2 = dataset->GetRasterBand(2)->RasterIO(GF_Read, x, y, 1, 1, &bg[1], 1, 1, GDT_Byte, 0, 0);
        CPLErr err3 = dataset->GetRasterBand(3)->RasterIO(GF_Read, x, y, 1, 1, &bg[2], 1, 1, GDT_Byte, 0, 0);
        if (err1 != CE_None || err2 != CE_None || err3 != CE_None) {
            // Failed to read background, use default
            bg = {0, 0, 0, 0};
        }
        if (dataset->GetRasterCount() >= 4) {
            CPLErr err4 = dataset->GetRasterBand(4)->RasterIO(GF_Read, x, y, 1, 1, &bg[3], 1, 1, GDT_Byte, 0, 0);
            if (err4 != CE_None) {
                bg[3] = 255;
            }
        } else {
            bg[3] = 255;
        }
    }

    // Alpha blend: result = (fg * alpha) + (bg * (1 - alpha))
    float a = alpha / 255.0f;
    std::array<uint8_t, 4> result = {
        static_cast<uint8_t>(color[0] * a + bg[0] * (1.0f - a)),
        static_cast<uint8_t>(color[1] * a + bg[1] * (1.0f - a)),
        static_cast<uint8_t>(color[2] * a + bg[2] * (1.0f - a)),
        255
    };

    // Write blended pixel
    set_pixel(dataset, x, y, result);
}

bool RasterAnnotator::draw_text(
    GDALDataset* dataset,
    const std::string& text,
    int x, int y,
    int font_size_px,
    const std::array<uint8_t, 4>& color,
    const std::string& anchor) {

    if (!dataset) return false;
    if (text.empty()) return true;

    // Resolve and load font
    std::string font_path = resolve_font_path(config_.font_path, config_.font_face);
    if (font_path.empty()) {
        std::cerr << "RasterAnnotator: No font available for text rendering" << std::endl;
        return false;
    }

    if (!load_font(font_path)) {
        return false;
    }

    // Set font size in pixels
    FT_Error error = FT_Set_Pixel_Sizes(ft_face_, 0, font_size_px);
    if (error) {
        std::cerr << "RasterAnnotator: Failed to set font size (error " << error << ")" << std::endl;
        return false;
    }

    // Measure text width for anchor positioning
    int total_width = 0;
    for (char c : text) {
        error = FT_Load_Char(ft_face_, c, FT_LOAD_DEFAULT);
        if (error) continue;
        total_width += ft_face_->glyph->advance.x >> 6; // Convert from 26.6 fixed-point
    }

    // Adjust starting x based on anchor
    int pen_x = x;
    if (anchor == "middle") {
        pen_x = x - total_width / 2;
    } else if (anchor == "end") {
        pen_x = x - total_width;
    }

    // Render each character
    int pen_y = y;
    for (char c : text) {
        // Load glyph with rendering
        error = FT_Load_Char(ft_face_, c, FT_LOAD_RENDER);
        if (error) {
            std::cerr << "RasterAnnotator: Failed to load glyph for '" << c << "' (error " << error << ")" << std::endl;
            continue;
        }

        FT_GlyphSlot glyph = ft_face_->glyph;
        FT_Bitmap& bitmap = glyph->bitmap;

        // Composite glyph bitmap onto raster
        int glyph_x = pen_x + glyph->bitmap_left;
        int glyph_y = pen_y - glyph->bitmap_top; // FreeType uses baseline coordinates

        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            for (unsigned int col = 0; col < bitmap.width; ++col) {
                int pixel_x = glyph_x + col;
                int pixel_y = glyph_y + row;

                // Get alpha value from FreeType bitmap
                uint8_t alpha = bitmap.buffer[row * bitmap.pitch + col];

                if (alpha > 0) {
                    blend_pixel(dataset, pixel_x, pixel_y, color, alpha);
                }
            }
        }

        // Advance pen position
        pen_x += glyph->advance.x >> 6; // Convert from 26.6 fixed-point
    }

    return true;
}

bool RasterAnnotator::draw_curved_text(
    GDALDataset* dataset,
    const std::string& text,
    const std::vector<std::pair<double, double>>& char_positions,
    const std::vector<double>& char_rotations,
    int font_size_px,
    const std::array<uint8_t, 4>& color) {

    if (!dataset) return false;
    if (text.empty()) return true;
    if (char_positions.empty()) return false;

    // Resolve and load font
    std::string font_path = resolve_font_path(config_.font_path, config_.font_face);
    if (font_path.empty()) {
        std::cerr << "RasterAnnotator: No font available for curved text rendering" << std::endl;
        return false;
    }

    if (!load_font(font_path)) {
        return false;
    }

    // Set font size in pixels
    FT_Error error = FT_Set_Pixel_Sizes(ft_face_, 0, font_size_px);
    if (error) {
        std::cerr << "RasterAnnotator: Failed to set font size (error " << error << ")" << std::endl;
        return false;
    }

    // Calculate character spacing along path
    size_t num_chars = std::min(text.length(), char_positions.size());
    size_t positions_per_char = char_positions.size() / std::max(size_t(1), num_chars);

    // Render each character at its position
    for (size_t i = 0; i < num_chars; ++i) {
        char c = text[i];

        // Get position for this character (sample from path)
        size_t pos_idx = i * positions_per_char;
        if (pos_idx >= char_positions.size()) {
            pos_idx = char_positions.size() - 1;
        }

        int char_x = static_cast<int>(char_positions[pos_idx].first);
        int char_y = static_cast<int>(char_positions[pos_idx].second);

        // Get rotation angle if available (TODO: implement glyph rotation)
        [[maybe_unused]] double rotation_deg = 0.0;
        if (pos_idx < char_rotations.size()) {
            rotation_deg = char_rotations[pos_idx];
        }

        // Load glyph with rendering
        error = FT_Load_Char(ft_face_, c, FT_LOAD_RENDER);
        if (error) {
            std::cerr << "RasterAnnotator: Failed to load glyph for '" << c << "' (error " << error << ")" << std::endl;
            continue;
        }

        FT_GlyphSlot glyph = ft_face_->glyph;
        FT_Bitmap& bitmap = glyph->bitmap;

        // For now, render without rotation (rotation would require glyph transformation)
        // Just position each character along the path
        int glyph_x = char_x - bitmap.width / 2;  // Center character on path point
        int glyph_y = char_y - glyph->bitmap_top;

        // Composite glyph bitmap onto raster
        for (unsigned int row = 0; row < bitmap.rows; ++row) {
            for (unsigned int col = 0; col < bitmap.width; ++col) {
                int pixel_x = glyph_x + col;
                int pixel_y = glyph_y + row;

                // Get alpha value from FreeType bitmap
                uint8_t alpha = bitmap.buffer[row * bitmap.pitch + col];

                if (alpha > 0) {
                    blend_pixel(dataset, pixel_x, pixel_y, color, alpha);
                }
            }
        }
    }

    return true;
}

int RasterAnnotator::measure_text_width(const std::string& text, int font_size_px) {
    if (text.empty()) return 0;

    // Resolve and load font
    std::string font_path = resolve_font_path(config_.font_path, config_.font_face);
    if (font_path.empty()) {
        return -1;  // No font available
    }

    if (!load_font(font_path)) {
        return -1;  // Failed to load font
    }

    // Set font size in pixels
    FT_Error error = FT_Set_Pixel_Sizes(ft_face_, 0, font_size_px);
    if (error) {
        return -1;  // Failed to set size
    }

    // Measure text width
    int total_width = 0;
    for (char c : text) {
        error = FT_Load_Char(ft_face_, c, FT_LOAD_DEFAULT);
        if (error) continue;
        total_width += ft_face_->glyph->advance.x >> 6; // Convert from 26.6 fixed-point
    }

    return total_width;
}

std::array<uint8_t, 4> RasterAnnotator::parse_hex_color(const std::string& hex_color, uint8_t alpha) {
    // Strip leading '#' if present
    std::string color = hex_color;
    if (!color.empty() && color[0] == '#') {
        color = color.substr(1);
    }

    // Expected format: "RRGGBB" (6 characters)
    if (color.length() != 6) {
        std::cerr << "RasterAnnotator::parse_hex_color: invalid hex color length: " << hex_color << std::endl;
        return {255, 0, 0, 255}; // Default to red
    }

    try {
        std::string r_str = color.substr(0, 2);
        std::string g_str = color.substr(2, 2);
        std::string b_str = color.substr(4, 2);

        uint8_t r = static_cast<uint8_t>(std::stoi(r_str, nullptr, 16));
        uint8_t g = static_cast<uint8_t>(std::stoi(g_str, nullptr, 16));
        uint8_t b = static_cast<uint8_t>(std::stoi(b_str, nullptr, 16));

        return {r, g, b, alpha};
    } catch (const std::exception& e) {
        std::cerr << "RasterAnnotator::parse_hex_color: failed to parse '" << hex_color << "': " << e.what() << std::endl;
        return {255, 0, 0, 255}; // Default to red on error
    }
}

} // namespace topo
