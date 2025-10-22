/**
 * @file RasterAnnotator.hpp
 * @brief Draws annotations on rasterized GDAL datasets
 *
 * Provides methods for adding registration marks, borders, and labels
 * to PNG and GeoTIFF raster outputs. Works directly with GDAL datasets
 * using pixel-level operations.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include <gdal_priv.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <array>
#include <vector>

namespace topo {

/**
 * @brief Configuration for raster annotations
 */
struct AnnotationConfig {
    // Colors (RGBA 0-255)
    std::array<uint8_t, 4> stroke_color = {255, 0, 0, 255};      // Red
    std::array<uint8_t, 4> alignment_color = {0, 0, 255, 255};   // Blue
    std::array<uint8_t, 4> text_color = {0, 0, 0, 255};          // Black

    // Dimensions (in pixels)
    double mark_size_px = 50.0;           ///< Registration mark size
    double stroke_width_px = 5.0;         ///< Line stroke width (increased for visibility)
    double margin_px = 100.0;             ///< Margin from edges
    int text_size_px = 24;                ///< Text size (for backward compatibility)

    // Font configuration
    std::string font_path = "";           ///< Font file path (empty = auto-detect)
    std::string font_face = "Arial";      ///< Font face name
    double dpi = 600.0;                   ///< DPI for mm-to-pixel conversion

    // Feature toggles
    bool add_alignment_marks = true;      ///< Add corner registration marks
    bool add_border = true;               ///< Add cutting border
    bool add_elevation_labels = false;    ///< Add elevation text
};

/**
 * @brief Utility class for drawing annotations on GDAL raster datasets
 *
 * Provides methods for drawing lines, crosses, rectangles, and text on
 * raster datasets. Used to add registration marks and borders to PNG/GeoTIFF
 * exports to match SVG functionality.
 */
class RasterAnnotator {
public:
    explicit RasterAnnotator(const AnnotationConfig& config = AnnotationConfig{});
    ~RasterAnnotator();

    /**
     * @brief Add all configured annotations to a dataset
     * @param dataset Target GDAL dataset (must have 4 bands: RGBA)
     * @param elevation Optional elevation for labeling
     * @return true if successful
     */
    bool annotate_dataset(GDALDataset* dataset, double elevation = 0.0);

    /**
     * @brief Draw a line on the dataset
     * @param dataset Target GDAL dataset
     * @param x1, y1 Starting point (pixel coordinates)
     * @param x2, y2 Ending point (pixel coordinates)
     * @param color RGBA color
     * @param width Line width in pixels
     * @return true if successful
     */
    bool draw_line(GDALDataset* dataset,
                  int x1, int y1, int x2, int y2,
                  const std::array<uint8_t, 4>& color,
                  double width = 1.0);

    /**
     * @brief Draw a cross (registration mark) on the dataset
     * @param dataset Target GDAL dataset
     * @param x, y Center point (pixel coordinates)
     * @param size Cross size in pixels
     * @param color RGBA color
     * @param width Line width in pixels
     * @return true if successful
     */
    bool draw_cross(GDALDataset* dataset,
                   int x, int y, double size,
                   const std::array<uint8_t, 4>& color,
                   double width = 1.0);

    /**
     * @brief Draw a rectangle border on the dataset
     * @param dataset Target GDAL dataset
     * @param x, y Top-left corner (pixel coordinates)
     * @param width, height Rectangle dimensions in pixels
     * @param color RGBA color
     * @param stroke_width Line width in pixels
     * @return true if successful
     */
    bool draw_rectangle(GDALDataset* dataset,
                       int x, int y, int width, int height,
                       const std::array<uint8_t, 4>& color,
                       double stroke_width = 1.0);

    /**
     * @brief Draw registration marks at four corners
     * @param dataset Target GDAL dataset
     * @return true if successful
     */
    bool draw_alignment_marks(GDALDataset* dataset);

