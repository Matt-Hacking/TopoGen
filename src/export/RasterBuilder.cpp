/**
 * @file RasterBuilder.cpp
 * @brief Implementation of RasterBuilder for GDAL-based rasterization
 */

#include "RasterBuilder.hpp"
#include "../core/Logger.hpp"
#include <gdal_alg.h>
#include <ogr_geometry.h>
#include <ogrsf_frmts.h>
#include <algorithm>
#include <cmath>

namespace topo {

RasterBuilder::RasterBuilder(const RasterConfig& config)
    : config_(config) {
    GDALAllRegister();
}

std::pair<int, int> RasterBuilder::calculate_dimensions(const BoundingBox& bounds) const {
    int width = config_.width_px;

    // Calculate geographic aspect ratio accounting for latitude compression
    // Longitude degrees shrink by cos(latitude) factor
    double center_lat = (bounds.min_y + bounds.max_y) / 2.0;
    double lat_degrees = bounds.max_y - bounds.min_y;
    double lon_degrees = bounds.max_x - bounds.min_x;

    // Convert to approximate meters (~111km per degree latitude)
    double height_m = lat_degrees * 111000.0;
    double width_m = lon_degrees * 111000.0 * std::cos(center_lat * M_PI / 180.0);

    double aspect_ratio = height_m / width_m;
    int height = config_.height_px > 0 ? config_.height_px :
                 static_cast<int>(width * aspect_ratio);
    return {width, height};
}

GDALDataset* RasterBuilder::create_empty_dataset(int width, int height) {
    Logger logger("RasterBuilder");

    GDALDriver* mem_driver = GetGDALDriverManager()->GetDriverByName("MEM");
    if (!mem_driver) {
        logger.error("MEM driver not available");
        return nullptr;
    }

    GDALDataset* dataset = mem_driver->Create("", width, height, 4, GDT_Byte, nullptr);
    if (!dataset) {
        logger.error("Failed to create MEM dataset");
        return nullptr;
    }

    // Initialize to background color
    for (int band = 1; band <= 4; ++band) {
        GDALRasterBand* raster_band = dataset->GetRasterBand(band);
        std::vector<uint8_t> band_data(width * height, config_.background_color[band - 1]);

        CPLErr err = raster_band->RasterIO(GF_Write, 0, 0, width, height,
                                          band_data.data(), width, height,
                                          GDT_Byte, 0, 0);
        if (err != CE_None) {
            logger.error("Failed to initialize raster band " + std::to_string(band));
            GDALClose(dataset);
            return nullptr;
        }
    }

    return dataset;
}

ColorMapper::Scheme RasterBuilder::convert_color_scheme(TopographicConfig::ColorScheme scheme) const {
    // Static cast works because enum values are in same order
    return static_cast<ColorMapper::Scheme>(scheme);
}

std::array<uint8_t, 4> RasterBuilder::get_color_for_elevation(
    double elevation,
    double min_elevation,
    double max_elevation) const {

    ColorMapper color_mapper(convert_color_scheme(config_.color_scheme));
    auto rgb = color_mapper.map_elevation_to_color(elevation, min_elevation, max_elevation);

    return {
        static_cast<uint8_t>(rgb[0] * 255),
        static_cast<uint8_t>(rgb[1] * 255),
        static_cast<uint8_t>(rgb[2] * 255),
        255  // Full opacity
    };
}

bool RasterBuilder::rasterize_layers(
    GDALDataset* dataset,
    const std::vector<ContourLayer>& layers,
    const BoundingBox& bounds,
    double override_min_elev,
    double override_max_elev) {

    Logger logger("RasterBuilder");

    if (layers.empty()) {
        logger.warning("No layers to rasterize");
        return false;
    }

    // Determine elevation range - use override if provided, otherwise calculate from layers
    double min_elevation, max_elevation;
    if (std::isnan(override_min_elev) || std::isnan(override_max_elev)) {
        // Auto-calculate from layers
        min_elevation = layers.front().elevation;
        max_elevation = layers.back().elevation;
        for (const auto& layer : layers) {
            min_elevation = std::min(min_elevation, layer.elevation);
            max_elevation = std::max(max_elevation, layer.elevation);
        }
    } else {
        // Use provided global range
        min_elevation = override_min_elev;
        max_elevation = override_max_elev;
    }

    auto [width, height] = calculate_dimensions(bounds);

    // Calculate content area (accounting for margin)
    double content_width = width - 2 * config_.margin_px;
    double content_height = height - 2 * config_.margin_px;

    // Calculate pixel size in geographic units
    double pixel_width = (bounds.max_x - bounds.min_x) / content_width;
    double pixel_height = (bounds.max_y - bounds.min_y) / content_height;

    // Set geotransform for proper coordinate mapping
    // Geographic origin must be shifted by margin pixels to align fills with outlines
    // Pixel (margin_px, margin_px) should map to (bounds.min_x, bounds.max_y)
    double geotransform[6] = {
        bounds.min_x - config_.margin_px * pixel_width,    // Top-left X (shifted left by margin)
        pixel_width,                                        // Pixel width (geographic units per pixel)
        0,                                                  // Rotation (0 for north-up)
        bounds.max_y + config_.margin_px * pixel_height,   // Top-left Y (shifted up by margin)
        0,                                                  // Rotation (0 for north-up)
        -pixel_height                                       // Pixel height (negative for north-up)
    };
    dataset->SetGeoTransform(geotransform);

    // Check render mode to determine if we should fill
    bool should_fill = (config_.render_mode != TopographicConfig::RenderMode::MONOCHROME);

    if (!should_fill) {
        logger.info("MONOCHROME mode - skipping polygon fills (outline only)");
        return true;
    }

    // Rasterize each layer
    for (const auto& layer : layers) {
        auto color = get_color_for_elevation(layer.elevation, min_elevation, max_elevation);

        // Create in-memory vector layer for this contour layer
        GDALDriver* vector_driver = GetGDALDriverManager()->GetDriverByName("MEM");
        if (!vector_driver) {
            logger.error("MEM vector driver not available");
            return false;
        }

        GDALDataset* vector_ds = vector_driver->Create("", 0, 0, 0, GDT_Unknown, nullptr);
        if (!vector_ds) {
            logger.error("Failed to create memory vector dataset");
            return false;
        }

        OGRLayer* ogr_layer = vector_ds->CreateLayer("contours", nullptr, wkbPolygon, nullptr);
        if (!ogr_layer) {
            logger.error("Failed to create OGR layer");
            GDALClose(vector_ds);
            return false;
        }

        // Add polygons to OGR layer
        for (const auto& poly : layer.polygons) {
            OGRFeature* feature = OGRFeature::CreateFeature(ogr_layer->GetLayerDefn());

            OGRPolygon ogr_poly;
            OGRLinearRing ogr_ring;

            const auto& exterior = poly.exterior();
            for (const auto& [x, y] : exterior) {
                ogr_ring.addPoint(x, y);
            }
            ogr_poly.addRing(&ogr_ring);

            // Add holes (only if not removing them for simpler 2D output)
            if (!config_.remove_holes) {
                for (const auto& hole : poly.holes()) {
                    OGRLinearRing hole_ring;
                    for (const auto& [x, y] : hole) {
                        hole_ring.addPoint(x, y);
                    }
                    ogr_poly.addRing(&hole_ring);
                }
            }

            feature->SetGeometry(&ogr_poly);
            OGRErr create_err = ogr_layer->CreateFeature(feature);
            if (create_err != OGRERR_NONE) {
                logger.warning("Failed to create OGR feature");
            }
            OGRFeature::DestroyFeature(feature);
        }

        // Rasterize this layer into the dataset
        int band_list[4] = {1, 2, 3, 4};  // R, G, B, A
        double burn_values[4] = {
            static_cast<double>(color[0]),
            static_cast<double>(color[1]),
            static_cast<double>(color[2]),
            static_cast<double>(color[3])
        };

        OGRLayerH layers_to_burn[1] = {reinterpret_cast<OGRLayerH>(ogr_layer)};

        CPLErr err = GDALRasterizeLayers(
            dataset,                    // Target dataset
            4,                          // Number of bands
            band_list,                  // Band list
            1,                          // Number of layers
            layers_to_burn,             // Layers to burn
            nullptr,                    // Transform function
            nullptr,                    // Transform argument
            burn_values,                // Values to burn
            nullptr,                    // Options
            nullptr,                    // Progress function
            nullptr                     // Progress data
        );

        GDALClose(vector_ds);

        if (err != CE_None) {
            logger.error("Failed to rasterize layer at elevation " + std::to_string(layer.elevation));
            return false;
        }
    }

    logger.info("Rasterized " + std::to_string(layers.size()) + " layers successfully");
    return true;
}

GDALDataset* RasterBuilder::build_dataset(
    const std::vector<ContourLayer>& layers,
    const BoundingBox& bounds) {

    Logger logger("RasterBuilder");

    auto [width, height] = calculate_dimensions(bounds);
    logger.info("Building raster dataset: " + std::to_string(width) + "x" + std::to_string(height));

    GDALDataset* dataset = create_empty_dataset(width, height);
    if (!dataset) {
        return nullptr;
    }

    // Let rasterize_layers auto-calculate elevation range from layers
    bool success = rasterize_layers(dataset, layers, bounds);
    if (!success) {
        GDALClose(dataset);
        return nullptr;
    }

    return dataset;
}

GDALDataset* RasterBuilder::build_dataset_single_layer(
    const ContourLayer& layer,
    const BoundingBox& bounds,
    double global_min_elev,
    double global_max_elev) {

    std::vector<ContourLayer> single_layer = {layer};

    // Pass global elevation range to ensure consistent coloring across per-layer exports
    // If NaN values are provided, rasterize_layers will auto-calculate from the single layer
    return build_dataset_internal(single_layer, bounds, global_min_elev, global_max_elev);
}

GDALDataset* RasterBuilder::build_dataset_internal(
    const std::vector<ContourLayer>& layers,
    const BoundingBox& bounds,
    double min_elev,
    double max_elev) {

    Logger logger("RasterBuilder");

    auto [width, height] = calculate_dimensions(bounds);
    logger.info("Building raster dataset: " + std::to_string(width) + "x" + std::to_string(height));

    GDALDataset* dataset = create_empty_dataset(width, height);
    if (!dataset) {
        return nullptr;
    }

    bool success = rasterize_layers(dataset, layers, bounds, min_elev, max_elev);
    if (!success) {
        GDALClose(dataset);
        return nullptr;
    }

    // Add terrain outline strokes if enabled
    if (config_.add_terrain_outline) {
        success = add_terrain_outline(dataset, layers, bounds);
        if (!success) {
            logger.warning("Failed to add terrain outline strokes");
            // Don't fail the whole operation, just continue without outlines
        }
    }

    return dataset;
}

void RasterBuilder::geo_to_pixel(
    double geo_x, double geo_y,
    const BoundingBox& bounds,
    int width, int height,
    int& pixel_x, int& pixel_y) const {

    // Calculate pixel coordinates from geographic coordinates
    // Geographic coordinates are in the bounds coordinate system
    double x_range = bounds.max_x - bounds.min_x;
    double y_range = bounds.max_y - bounds.min_y;

    // Normalize to [0, 1]
    double norm_x = (geo_x - bounds.min_x) / x_range;
    double norm_y = (geo_y - bounds.min_y) / y_range;

    // Calculate content area (accounting for margin)
    double content_width = width - 2 * config_.margin_px;
    double content_height = height - 2 * config_.margin_px;

    // Convert to pixel coordinates within content area
    pixel_x = static_cast<int>(config_.margin_px + norm_x * content_width);
    // Flip Y because raster Y increases downward, but geographic Y increases upward
    pixel_y = static_cast<int>(config_.margin_px + (1.0 - norm_y) * content_height);

    // Clamp to bounds
    pixel_x = std::max(0, std::min(width - 1, pixel_x));
    pixel_y = std::max(0, std::min(height - 1, pixel_y));
}

bool RasterBuilder::add_terrain_outline(
    GDALDataset* dataset,
    const std::vector<ContourLayer>& layers,
    const BoundingBox& bounds) {

    if (!dataset || layers.empty()) {
        return false;
    }

    if (!config_.add_terrain_outline) {
        return true;  // Not an error, just disabled
    }

    Logger logger("RasterBuilder");
    logger.debug("Adding terrain outline strokes");

    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    // Create annotator with outline settings
    AnnotationConfig annot_config;
    annot_config.add_alignment_marks = false;
    annot_config.add_border = false;
    annot_config.stroke_color = config_.outline_color;
    annot_config.stroke_width_px = config_.outline_width_px;

    RasterAnnotator annotator(annot_config);

    // Draw outline for each polygon in each layer
    int total_polygons = 0;
    int total_lines = 0;

    for (const auto& layer : layers) {
        if (layer.polygons.empty()) continue;

        for (const auto& polygon : layer.polygons) {
            if (polygon.rings.empty()) continue;

            total_polygons++;

            // Draw outline only for exterior ring (first ring), not interior holes
            // Interior holes represent areas covered by higher elevation layers
            if (!polygon.rings.empty() && polygon.rings[0].size() >= 2) {
                const auto& ring = polygon.rings[0];  // Exterior ring only

                // Draw lines between consecutive vertices
                for (size_t i = 0; i < ring.size(); ++i) {
                    size_t next_i = (i + 1) % ring.size();

                    const auto& [x1, y1] = ring[i];
                    const auto& [x2, y2] = ring[next_i];

                    // Convert geographic coordinates to pixel coordinates
                    int px1, py1, px2, py2;
                    geo_to_pixel(x1, y1, bounds, width, height, px1, py1);
                    geo_to_pixel(x2, y2, bounds, width, height, px2, py2);

                    // Draw line segment
                    annotator.draw_line(dataset, px1, py1, px2, py2,
                                       config_.outline_color, config_.outline_width_px);
                    total_lines++;
                }
            }
        }
    }

    logger.debug("Drew terrain outline: " + std::to_string(total_polygons) +
                 " polygons, " + std::to_string(total_lines) + " line segments");

    return true;
}

} // namespace topo
