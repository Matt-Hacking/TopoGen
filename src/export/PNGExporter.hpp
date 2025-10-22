/**
 * @file PNGExporter.hpp
 * @brief PNG raster export for topographic data
 *
 * Exports topographic contour data as rasterized PNG images with
 * elevation-based coloring using GDAL for raster operations.
 * Uses RasterBuilder for proper polygon rasterization.
 */

#pragma once

#include "topographic_generator.hpp"
#include "../core/ContourGenerator.hpp"
#include "RasterBuilder.hpp"
#include "RasterAnnotator.hpp"
#include <gdal_priv.h>
#include <string>
#include <vector>
#include <optional>

namespace topo {

/**
 * @brief Rectangle result for text placement
 */
struct TextRect {
    int x, y;           ///< Center position
    int width, height;  ///< Rectangle dimensions
    bool valid;         ///< True if valid placement found
};

/**
 * @brief Exports contour data as PNG raster images
 *
 * Uses RasterBuilder for robust polygon rasterization with GDAL.
 * Supports per-layer export and elevation-based coloring.
 */
class PNGExporter {
public:
    struct Options {
        int width_px;
        int height_px;
        double margin_px;              // Margin around content (should match SVG margin converted to pixels)
        TopographicConfig::ColorScheme color_scheme;
        TopographicConfig::RenderMode render_mode;
        bool add_legend;
        bool add_scale_bar;
        std::string title;
        int elevation_bands;

        // Annotation options
        bool add_alignment_marks;
        bool add_border;
        std::string stroke_color;      // RGB hex (e.g., "FF0000")
        std::string background_color;  // RGB hex (e.g., "FFFFFF")
        double stroke_width;           // Stroke width in pixels

        // Font configuration for text rendering
        std::string font_path;         // Empty = auto-detect system font
        std::string font_face;         // Preferred font face name

        // Polygon options
        bool remove_holes;             // Remove interior holes for simpler 2D output

        // Filename configuration
        std::string filename_pattern;  // Filename pattern with %{b}, %{l}, etc.

        // Label configuration
        std::string base_label_visible;
        std::string base_label_hidden;
        std::string layer_label_visible;
        std::string layer_label_hidden;
        std::string visible_label_color;
        std::string hidden_label_color;
        double base_font_size_mm;
        double layer_font_size_mm;

        Options()
            : width_px(2048),
              height_px(0),
              margin_px(236.0),        // Default matches 10mm @ 600 DPI
              color_scheme(TopographicConfig::ColorScheme::TERRAIN),
              render_mode(TopographicConfig::RenderMode::FULL_COLOR),
              add_legend(false),
              add_scale_bar(false),
              title(""),
              elevation_bands(10),
              add_alignment_marks(true),
              add_border(true),
              stroke_color("FF0000"),
              background_color("FFFFFF"),
              stroke_width(3.0),
              font_path(""),
              font_face("Arial"),
              remove_holes(true),
              filename_pattern(""),
              base_label_visible(""),
              base_label_hidden(""),
              layer_label_visible(""),
              layer_label_hidden(""),
              visible_label_color("000000"),
              hidden_label_color("666666"),
              base_font_size_mm(4.0),
              layer_font_size_mm(3.0) {}
    };

    PNGExporter();
    explicit PNGExporter(const Options& options);

    /**
     * @brief Export contour layers as PNG raster(s)
     * @param layers Contour layers to rasterize
     * @param base_filename Base output filename (without extension)
     * @param bounds Geographic bounding box
     * @param separate_layers If true, create one PNG per layer
     * @return Vector of generated PNG file paths
     */
    std::vector<std::string> export_png(
        const std::vector<ContourLayer>& layers,
        const std::string& base_filename,
        const BoundingBox& bounds,
        bool separate_layers = false);

    /**
     * @brief Export single layer as PNG
     * @param layer Single contour layer
     * @param filename Output PNG filename
     * @param bounds Geographic bounding box
     * @return true if export succeeded
     */
    bool export_layer(const ContourLayer& layer,
                      const std::string& filename,
                      const BoundingBox& bounds);

    /**
     * @brief Export from pre-built GDAL dataset
     * @param dataset GDALDataset to export (e.g., from RasterBuilder)
     * @param filename Output PNG filename
     * @return true if export succeeded
     */
    bool export_from_dataset(GDALDataset* dataset, const std::string& filename);

    /**
     * @brief Set color scheme for elevation visualization
     */
    void set_color_scheme(TopographicConfig::ColorScheme scheme);

    /**
     * @brief Set render mode (fill style)
     */
    void set_render_mode(TopographicConfig::RenderMode mode);

private:
    Options options_;

    /**
     * @brief Write PNG using GDAL CreateCopy
     */
    bool write_png_with_copy(GDALDataset* source, const std::string& filename);

    /**
     * @brief Get RasterBuilder configuration from options
     */
    RasterConfig get_raster_config() const;

    // Overlay elements (TODO: implement)
    void add_legend_overlay(GDALDataset* dataset, double min_elev, double max_elev);
    void add_scale_bar_overlay(GDALDataset* dataset, const BoundingBox& bounds);

    /**
     * @brief Calculate bounding box from polygon vertices in pixel coordinates
     * @param polygons Polygon data in geographic coordinates
     * @param geo_bounds Geographic bounding box for coordinate transformation
     * @param width Raster width in pixels
     * @param height Raster height in pixels
     * @return Bounding box in pixel coordinates
     */
    BoundingBox calculate_polygon_bbox_pixels(
        const std::vector<ContourLayer::PolygonData>& polygons,
        const BoundingBox& geo_bounds,
        int width,
        int height) const;

    /**
     * @brief Convert polygon geographic coordinates to pixel coordinates
     * @param polygon Polygon data in geographic coordinates
     * @param geo_bounds Geographic bounding box
     * @param width Raster width in pixels
     * @param height Raster height in pixels
     * @return Vector of pixel coordinates
     */
    std::vector<std::pair<int, int>> get_polygon_pixels(
        const ContourLayer::PolygonData& polygon,
        const BoundingBox& geo_bounds,
        int width,
        int height) const;

    /**
     * @brief Find optimal rectangle for text placement within polygon
     * @param polygon_pixels Polygon vertices in pixel coordinates
     * @param text_width_px Text width in pixels
     * @param text_height_px Text height in pixels
     * @return TextRect with placement info, or invalid if text doesn't fit
     */
    TextRect find_optimal_text_rect(
        const std::vector<std::pair<int, int>>& polygon_pixels,
        int text_width_px,
        int text_height_px) const;
};

} // namespace topo