    /**
     * @brief Draw cutting border around content
     * @param dataset Target GDAL dataset
     * @return true if successful
     */
    bool draw_border(GDALDataset* dataset);

    /**
     * @brief Draw text on the dataset using FreeType
     *
     * Renders text with antialiasing and alpha blending. Font is loaded from
     * config.font_path or auto-detected from system fonts.
     *
     * @param dataset Target GDAL dataset
     * @param text Text string to render
     * @param x, y Position (pixel coordinates)
     * @param font_size_px Font size in pixels
     * @param color RGBA color
     * @param anchor Text anchor (start, middle, end)
     * @return true if successful, false if font loading fails
     */
    bool draw_text(GDALDataset* dataset,
                   const std::string& text,
                   int x, int y,
                   int font_size_px,
                   const std::array<uint8_t, 4>& color,
                   const std::string& anchor = "start");

    /**
     * @brief Draw curved text along a path with per-character rotation
     * @param dataset Target GDAL dataset
     * @param text Text to render
     * @param char_positions Position for each character
     * @param char_rotations Rotation angle for each character (degrees)
     * @param font_size_px Font size in pixels
     * @param color RGBA color
     * @return true if successful
     */
    bool draw_curved_text(GDALDataset* dataset,
                          const std::string& text,
                          const std::vector<std::pair<double, double>>& char_positions,
                          const std::vector<double>& char_rotations,
                          int font_size_px,
                          const std::array<uint8_t, 4>& color);

    /**
     * @brief Measure text width in pixels using FreeType
     * @param text Text string to measure
     * @param font_size_px Font size in pixels
     * @return Width in pixels, or -1 if font not loaded
     */
    int measure_text_width(const std::string& text, int font_size_px);

    /**
     * @brief Parse RGB hex string (e.g., "FF0000") to RGBA array
     * @param hex_color RGB hex string (6 characters)
     * @param alpha Optional alpha value (0-255), defaults to 255
     * @return RGBA array
     */
    static std::array<uint8_t, 4> parse_hex_color(const std::string& hex_color, uint8_t alpha = 255);

    /**
     * @brief Get current configuration
     */
    const AnnotationConfig& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void set_config(const AnnotationConfig& config) { config_ = config; }

private:
    AnnotationConfig config_;

    // FreeType state
    FT_Library ft_library_;
    FT_Face ft_face_;
    bool ft_initialized_;
    std::string loaded_font_path_;

    /**
     * @brief Initialize FreeType library
     * @return true if successful
     */
    bool initialize_freetype();

    /**
     * @brief Load font from path
     * @param font_path Path to font file
     * @return true if successful
     */
    bool load_font(const std::string& font_path);

    /**
     * @brief Cleanup FreeType resources
     */
    void cleanup_freetype();

    /**
     * @brief Resolve font path from preferences
     * @param preferred_path User-specified path (may be empty)
     * @param face_name Font face name
     * @return Resolved font path, or empty if not found
     */
    std::string resolve_font_path(const std::string& preferred_path,
                                   const std::string& face_name) const;

    /**
     * @brief Blend pixel with alpha
     * @param dataset Target GDAL dataset
     * @param x, y Pixel coordinates
     * @param color RGBA color
     * @param alpha Alpha value (0-255)
     */
    void blend_pixel(GDALDataset* dataset, int x, int y,
                     const std::array<uint8_t, 4>& color, uint8_t alpha);

    /**
     * @brief Set a single pixel in the dataset
     * @param dataset Target GDAL dataset
     * @param x, y Pixel coordinates
     * @param color RGBA color
     * @return true if successful
     */
    bool set_pixel(GDALDataset* dataset, int x, int y, const std::array<uint8_t, 4>& color);

    /**
     * @brief Draw a thick line using Bresenham's algorithm
     */
    void draw_line_bresenham(GDALDataset* dataset,
                            int x1, int y1, int x2, int y2,
                            const std::array<uint8_t, 4>& color,
                            double width);
};

} // namespace topo
