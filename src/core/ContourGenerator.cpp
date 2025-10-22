/**
 * @file ContourGenerator.cpp
 * @brief Implementation of contour polygon generation
 * 
 * High-performance C++ port of Python contour_ops.py functionality
 * Uses marching squares algorithm and CGAL for robust polygon operations
 * 
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "ContourGenerator.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>
#include <unordered_map>
#include <queue>
// CGAL Boolean operations for robust polygon processing
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/convex_hull_2.h>
#include <gdal_priv.h>
#include <gdal_alg.h>
#include <ogr_spatialref.h>
#include <ogr_api.h>
#include <ogr_geometry.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <iomanip>
#include <nlohmann/json.hpp>

// PROJ includes removed - using WGS84 coordinates throughout

namespace topo {

// ============================================================================
// ContourLayer Implementation
// ============================================================================

void ContourLayer::calculate_area() {
    area = 0.0;
    for (const auto& poly : polygons) {
        if (!poly.empty()) {
            // Calculate exterior ring area using shoelace formula
            double poly_area = 0.0;
            const auto& exterior = poly.exterior();
            for (size_t i = 0; i < exterior.size(); ++i) {
                size_t j = (i + 1) % exterior.size();
                poly_area += exterior[i].first * exterior[j].second;
                poly_area -= exterior[j].first * exterior[i].second;
            }
            poly_area = std::abs(poly_area) / 2.0;

            // Subtract hole areas
            for (const auto& hole : poly.holes()) {
                double hole_area = 0.0;
                for (size_t i = 0; i < hole.size(); ++i) {
                    size_t j = (i + 1) % hole.size();
                    hole_area += hole[i].first * hole[j].second;
                    hole_area -= hole[j].first * hole[i].second;
                }
                poly_area -= std::abs(hole_area) / 2.0;
            }

            area += poly_area; // Add polygon area (already positive)
        }
    }
}

// ============================================================================
// ContourGenerator Implementation  
// ============================================================================

ContourGenerator::ContourGenerator(const ContourConfig& config)
    : config_(config) {
    // Wire logger to verbose/log level from config
    if (config_.verbose) {
        logger_.setLogLevel(LogLevel::DEBUG);
    }

    // Initialize monitoring infrastructure without crash handlers to prevent conflicts
    initialize_monitoring(false);  // Disable crash handlers during construction
}

ContourGenerator::~ContourGenerator() {
    logger_.flush();
}

std::vector<double> ContourGenerator::generate_contour_levels(
    double min_elevation, double max_elevation, int num_layers) const {

    std::vector<double> levels;

    // Apply elevation filters only if user explicitly specified them
    double min_elev = min_elevation;
    double max_elev = max_elevation;

    if (config_.min_elevation.has_value()) {
        min_elev = std::max(min_elevation, config_.min_elevation.value());
    }
    if (config_.max_elevation.has_value()) {
        max_elev = std::min(max_elevation, config_.max_elevation.value());
    }
    
    // Apply elevation threshold
    if (config_.elevation_threshold > 0) {
        // Positive: include features up to this height from lowest point
        max_elev = std::min(max_elev, min_elevation + config_.elevation_threshold);
    } else if (config_.elevation_threshold < 0) {
        // Negative: include features down to this depth from highest point  
        min_elev = std::max(min_elev, max_elevation + config_.elevation_threshold);
    }
    
    // Ensure valid range
    if (min_elev >= max_elev) {
        logger_.warning("Invalid elevation range after filtering: " + std::to_string(min_elev) + " to " + std::to_string(max_elev));
        // Fallback to small range around midpoint
        double mid = (min_elevation + max_elevation) / 2.0;
        min_elev = mid - 50.0;
        max_elev = mid + 50.0;
    }
    
    int actual_num_layers = (num_layers > 0) ? num_layers : 
                           static_cast<int>(std::ceil((max_elev - min_elev) / config_.interval));
    
    levels.resize(actual_num_layers + 1);
    
    switch (config_.strategy) {
        case ContourStrategy::UNIFORM: {
            double step = (max_elev - min_elev) / actual_num_layers;
            for (int i = 0; i <= actual_num_layers; ++i) {
                levels[i] = min_elev + i * step;
            }
            break;
        }
        
        case ContourStrategy::LOGARITHMIC: {
            double log_min = std::log(std::max(min_elev, 1.0));
            double log_max = std::log(std::max(max_elev, min_elev + 1.0));
            double step = (log_max - log_min) / actual_num_layers;
            
            for (int i = 0; i <= actual_num_layers; ++i) {
                levels[i] = std::exp(log_min + i * step);
            }
            break;
        }
        
        case ContourStrategy::EXPONENTIAL: {
            double range = max_elev - min_elev;
            for (int i = 0; i <= actual_num_layers; ++i) {
                double t = static_cast<double>(i) / actual_num_layers;
                double exp_t = (std::exp(t * 2.0) - 1.0) / (std::exp(2.0) - 1.0);
                levels[i] = min_elev + exp_t * range;
            }
            break;
        }
    }
    
    // Add fixed elevation for water if specified
    if (config_.fixed_elevation > -1e8) {
        bool found = false;
        for (double level : levels) {
            if (std::abs(level - config_.fixed_elevation) < 1e-6) {
                found = true;
                break;
            }
        }
        if (!found) {
            levels.push_back(config_.fixed_elevation);
            std::sort(levels.begin(), levels.end());
        }
    }

    logger_.info("Generated " + std::to_string(levels.size()) + " contour levels from "
                + std::to_string(levels.front()) + "m to " + std::to_string(levels.back()) + "m");

    return levels;
}

std::vector<ContourLayer> ContourGenerator::generate_contours(
    const float* elevation_data,
    size_t width, size_t height,
    const double* geotransform,
    int num_layers,
    [[maybe_unused]] double center_lon, [[maybe_unused]] double center_lat) {

    logger_.info("Generating contours from " + std::to_string(width) + "x" + std::to_string(height)
                + " elevation grid");

    // Log geographic bounds for debugging
    if (geotransform) {
        double min_x = geotransform[0];
        double max_y = geotransform[3];
        double pixel_width = geotransform[1];
        double pixel_height = -geotransform[5];  // Usually negative
        double max_x = min_x + width * pixel_width;
        double min_y = max_y - height * pixel_height;

        logger_.debug("Geographic bounds: [" + std::to_string(min_x) + ", " + std::to_string(min_y) +
                     "] to [" + std::to_string(max_x) + ", " + std::to_string(max_y) + "]");
        logger_.debug("  Width: " + std::to_string(max_x - min_x) + " degrees, Height: " +
                     std::to_string(max_y - min_y) + " degrees");
        logger_.debug("  Center: (" + std::to_string((min_x + max_x)/2) + ", " +
                     std::to_string((min_y + max_y)/2) + ")");
    }

    // Find elevation range
    auto minmax = std::minmax_element(elevation_data, elevation_data + width * height,
        [](float a, float b) {
            // Skip nodata values (-32768 is common SRTM nodata)
            if (a < -30000 && b >= -30000) return false;
            if (a >= -30000 && b < -30000) return true;
            if (a < -30000 && b < -30000) return false;
            return a < b;
        });

    double min_elevation = static_cast<double>(*minmax.first);
    double max_elevation = static_cast<double>(*minmax.second);

    logger_.info("Elevation range: " + std::to_string(min_elevation) + "m to " + std::to_string(max_elevation) + "m");

    // Generate contour levels
    std::vector<double> levels = generate_contour_levels(min_elevation, max_elevation, num_layers);

    // Generate contours directly - now returns ContourLayer objects
    std::vector<ContourLayer> contour_layers;

    // Select contour generation method based on user configuration
    if (config_.vertical_contour_relief) {
        // Standard contour map with vertical walls - use GDAL polygon extraction
        logger_.info("Generating standard contour map (vertical walls) using GDAL polygon extraction");
        contour_layers = generate_contours_with_gdal_api(elevation_data, width, height, geotransform, levels);
    } else {
        // Terrain-following map - layers follow terrain surface
        logger_.info("Generating terrain-following map (layers follow surface)");
        // TODO: Update extract_terrain_following_polygons_with_geotransform to return ContourLayer
        logger_.warning("Terrain-following mode not yet updated for CGAL-free operation, using vertical mode");
        contour_layers = generate_contours_with_gdal_api(elevation_data, width, height, geotransform, levels);
    }

    logger_.info("Extracted " + std::to_string(contour_layers.size()) + " elevation levels");

    for (const auto& layer : contour_layers) {
        logger_.debug("  Level " + std::to_string(layer.elevation) + "m: " + std::to_string(layer.polygons.size()) + " polygons");
    }

    // TODO: Water body processing needs to be updated for CGAL-free operation
    // For now, skip if water polygon is configured
    if (config_.has_water_polygon && config_.fixed_elevation > -1e8) {
        logger_.warning("Water body processing not yet updated for CGAL-free operation, skipping");
    }

    // Layer band computation is now done in generate_contours_with_gdal_api
    // No need for separate compute_layer_bands call


    logger_.info("Generated " + std::to_string(contour_layers.size()) + " contour layers");
    for (size_t i = 0; i < contour_layers.size(); ++i) {
        logger_.debug("  Layer " + std::to_string(i) + ": " + std::to_string(contour_layers[i].polygons.size()) + " polygons, area=" + std::to_string(contour_layers[i].area));
    }
    
    // UTM conversion removed - using WGS84 coordinates throughout
    
    // Calculate areas
    for (auto& layer : contour_layers) {
        layer.calculate_area();
    }
    
    logger_.debug("About to filter empty layers, contour_layers.size() = " + std::to_string(contour_layers.size()));

    // Enhanced empty layer detection with detailed diagnostics
    if (!config_.force_all_layers) {
        logger_.debug("Starting enhanced empty layer detection...");

        size_t layers_before = contour_layers.size();
        size_t empty_by_polygon_count = 0;
        size_t empty_by_area = 0;
        size_t valid_layers = 0;

        // First pass: analyze each layer and provide diagnostics
        for (size_t i = 0; i < contour_layers.size(); ++i) {
            const auto& layer = contour_layers[i];

            // Calculate area if not already done
            if (layer.area == 0.0 && !layer.polygons.empty()) {
                // Need to calculate area - this might be the issue!
                const_cast<ContourLayer&>(layer).calculate_area();
            }

            bool empty_polys = layer.polygons.empty();
            bool empty_area = layer.area < 1e-12;
            bool is_empty = empty_polys || empty_area;

            std::string layer_info = "        Layer " + std::to_string(i) + " @ " + std::to_string(layer.elevation) + "m: " + std::to_string(layer.polygons.size()) + " polygons, area=" + std::to_string(layer.area) + "m²";

            if (is_empty) {
                layer_info += " [EMPTY - ";
                if (empty_polys) {
                    layer_info += "no polygons";
                    empty_by_polygon_count++;
                } else {
                    layer_info += "area too small (<1e-12)";
                    empty_by_area++;
                }
                layer_info += "]";
            } else {
                layer_info += " [VALID]";
                valid_layers++;
            }
            logger_.debug(layer_info);
        }

        logger_.debug("Layer analysis complete - " + std::to_string(valid_layers) + " valid, " + std::to_string(empty_by_polygon_count) + " empty by polygon count, " + std::to_string(empty_by_area) + " empty by area");

        // Second pass: remove empty layers
        contour_layers.erase(
            std::remove_if(contour_layers.begin(), contour_layers.end(),
                [](const ContourLayer& layer) { return layer.empty(); }),
            contour_layers.end()
        );

        size_t layers_after = contour_layers.size();
        logger_.debug("Empty layer removal complete: " + std::to_string(layers_before) + " -> " + std::to_string(layers_after) + " (" + std::to_string(layers_before - layers_after) + " removed)");
    }
    
    logger_.info("Final output: " + std::to_string(contour_layers.size()) + " contour layers");
    
    return contour_layers;
}

void ContourGenerator::create_meshgrid(
    size_t width, size_t height, 
    const double* geotransform,
    std::vector<double>& lon, 
    std::vector<double>& lat) const {
    
    lon.resize(width * height);
    lat.resize(width * height);
    
    // GDAL geotransform: [x_origin, pixel_width, 0, y_origin, 0, pixel_height]
    double x_origin = geotransform[0];
    double pixel_width = geotransform[1]; 
    double y_origin = geotransform[3];
    double pixel_height = geotransform[5]; // Usually negative
    
    for (size_t j = 0; j < height; ++j) {
        for (size_t i = 0; i < width; ++i) {
            size_t idx = j * width + i;
            
            // Pixel center coordinates
            lon[idx] = x_origin + (i + 0.5) * pixel_width;
            lat[idx] = y_origin + (j + 0.5) * pixel_height;
        }
    }
}

bool ContourGenerator::write_temporary_geotiff(
    const float* elevation_data,
    size_t width,
    size_t height,
    const double* geotransform,
    const std::string& filename) const {

    try {
        // Initialize GDAL drivers
        GDALAllRegister();

        // Get GeoTIFF driver
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        if (!driver) {
            logger_.error("Failed to get GeoTIFF driver");
            return false;
        }

        // Create dataset
        GDALDataset* dataset = driver->Create(
            filename.c_str(),
            static_cast<int>(width),
            static_cast<int>(height),
            1,  // Number of bands
            GDT_Float32,
            nullptr
        );

        if (!dataset) {
            logger_.error("Failed to create GeoTIFF dataset: " + filename);
            return false;
        }

        // Set geotransform
        if (dataset->SetGeoTransform(const_cast<double*>(geotransform)) != CE_None) {
            logger_.warning("Failed to set geotransform for temporary GeoTIFF");
        }

        // Set projection (WGS84)
        OGRSpatialReference srs;
        srs.SetWellKnownGeogCS("WGS84");
        char* proj_string = nullptr;
        srs.exportToWkt(&proj_string);
        if (proj_string) {
            dataset->SetProjection(proj_string);
            CPLFree(proj_string);
        }

        // Get the raster band
        GDALRasterBand* band = dataset->GetRasterBand(1);
        if (!band) {
            logger_.error("Failed to get raster band");
            GDALClose(dataset);
            return false;
        }

        // Set NODATA value
        band->SetNoDataValue(-32768.0);

        // Write elevation data
        if (band->RasterIO(
            GF_Write,
            0, 0,
            static_cast<int>(width),
            static_cast<int>(height),
            const_cast<float*>(elevation_data),
            static_cast<int>(width),
            static_cast<int>(height),
            GDT_Float32,
            0, 0
        ) != CE_None) {
            logger_.error("Failed to write elevation data to GeoTIFF");
            GDALClose(dataset);
            return false;
        }

        // Close dataset
        GDALClose(dataset);
        logger_.debug("Successfully wrote temporary GeoTIFF: " + filename);
        return true;

    } catch (const std::exception& e) {
        logger_.error("Exception writing temporary GeoTIFF: " + std::string(e.what()));
        return false;
    }
}

bool ContourGenerator::ogr_polygon_to_polygon_data(
    void* ogr_geometry,
    ContourLayer::PolygonData& polygon_data) const {

    OGRGeometryH geometry = static_cast<OGRGeometryH>(ogr_geometry);

    if (!geometry) {
        logger_.warning("Invalid OGR geometry: null pointer");
        return false;
    }

    OGRwkbGeometryType geom_type = OGR_G_GetGeometryType(geometry);
    if (geom_type != wkbPolygon) {
        logger_.warning("Invalid OGR geometry type: expected wkbPolygon, got " + std::to_string(geom_type));
        return false;
    }

    // FIX: Skip geometry validation - both Buffer(0) and MakeValid collapse polygons on geographic coordinates
    // GDALPolygonize output is generally valid, and validation destroys large amounts of geometry
    // If self-intersections are an issue, they should be fixed AFTER coordinate projection to UTM

    // Extract exterior ring
    OGRGeometryH exterior_ring = OGR_G_GetGeometryRef(geometry, 0);
    if (!exterior_ring) {
        logger_.warning("Failed to get exterior ring from OGR polygon");
        return false;
    }

    int num_points = OGR_G_GetPointCount(exterior_ring);
    if (num_points < 3) {
        logger_.debug("Exterior ring has fewer than 3 points: " + std::to_string(num_points));
        return false;
    }

    // Extract exterior ring coordinates
    std::vector<std::pair<double, double>> exterior_coords;
    exterior_coords.reserve(num_points);

    for (int i = 0; i < num_points; ++i) {
        double x = OGR_G_GetX(exterior_ring, i);
        double y = OGR_G_GetY(exterior_ring, i);

        // Validate coordinates
        if (!std::isfinite(x) || !std::isfinite(y)) {
            logger_.warning("Invalid coordinates: x=" + std::to_string(x) + ", y=" + std::to_string(y));
            return false;
        }

        exterior_coords.emplace_back(x, y);
    }

    // Add exterior ring to polygon data
    polygon_data.rings.push_back(std::move(exterior_coords));

    // Extract interior rings (holes) - only if remove_holes is false
    int total_interior_rings = OGR_G_GetGeometryCount(geometry) - 1;
    if (total_interior_rings > 0) {
        logger_.debug("Polygon has " + std::to_string(total_interior_rings) + " interior rings, remove_holes=" + std::to_string(config_.remove_holes));
    }
    if (!config_.remove_holes) {
        int num_interior_rings = total_interior_rings; // First is exterior
        for (int ring_idx = 1; ring_idx <= num_interior_rings; ++ring_idx) {
        OGRGeometryH interior_ring = OGR_G_GetGeometryRef(geometry, ring_idx);
        if (!interior_ring) {
            logger_.debug("Failed to get interior ring " + std::to_string(ring_idx));
            continue;
        }

        int hole_num_points = OGR_G_GetPointCount(interior_ring);
        if (hole_num_points < 3) {
            logger_.debug("Interior ring " + std::to_string(ring_idx) +
                         " has fewer than 3 points: " + std::to_string(hole_num_points));
            continue;
        }

        std::vector<std::pair<double, double>> hole_coords;
        hole_coords.reserve(hole_num_points);

        bool valid_hole = true;
        for (int i = 0; i < hole_num_points; ++i) {
            double x = OGR_G_GetX(interior_ring, i);
            double y = OGR_G_GetY(interior_ring, i);

            // Validate coordinates
            if (!std::isfinite(x) || !std::isfinite(y)) {
                logger_.debug("Invalid hole coordinates: x=" + std::to_string(x) +
                             ", y=" + std::to_string(y) + ", skipping hole");
                valid_hole = false;
                break;
            }

            hole_coords.emplace_back(x, y);
        }

        if (valid_hole && hole_coords.size() >= 3) {
            polygon_data.rings.push_back(std::move(hole_coords));
        }
        }  // end for loop over interior rings
    } else {
        // Holes are being removed as per config
        int num_holes = OGR_G_GetGeometryCount(geometry) - 1;
        if (num_holes > 0) {
            logger_.debug("Skipped " + std::to_string(num_holes) + " interior rings (remove_holes=true)");
        }
    }

    // Verification: confirm holes were removed if configured
    if (config_.remove_holes && polygon_data.rings.size() > 1) {
        logger_.warning("PROBLEM: Polygon has " + std::to_string(polygon_data.rings.size() - 1) +
                       " holes in PolygonData despite remove_holes=true!");
    }

    return true;
}

std::vector<ContourLayer> ContourGenerator::generate_contours_with_gdal_api(
    const float* elevation_data,
    size_t width, size_t height,
    const double* geotransform,
    const std::vector<double>& levels) const {

    std::vector<ContourLayer> contour_layers;

    logger_.info("🔍 ENTERED generate_contours_with_gdal_api() - BINARY MASK CODE PATH");
    logger_.debug("Generating contours using GDAL's native API (CGAL-free)...");

    // Enhanced elevation data statistics and validation for debugging
    float min_elev = std::numeric_limits<float>::max();
    float max_elev = std::numeric_limits<float>::lowest();
    size_t valid_pixels = 0;
    size_t nodata_pixels = 0;
    size_t extreme_pixels = 0;

    // Also track elevation distribution
    std::vector<float> sample_values;
    sample_values.reserve(std::min(size_t(1000), width * height)); // Sample up to 1000 values

    for (size_t i = 0; i < width * height; ++i) {
        float val = elevation_data[i];
        if (val <= -30000) {
            nodata_pixels++;
        } else if (val < -10000 || val > 10000) {
            extreme_pixels++;
        } else {
            min_elev = std::min(min_elev, val);
            max_elev = std::max(max_elev, val);
            valid_pixels++;

            // Collect sample values for distribution analysis
            if (sample_values.size() < 1000 && i % (width * height / 1000 + 1) == 0) {
                sample_values.push_back(val);
            }
        }
    }

    logger_.info("=== GDAL Elevation Data Diagnostics ===");
    logger_.info("Dataset dimensions: " + std::to_string(width) + " x " + std::to_string(height) + " = " + std::to_string(width * height) + " pixels");
    logger_.info("Valid pixels: " + std::to_string(valid_pixels) + " / " + std::to_string(width * height) + " (" + std::to_string(100.0 * valid_pixels / (width * height)) + "%)");
    logger_.info("NoData pixels (<= -30000): " + std::to_string(nodata_pixels));
    logger_.info("Extreme value pixels: " + std::to_string(extreme_pixels));

    if (valid_pixels > 0) {
        logger_.info("Elevation range: " + std::to_string(min_elev) + "m to " + std::to_string(max_elev) + "m");
        logger_.info("Elevation span: " + std::to_string(max_elev - min_elev) + "m");
    }

    // Log geotransform details
    logger_.info("=== Geotransform Parameters ===");
    logger_.info("Origin (top-left): (" + std::to_string(geotransform[0]) + ", " + std::to_string(geotransform[3]) + ")");
    logger_.info("Pixel size: " + std::to_string(geotransform[1]) + " x " + std::to_string(std::abs(geotransform[5])) + " degrees");
    logger_.info("Rotation: " + std::to_string(geotransform[2]) + ", " + std::to_string(geotransform[4]));

    // Calculate geographic bounds
    double min_lon = geotransform[0];
    double max_lon = geotransform[0] + width * geotransform[1];
    double max_lat = geotransform[3];
    double min_lat = geotransform[3] + height * geotransform[5];  // geotransform[5] is negative
    logger_.info("Geographic bounds: (" + std::to_string(min_lon) + ", " + std::to_string(min_lat) +
                 ") to (" + std::to_string(max_lon) + ", " + std::to_string(max_lat) + ")");

    // Log contour levels
    logger_.info("=== Contour Levels ===");
    logger_.info("Number of contour levels: " + std::to_string(levels.size()));
    if (!levels.empty()) {
        logger_.info("Level range: " + std::to_string(levels.front()) + "m to " + std::to_string(levels.back()) + "m");
        if (levels.size() > 1) {
            double avg_interval = (levels.back() - levels.front()) / (levels.size() - 1);
            logger_.info("Average level interval: " + std::to_string(avg_interval) + "m");
        }
        // Log first few and last few levels
        size_t num_to_show = std::min(size_t(5), levels.size());
        std::string levels_str = "First " + std::to_string(num_to_show) + " levels: ";
        for (size_t i = 0; i < num_to_show; ++i) {
            levels_str += std::to_string(levels[i]) + "m";
            if (i < num_to_show - 1) levels_str += ", ";
        }
        logger_.info(levels_str);
        if (levels.size() > num_to_show) {
            levels_str = "Last " + std::to_string(num_to_show) + " levels: ";
            for (size_t i = levels.size() - num_to_show; i < levels.size(); ++i) {
                levels_str += std::to_string(levels[i]) + "m";
                if (i < levels.size() - 1) levels_str += ", ";
            }
            logger_.info(levels_str);
        }
    }
    logger_.info("=================================");

    // MEMORY-BASED DOWNSAMPLING: Check actual memory usage instead of pixel count heuristics
    size_t total_pixels = width * height;
    auto memory_stats = memory_monitor_.get_current_stats();

    logger_.info("Current memory usage: " + std::to_string(memory_stats.heap_used_mb) + " MB");

    if (memory_stats.is_critical_level()) {
        const size_t CRITICAL_THRESHOLD_MB = 4096; // 4GB critical threshold (from MemoryStats::is_critical_level())
        logger_.error("CRITICAL MEMORY PRESSURE: " + std::to_string(memory_stats.heap_used_mb) +
                        " MB used (threshold: " + std::to_string(CRITICAL_THRESHOLD_MB) + " MB)");
        logger_.error("Implementing automatic downsampling to prevent OOM termination");

        // Determine downsampling factor based on memory pressure
        // Start with 2x and increase if still under heavy memory pressure
        int downsample_factor = 2;
        double memory_pressure_ratio = static_cast<double>(memory_stats.heap_used_mb) / CRITICAL_THRESHOLD_MB;

        if (memory_pressure_ratio > 0.95) {
            downsample_factor = 8; // Extreme memory pressure
            logger_.error("EXTREME memory pressure (" + std::to_string(memory_pressure_ratio * 100) + "% of critical), using 8x downsampling");
        } else if (memory_pressure_ratio > 0.85) {
            downsample_factor = 4; // High memory pressure
            logger_.warning("HIGH memory pressure (" + std::to_string(memory_pressure_ratio * 100) + "% of critical), using 4x downsampling");
        } else {
            logger_.warning("MODERATE memory pressure (" + std::to_string(memory_pressure_ratio * 100) + "% of critical), using 2x downsampling");
        }

        size_t new_width = width / downsample_factor;
        size_t new_height = height / downsample_factor;
        logger_.warning("Downsampling: " + std::to_string(width) + "x" + std::to_string(height) +
                       " (" + std::to_string(total_pixels / 1000.0) + "k pixels) -> " +
                       std::to_string(new_width) + "x" + std::to_string(new_height) +
                       " (" + std::to_string((new_width * new_height) / 1000.0) + "k pixels)");

        std::vector<float> downsampled_data(new_width * new_height);

        for (size_t y = 0; y < new_height; ++y) {
            for (size_t x = 0; x < new_width; ++x) {
                // Sample from NxN grid and take the average of valid values
                size_t orig_x = x * downsample_factor;
                size_t orig_y = y * downsample_factor;

                std::vector<float> samples;
                samples.reserve(downsample_factor * downsample_factor);

                // Collect valid samples from NxN neighborhood
                for (int dy = 0; dy < downsample_factor && orig_y + dy < height; ++dy) {
                    for (int dx = 0; dx < downsample_factor && orig_x + dx < width; ++dx) {
                        float val = elevation_data[(orig_y + dy) * width + (orig_x + dx)];
                        if (val > -30000) { // Valid data
                            samples.push_back(val);
                        }
                    }
                }

                if (!samples.empty()) {
                    // Average valid samples
                    float sum = 0;
                    for (float val : samples) sum += val;
                    downsampled_data[y * new_width + x] = sum / samples.size();
                } else {
                    downsampled_data[y * new_width + x] = -30000; // NoData
                }
            }
        }

        logger_.info("Downsampling completed: " + std::to_string(downsampled_data.size()) + " pixels (" + std::to_string(downsampled_data.size() / 1000.0) + "k)");

        // CRITICAL FIX: Calculate actual elevation range from downsampled data
        // Levels must be regenerated to match the new data range after downsampling
        float ds_min_elev = std::numeric_limits<float>::max();
        float ds_max_elev = std::numeric_limits<float>::lowest();
        for (size_t i = 0; i < downsampled_data.size(); ++i) {
            float val = downsampled_data[i];
            if (val > -30000) { // Valid data
                ds_min_elev = std::min(ds_min_elev, val);
                ds_max_elev = std::max(ds_max_elev, val);
            }
        }

        logger_.info("Downsampled data elevation range: " + std::to_string(ds_min_elev) + "m to " + std::to_string(ds_max_elev) + "m");
        logger_.info("Original levels range: " + std::to_string(levels.front()) + "m to " + std::to_string(levels.back()) + "m");

        // Regenerate levels from downsampled data range
        int num_layers = static_cast<int>(levels.size()) - 1; // levels.size() = num_layers + 1
        std::vector<double> new_levels = generate_contour_levels(ds_min_elev, ds_max_elev, num_layers);

        logger_.info("Regenerated levels for downsampled data: " + std::to_string(new_levels.size()) + " levels");
        logger_.info("New levels range: " + std::to_string(new_levels.front()) + "m to " + std::to_string(new_levels.back()) + "m");

        // Update geotransform for downsampled resolution
        std::vector<double> new_geotransform(geotransform, geotransform + 6);
        new_geotransform[1] *= downsample_factor; // Scale pixel size X by downsample factor
        new_geotransform[5] *= downsample_factor; // Scale pixel size Y by downsample factor (typically negative)

        // Recursively call with downsampled data and regenerated levels
        return generate_contours_with_gdal_api(downsampled_data.data(), new_width, new_height, new_geotransform.data(), new_levels);
    } else if (memory_stats.is_warning_level()) {
        const size_t WARNING_THRESHOLD_MB = 2048; // 2GB warning threshold (from MemoryStats::is_warning_level())
        logger_.warning("ELEVATED memory usage: " + std::to_string(memory_stats.heap_used_mb) +
                       " MB (warning threshold: " + std::to_string(WARNING_THRESHOLD_MB) + " MB)");
        logger_.warning("Proceeding with full-resolution processing, monitoring memory closely");
    } else {
        logger_.info("Memory usage is nominal (" + std::to_string(memory_stats.heap_used_mb) + " MB), proceeding with full-resolution processing");
    }

    if (valid_pixels > 0) {
        logger_.info("Elevation range: " + std::to_string(min_elev) + "m to " + std::to_string(max_elev) + "m");
        logger_.info("Elevation span: " + std::to_string(max_elev - min_elev) + "m");

        // Log sample values for distribution check
        if (!sample_values.empty()) {
            std::sort(sample_values.begin(), sample_values.end());
            logger_.debug("Sample elevation values (" + std::to_string(sample_values.size()) + " samples):");
            logger_.debug("  Min: " + std::to_string(sample_values.front()) + "m");
            logger_.debug("  25%: " + std::to_string(sample_values[sample_values.size() / 4]) + "m");
            logger_.debug("  50%: " + std::to_string(sample_values[sample_values.size() / 2]) + "m");
            logger_.debug("  75%: " + std::to_string(sample_values[3 * sample_values.size() / 4]) + "m");
            logger_.debug("  Max: " + std::to_string(sample_values.back()) + "m");
        }
    } else {
        logger_.error("CRITICAL: No valid elevation data found!");
        return contour_layers;
    }

    // Log and validate contour levels
    logger_.info("=== Contour Level Analysis ===");
    logger_.info("Requested levels (" + std::to_string(levels.size()) + " levels):");

    size_t levels_in_range = 0;
    size_t levels_below_min = 0;
    size_t levels_above_max = 0;

    for (size_t i = 0; i < levels.size(); ++i) {
        double level = levels[i];
        logger_.info("  Level " + std::to_string(i) + ": " + std::to_string(level) + "m");

        if (level < min_elev) {
            levels_below_min++;
            logger_.warning("    WARNING: Level " + std::to_string(level) + "m is below minimum elevation " + std::to_string(min_elev) + "m");
        } else if (level > max_elev) {
            levels_above_max++;
            logger_.warning("    WARNING: Level " + std::to_string(level) + "m is above maximum elevation " + std::to_string(max_elev) + "m");
        } else {
            levels_in_range++;
        }
    }

    logger_.info("Level validation: " + std::to_string(levels_in_range) + " in range, " +
                std::to_string(levels_below_min) + " below min, " + std::to_string(levels_above_max) + " above max");

    if (levels_in_range == 0) {
        logger_.error("CRITICAL: No contour levels fall within the elevation data range!");
        logger_.error("Data range: " + std::to_string(min_elev) + "m to " + std::to_string(max_elev) + "m");
        logger_.error("This explains why GDAL generates no contours.");
        return contour_layers;
    }

    // Create GDAL memory dataset from elevation data
    GDALDriverH memory_driver = GDALGetDriverByName("MEM");
    if (!memory_driver) {
        throw std::runtime_error("Could not get GDAL MEM driver");
    }

    GDALDatasetH dataset = GDALCreate(memory_driver, "", static_cast<int>(width), static_cast<int>(height), 1, GDT_Float32, nullptr);
    if (!dataset) {
        throw std::runtime_error("Could not create GDAL memory dataset");
    }

    // Set geotransform and validate
    logger_.debug("=== Geotransform Setup ===");
    logger_.debug("Geotransform: [" + std::to_string(geotransform[0]) + ", " + std::to_string(geotransform[1]) + ", " +
                 std::to_string(geotransform[2]) + ", " + std::to_string(geotransform[3]) + ", " +
                 std::to_string(geotransform[4]) + ", " + std::to_string(geotransform[5]) + "]");

    CPLErr gt_err = GDALSetGeoTransform(dataset, const_cast<double*>(geotransform));
    if (gt_err != CE_None) {
        logger_.warning("Failed to set geotransform, continuing anyway...");
    }

    // Write elevation data to dataset with validation
    logger_.debug("=== Writing Elevation Data to GDAL Dataset ===");
    GDALRasterBandH band = GDALGetRasterBand(dataset, 1);
    if (!band) {
        GDALClose(dataset);
        throw std::runtime_error("Failed to get raster band from GDAL dataset");
    }

    // Set NoData value
    CPLErr nodata_err = GDALSetRasterNoDataValue(band, -30000.0);
    if (nodata_err != CE_None) {
        logger_.warning("Failed to set NoData value, continuing anyway...");
    }

    // Write the elevation data
    CPLErr err = GDALRasterIO(band, GF_Write, 0, 0, static_cast<int>(width), static_cast<int>(height),
                             const_cast<float*>(elevation_data), static_cast<int>(width), static_cast<int>(height),
                             GDT_Float32, 0, 0);

    if (err != CE_None) {
        logger_.error("Failed to write elevation data to GDAL dataset, error code: " + std::to_string(err));
        GDALClose(dataset);
        throw std::runtime_error("Failed to write elevation data to GDAL dataset");
    }

    logger_.debug("Successfully wrote " + std::to_string(width * height) + " elevation values to GDAL dataset");

    // Verify data was written correctly by reading back a few sample values
    std::vector<float> verification_data(width * height);
    CPLErr read_err = GDALRasterIO(band, GF_Read, 0, 0, static_cast<int>(width), static_cast<int>(height),
                                  verification_data.data(), static_cast<int>(width), static_cast<int>(height),
                                  GDT_Float32, 0, 0);

    if (read_err == CE_None) {
        // Check a few sample points
        bool data_matches = true;
        for (size_t i = 0; i < std::min(size_t(10), width * height); i += width * height / 10) {
            if (std::abs(verification_data[i] - elevation_data[i]) > 1e-6) {
                data_matches = false;
                logger_.warning("Data mismatch at index " + std::to_string(i) + ": wrote " +
                               std::to_string(elevation_data[i]) + ", read " + std::to_string(verification_data[i]));
            }
        }
        if (data_matches) {
            logger_.debug("Data verification successful - GDAL dataset contains expected values");
        }
    } else {
        logger_.warning("Could not verify written data, continuing anyway...");
    }

    // === DIAGNOSTIC OUTPUT ===
    // Debug file creation removed as no longer needed

    // Create memory layer to collect all contours
    OGRSFDriverH ogr_driver = OGRGetDriverByName("MEM");
    OGRDataSourceH datasource = OGR_Dr_CreateDataSource(ogr_driver, "", nullptr);
    if (!datasource) {
        logger_.error("Failed to create OGR memory datasource");
        GDALClose(dataset);
        return contour_layers;
    }

    // Use polygon geometry type for GDAL polygon extraction
    OGRwkbGeometryType geom_type = wkbPolygon;

    OGRLayerH layer = OGR_DS_CreateLayer(datasource, "contours", nullptr, geom_type, nullptr);
    if (!layer) {
        logger_.error("Failed to create OGR contour layer");
        OGR_DS_Destroy(datasource);
        GDALClose(dataset);
        return contour_layers;
    }

    // Add fields based on mode
    logger_.debug("=== Setting up OGR Layer ===");

    // Create ELEV_MIN field for binary mask approach
    logger_.info("Creating ELEV_MIN field for binary mask polygons");
    OGRFieldDefnH field_defn = OGR_Fld_Create("ELEV_MIN", OFTReal);
    if (!field_defn) {
        logger_.error("Failed to create ELEV_MIN field definition");
        OGR_DS_Destroy(datasource);
        GDALClose(dataset);
        return contour_layers;
    }

    OGRErr field_err = OGR_L_CreateField(layer, field_defn, FALSE);
    if (field_err != OGRERR_NONE) {
        logger_.error("Failed to create ELEV_MIN field in layer, error: " + std::to_string(field_err));
        OGR_Fld_Destroy(field_defn);
        OGR_DS_Destroy(datasource);
        GDALClose(dataset);
        return contour_layers;
    }

    OGR_Fld_Destroy(field_defn);

    // Verify fields were created
    OGRFeatureDefnH layer_defn = OGR_L_GetLayerDefn(layer);
    int field_count = OGR_FD_GetFieldCount(layer_defn);
    int elev_field_index = OGR_FD_GetFieldIndex(layer_defn, "ELEV");

    logger_.debug("OGR layer setup complete:");
    logger_.debug("  Geometry type: " + std::string(geom_type == wkbPolygon ? "Polygon" : "LineString"));
    logger_.debug("  Field count: " + std::to_string(field_count));

    // GDAL polygon mode debug output
    logger_.debug("  ID field index: " + std::to_string(OGR_FD_GetFieldIndex(layer_defn, "ID")));
    logger_.debug("  ELEV_MIN field index: " + std::to_string(OGR_FD_GetFieldIndex(layer_defn, "ELEV_MIN")));
    logger_.debug("  ELEV_MAX field index: " + std::to_string(OGR_FD_GetFieldIndex(layer_defn, "ELEV_MAX")));

    // Generate contours directly from elevation data at all levels at once
    std::vector<double> contour_levels;
    for (const auto& level : levels) {
        contour_levels.push_back(level);
    }

    logger_.info("Generating contours at " + std::to_string(contour_levels.size()) + " elevation levels");
    std::string levels_str = "Contour levels: ";
    for (size_t i = 0; i < contour_levels.size(); ++i) {
        levels_str += std::to_string(contour_levels[i]);
        if (i < contour_levels.size() - 1) levels_str += ", ";
    }
    logger_.debug(levels_str);

    // CRITICAL FIX: Correct NoData handling AND output field specification
    logger_.info("=== Generating Contours with GDAL ===");
    logger_.debug("Using parameters:");
    logger_.debug("  Band: " + std::to_string(reinterpret_cast<uintptr_t>(band)));
    logger_.debug("  Fixed level count: " + std::to_string(contour_levels.size()));
    logger_.debug("  bUseNoData: TRUE (corrected)");
    logger_.debug("  dfNoDataValue: -30000.0");
    logger_.debug("  iIDField: " + std::to_string(elev_field_index) + " (CORRECTED: was -1, now using ELEV field for both ID and elevation)");
    logger_.debug("  iElevField: " + std::to_string(elev_field_index));

    // CRITICAL BUG FIX: iIDField was -1, which prevents GDAL from writing proper feature data
    // Setting iIDField = elev_field_index to use the ELEV field for both ID and elevation
    // This matches the behavior of the working external gdal_contour command

    CPLErr contour_err = CE_None;
    int total_features = 0;  // Track feature count to avoid expensive OGR_L_GetFeatureCount call

    // DEBUG: Check config value before binary mask check
    // Use binary mask approach to match Python algorithm (GDAL polygon mode)
    logger_.info("Using binary mask + polygonize approach");

    // Get raster dimensions
    int raster_width = GDALGetRasterBandXSize(band);
    int raster_height = GDALGetRasterBandYSize(band);

    logger_.info("Raster dimensions: " + std::to_string(raster_width) + "x" + std::to_string(raster_height));

    // Read elevation data into memory
    std::vector<float> elevation_buffer(raster_width * raster_height);
    CPLErr elev_read_err = GDALRasterIO(
        band, GF_Read,
        0, 0, raster_width, raster_height,
        elevation_buffer.data(), raster_width, raster_height,
        GDT_Float32, 0, 0
    );

    if (elev_read_err != CE_None) {
        logger_.error("Failed to read elevation data");
        OGR_DS_Destroy(datasource);
        GDALClose(dataset);
        return contour_layers;
        }

        // Sample elevation values for diagnostic purposes
        if (logger_.shouldOutput(LogLevel::DEBUG)) {
            std::ostringstream sample_msg;
            sample_msg << "Elevation buffer sample (first 10 values): ";
            for (int s = 0; s < std::min(10, raster_width * raster_height); ++s) {
                sample_msg << elevation_buffer[s] << " ";
            }
            logger_.debug(sample_msg.str());

            // Count pixels below typical thresholds
            int below_1000 = 0, below_2000 = 0, below_3000 = 0;
            for (int s = 0; s < raster_width * raster_height; ++s) {
                if (elevation_buffer[s] > -30000.0) {
                    if (elevation_buffer[s] < 1000) below_1000++;
                    if (elevation_buffer[s] < 2000) below_2000++;
                    if (elevation_buffer[s] < 3000) below_3000++;
                }
            }
            std::ostringstream threshold_msg;
            threshold_msg << "Pixels below thresholds: <1000m=" << below_1000
                         << ", <2000m=" << below_2000 << ", <3000m=" << below_3000;
            logger_.debug(threshold_msg.str());
        }

        logger_.info("Processing " + std::to_string(contour_levels.size() - 1) + " elevation bands");

        int prev_feature_count = 0;  // Track feature count to identify newly added features
        int prev_pixels_at_or_above = -1;  // Track pixel counts to verify cumulative logic

        // Process each elevation band [lower, upper)
        for (size_t i = 0; i < contour_levels.size() - 1; ++i) {
        double lower_elev = contour_levels[i];
        double upper_elev = contour_levels[i + 1];

        logger_.debug("Processing band " + std::to_string(i) + ": [" +
                     std::to_string(lower_elev) + ", " + std::to_string(upper_elev) + ")");

        // Create binary mask: 1 where elevation in range [lower, upper), 0 elsewhere
        std::vector<uint8_t> mask_buffer(raster_width * raster_height);
        bool has_pixels = false;

        // DEBUG: Track elevation statistics
        int pixels_at_or_above = 0;  // Cumulative: all terrain >= lower_elev
        int pixels_below = 0;
        int pixels_nodata = 0;
        double min_elev_in_data = 1e9;
        double max_elev_in_data = -1e9;

        // CUMULATIVE APPROACH: Include all pixels at or above lower_elev
        // This represents solid material (rock/dirt) at this elevation and above
        // Creates nesting layers where higher layers sit on top of lower ones
        for (int pixel = 0; pixel < raster_width * raster_height; ++pixel) {
            float elev = elevation_buffer[pixel];

            // Track min/max elevations in the data
            if (elev > -30000.0) {
                min_elev_in_data = std::min(min_elev_in_data, static_cast<double>(elev));
                max_elev_in_data = std::max(max_elev_in_data, static_cast<double>(elev));
            }

            // Cumulative: include all pixels at or above this layer's lower elevation
            if (elev <= -30000.0) {
                mask_buffer[pixel] = 0;
                pixels_nodata++;
            } else if (elev >= lower_elev) {
                mask_buffer[pixel] = 1;
                has_pixels = true;
                pixels_at_or_above++;
            } else {  // elev < lower_elev
                mask_buffer[pixel] = 0;
                pixels_below++;
            }
        }


        // Log elevation statistics for debugging
        logger_.info("Mask band " + std::to_string(i) + " [>= " + std::to_string(lower_elev) + "m]: " +
                     std::to_string(pixels_at_or_above) + " pixels at/above (cumulative), " +
                     std::to_string(pixels_below) + " pixels below, " +
                     std::to_string(pixels_nodata) + " nodata");

        // Verify subset relationship - each band should have <= previous band pixels
        if (prev_pixels_at_or_above >= 0 && pixels_at_or_above > prev_pixels_at_or_above) {
            logger_.error("Band " + std::to_string(i) + " has MORE pixels (" +
                         std::to_string(pixels_at_or_above) + ") than previous band (" +
                         std::to_string(prev_pixels_at_or_above) + ") - cumulative logic VIOLATED!");
        }
        prev_pixels_at_or_above = pixels_at_or_above;

        // Apply inset if enabled (cut holes for next layer with offset)
        if (config_.inset_upper_layers && (i + 1 < contour_levels.size() - 1)) {
            // CUMULATIVE: Create mask for NEXT layer (all terrain >= next_lower)
            double next_lower = contour_levels[i + 1];

            logger_.debug("Applying inset: creating mask for next layer [>= " +
                         std::to_string(next_lower) + "m] (cumulative)");

            std::vector<uint8_t> next_mask(raster_width * raster_height, 0);
            bool next_has_pixels = false;

            // Create cumulative mask for next layer (all terrain at or above)
            for (int pixel = 0; pixel < raster_width * raster_height; ++pixel) {
                float elev = elevation_buffer[pixel];

                if (elev >= next_lower && elev > -30000.0) {
                    next_mask[pixel] = 1;
                    next_has_pixels = true;
                }
            }

            if (next_has_pixels) {
                // Convert inset offset from mm to pixels
                // geotransform[1] = pixel width in geographic units (degrees for WGS84)
                // Approximate conversion: 1 degree ≈ 111km at equator
                // For more accuracy, we'd need the actual scale factor, but for now use pixel size directly
                double pixel_size_deg = std::abs(geotransform[1]);
                // Approximate meters per degree at this latitude (simplified)
                double approx_meters_per_deg = 111000.0; // At equator
                double pixel_size_m = pixel_size_deg * approx_meters_per_deg;
                double inset_offset_m = config_.inset_offset_mm / 1000.0;
                int erosion_pixels = std::max(1, static_cast<int>(std::ceil(inset_offset_m / pixel_size_m)));

                logger_.debug("Inset offset: " + std::to_string(config_.inset_offset_mm) + "mm = " +
                             std::to_string(erosion_pixels) + " pixels (pixel size: " +
                             std::to_string(pixel_size_m) + "m)");

                // Erode next layer mask: simple morphological erosion
                // Set pixels to 0 if they're on the boundary (have a 0-neighbor within erosion radius)
                std::vector<uint8_t> eroded_mask = next_mask;
                for (int y = 0; y < raster_height; ++y) {
                    for (int x = 0; x < raster_width; ++x) {
                        int idx = y * raster_width + x;
                        if (next_mask[idx] == 1) {
                            // Check if this pixel is on the boundary (has 0-neighbor)
                            bool on_boundary = false;
                            for (int dy = -erosion_pixels; dy <= erosion_pixels && !on_boundary; ++dy) {
                                for (int dx = -erosion_pixels; dx <= erosion_pixels && !on_boundary; ++dx) {
                                    int ny = y + dy;
                                    int nx = x + dx;
                                    if (ny >= 0 && ny < raster_height && nx >= 0 && nx < raster_width) {
                                        int nidx = ny * raster_width + nx;
                                        if (next_mask[nidx] == 0) {
                                            // Check if within circular erosion radius
                                            double dist = std::sqrt(dx * dx + dy * dy);
                                            if (dist <= erosion_pixels) {
                                                on_boundary = true;
                                            }
                                        }
                                    } else {
                                        // Edge of raster is also a boundary
                                        on_boundary = true;
                                    }
                                }
                            }
                            if (on_boundary) {
                                eroded_mask[idx] = 0;
                            }
                        }
                    }
                }

                // Subtract eroded next layer mask from current layer mask
                int pixels_removed = 0;
                for (int pixel = 0; pixel < raster_width * raster_height; ++pixel) {
                    if (mask_buffer[pixel] == 1 && eroded_mask[pixel] == 1) {
                        mask_buffer[pixel] = 0;
                        pixels_removed++;
                    }
                }

                logger_.debug("Inset removed " + std::to_string(pixels_removed) + " pixels from current layer");

                // Update has_pixels flag
                has_pixels = false;
                for (int pixel = 0; pixel < raster_width * raster_height; ++pixel) {
                    if (mask_buffer[pixel] == 1) {
                        has_pixels = true;
                        break;
                    }
                }
            } else {
                logger_.debug("Next layer has no pixels, skipping inset");
            }
        }

        if (!has_pixels) {
            logger_.debug("Band " + std::to_string(i) + " has no pixels, skipping");
            continue;
        }

        // Create in-memory raster for the binary mask
        GDALDriver* mem_driver = GetGDALDriverManager()->GetDriverByName("MEM");
        if (!mem_driver) {
            logger_.error("Failed to get MEM driver");
            continue;
        }

        GDALDataset* mask_dataset = mem_driver->Create("", raster_width, raster_height, 1, GDT_Byte, nullptr);
        if (!mask_dataset) {
            logger_.error("Failed to create mask dataset");
            continue;
        }

        // Set same geotransform as source
        double gt[6];
        GDALGetGeoTransform(dataset, gt);
        mask_dataset->SetGeoTransform(gt);
        mask_dataset->SetProjection(GDALGetProjectionRef(dataset));

        // Write mask data
        GDALRasterBand* mask_band = mask_dataset->GetRasterBand(1);

        CPLErr write_err = mask_band->RasterIO(
            GF_Write, 0, 0, raster_width, raster_height,
            mask_buffer.data(), raster_width, raster_height,
            GDT_Byte, 0, 0
        );

        if (write_err != CE_None) {
            logger_.error("Failed to write mask data");
            GDALClose(mask_dataset);
            continue;
        }

        // DIAGNOSTIC: Export mask for first few bands (AFTER writing data!)
        if (i < 3) {
            std::string mask_filename = "/tmp/mask_band_" + std::to_string(i) + ".tif";
            GDALDriver* gtiff_driver = GetGDALDriverManager()->GetDriverByName("GTiff");
            if (gtiff_driver) {
                GDALDataset* debug_ds = gtiff_driver->CreateCopy(mask_filename.c_str(), mask_dataset, FALSE, nullptr, nullptr, nullptr);
                if (debug_ds) {
                    GDALClose(debug_ds);
                    logger_.debug("Exported mask to " + mask_filename);
                }
            }
        }

        // Polygonize the binary mask
        logger_.debug("Polygonizing mask for band " + std::to_string(i));

        // Use 8-connectedness to reduce polygon complexity and self-intersections
        // 8-connected mode considers diagonal neighbors, producing simpler polygons
        char** polygonize_options = nullptr;
        polygonize_options = CSLSetNameValue(polygonize_options, "8CONNECTED", "YES");

        CPLErr poly_err = GDALPolygonize(
            mask_band,          // Source raster band
            mask_band,          // Use same band as mask (only polygonize non-zero pixels)
            layer,              // Destination layer
            -1,                 // No field index (we'll add ELEV_MIN field manually)
            polygonize_options, // Options: use 8-connectedness
            nullptr,            // No progress callback
            nullptr             // No progress data
        );

        CSLDestroy(polygonize_options);

        GDALClose(mask_dataset);

        if (poly_err != CE_None) {
            logger_.error("Polygonize failed for band " + std::to_string(i));
            continue;
        }

        // GDALPolygonize adds features without the elevation attribute
        // We need to update the newly added features with the elevation
        OGR_L_SyncToDisk(layer);
        int new_feature_count = OGR_L_GetFeatureCount(layer, FALSE);
        int features_added = new_feature_count - prev_feature_count;

        // Get ELEV_MIN field index
        OGRFeatureDefnH layer_defn = OGR_L_GetLayerDefn(layer);
        int elev_min_field = OGR_FD_GetFieldIndex(layer_defn, "ELEV_MIN");

        if (elev_min_field >= 0 && features_added > 0) {
            // Set ELEV_MIN ONLY on newly added features (FID >= prev_feature_count)
            [[maybe_unused]] int updated = 0;
            for (int fid = prev_feature_count; fid < new_feature_count; ++fid) {
                OGRFeatureH feature = OGR_L_GetFeature(layer, fid);
                if (feature) {
                    OGR_F_SetFieldDouble(feature, elev_min_field, lower_elev);
                    OGRErr set_err = OGR_L_SetFeature(layer, feature);
                    if (set_err == OGRERR_NONE) {
                        updated++;
                    }
                    OGR_F_Destroy(feature);
                }
            }
        } else if (elev_min_field < 0) {
        }

        prev_feature_count = new_feature_count;  // Update for next iteration

        logger_.info("Band " + std::to_string(i) + " generated " +
                     std::to_string(features_added) + " new features (" +
                     std::to_string(new_feature_count) + " total cumulative)");
        }

    contour_err = CE_None;  // Success
    total_features = prev_feature_count;  // Save feature count to avoid expensive OGR_L_GetFeatureCount call
    logger_.info("Binary mask approach completed with " + std::to_string(total_features) + " features");
    logger_.debug("Proceeding to post-processing");

    logger_.debug("GDAL contour generation returned with error code: " + std::to_string(contour_err));

    if (contour_err != CE_None) {
        logger_.error("GDAL contour generation failed");
        OGR_DS_Destroy(datasource);
        GDALClose(dataset);
        return contour_layers;
    }

    // For binary mask mode, we already have the feature count from processing
    logger_.info("=== Post-Contour Generation Analysis ===");
    logger_.info("GDAL generated " + std::to_string(total_features) + " contour features");

    // For polygon mode, read ELEV_MIN field instead of ELEV
    int read_field_index = OGR_FD_GetFieldIndex(layer_defn, "ELEV_MIN");
    logger_.info("Reading ELEV_MIN field (index: " + std::to_string(read_field_index) + ")");

    // Skip expensive diagnostic iterations for binary mask mode - we already know the feature count and elevations
    if (false) {  // Disabled: non-GDAL mode
        // DEBUG: Collect all unique elevations actually generated
        std::set<double> unique_generated_elevations;
        OGR_L_ResetReading(layer);
        OGRFeatureH temp_feature;

        while ((temp_feature = OGR_L_GetNextFeature(layer)) != nullptr) {
        if (read_field_index >= 0) {
            double elev = OGR_F_GetFieldAsDouble(temp_feature, read_field_index);
            unique_generated_elevations.insert(elev);
        }
        OGR_F_Destroy(temp_feature);
    }
    OGR_L_ResetReading(layer);

    logger_.info("Found " + std::to_string(unique_generated_elevations.size()) + " unique elevation levels in generated contours:");
    for (double elev : unique_generated_elevations) {
        // Check if this matches a requested level
        bool matches_requested = false;
        for (double req_level : levels) {
            if (std::abs(elev - req_level) < 0.1) {
                matches_requested = true;
                break;
            }
        }
        logger_.info("  " + std::to_string(elev) + "m" + (matches_requested ? " [REQUESTED]" : " [UNEXPECTED]"));
    }

    // DEBUG: Export contours to GeoJSON for inspection
    try {
        std::filesystem::create_directories("test/output");
        std::string debug_geojson = "test/output/debug_gdal_api_contours.geojson";

        GDALDriverH geojson_driver = OGRGetDriverByName("GeoJSON");
        if (geojson_driver) {
            GDALDatasetH export_ds = GDALCreate(geojson_driver, debug_geojson.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
            if (export_ds) {
                OGRLayerH export_layer = OGR_DS_CreateLayer(export_ds, "contours", nullptr, wkbLineString, nullptr);
                if (export_layer) {
                    // Copy field definition
                    OGRFieldDefnH export_field = OGR_Fld_Create("ELEV", OFTReal);
                    OGR_L_CreateField(export_layer, export_field, FALSE);
                    OGR_Fld_Destroy(export_field);

                    // Copy all features
                    OGR_L_ResetReading(layer);
                    OGRFeatureH copy_feature;
                    int copied = 0;
                    while ((copy_feature = OGR_L_GetNextFeature(layer)) != nullptr) {
                        OGRFeatureH new_feature = OGR_F_Create(OGR_L_GetLayerDefn(export_layer));
                        OGR_F_SetGeometry(new_feature, OGR_F_GetGeometryRef(copy_feature));
                        if (read_field_index >= 0) {
                            OGR_F_SetFieldDouble(new_feature, 0, OGR_F_GetFieldAsDouble(copy_feature, read_field_index));
                        }
                        OGRErr err = OGR_L_CreateFeature(export_layer, new_feature);
                        if (err == OGRERR_NONE) {
                            copied++;
                        }
                        OGR_F_Destroy(new_feature);
                        OGR_F_Destroy(copy_feature);
                    }
                    OGR_L_ResetReading(layer);
                    logger_.info("DEBUG: Exported " + std::to_string(copied) + " contour features to " + debug_geojson);
                }
                GDALClose(export_ds);
            }
        }
    } catch (const std::exception& e) {
        logger_.warning("Could not export debug GeoJSON: " + std::string(e.what()));
    }
    }  // End of if (false) block

    // Additional diagnostics about the layer state
    OGRFeatureDefnH layer_defn_post = OGR_L_GetLayerDefn(layer);
    int field_count_post = OGR_FD_GetFieldCount(layer_defn_post);
    logger_.debug("Layer has " + std::to_string(field_count_post) + " fields after contour generation");

    for (int i = 0; i < field_count_post; ++i) {
        OGRFieldDefnH field_defn_info = OGR_FD_GetFieldDefn(layer_defn_post, i);
        const char* field_name = OGR_Fld_GetNameRef(field_defn_info);
        OGRFieldType field_type = OGR_Fld_GetType(field_defn_info);
        logger_.debug("  Field " + std::to_string(i) + ": " + std::string(field_name) + " (type: " + std::to_string(field_type) + ")");
    }

    // Sample a few features to validate their content
    if (total_features > 0) {
        logger_.debug("=== Sampling Feature Content ===");
        OGR_L_ResetReading(layer);

        int samples_to_check = std::min(3, total_features);
        for (int sample = 0; sample < samples_to_check; ++sample) {
            OGRFeatureH feature = OGR_L_GetNextFeature(layer);
            if (feature) {
                // Get elevation value from the field
                double elevation = (read_field_index >= 0) ? OGR_F_GetFieldAsDouble(feature, read_field_index) : 0.0;
                logger_.debug("  Sample feature " + std::to_string(sample) + " elevation: " + std::to_string(elevation));

                // Get geometry info
                OGRGeometryH geometry = OGR_F_GetGeometryRef(feature);
                if (geometry) {
                    OGRwkbGeometryType geom_type = OGR_G_GetGeometryType(geometry);
                    int point_count = OGR_G_GetPointCount(geometry);
                    logger_.debug("    Geometry type: " + std::to_string(geom_type) + ", Points: " + std::to_string(point_count));
                } else {
                    logger_.warning("    No geometry found in feature!");
                }

                OGR_F_Destroy(feature);
            }
        }

        OGR_L_ResetReading(layer); // Reset for subsequent processing
    }

    // === OGR POLYGON VALIDATION (Pre-CGAL Diagnostic) ===
    // This validation runs BEFORE any CGAL conversion to determine if OGR polygons
    // from GDALPolygonize are valid. If degenerate polygons exist here, the problem
    // is in GDAL/binary mask generation, NOT in CGAL.
    logger_.debug("Validating OGR polygons before CGAL conversion...");

    OGR_L_ResetReading(layer);
    OGRFeatureH diagnostic_feature;
    int diagnostic_count = 0;
    int ogr_degenerate_count = 0;
    std::map<double, std::vector<size_t>> ogr_vertex_counts_by_elevation;

    while ((diagnostic_feature = OGR_L_GetNextFeature(layer)) != nullptr) {
        diagnostic_count++;

        // Get elevation from ELEV_MIN field
        double elev = (read_field_index >= 0) ?
                      OGR_F_GetFieldAsDouble(diagnostic_feature, read_field_index) : 0.0;

        // Get geometry
        OGRGeometryH geom = OGR_F_GetGeometryRef(diagnostic_feature);

        if (geom) {
            OGRwkbGeometryType geom_type = OGR_G_GetGeometryType(geom);

            // Check polygon geometries
            if (geom_type == wkbPolygon) {
                // Get exterior ring
                OGRGeometryH exterior_ring = OGR_G_GetGeometryRef(geom, 0);
                if (exterior_ring) {
                    size_t vertex_count = OGR_G_GetPointCount(exterior_ring);
                    ogr_vertex_counts_by_elevation[elev].push_back(vertex_count);

                    // Detect degenerate polygons (< 3 vertices)
                    if (vertex_count < 3) {
                        ogr_degenerate_count++;
                        logger_.debug("[OGR_VALIDATION] DEGENERATE OGR POLYGON DETECTED!");
                        logger_.debug("[OGR_VALIDATION]   Feature ID: " + std::to_string(diagnostic_count));
                        logger_.debug("[OGR_VALIDATION]   Elevation: " + std::to_string(elev) + "m");
                        logger_.debug("[OGR_VALIDATION]   Vertex count: " + std::to_string(vertex_count));
                        logger_.debug("[OGR_VALIDATION]   *** This polygon came from GDALPolygonize, NOT from CGAL ***");
                    }
                }
            } else if (geom_type == wkbMultiPolygon) {
                // Check each sub-polygon
                int num_geoms = OGR_G_GetGeometryCount(geom);
                for (int i = 0; i < num_geoms; ++i) {
                    OGRGeometryH sub_geom = OGR_G_GetGeometryRef(geom, i);
                    if (sub_geom && OGR_G_GetGeometryType(sub_geom) == wkbPolygon) {
                        OGRGeometryH exterior_ring = OGR_G_GetGeometryRef(sub_geom, 0);
                        if (exterior_ring) {
                            size_t vertex_count = OGR_G_GetPointCount(exterior_ring);
                            ogr_vertex_counts_by_elevation[elev].push_back(vertex_count);

                            if (vertex_count < 3) {
                                ogr_degenerate_count++;
                                logger_.debug("[OGR_VALIDATION] DEGENERATE OGR SUB-POLYGON DETECTED!");
                                logger_.debug("[OGR_VALIDATION]   Feature ID: " + std::to_string(diagnostic_count) +
                                            ", Sub-polygon: " + std::to_string(i));
                                logger_.debug("[OGR_VALIDATION]   Elevation: " + std::to_string(elev) + "m");
                                logger_.debug("[OGR_VALIDATION]   Vertex count: " + std::to_string(vertex_count));
                            }
                        }
                    }
                }
            }
        }

        OGR_F_Destroy(diagnostic_feature);
    }

    // Log summary statistics
    logger_.debug("[OGR_VALIDATION] Analyzed " + std::to_string(diagnostic_count) + " OGR features");

    if (ogr_degenerate_count > 0) {
        logger_.debug("[OGR_VALIDATION] Found " + std::to_string(ogr_degenerate_count) +
                     " DEGENERATE OGR polygons (< 3 vertices)");
        logger_.debug("[OGR_VALIDATION] *** CONCLUSION: Problem is in GDAL/GDALPolygonize, NOT in CGAL ***");
    } else {
        logger_.debug("[OGR_VALIDATION] All OGR polygons are valid (>= 3 vertices)");
        logger_.debug("[OGR_VALIDATION] If CGAL validation reports degenerate polygons, problem is in CGAL operations");
    }

    // Log vertex count statistics by elevation
    for (const auto& [elev, counts] : ogr_vertex_counts_by_elevation) {
        if (!counts.empty()) {
            size_t min_verts = *std::min_element(counts.begin(), counts.end());
            size_t max_verts = *std::max_element(counts.begin(), counts.end());
            size_t avg_verts = std::accumulate(counts.begin(), counts.end(), 0UL) / counts.size();

            logger_.debug("[OGR_VALIDATION] Elevation " + std::to_string(elev) + "m: " +
                        std::to_string(counts.size()) + " polygons, vertex range: " +
                        std::to_string(min_verts) + "-" + std::to_string(max_verts) +
                        " (avg: " + std::to_string(avg_verts) + ")");
        }
    }

    // Reset reading position for subsequent processing
    OGR_L_ResetReading(layer);

    // Convert OGR features to PolygonData (CGAL-free)
    logger_.info("Converting OGR features to polygon data for export");
    logger_.info("Processing " + std::to_string(total_features) + " OGR polygon features");

    // Group features by elevation
    std::map<double, std::vector<ContourLayer::PolygonData>> polygons_by_elevation;

    OGR_L_ResetReading(layer);
    OGRFeatureH feature;
    int features_processed = 0;
    int features_converted = 0;

    while ((feature = OGR_L_GetNextFeature(layer)) != nullptr) {
        features_processed++;

        // Get elevation from ELEV_MIN field
        double elevation = OGR_F_GetFieldAsDouble(feature, read_field_index);

        // Get geometry
        OGRGeometryH geometry = OGR_F_GetGeometryRef(feature);
        if (!geometry) {
            OGR_F_Destroy(feature);
            continue;
        }

        OGRwkbGeometryType geom_type = OGR_G_GetGeometryType(geometry);

        // Only process polygon geometries
        if (geom_type == wkbPolygon || geom_type == wkbMultiPolygon) {
            // Convert OGR polygon to PolygonData
            if (geom_type == wkbPolygon) {
                ContourLayer::PolygonData poly_data;
                if (ogr_polygon_to_polygon_data(geometry, poly_data)) {
                    if (!poly_data.empty()) {
                        polygons_by_elevation[elevation].push_back(std::move(poly_data));
                        features_converted++;
                    } else {
                        logger_.debug("Skipping empty polygon at elevation " +
                                    std::to_string(elevation) + "m");
                    }
                } else {
                    logger_.debug("Failed to convert polygon at elevation " +
                                std::to_string(elevation) + "m");
                }
            } else if (geom_type == wkbMultiPolygon) {
                int num_geoms = OGR_G_GetGeometryCount(geometry);
                for (int i = 0; i < num_geoms; ++i) {
                    OGRGeometryH sub_geom = OGR_G_GetGeometryRef(geometry, i);
                    if (!sub_geom) {
                        continue;
                    }
                    if (OGR_G_GetGeometryType(sub_geom) == wkbPolygon) {
                        ContourLayer::PolygonData poly_data;
                        if (ogr_polygon_to_polygon_data(sub_geom, poly_data)) {
                            if (!poly_data.empty()) {
                                polygons_by_elevation[elevation].push_back(std::move(poly_data));
                                features_converted++;
                            }
                        }
                    }
                }
            }
        }

        OGR_F_Destroy(feature);
    }

    logger_.info("Converted " + std::to_string(features_converted) + "/" +
                std::to_string(features_processed) + " features to polygon data");

    // Convert map to ContourLayer vector and log detailed polygon information
    logger_.info("=== Polygon Extraction Results ===");
    size_t total_polygons = 0;
    size_t total_vertices = 0;
    double total_area = 0.0;

    for (const auto& [elev, polys] : polygons_by_elevation) {
        // Create ContourLayer for this elevation
        ContourLayer layer(elev);
        layer.polygons = polys;

        // Calculate statistics for this elevation level
        size_t level_vertices = 0;
        double level_area = 0.0;
        size_t level_holes = 0;

        for (const auto& poly : polys) {
            // Count vertices in all rings
            for (const auto& ring : poly.rings) {
                level_vertices += ring.size();
            }

            // Count holes (all rings except first)
            if (poly.rings.size() > 1) {
                level_holes += (poly.rings.size() - 1);
            }

            // Calculate area using shoelace formula
            if (!poly.rings.empty()) {
                const auto& exterior = poly.rings[0];
                double poly_area = 0.0;
                for (size_t i = 0; i < exterior.size(); ++i) {
                    size_t j = (i + 1) % exterior.size();
                    poly_area += exterior[i].first * exterior[j].second;
                    poly_area -= exterior[j].first * exterior[i].second;
                }
                level_area += std::abs(poly_area) / 2.0;
            }
        }

        total_polygons += polys.size();
        total_vertices += level_vertices;
        total_area += level_area;

        logger_.info("  Level " + std::to_string(elev) + "m: " +
                    std::to_string(polys.size()) + " polygons, " +
                    std::to_string(level_vertices) + " vertices, " +
                    std::to_string(level_holes) + " holes, " +
                    "area=" + std::to_string(level_area / 1e6) + " km²");

        contour_layers.push_back(std::move(layer));
    }

    logger_.info("Total: " + std::to_string(total_polygons) + " polygons, " +
                std::to_string(total_vertices) + " vertices, " +
                "total_area=" + std::to_string(total_area / 1e6) + " km²");
    logger_.info("Created " + std::to_string(contour_layers.size()) + " elevation levels");
    logger_.info("==================================");

    // Clean up and return
    OGR_DS_Destroy(datasource);
    GDALClose(dataset);
    return contour_layers;
}

void ContourGenerator::initialize_monitoring(bool enable_crash_handlers) {
    // Initialize memory monitoring with appropriate thresholds
    memory_monitor_.set_warning_thresholds(2048, 4096);  // 2GB warning, 4GB critical

    // Initialize stack guard with safety margins (percentage-based thresholds)
    stack_guard_.set_warning_thresholds(75.0, 90.0); // 75% warning, 90% critical

    // Set up monitors for interconnected operation (CGAL wrapper removed)
    crash_handler_.set_monitors(&memory_monitor_, &stack_guard_);

    // Install crash handlers if requested
    if (enable_crash_handlers) {
        crash_handler_.install_handlers();
        crash_handler_.set_auto_reporting(true);
        crash_handler_.set_crash_log_file("/tmp/contour_generation_crash.log");
    }

    logger_.info("Memory and crash monitoring infrastructure initialized");
    logger_.info("Memory thresholds: 2GB warning, 4GB critical");
    logger_.info("Stack thresholds: 75% warning, 90% critical");
    if (enable_crash_handlers) {
        logger_.info("Crash handlers installed with automatic reporting");
    }
}


} // namespace topo
