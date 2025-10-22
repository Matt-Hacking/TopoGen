/**
 * @file GeoTIFFExporter.cpp
 * @brief Implementation of GeoTIFF export with georeferencing
 */

#include "GeoTIFFExporter.hpp"
#include "../core/Logger.hpp"
#include <ogr_spatialref.h>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace topo {

GeoTIFFExporter::GeoTIFFExporter()
    : options_() {
    GDALAllRegister();
}

GeoTIFFExporter::GeoTIFFExporter(const Options& options)
    : options_(options) {
    GDALAllRegister();
}

void GeoTIFFExporter::set_compression(Options::Compression compression) {
    options_.compression = compression;
}

void GeoTIFFExporter::set_color_scheme(TopographicConfig::ColorScheme scheme) {
    options_.color_scheme = scheme;
}

void GeoTIFFExporter::set_render_mode(TopographicConfig::RenderMode mode) {
    options_.render_mode = mode;
}

RasterConfig GeoTIFFExporter::get_raster_config() const {
    RasterConfig config;
    config.width_px = options_.width_px;
    config.height_px = options_.height_px;
    config.color_scheme = options_.color_scheme;
    config.render_mode = options_.render_mode;
    config.elevation_bands = options_.elevation_bands;

    // Parse background color from hex string
    config.background_color = RasterAnnotator::parse_hex_color(options_.background_color);

    // Configure terrain outline (enabled by default for GeoTIFF)
    config.add_terrain_outline = true;
    config.outline_color = RasterAnnotator::parse_hex_color(options_.stroke_color);
    config.outline_width_px = options_.stroke_width;

    return config;
}

const char* GeoTIFFExporter::get_compression_option() const {
    switch (options_.compression) {
        case Options::Compression::NONE:
            return "NONE";
        case Options::Compression::LZW:
            return "LZW";
        case Options::Compression::DEFLATE:
            return "DEFLATE";
        case Options::Compression::JPEG:
            return "JPEG";
        default:
            return "DEFLATE";
    }
}

std::string GeoTIFFExporter::get_wgs84_wkt() const {
    OGRSpatialReference srs;
    srs.SetWellKnownGeogCS("WGS84");

    char* wkt = nullptr;
    srs.exportToWkt(&wkt);
    std::string result = wkt ? wkt : "";
    CPLFree(wkt);

    return result;
}

void GeoTIFFExporter::set_geotransform(GDALDataset* dataset, const BoundingBox& bounds) {
    if (!dataset) return;

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    double geotransform[6] = {
        bounds.min_x,                           // Top-left X
        (bounds.max_x - bounds.min_x) / width,  // Pixel width
        0,                                       // Rotation (0 for north-up)
        bounds.max_y,                           // Top-left Y
        0,                                       // Rotation (0 for north-up)
        -(bounds.max_y - bounds.min_y) / height // Pixel height (negative for north-up)
    };

    dataset->SetGeoTransform(geotransform);
}

void GeoTIFFExporter::set_spatial_reference(GDALDataset* dataset) {
    if (!dataset) return;

    std::string wkt;
    if (options_.projection_wkt.empty() || options_.projection_wkt == "EPSG:4326") {
        wkt = get_wgs84_wkt();
    } else if (options_.projection_wkt.find("EPSG:") == 0) {
        // Convert EPSG code to WKT
        OGRSpatialReference srs;
        std::string epsg_code = options_.projection_wkt.substr(5);
        if (srs.importFromEPSG(std::stoi(epsg_code)) == OGRERR_NONE) {
            char* wkt_str = nullptr;
            srs.exportToWkt(&wkt_str);
            wkt = wkt_str ? wkt_str : "";
            CPLFree(wkt_str);
        } else {
            wkt = get_wgs84_wkt();
        }
    } else {
        wkt = options_.projection_wkt;
    }

    if (!wkt.empty()) {
        dataset->SetProjection(wkt.c_str());
    }
}

bool GeoTIFFExporter::write_geotiff_with_copy(GDALDataset* source,
                                              const std::string& filename,
                                              const BoundingBox& bounds) {
    Logger logger("GeoTIFFExporter");

    if (!source) {
        logger.error("Null source dataset");
        return false;
    }

    // Set georeferencing on source dataset
    if (options_.georeference) {
        set_geotransform(source, bounds);
        set_spatial_reference(source);
    }

    // Get GeoTIFF driver
    GDALDriver* gtiff_driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!gtiff_driver) {
        logger.error("GeoTIFF driver not available");
        return false;
    }

    // Set creation options
    char** options = nullptr;
    options = CSLSetNameValue(options, "COMPRESS", get_compression_option());
    options = CSLSetNameValue(options, "TILED", "YES");
    options = CSLSetNameValue(options, "BLOCKXSIZE", "256");
    options = CSLSetNameValue(options, "BLOCKYSIZE", "256");

    // Copy source dataset to GeoTIFF file
    GDALDataset* gtiff_dataset = gtiff_driver->CreateCopy(
        filename.c_str(),
        source,
        FALSE,      // Not strict
        options,    // Creation options
        nullptr,    // Progress function
        nullptr     // Progress data
    );

    CSLDestroy(options);

    if (!gtiff_dataset) {
        logger.error("Failed to create GeoTIFF file: " + filename);
        return false;
    }

    GDALClose(gtiff_dataset);
    return true;
}

