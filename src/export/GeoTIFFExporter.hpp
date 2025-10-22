/**
 * @file GeoTIFFExporter.hpp
 * @brief GeoTIFF raster export with georeferencing
 *
 * Exports topographic contour data as georeferenced TIFF/GeoTIFF rasters
 * with spatial reference information for GIS applications.
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
 * @brief Exports contour data as GeoTIFF raster images
 *
 * Creates georeferenced TIFF rasters with coordinate system information,
 * enabling direct import into GIS software. Uses RasterBuilder for
 * robust polygon rasterization and supports per-layer export.
 */
class GeoTIFFExporter {
public:
    struct Options {
        enum class Compression {
            NONE,
            LZW,
            DEFLATE,
            JPEG
        };

        int width_px;
        int height_px;
        bool georeference;
        std::string projection_wkt;
        Compression compression;
        TopographicConfig::ColorScheme color_scheme;
        TopographicConfig::RenderMode render_mode;
        int elevation_bands;

        // Annotation options
        bool add_alignment_marks;
        bool add_border;
        std::string stroke_color;      // RGB hex (e.g., "FF0000")
        std::string background_color;  // RGB hex (e.g., "FFFFFF")
        double stroke_width;           // Stroke width in pixels

        Options()
            : width_px(2048),
              height_px(0),
              georeference(true),
              projection_wkt("EPSG:4326"),  // WGS84
              compression(Compression::DEFLATE),
              color_scheme(TopographicConfig::ColorScheme::TERRAIN),
              render_mode(TopographicConfig::RenderMode::FULL_COLOR),
              elevation_bands(10),
              add_alignment_marks(true),
              add_border(true),
              stroke_color("FF0000"),
              background_color("FFFFFF"),
              stroke_width(3.0) {}
    };

    GeoTIFFExporter();
    explicit GeoTIFFExporter(const Options& options);

    /**
     * @brief Export contour layers as GeoTIFF raster(s)
     * @param layers Contour layers to rasterize
     * @param base_filename Base output filename (without extension)
     * @param bounds Geographic bounding box
     * @param separate_layers If true, create one GeoTIFF per layer
     * @return Vector of generated GeoTIFF file paths
     */
    std::vector<std::string> export_geotiff(
        const std::vector<ContourLayer>& layers,
        const std::string& base_filename,
        const BoundingBox& bounds,
        bool separate_layers = false);

    /**
     * @brief Export single layer as GeoTIFF
     * @param layer Single contour layer
     * @param filename Output GeoTIFF filename
     * @param bounds Geographic bounding box
     * @return true if export succeeded
     */
    bool export_layer(const ContourLayer& layer,
                      const std::string& filename,
                      const BoundingBox& bounds);

    /**
     * @brief Export from pre-built GDAL dataset
     * @param dataset GDALDataset to export (e.g., from RasterBuilder)
     * @param filename Output GeoTIFF filename
     * @param bounds Geographic bounding box for georeferencing
     * @return true if export succeeded
     */
    bool export_from_dataset(GDALDataset* dataset,
                            const std::string& filename,
                            const BoundingBox& bounds);

    /**
     * @brief Set compression method
     */
    void set_compression(Options::Compression compression);

    /**
     * @brief Set color scheme
     */
    void set_color_scheme(TopographicConfig::ColorScheme scheme);

    /**
     * @brief Set render mode
     */
    void set_render_mode(TopographicConfig::RenderMode mode);

private:
    Options options_;

    /**
     * @brief Write GeoTIFF using GDAL CreateCopy with georeferencing
     */
    bool write_geotiff_with_copy(GDALDataset* source,
                                 const std::string& filename,
                                 const BoundingBox& bounds);

    /**
     * @brief Get RasterBuilder configuration from options
     */
    RasterConfig get_raster_config() const;

    /**
     * @brief Get compression option string for GDAL
     */
    const char* get_compression_option() const;

    /**
     * @brief Set geotransform on dataset
     */
    void set_geotransform(GDALDataset* dataset,
                         const BoundingBox& bounds);

    /**
     * @brief Set spatial reference on dataset
     */
    void set_spatial_reference(GDALDataset* dataset);

    /**
     * @brief Get WGS84 WKT string
     */
    std::string get_wgs84_wkt() const;
};

} // namespace topo
