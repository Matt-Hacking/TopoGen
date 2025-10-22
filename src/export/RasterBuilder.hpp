/**
 * @file RasterBuilder.hpp
 * @brief Builds rasterized GDAL datasets from contour layers
 *
 * Provides a unified rasterization engine that creates GDAL in-memory datasets
 * from vector contour data. The resulting datasets can be used by multiple
 * export formats (PNG, GeoTIFF, BMP, etc.) without redundant rasterization.
 */

#pragma once

#include "topographic_generator.hpp"
#include "../core/ContourGenerator.hpp"
#include "MultiFormatExporter.hpp"
#include "RasterAnnotator.hpp"
#include <gdal_priv.h>
#include <ogr_api.h>
#include <memory>

namespace topo {

/**
 * @brief Configuration for raster generation
 */
struct RasterConfig {
    int width_px = 2048;
    int height_px = 0;  // Auto-calculate from aspect ratio if 0
    double margin_px = 236.0;  // Margin around content (default matches 10mm @ 600 DPI)

    TopographicConfig::ColorScheme color_scheme = TopographicConfig::ColorScheme::TERRAIN;
    TopographicConfig::RenderMode render_mode = TopographicConfig::RenderMode::FULL_COLOR;

    int elevation_bands = 10;
    bool use_antialiasing = true;

    // Background color (RGBA 0-255)
    std::array<uint8_t, 4> background_color = {255, 255, 255, 255};  // White

    // Terrain outline options
    bool add_terrain_outline = true;  // Draw stroke around terrain polygons
    std::array<uint8_t, 4> outline_color = {255, 0, 0, 255};  // Red
    double outline_width_px = 5.0;  // Stroke width in pixels

    // Polygon hole options
    bool remove_holes = true;  // Remove interior holes for simpler 2D output (minimize cuts)
};

/**
 * @brief Builds rasterized datasets from vector contour layers
 *
 * Uses GDAL's GDALRasterizeGeometries for robust polygon filling.
 * Integrates ColorMapper for elevation-based coloring.
 * Produces in-memory MEM datasets that can be efficiently copied to various formats.
 */
class RasterBuilder {
public:
    explicit RasterBuilder(const RasterConfig& config = RasterConfig{});

    /**
     * @brief Build rasterized dataset from contour layers
     * @param layers Vector contour layers to rasterize
     * @param bounds Geographic bounding box
     * @return GDALDataset* in-memory dataset (caller must GDALClose())
     *
     * Creates an in-memory GDAL dataset (driver="MEM") with RGBA bands.
     * Polygons are filled according to render_mode and colored according to color_scheme.
     * Caller is responsible for calling GDALClose() when done.
     */
    GDALDataset* build_dataset(
        const std::vector<ContourLayer>& layers,
        const BoundingBox& bounds);

    /**
     * @brief Build rasterized dataset for a single layer
     * @param layer Single contour layer
     * @param bounds Geographic bounding box
     * @param global_min_elev Optional minimum elevation for color mapping (for per-layer export consistency)
     * @param global_max_elev Optional maximum elevation for color mapping (for per-layer export consistency)
     * @return GDALDataset* in-memory dataset (caller must GDALClose())
     */
    GDALDataset* build_dataset_single_layer(
        const ContourLayer& layer,
        const BoundingBox& bounds,
        double global_min_elev = std::numeric_limits<double>::quiet_NaN(),
        double global_max_elev = std::numeric_limits<double>::quiet_NaN());

    /**
     * @brief Get calculated raster dimensions
     * @param bounds Geographic bounding box
     * @return pair of (width, height) in pixels
     */
    std::pair<int, int> calculate_dimensions(const BoundingBox& bounds) const;

private:
    RasterConfig config_;

    /**
     * @brief Internal method to build dataset with optional elevation range override
     */
    GDALDataset* build_dataset_internal(
        const std::vector<ContourLayer>& layers,
        const BoundingBox& bounds,
        double min_elev,
        double max_elev);

    /**
     * @brief Create empty RGBA dataset
     */
    GDALDataset* create_empty_dataset(int width, int height);

    /**
     * @brief Rasterize contour layers into dataset
     * @param dataset Target GDAL dataset
     * @param layers Contour layers to rasterize
     * @param bounds Geographic bounding box
     * @param override_min_elev Optional override for minimum elevation (NaN = auto-calculate)
     * @param override_max_elev Optional override for maximum elevation (NaN = auto-calculate)
     */
    bool rasterize_layers(
        GDALDataset* dataset,
        const std::vector<ContourLayer>& layers,
        const BoundingBox& bounds,
        double override_min_elev = std::numeric_limits<double>::quiet_NaN(),
        double override_max_elev = std::numeric_limits<double>::quiet_NaN());

    /**
     * @brief Get color for elevation using ColorMapper
     */
    std::array<uint8_t, 4> get_color_for_elevation(
        double elevation,
        double min_elevation,
        double max_elevation) const;

    /**
     * @brief Convert ColorMapper::Scheme to TopographicConfig::ColorScheme
     */
    ColorMapper::Scheme convert_color_scheme(TopographicConfig::ColorScheme scheme) const;

    /**
     * @brief Add terrain outline strokes to dataset
     * @param dataset Target GDAL dataset (must have RGBA bands)
     * @param layers Contour layers whose outlines to draw
     * @param bounds Geographic bounding box for coordinate transformation
     * @return true if successful
     *
     * Draws strokes around terrain polygon perimeters to match SVG cut lines.
     * Uses configured outline_color and outline_width_px from config.
     */
    bool add_terrain_outline(
        GDALDataset* dataset,
        const std::vector<ContourLayer>& layers,
        const BoundingBox& bounds);

    /**
     * @brief Transform geographic coordinates to pixel coordinates
     * @param geo_x Geographic X coordinate
     * @param geo_y Geographic Y coordinate
     * @param bounds Geographic bounding box
     * @param width Raster width in pixels
     * @param height Raster height in pixels
     * @param pixel_x Output pixel X coordinate
     * @param pixel_y Output pixel Y coordinate
     */
    void geo_to_pixel(
        double geo_x, double geo_y,
        const BoundingBox& bounds,
        int width, int height,
        int& pixel_x, int& pixel_y) const;
};

} // namespace topo
