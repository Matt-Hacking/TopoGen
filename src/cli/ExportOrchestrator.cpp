/**
 * @file ExportOrchestrator.cpp
 * @brief Implementation of export orchestration
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "ExportOrchestrator.hpp"
#include "topographic_generator.hpp"
#include "LabelRenderer.hpp"
#include "UnitParser.hpp"
#include "../core/TopographicMesh.hpp"
#include "../core/ContourGenerator.hpp"
#include "../core/ScalingCalculator.hpp"
#include "../export/MultiFormatExporter.hpp"
#include "../export/SVGExporter.hpp"
#include "../export/GeoJSONExporter.hpp"
#include "../export/ShapefileExporter.hpp"
#include "../export/PNGExporter.hpp"
#include "../export/GeoTIFFExporter.hpp"
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace topo {

ExportOrchestrator::ExportOrchestrator(const TopographicGenerator& generator)
    : generator_(generator)
    , logger_("ExportOrchestrator")
{
    logger_.info("Export orchestrator initialized");
}

ExportOrchestrator::~ExportOrchestrator() = default;

bool ExportOrchestrator::export_all_formats() {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Get configuration from generator
    const auto& config = generator_.get_config();

    // Get output tracker from generator
    const auto& output_tracker = generator_.get_output_tracker();

    // Create output directory
    std::filesystem::create_directories(config.output_directory);

    bool success = true;

    // Get mesh and layer data using getters
    const TopographicMesh* mesh = generator_.get_mesh_ptr();
    const auto& layer_meshes = generator_.get_layer_meshes();
    const auto& contour_layers = generator_.get_contour_layers();

    // DEBUG: Check state at export time
    logger_.debug("export_all_formats() - State check:");
    logger_.debug("  mesh is " + std::string(mesh ? "NON-NULL" : "NULL"));
    logger_.debug("  layer_meshes.size() = " + std::to_string(layer_meshes.size()));

    // Calculate scale factors using ScalingCalculator
    // Get mesh bounds to determine XY and Z extents
    double xy_extent_meters = 0.0;
    double z_extent_meters = 0.0;
    [[maybe_unused]] double common_2d_scale_factor = 1.0;
    double common_3d_scale_factor = 1.0;

    // Calculate bounds from either stacked mesh or layer meshes
    if (mesh && mesh->num_vertices() > 0) {
        logger_.debug("Taking FIRST branch - calculating bounds from mesh");
        // Get stacked mesh bounds to calculate XY extent and Z extent
        double stacked_min_x = std::numeric_limits<double>::max();
        double stacked_max_x = std::numeric_limits<double>::lowest();
        double stacked_min_y = std::numeric_limits<double>::max();
        double stacked_max_y = std::numeric_limits<double>::lowest();
        double stacked_min_z = std::numeric_limits<double>::max();
        double stacked_max_z = std::numeric_limits<double>::lowest();

        for (auto it = mesh->vertices_begin(); it != mesh->vertices_end(); ++it) {
            stacked_min_x = std::min(stacked_min_x, it->position.x());
            stacked_max_x = std::max(stacked_max_x, it->position.x());
            stacked_min_y = std::min(stacked_min_y, it->position.y());
            stacked_max_y = std::max(stacked_max_y, it->position.y());
            stacked_min_z = std::min(stacked_min_z, it->position.z());
            stacked_max_z = std::max(stacked_max_z, it->position.z());
        }

        double range_x = stacked_max_x - stacked_min_x;
        double range_y = stacked_max_y - stacked_min_y;
        double range_z = stacked_max_z - stacked_min_z;
        xy_extent_meters = std::max(range_x, range_y);
        z_extent_meters = range_z;
    } else if (!layer_meshes.empty()) {
        // Calculate bounds from layer meshes
        logger_.debug("Calculating bounds from " + std::to_string(layer_meshes.size()) + " layer meshes");
        double stacked_min_x = std::numeric_limits<double>::max();
        double stacked_max_x = std::numeric_limits<double>::lowest();
        double stacked_min_y = std::numeric_limits<double>::max();
        double stacked_max_y = std::numeric_limits<double>::lowest();
        double stacked_min_z = std::numeric_limits<double>::max();
        double stacked_max_z = std::numeric_limits<double>::lowest();

        size_t total_vertices = 0;
        for (const auto& layer_mesh : layer_meshes) {
            if (layer_mesh) {
                size_t mesh_vertices = layer_mesh->num_vertices();
                total_vertices += mesh_vertices;
                logger_.debug("  Layer mesh has " + std::to_string(mesh_vertices) + " vertices");
                for (auto it = layer_mesh->vertices_begin(); it != layer_mesh->vertices_end(); ++it) {
                    stacked_min_x = std::min(stacked_min_x, it->position.x());
                    stacked_max_x = std::max(stacked_max_x, it->position.x());
                    stacked_min_y = std::min(stacked_min_y, it->position.y());
                    stacked_max_y = std::max(stacked_max_y, it->position.y());
                    stacked_min_z = std::min(stacked_min_z, it->position.z());
                    stacked_max_z = std::max(stacked_max_z, it->position.z());
                }
            }
        }

        logger_.debug("Total vertices processed: " + std::to_string(total_vertices));
        logger_.debug("Bounds: X[" + std::to_string(stacked_min_x) + ", " + std::to_string(stacked_max_x) + "]");
        logger_.debug("Bounds: Y[" + std::to_string(stacked_min_y) + ", " + std::to_string(stacked_max_y) + "]");
        logger_.debug("Bounds: Z[" + std::to_string(stacked_min_z) + ", " + std::to_string(stacked_max_z) + "]");

        double range_x = stacked_max_x - stacked_min_x;
        double range_y = stacked_max_y - stacked_min_y;
        double range_z = stacked_max_z - stacked_min_z;
        xy_extent_meters = std::max(range_x, range_y);
        z_extent_meters = range_z;

        logger_.debug("Calculated extents: XY=" + std::to_string(xy_extent_meters) + "m, Z=" + std::to_string(z_extent_meters) + "m");
    }

    if (mesh || !layer_meshes.empty()) {

        // Calculate scales using ScalingCalculator
        ScalingCalculator scaling_calc(config);

        // Determine which outputs we're generating
        bool has_2d_output = false;
        bool has_3d_output = false;
        for (const auto& format : config.output_formats) {
            if (format == "svg" || format == "dxf" || format == "pdf") {
                has_2d_output = true;
            } else if (format == "stl" || format == "obj" || format == "ply") {
                has_3d_output = true;
            }
        }

        // Calculate appropriate scale factors
        if (has_3d_output) {
            auto scale_result_3d = scaling_calc.calculate_3d_scale(xy_extent_meters, z_extent_meters);
            common_3d_scale_factor = scale_result_3d.scale_factor;
            logger_.info("\n=== 3D Scaling Calculation ===");
            logger_.info(scale_result_3d.explanation);
            logger_.info("==============================\n");
        }

        if (has_2d_output) {
            auto scale_result_2d = scaling_calc.calculate_2d_scale(xy_extent_meters, z_extent_meters);
            common_2d_scale_factor = scale_result_2d.scale_factor;
            logger_.info("\n=== 2D Scaling Calculation ===");
            logger_.info(scale_result_2d.explanation);
            logger_.info("==============================\n");
        }

        // If both outputs use the same scaling method, use the same scale factor
        if (config.use_2d_scaling_for_3d || config.use_3d_scaling_for_2d) {
            logger_.info("Cross-mode scaling: Using consistent scale factor across 2D and 3D outputs");
        }
    }

    if (config.output_layers && !layer_meshes.empty()) {
        logger_.info("Exporting individual layer files...");

        // Export each layer
        size_t layer_num = 1;
        for (const auto& mesh_ptr : layer_meshes) {
            if (mesh_ptr) {
                // Calculate elevation for this layer (estimate from contour layers if available)
                double layer_elevation = 0.0;
                if (layer_num - 1 < contour_layers.size()) {
                    layer_elevation = contour_layers[layer_num - 1].elevation;
                }

                // Create a temporary exporter with layer-specific filename
                std::string layer_filename;
                if (config.filename_pattern.empty()) {
                    layer_filename = config.base_name + "_layer_" + std::to_string(layer_num);
                } else {
                    layer_filename = LabelRenderer::substitute_filename_pattern(
                        config.filename_pattern,
                        config.base_name,
                        static_cast<int>(layer_num),
                        layer_elevation
                    );
                }

                MultiFormatExporter::GlobalOptions layer_opts;
                layer_opts.base_filename = layer_filename;
                layer_opts.output_directory = config.output_directory;
                layer_opts.filename_pattern = "";  // Pattern already applied to base_filename
                layer_opts.verbose = (config.log_level >= 4);
                layer_opts.output_individual_layers = false; // Each layer is already individual

                MultiFormatExporter layer_exporter(layer_opts);

                // Configure STL options for layer exporter
                // Use common 3D scale factor instead of auto-scaling each layer independently
                STLExporter::Options layer_stl_opts;
                layer_stl_opts.binary_format = true;
                layer_stl_opts.validate_mesh = true;
                layer_stl_opts.auto_scale = false;  // Disable auto-scale
                layer_stl_opts.scale_factor = common_3d_scale_factor;  // Use common 3D scale

                if (config.cutting_bed_size_mm.has_value()) {
                    layer_stl_opts.target_bed_size_mm = config.cutting_bed_size_mm.value();
                } else if (config.cutting_bed_x_mm.has_value() && config.cutting_bed_y_mm.has_value()) {
                    layer_stl_opts.target_bed_size_mm = std::min(config.cutting_bed_x_mm.value(), config.cutting_bed_y_mm.value());
                } else {
                    layer_stl_opts.target_bed_size_mm = config.substrate_size_mm;
                }

                layer_exporter.set_stl_options(layer_stl_opts);

                logger_.debug("Exporting layer " + std::to_string(layer_num) + " mesh");

                bool layer_success = layer_exporter.export_all_formats(*mesh_ptr, config.output_formats);

                // Note: Output tracking handled by generator's output_tracker
                // We can't modify it from here since it's const

                success &= layer_success;
                layer_num++;
            }
        }

        if (config.log_level >= 4) {
            logger_.info("Exported " + std::to_string(layer_meshes.size()) + " layer files");
        }
    }

    // Export stacked model if requested
    if (config.output_stacked && mesh) {
        // Create stacked filename
        std::string stacked_filename = config.base_name + "_stacked";

        MultiFormatExporter::GlobalOptions stacked_opts;
        stacked_opts.base_filename = stacked_filename;
        stacked_opts.output_directory = config.output_directory;
        stacked_opts.filename_pattern = config.filename_pattern;
        stacked_opts.verbose = (config.log_level >= 4);
        stacked_opts.output_individual_layers = false;

        MultiFormatExporter stacked_exporter(stacked_opts);

        // Configure STL options for stacked exporter (same as main exporter)
        STLExporter::Options stacked_stl_opts;
        stacked_stl_opts.binary_format = true;
        stacked_stl_opts.validate_mesh = true;
        stacked_stl_opts.auto_scale = true;

        if (config.cutting_bed_size_mm.has_value()) {
            stacked_stl_opts.target_bed_size_mm = config.cutting_bed_size_mm.value();
        } else if (config.cutting_bed_x_mm.has_value() && config.cutting_bed_y_mm.has_value()) {
            stacked_stl_opts.target_bed_size_mm = std::min(config.cutting_bed_x_mm.value(), config.cutting_bed_y_mm.value());
        } else {
            stacked_stl_opts.target_bed_size_mm = config.substrate_size_mm;
        }

        stacked_exporter.set_stl_options(stacked_stl_opts);

        logger_.debug("Exporting stacked model");

        bool stacked_success = stacked_exporter.export_all_formats(*mesh, config.output_formats);

        if (config.log_level >= 4) {
            if (stacked_success) {
                logger_.info("Exported stacked model");
            } else {
                logger_.error("Failed to export stacked model");
            }
        }

        success &= stacked_success;
    } else if (!config.output_layers && mesh) {
        // Export combined mesh when not outputting individual layers
        logger_.debug("Exporting combined model");

        // Create a new exporter for the combined model
        MultiFormatExporter::GlobalOptions combined_opts;
        combined_opts.base_filename = config.base_name;
        combined_opts.output_directory = config.output_directory;
        combined_opts.filename_pattern = config.filename_pattern;
        combined_opts.verbose = (config.log_level >= 4);
        combined_opts.output_individual_layers = false;

        MultiFormatExporter combined_exporter(combined_opts);

        success = combined_exporter.export_all_formats(*mesh, config.output_formats);
    } else if (!config.output_layers && !config.output_stacked) {
        success = false;
    }

    // Calculate global elevation range before filtering for consistent color mapping
    double global_min_elev = contour_layers.empty() ? 0.0 : contour_layers.front().elevation;
    double global_max_elev = contour_layers.empty() ? 0.0 : contour_layers.back().elevation;
    for (const auto& layer : contour_layers) {
        global_min_elev = std::min(global_min_elev, layer.elevation);
        global_max_elev = std::max(global_max_elev, layer.elevation);
    }

    // Filter layers if specific_layers is specified
    std::vector<ContourLayer> layers_to_export = contour_layers;
    if (!config.specific_layers.empty()) {
        std::vector<ContourLayer> filtered_layers;
        for (size_t i = 0; i < contour_layers.size(); ++i) {
            // Layer numbers are 1-indexed in user input, but 0-indexed in vector
            int layer_number = static_cast<int>(i + 1);
            if (std::find(config.specific_layers.begin(), config.specific_layers.end(), layer_number) != config.specific_layers.end()) {
                filtered_layers.push_back(contour_layers[i]);
            }
        }
        layers_to_export = filtered_layers;

        if (config.log_level >= 4) {
            logger_.info("Layer filtering active: exporting " + std::to_string(layers_to_export.size()) +
                       " of " + std::to_string(contour_layers.size()) + " layers");
        }
    }

    // Handle SVG export separately if requested and contour data is available
    auto svg_it = std::find(config.output_formats.begin(), config.output_formats.end(), "svg");
    if (svg_it != config.output_formats.end() && !layers_to_export.empty()) {
        // Export SVG files using SVGExporter
        SVGConfig svg_config;
        svg_config.base_filename = config.base_name;
        svg_config.filename_pattern = config.filename_pattern;
        svg_config.output_directory = config.output_directory;
        svg_config.verbose = (config.log_level >= 4);
        svg_config.separate_layers = config.output_layers;
        svg_config.force_all_layers = config.force_all_layers;
        svg_config.remove_holes = config.remove_holes;
        svg_config.render_mode = config.render_mode;
        svg_config.color_scheme = config.color_scheme;  // Pass color scheme to SVG exporter

        // Pass configurable colors (convert RGB hex to #RRGGBB format)
        svg_config.stroke_color = "#" + config.stroke_color;
        svg_config.background_color = "#" + config.background_color;

        // Pass stroke width (treat as millimeters for SVG)
        svg_config.cut_stroke_width = config.stroke_width;

        // Pass label configuration
        svg_config.base_label_visible = config.base_label_visible;
        svg_config.base_label_hidden = config.base_label_hidden;
        svg_config.layer_label_visible = config.layer_label_visible;
        svg_config.layer_label_hidden = config.layer_label_hidden;
        svg_config.visible_label_color = config.visible_label_color;
        svg_config.hidden_label_color = config.hidden_label_color;
        svg_config.base_font_size_mm = config.base_font_size_mm;
        svg_config.layer_font_size_mm = config.layer_font_size_mm;

        // Pass label context information
        svg_config.geographic_bounds = config.bounds;

        // Compute scale ratio from scale factor (e.g., 1:25000)
        // If explicit_2d_scale_factor is set (in mm/m), compute ratio as 1000/scale_factor
        // Otherwise use a default value of 1.0 (will be substituted as "Scale 1:1")
        if (config.explicit_2d_scale_factor.has_value() && config.explicit_2d_scale_factor.value() > 0) {
            svg_config.scale_ratio = 1000.0 / config.explicit_2d_scale_factor.value();
        } else {
            svg_config.scale_ratio = 1.0;  // Default ratio if not specified
        }

        svg_config.contour_height_m = config.contour_interval;
        svg_config.substrate_size_mm = config.substrate_size_mm;
        svg_config.label_units = config.label_units;
        svg_config.print_units = config.print_units;
        svg_config.land_units = config.land_units;

        SVGExporter svg_exporter(svg_config);

        logger_.debug("Exporting SVG files");

        // Pass geographic bounds for consistent coordinate transformation across all layers
        auto svg_files = svg_exporter.export_layers(layers_to_export, config.output_layers,
                                                   global_min_elev, global_max_elev,
                                                   &config.bounds);

        if (svg_files.empty()) {
            if (config.log_level >= 4) {
                logger_.warning("SVG export failed - no files generated");
            }
            success = false;
        } else if (config.log_level >= 4) {
            logger_.info("Successfully exported " + std::to_string(svg_files.size()) + " SVG files");
        }
    }

    // Handle PNG export if requested and contour data is available
    auto png_it = std::find(config.output_formats.begin(), config.output_formats.end(), "png");
    if (png_it != config.output_formats.end() && !layers_to_export.empty()) {
        PNGExporter::Options png_opts;
        png_opts.width_px = 2048;
        png_opts.height_px = 0;  // Auto-calculate from geographic bounds aspect ratio

        // Convert SVG margin from mm to pixels to ensure consistent scaling
        // SVGConfig defaults to 10mm margin, convert using DPI
        constexpr double SVG_DEFAULT_MARGIN_MM = 10.0;
        png_opts.margin_px = UnitParser::mm_to_pixels(SVG_DEFAULT_MARGIN_MM, config.print_resolution_dpi);

        png_opts.color_scheme = config.color_scheme;
        png_opts.render_mode = config.render_mode;
        png_opts.add_alignment_marks = config.add_registration_marks;
        png_opts.stroke_color = config.stroke_color;
        png_opts.background_color = config.background_color;
        // Convert stroke_width from mm to pixels using DPI
        png_opts.stroke_width = UnitParser::mm_to_pixels(config.stroke_width, config.print_resolution_dpi);
        png_opts.font_path = config.font_path;
        png_opts.font_face = config.font_face;
        png_opts.remove_holes = config.remove_holes;
        png_opts.filename_pattern = config.filename_pattern;
        png_opts.base_label_visible = config.base_label_visible;
        png_opts.base_label_hidden = config.base_label_hidden;
        png_opts.layer_label_visible = config.layer_label_visible;
        png_opts.layer_label_hidden = config.layer_label_hidden;
        png_opts.visible_label_color = config.visible_label_color;
        png_opts.hidden_label_color = config.hidden_label_color;
        png_opts.base_font_size_mm = config.base_font_size_mm;
        png_opts.layer_font_size_mm = config.layer_font_size_mm;
        PNGExporter png_exporter(png_opts);

        std::string base_filename = config.output_directory + "/" + config.base_name;
        logger_.debug("Exporting PNG raster");

        auto png_files = png_exporter.export_png(layers_to_export, base_filename, config.bounds, config.output_layers);

        if (!png_files.empty()) {
            if (config.log_level >= 4) {
                logger_.info("Successfully exported " + std::to_string(png_files.size()) + " PNG file(s)");
            }
        } else {
            if (config.log_level >= 4) {
                logger_.warning("PNG export failed - no files generated");
            }
            success = false;
        }
    }

    // Handle GeoTIFF export if requested and contour data is available
    auto geotiff_it = std::find(config.output_formats.begin(), config.output_formats.end(), "geotiff");
    if (geotiff_it != config.output_formats.end() && !layers_to_export.empty()) {
        GeoTIFFExporter::Options geotiff_opts;
        geotiff_opts.width_px = 2048;
        geotiff_opts.height_px = 0;  // Auto-calculate from aspect ratio
        geotiff_opts.color_scheme = config.color_scheme;
        geotiff_opts.render_mode = config.render_mode;
        geotiff_opts.projection_wkt = "EPSG:4326";  // WGS84
        geotiff_opts.compression = GeoTIFFExporter::Options::Compression::DEFLATE;
        geotiff_opts.add_alignment_marks = config.add_registration_marks;
        geotiff_opts.stroke_color = config.stroke_color;
        geotiff_opts.background_color = config.background_color;
        geotiff_opts.stroke_width = config.stroke_width;
        GeoTIFFExporter geotiff_exporter(geotiff_opts);

        std::string base_filename = config.output_directory + "/" + config.base_name;
        logger_.debug("Exporting GeoTIFF raster");

        auto geotiff_files = geotiff_exporter.export_geotiff(layers_to_export, base_filename, config.bounds, config.output_layers);

        if (!geotiff_files.empty()) {
            if (config.log_level >= 4) {
                logger_.info("Successfully exported " + std::to_string(geotiff_files.size()) + " GeoTIFF file(s)");
            }
        } else {
            if (config.log_level >= 4) {
                logger_.warning("GeoTIFF export failed - no files generated");
            }
            success = false;
        }
    }

    // Handle GeoJSON export if requested and contour data is available
    auto geojson_it = std::find(config.output_formats.begin(), config.output_formats.end(), "geojson");
    if (geojson_it != config.output_formats.end() && !layers_to_export.empty()) {
        GeoJSONExporter::Options geojson_opts;
        geojson_opts.pretty_print = true;
        geojson_opts.include_crs = true;
        geojson_opts.crs = "urn:ogc:def:crs:OGC:1.3:CRS84";  // WGS84
        geojson_opts.precision = 6;
        GeoJSONExporter geojson_exporter(geojson_opts);

        logger_.debug("Exporting GeoJSON vector");

        if (config.output_layers) {
            // Export each layer separately
            int exported_count = 0;
            for (size_t i = 0; i < layers_to_export.size(); ++i) {
                const auto& layer = layers_to_export[i];

                // Format elevation with proper precision for filename
                std::ostringstream elev_str;
                elev_str << std::fixed << std::setprecision(0) << layer.elevation;

                std::string layer_filename = config.output_directory + "/" +
                                            config.base_name +
                                            "_layer_" + std::to_string(i + 1) +
                                            "_elev_" + elev_str.str() + "m.geojson";

                bool layer_success = geojson_exporter.export_layer(layer, layer_filename, static_cast<int>(i + 1));

                if (layer_success) {
                    exported_count++;
                } else {
                    if (config.log_level >= 4) {
                        logger_.warning("Failed to export GeoJSON layer " + std::to_string(i + 1));
                    }
                    success = false;
                }
            }

            if (config.log_level >= 4) {
                logger_.info("Successfully exported " + std::to_string(exported_count) + " GeoJSON layer file(s)");
            }
        } else {
            // Export all layers combined
            std::string filename = config.output_directory + "/" + config.base_name + ".geojson";
            bool geojson_success = geojson_exporter.export_geojson(layers_to_export, filename);

            if (geojson_success) {
                if (config.log_level >= 4) {
                    logger_.info("Successfully exported GeoJSON: " + filename);
                }
            } else {
                if (config.log_level >= 4) {
                    logger_.warning("GeoJSON export failed");
                }
                success = false;
            }
        }
    }

    // Handle Shapefile export if requested and contour data is available
    auto shapefile_it = std::find(config.output_formats.begin(), config.output_formats.end(), "shapefile");
    if (shapefile_it != config.output_formats.end() && !layers_to_export.empty()) {
        ShapefileExporter::Options shapefile_opts;
        shapefile_opts.add_layer_field = true;
        shapefile_opts.add_elevation_field = true;
        shapefile_opts.projection_wkt = "EPSG:4326";  // WGS84
        ShapefileExporter shapefile_exporter(shapefile_opts);

        logger_.debug("Exporting Shapefile vector");

        if (config.output_layers) {
            // Export each layer separately
            int exported_count = 0;
            for (size_t i = 0; i < layers_to_export.size(); ++i) {
                const auto& layer = layers_to_export[i];

                // Format elevation with proper precision for filename
                std::ostringstream elev_str;
                elev_str << std::fixed << std::setprecision(0) << layer.elevation;

                std::string layer_filename = config.output_directory + "/" +
                                            config.base_name +
                                            "_layer_" + std::to_string(i + 1) +
                                            "_elev_" + elev_str.str() + "m.shp";

                bool layer_success = shapefile_exporter.export_layer(layer, layer_filename, static_cast<int>(i + 1));

                if (layer_success) {
                    exported_count++;
                } else {
                    if (config.log_level >= 4) {
                        logger_.warning("Failed to export Shapefile layer " + std::to_string(i + 1));
                    }
                    success = false;
                }
            }

            if (config.log_level >= 4) {
                logger_.info("Successfully exported " + std::to_string(exported_count) + " Shapefile layer file(s)");
            }
        } else {
            // Export all layers combined
            std::string filename = config.output_directory + "/" + config.base_name + ".shp";
            bool shapefile_success = shapefile_exporter.export_shapefile(layers_to_export, filename);

            if (shapefile_success) {
                if (config.log_level >= 4) {
                    logger_.info("Successfully exported Shapefile: " + filename);
                }
            } else {
                if (config.log_level >= 4) {
                    logger_.warning("Shapefile export failed");
                }
                success = false;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto export_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    logger_.info("Export completed in " + std::to_string(export_duration.count()) + "ms");

    return success;
}

} // namespace topo