bool GeoTIFFExporter::export_from_dataset(GDALDataset* dataset,
                                          const std::string& filename,
                                          const BoundingBox& bounds) {
    Logger logger("GeoTIFFExporter");

    if (!dataset) {
        logger.error("Null dataset");
        return false;
    }

    bool success = write_geotiff_with_copy(dataset, filename, bounds);

    if (success) {
        logger.info("Exported GeoTIFF: " + filename + " (" +
                   std::to_string(dataset->GetRasterXSize()) + "x" +
                   std::to_string(dataset->GetRasterYSize()) + ")");
    }

    return success;
}

std::vector<std::string> GeoTIFFExporter::export_geotiff(
    const std::vector<ContourLayer>& layers,
    const std::string& base_filename,
    const BoundingBox& bounds,
    bool separate_layers) {

    Logger logger("GeoTIFFExporter");
    std::vector<std::string> exported_files;

    if (layers.empty()) {
        logger.error("No layers to export");
        return exported_files;
    }

    RasterConfig raster_config = get_raster_config();
    RasterBuilder builder(raster_config);

    if (separate_layers) {
        // Calculate global elevation range for consistent coloring across layers
        double global_min_elev = layers.front().elevation;
        double global_max_elev = layers.back().elevation;
        for (const auto& layer : layers) {
            global_min_elev = std::min(global_min_elev, layer.elevation);
            global_max_elev = std::max(global_max_elev, layer.elevation);
        }

        // Export each layer separately
        for (size_t i = 0; i < layers.size(); ++i) {
            const auto& layer = layers[i];

            // Generate filename: base_layer_N.tif
            std::filesystem::path base_path(base_filename);
            std::string layer_filename = base_path.parent_path().string();
            if (!layer_filename.empty()) layer_filename += "/";
            layer_filename += base_path.stem().string() + "_layer_" + std::to_string(i + 1) + ".tif";

            // Build dataset for this layer with global elevation range
            GDALDataset* dataset = builder.build_dataset_single_layer(layer, bounds,
                                                                      global_min_elev, global_max_elev);
            if (!dataset) {
                logger.error("Failed to build raster for layer " + std::to_string(i + 1));
                continue;
            }

            // Add annotations (registration marks, borders) if requested
            if (options_.add_alignment_marks || options_.add_border) {
                AnnotationConfig annot_config;
                annot_config.add_alignment_marks = options_.add_alignment_marks;
                annot_config.add_border = options_.add_border;
                annot_config.stroke_color = RasterAnnotator::parse_hex_color(options_.stroke_color);
                annot_config.alignment_color = {0, 0, 255, 255};  // Blue

                RasterAnnotator annotator(annot_config);
                annotator.annotate_dataset(dataset, layer.elevation);
            }

            // Export to GeoTIFF
            bool success = export_from_dataset(dataset, layer_filename, bounds);
            GDALClose(dataset);

            if (success) {
                exported_files.push_back(layer_filename);
            }
        }
    } else {
        // Export all layers combined into single GeoTIFF
        std::string combined_filename = base_filename;
        if (combined_filename.find(".tif") == std::string::npos) {
            combined_filename += ".tif";
        }

        GDALDataset* dataset = builder.build_dataset(layers, bounds);
        if (!dataset) {
            logger.error("Failed to build raster dataset");
            return exported_files;
        }

        // Add annotations (registration marks, borders) if requested
        if (options_.add_alignment_marks || options_.add_border) {
            AnnotationConfig annot_config;
            annot_config.add_alignment_marks = options_.add_alignment_marks;
            annot_config.add_border = options_.add_border;
            annot_config.stroke_color = RasterAnnotator::parse_hex_color(options_.stroke_color);
            annot_config.alignment_color = {0, 0, 255, 255};  // Blue

            RasterAnnotator annotator(annot_config);
            annotator.annotate_dataset(dataset);
        }

        bool success = export_from_dataset(dataset, combined_filename, bounds);
        GDALClose(dataset);

        if (success) {
            exported_files.push_back(combined_filename);
        }
    }

    logger.info("Exported " + std::to_string(exported_files.size()) + " GeoTIFF file(s)");
    return exported_files;
}

bool GeoTIFFExporter::export_layer(const ContourLayer& layer,
                                   const std::string& filename,
                                   const BoundingBox& bounds) {
    std::vector<ContourLayer> single_layer = {layer};
    auto files = export_geotiff(single_layer, filename, bounds, false);
    return !files.empty();
}

} // namespace topo
