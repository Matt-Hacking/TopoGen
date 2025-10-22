/**
 * @file PNGExporter.cpp
 * @brief Implementation of PNG raster export
 */

#include "PNGExporter.hpp"
#include "../core/Logger.hpp"
#include "LabelRenderer.hpp"
#include "UnitParser.hpp"
#include <gdal_priv.h>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace topo {

PNGExporter::PNGExporter()
    : options_() {
    GDALAllRegister();
}

PNGExporter::PNGExporter(const Options& options)
    : options_(options) {
    GDALAllRegister();
}

void PNGExporter::set_color_scheme(TopographicConfig::ColorScheme scheme) {
    options_.color_scheme = scheme;
}

void PNGExporter::set_render_mode(TopographicConfig::RenderMode mode) {
    options_.render_mode = mode;
}

RasterConfig PNGExporter::get_raster_config() const {
    RasterConfig config;
    config.width_px = options_.width_px;
    config.height_px = options_.height_px;
    config.margin_px = options_.margin_px;  // Pass margin to raster builder
    config.color_scheme = options_.color_scheme;
    config.render_mode = options_.render_mode;
    config.elevation_bands = options_.elevation_bands;

    // Parse background color from hex string
    config.background_color = RasterAnnotator::parse_hex_color(options_.background_color);

    // Configure terrain outline (enabled by default for PNG)
    config.add_terrain_outline = true;
    config.outline_color = RasterAnnotator::parse_hex_color(options_.stroke_color);
    config.outline_width_px = options_.stroke_width;

    // Configure hole removal (default true for PNG to minimize cuts)
    config.remove_holes = options_.remove_holes;

    return config;
}

bool PNGExporter::write_png_with_copy(GDALDataset* source, const std::string& filename) {
    Logger logger("PNGExporter");

    if (!source) {
        logger.error("Null source dataset");
        return false;
    }

    // Get PNG driver
    GDALDriver* png_driver = GetGDALDriverManager()->GetDriverByName("PNG");
    if (!png_driver) {
        logger.error("PNG driver not available");
        return false;
    }

    // Temporarily disable PAM (Persistent Auxiliary Metadata) to prevent .aux.xml file creation
    // PNG doesn't need georeferencing metadata for visualization purposes
    const char* old_pam_setting = CPLGetConfigOption("GDAL_PAM_ENABLED", nullptr);
    CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");

    // Copy source dataset to PNG file using CreateCopy
    GDALDataset* png_dataset = png_driver->CreateCopy(
        filename.c_str(),
        source,
        FALSE,      // Not strict
        nullptr,    // Options
        nullptr,    // Progress function
        nullptr     // Progress data
    );

    // Restore original PAM setting
    CPLSetConfigOption("GDAL_PAM_ENABLED", old_pam_setting);

    if (!png_dataset) {
        logger.error("Failed to create PNG file: " + filename);
        return false;
    }

    GDALClose(png_dataset);
    return true;
}

bool PNGExporter::export_from_dataset(GDALDataset* dataset, const std::string& filename) {
    Logger logger("PNGExporter");

    if (!dataset) {
        logger.error("Null dataset");
        return false;
    }

    // Add overlays if requested (TODO: implement)
    // if (options_.add_legend) {
    //     add_legend_overlay(dataset, min_elev, max_elev);
    // }
    // if (options_.add_scale_bar) {
    //     add_scale_bar_overlay(dataset, bounds);
    // }

    bool success = write_png_with_copy(dataset, filename);

    if (success) {
        logger.info("Exported PNG: " + filename + " (" +
                   std::to_string(dataset->GetRasterXSize()) + "x" +
                   std::to_string(dataset->GetRasterYSize()) + ")");
    }

    return success;
}

std::vector<std::string> PNGExporter::export_png(
    const std::vector<ContourLayer>& layers,
    const std::string& base_filename,
    const BoundingBox& bounds,
    bool separate_layers) {

    Logger logger("PNGExporter");
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

            logger.debug("Export layer[" + std::to_string(i) + "] @ " + std::to_string(layer.elevation) + "m: " +
                        std::to_string(layer.polygons.size()) + " polygons");

            // Generate filename using pattern
            std::filesystem::path base_path(base_filename);
            std::string dir = base_path.parent_path().string();
            std::string basename = base_path.stem().string();

            // Use filename pattern if provided, otherwise use default
            std::string filename_no_ext = options_.filename_pattern.empty() ?
                basename + "_layer_" + std::to_string(i + 1) :
                LabelRenderer::substitute_filename_pattern(
                    options_.filename_pattern,
                    basename,
                    static_cast<int>(i + 1),
                    layer.elevation
                );

            std::string layer_filename = dir;
            if (!layer_filename.empty()) layer_filename += "/";
            layer_filename += filename_no_ext + ".png";

            // Build dataset for this layer with global elevation range
            GDALDataset* dataset = builder.build_dataset_single_layer(layer, bounds,
                                                                      global_min_elev, global_max_elev);
            if (!dataset) {
                logger.error("Failed to build raster for layer " + std::to_string(i + 1));
                continue;
            }

            // Add annotations and labels if requested
            AnnotationConfig annot_config;
            annot_config.add_alignment_marks = options_.add_alignment_marks;
            annot_config.add_border = options_.add_border;
            annot_config.margin_px = options_.margin_px;
            annot_config.stroke_color = RasterAnnotator::parse_hex_color(options_.stroke_color);
            annot_config.alignment_color = {0, 0, 255, 255};  // Blue
            annot_config.font_path = options_.font_path;
            annot_config.font_face = options_.font_face;

            RasterAnnotator annotator(annot_config);

            if (options_.add_alignment_marks || options_.add_border) {
                annotator.annotate_dataset(dataset, layer.elevation);
            }

            // Add labels if configured
            if (!options_.layer_label_visible.empty() || !options_.layer_label_hidden.empty()) {
                // Create LabelRenderer
                LabelConfig label_config;
                label_config.base_label_visible = options_.base_label_visible;
                label_config.base_label_hidden = options_.base_label_hidden;
                label_config.layer_label_visible = options_.layer_label_visible;
                label_config.layer_label_hidden = options_.layer_label_hidden;
                label_config.visible_label_color = options_.visible_label_color;
                label_config.hidden_label_color = options_.hidden_label_color;
                label_config.base_font_size_mm = options_.base_font_size_mm;
                label_config.layer_font_size_mm = options_.layer_font_size_mm;
                // Disable curved text for PNG - too slow with pixel-by-pixel rendering
                label_config.enable_curved_text = false;
                LabelRenderer label_renderer(label_config);

                int width = dataset->GetRasterXSize();
                int height = dataset->GetRasterYSize();

                // Create LabelContext with bounding boxes for label placement
                LabelContext context;
                context.layer_number = static_cast<int>(i + 1);
                context.elevation_m = layer.elevation;
                context.geographic_bounds = bounds;

                // Content area (within margins) - in pixel coordinates
                context.content_bbox.min_x = options_.margin_px;
                context.content_bbox.max_x = width - options_.margin_px;
                context.content_bbox.min_y = options_.margin_px;
                context.content_bbox.max_y = height - options_.margin_px;

                // Hidden area bbox (will be covered by next layer)
                // If next layer exists, use its actual geometry; otherwise use center 60% heuristic
                if (i + 1 < layers.size() && !layers[i + 1].polygons.empty()) {
                    // Calculate bbox from next layer's actual polygon geometry in pixel coordinates
                    context.hidden_bbox = calculate_polygon_bbox_pixels(
                        layers[i + 1].polygons,
                        bounds,
                        width,
                        height
                    );
                    // Pass polygon data for curved text path generation
                    context.next_layer_polygons = &layers[i + 1].polygons;
                } else {
                    // Fallback: use center 60% of content area
                    double content_width = context.content_bbox.max_x - context.content_bbox.min_x;
                    double content_height = context.content_bbox.max_y - context.content_bbox.min_y;
                    context.hidden_bbox.min_x = context.content_bbox.min_x + content_width * 0.2;
                    context.hidden_bbox.max_x = context.content_bbox.max_x - content_width * 0.2;
                    context.hidden_bbox.min_y = context.content_bbox.min_y + content_height * 0.2;
                    context.hidden_bbox.max_y = context.content_bbox.max_y - content_height * 0.2;
                    context.next_layer_polygons = nullptr;
                }

                // Render visible label
                auto visible_label_opt = label_renderer.generate_layer_visible_label(context);
                if (visible_label_opt) {
                    int font_size_px = static_cast<int>(
                        UnitParser::mm_to_pixels(options_.layer_font_size_mm, 600.0)
                    );

                    // Position label in bottom margin (centered horizontally)
                    int label_x = width / 2;
                    int label_y = height - static_cast<int>(options_.margin_px / 2);

                    annotator.draw_text(
                        dataset,
                        visible_label_opt->text,
                        label_x, label_y,
                        font_size_px,
                        RasterAnnotator::parse_hex_color(options_.visible_label_color),
                        "middle"
                    );
                }

                // Render hidden label (will be covered by next layer)
                // Generate base label text
                std::string hidden_text = "";
                if (!label_config.layer_label_hidden.empty()) {
                    LabelContext temp_context = context;
                    hidden_text = label_renderer.substitute_patterns(label_config.layer_label_hidden, temp_context);
                }

                if (!hidden_text.empty() && i + 1 < layers.size() && !layers[i + 1].polygons.empty()) {
                    // Get polygon pixels for optimal placement
                    auto polygon_pixels = get_polygon_pixels(
                        layers[i + 1].polygons[0],  // Use first (largest) polygon
                        bounds,
                        width,
                        height
                    );

                    if (!polygon_pixels.empty()) {
                        // Try scaling from 100% down to 50% in 10% steps
                        double dpi = 600.0;
                        int initial_font_px = static_cast<int>(UnitParser::mm_to_pixels(options_.layer_font_size_mm, dpi));
                        const double min_scale = 0.5;
                        const int scale_steps = 6;  // 100%, 90%, 80%, 70%, 60%, 50%

                        TextRect best_rect = {0, 0, 0, 0, false};
                        int best_font_px = initial_font_px;

                        for (int step = 0; step < scale_steps; ++step) {
                            double scale = 1.0 - (step * 0.1);
                            if (scale < min_scale) break;

                            int test_font_px = static_cast<int>(initial_font_px * scale);
                            int text_width_px = annotator.measure_text_width(hidden_text, test_font_px);

                            if (text_width_px < 0) {
                                logger.warning("Failed to measure text width for layer " + std::to_string(i + 1));
                                break;
                            }

                            int text_height_px = test_font_px;  // Approximate height

                            // Find optimal rectangle for this size
                            TextRect rect = find_optimal_text_rect(polygon_pixels, text_width_px, text_height_px);

                            if (rect.valid) {
                                best_rect = rect;
                                best_font_px = test_font_px;
                                break;  // Found a fit!
                            }
                        }

                        if (best_rect.valid) {
                            // Render the label at optimal position
                            annotator.draw_text(
                                dataset,
                                hidden_text,
                                best_rect.x,
                                best_rect.y,
                                best_font_px,
                                RasterAnnotator::parse_hex_color(options_.hidden_label_color),
                                "middle"
                            );
                        } else {
                            // Label suppressed - polygon too small
                            logger.info("Label suppressed for layer " + std::to_string(i + 1) +
                                       ": polygon too small to fit text '" + hidden_text + "'");
                        }
                    }
                }
            }

            // Export to PNG
            bool success = export_from_dataset(dataset, layer_filename);
            GDALClose(dataset);

            if (success) {
                exported_files.push_back(layer_filename);
            }
        }
    } else {
        // Export all layers combined into single PNG
        std::string combined_filename = base_filename;
        if (combined_filename.find(".png") == std::string::npos) {
            combined_filename += ".png";
        }

        GDALDataset* dataset = builder.build_dataset(layers, bounds);
        if (!dataset) {
            logger.error("Failed to build raster dataset");
            return exported_files;
        }

        // Add annotations and labels if requested
        AnnotationConfig annot_config;
        annot_config.add_alignment_marks = options_.add_alignment_marks;
        annot_config.add_border = options_.add_border;
        annot_config.margin_px = options_.margin_px;
        annot_config.stroke_color = RasterAnnotator::parse_hex_color(options_.stroke_color);
        annot_config.alignment_color = {0, 0, 255, 255};  // Blue
        annot_config.font_path = options_.font_path;
        annot_config.font_face = options_.font_face;

        RasterAnnotator annotator(annot_config);

        if (options_.add_alignment_marks || options_.add_border) {
            annotator.annotate_dataset(dataset);
        }

        // Add labels if configured (use base label for combined view)
        if (!options_.base_label_visible.empty() || !options_.base_label_hidden.empty()) {
            // Create LabelRenderer
            LabelConfig label_config;
            label_config.base_label_visible = options_.base_label_visible;
            label_config.base_label_hidden = options_.base_label_hidden;
            label_config.layer_label_visible = options_.layer_label_visible;
            label_config.layer_label_hidden = options_.layer_label_hidden;
            label_config.visible_label_color = options_.visible_label_color;
            label_config.hidden_label_color = options_.hidden_label_color;
            label_config.base_font_size_mm = options_.base_font_size_mm;
            label_config.layer_font_size_mm = options_.layer_font_size_mm;
            // Disable curved text for PNG - too slow with pixel-by-pixel rendering
            label_config.enable_curved_text = false;
            LabelRenderer label_renderer(label_config);

            int width = dataset->GetRasterXSize();
            int height = dataset->GetRasterYSize();

            // Create LabelContext with bounding boxes for label placement
            LabelContext context;
            context.layer_number = 0;  // Base layer
            context.geographic_bounds = bounds;
            if (!layers.empty()) {
                context.elevation_m = layers.front().elevation;
            }

            // Content area (within margins) - in pixel coordinates
            context.content_bbox.min_x = options_.margin_px;
            context.content_bbox.max_x = width - options_.margin_px;
            context.content_bbox.min_y = options_.margin_px;
            context.content_bbox.max_y = height - options_.margin_px;

            // Hidden area (center 60% that will be covered by next layer)
            double content_width = context.content_bbox.max_x - context.content_bbox.min_x;
            double content_height = context.content_bbox.max_y - context.content_bbox.min_y;
            context.hidden_bbox.min_x = context.content_bbox.min_x + content_width * 0.2;
            context.hidden_bbox.max_x = context.content_bbox.max_x - content_width * 0.2;
            context.hidden_bbox.min_y = context.content_bbox.min_y + content_height * 0.2;
            context.hidden_bbox.max_y = context.content_bbox.max_y - content_height * 0.2;

            // Render visible label
            auto visible_label_opt = label_renderer.generate_base_visible_label(context);
            if (visible_label_opt) {
                int font_size_px = static_cast<int>(
                    UnitParser::mm_to_pixels(options_.base_font_size_mm, 600.0)
                );

                // Position label in bottom margin (centered horizontally)
                int label_x = width / 2;
                int label_y = height - static_cast<int>(options_.margin_px / 2);

                annotator.draw_text(
                    dataset,
                    visible_label_opt->text,
                    label_x, label_y,
                    font_size_px,
                    RasterAnnotator::parse_hex_color(options_.visible_label_color),
                    "middle"
                );
            }

            // Render hidden label (will be covered by next layer)
            // Generate base label text
            std::string hidden_text = "";
            if (!label_config.base_label_hidden.empty()) {
                LabelContext temp_context = context;
                hidden_text = label_renderer.substitute_patterns(label_config.base_label_hidden, temp_context);
            }

            if (!hidden_text.empty() && !layers.empty() && !layers[0].polygons.empty()) {
                // Get polygon pixels for optimal placement (first layer polygon)
                auto polygon_pixels = get_polygon_pixels(
                    layers[0].polygons[0],  // Use first (largest) polygon of first layer
                    bounds,
                    width,
                    height
                );

                if (!polygon_pixels.empty()) {
                    // Try scaling from 100% down to 50% in 10% steps
                    double dpi = 600.0;
                    int initial_font_px = static_cast<int>(UnitParser::mm_to_pixels(options_.base_font_size_mm, dpi));
                    const double min_scale = 0.5;
                    const int scale_steps = 6;  // 100%, 90%, 80%, 70%, 60%, 50%

                    TextRect best_rect = {0, 0, 0, 0, false};
                    int best_font_px = initial_font_px;

                    for (int step = 0; step < scale_steps; ++step) {
                        double scale = 1.0 - (step * 0.1);
                        if (scale < min_scale) break;

                        int test_font_px = static_cast<int>(initial_font_px * scale);
                        int text_width_px = annotator.measure_text_width(hidden_text, test_font_px);

                        if (text_width_px < 0) {
                            logger.warning("Failed to measure text width for base label");
                            break;
                        }

                        int text_height_px = test_font_px;  // Approximate height

                        // Find optimal rectangle for this size
                        TextRect rect = find_optimal_text_rect(polygon_pixels, text_width_px, text_height_px);

                        if (rect.valid) {
                            best_rect = rect;
                            best_font_px = test_font_px;
                            break;  // Found a fit!
                        }
                    }

                    if (best_rect.valid) {
                        // Render the label at optimal position
                        annotator.draw_text(
                            dataset,
                            hidden_text,
                            best_rect.x,
                            best_rect.y,
                            best_font_px,
                            RasterAnnotator::parse_hex_color(options_.hidden_label_color),
                            "middle"
                        );
                    } else {
                        // Label suppressed - polygon too small
                        logger.info("Label suppressed for base layer: polygon too small to fit text '" +
                                   hidden_text + "'");
                    }
                }
            }
        }

        bool success = export_from_dataset(dataset, combined_filename);
        GDALClose(dataset);

        if (success) {
            exported_files.push_back(combined_filename);
        }
    }

    logger.info("Exported " + std::to_string(exported_files.size()) + " PNG file(s)");
    return exported_files;
}

bool PNGExporter::export_layer(const ContourLayer& layer,
                               const std::string& filename,
                               const BoundingBox& bounds) {
    std::vector<ContourLayer> single_layer = {layer};
    auto files = export_png(single_layer, filename, bounds, false);
    return !files.empty();
}

void PNGExporter::add_legend_overlay(GDALDataset* dataset,
                                      double min_elev,
                                      double max_elev) {
    if (!dataset) return;

    // Create RasterAnnotator for drawing primitives
    AnnotationConfig annot_config;
    annot_config.font_path = options_.font_path;
    annot_config.font_face = options_.font_face;
    RasterAnnotator annotator(annot_config);

    // Create ColorMapper to get elevation colors
    ColorMapper::Scheme color_scheme = static_cast<ColorMapper::Scheme>(options_.color_scheme);
    ColorMapper color_mapper(color_scheme);

    // Legend dimensions and position (bottom-right corner)
    int img_width = dataset->GetRasterXSize();
    int img_height = dataset->GetRasterYSize();

    const int legend_width = 40;    // Width of color bar in pixels
    const int legend_height = 200;  // Height of color bar in pixels
    const int legend_margin = 20;   // Margin from edge
    const int label_spacing = 10;   // Space between bar and labels

    int legend_x = img_width - legend_margin - legend_width;
    int legend_y = img_height - legend_margin - legend_height;

    // Draw colored gradient bar (top to bottom = high to low elevation)
    const int num_segments = 100;  // Number of color segments for smooth gradient
    for (int i = 0; i < num_segments; ++i) {
        // Calculate elevation for this segment (top = max, bottom = min)
        double segment_fraction = static_cast<double>(i) / (num_segments - 1);
        double elevation = max_elev - (max_elev - min_elev) * segment_fraction;

        // Get color for this elevation
        auto rgb = color_mapper.map_elevation_to_color(elevation, min_elev, max_elev);
        std::array<uint8_t, 4> color = {
            static_cast<uint8_t>(rgb[0] * 255),
            static_cast<uint8_t>(rgb[1] * 255),
            static_cast<uint8_t>(rgb[2] * 255),
            255  // Full opacity
        };

        // Draw segment rectangle
        int segment_height = legend_height / num_segments;
        int y_pos = legend_y + i * segment_height;
        annotator.draw_rectangle(dataset, legend_x, y_pos, legend_width, segment_height, color, 0);
    }

    // Draw black border around legend
    std::array<uint8_t, 4> border_color = {0, 0, 0, 255};
    annotator.draw_rectangle(dataset, legend_x - 1, legend_y - 1,
                            legend_width + 2, legend_height + 2, border_color, 1.0);

    // Add elevation labels (max at top, min at bottom, mid in middle)
    int font_size = 14;
    std::array<uint8_t, 4> text_color = {0, 0, 0, 255};  // Black text
    int label_x = legend_x + legend_width + label_spacing;

    // Max elevation (top)
    std::string max_label = std::to_string(static_cast<int>(std::round(max_elev))) + "m";
    annotator.draw_text(dataset, max_label, label_x, legend_y, font_size, text_color, "start");

    // Mid elevation (middle)
    double mid_elev = (min_elev + max_elev) / 2.0;
    std::string mid_label = std::to_string(static_cast<int>(std::round(mid_elev))) + "m";
    annotator.draw_text(dataset, mid_label, label_x, legend_y + legend_height / 2,
                       font_size, text_color, "start");

    // Min elevation (bottom)
    std::string min_label = std::to_string(static_cast<int>(std::round(min_elev))) + "m";
    annotator.draw_text(dataset, min_label, label_x, legend_y + legend_height,
                       font_size, text_color, "start");
}

void PNGExporter::add_scale_bar_overlay(GDALDataset* dataset,
                                        const BoundingBox& bounds) {
    if (!dataset) return;

    // Create RasterAnnotator for drawing primitives
    AnnotationConfig annot_config;
    annot_config.font_path = options_.font_path;
    annot_config.font_face = options_.font_face;
    RasterAnnotator annotator(annot_config);

    // Image dimensions
    int img_width = dataset->GetRasterXSize();
    int img_height = dataset->GetRasterYSize();

    // Calculate scale (meters per pixel)
    // Approximate: 1 degree latitude ≈ 111,000 meters
    // 1 degree longitude ≈ 111,000 * cos(latitude) meters
    double center_lat = (bounds.min_y + bounds.max_y) / 2.0;
    double lat_degrees = bounds.max_y - bounds.min_y;
    double lon_degrees = bounds.max_x - bounds.min_x;

    double height_meters = lat_degrees * 111000.0;  // Approximate meters for latitude span
    double width_meters = lon_degrees * 111000.0 * std::cos(center_lat * M_PI / 180.0);  // Longitude adjusted for latitude

    double meters_per_pixel_x = width_meters / img_width;
    double meters_per_pixel_y = height_meters / img_height;
    double meters_per_pixel = (meters_per_pixel_x + meters_per_pixel_y) / 2.0;  // Average

    // Determine appropriate scale bar length (round to nice numbers)
    // Target: scale bar should be about 150-200 pixels
    double target_pixels = 150.0;
    double target_meters = target_pixels * meters_per_pixel;

    // Round to nice numbers (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, etc.)
    double scale_meters;
    if (target_meters < 10) {
        scale_meters = std::round(target_meters);
    } else if (target_meters < 100) {
        scale_meters = std::round(target_meters / 10.0) * 10.0;
    } else if (target_meters < 1000) {
        scale_meters = std::round(target_meters / 50.0) * 50.0;
    } else if (target_meters < 10000) {
        scale_meters = std::round(target_meters / 500.0) * 500.0;
    } else {
        scale_meters = std::round(target_meters / 1000.0) * 1000.0;
    }

    // Calculate scale bar width in pixels
    int scale_bar_width = static_cast<int>(scale_meters / meters_per_pixel);

    // Scale bar position (bottom-left corner)
    const int scale_margin = 20;
    const int scale_bar_height = 8;
    int scale_x = scale_margin;
    int scale_y = img_height - scale_margin - scale_bar_height - 20;  // Extra space for label

    // Draw scale bar background (white with black border)
    std::array<uint8_t, 4> white = {255, 255, 255, 255};
    std::array<uint8_t, 4> black = {0, 0, 0, 255};

    // Draw alternating black and white segments (typical map scale bar style)
    int num_segments = 4;
    int segment_width = scale_bar_width / num_segments;
    for (int i = 0; i < num_segments; ++i) {
        std::array<uint8_t, 4> color = (i % 2 == 0) ? black : white;
        annotator.draw_rectangle(dataset, scale_x + i * segment_width, scale_y,
                                segment_width, scale_bar_height, color, 0);
    }

    // Draw border around entire scale bar
    annotator.draw_rectangle(dataset, scale_x - 1, scale_y - 1,
                            scale_bar_width + 2, scale_bar_height + 2, black, 1.0);

    // Add tick marks at start, middle, and end
    const int tick_height = 12;
    for (int i = 0; i <= num_segments; i += num_segments / 2) {  // 0, 2, 4 (start, middle, end)
        int tick_x = scale_x + i * segment_width;
        annotator.draw_line(dataset, tick_x, scale_y, tick_x, scale_y - tick_height, black, 2.0);
    }

    // Add distance labels
    int font_size = 12;
    int label_y = scale_y + scale_bar_height + 14;

    // Start label (0)
    annotator.draw_text(dataset, "0", scale_x, label_y, font_size, black, "middle");

    // End label (scale distance)
    std::string end_label;
    if (scale_meters >= 1000) {
        double scale_km = scale_meters / 1000.0;
        end_label = std::to_string(static_cast<int>(std::round(scale_km))) + " km";
    } else {
        end_label = std::to_string(static_cast<int>(std::round(scale_meters))) + " m";
    }
    annotator.draw_text(dataset, end_label, scale_x + scale_bar_width, label_y,
                       font_size, black, "middle");
}

BoundingBox PNGExporter::calculate_polygon_bbox_pixels(
    const std::vector<ContourLayer::PolygonData>& polygons,
    const BoundingBox& geo_bounds,
    int width,
    int height) const {

    if (polygons.empty()) {
        // Return degenerate bbox if no polygons
        return {0, 0, 0, 0};
    }

    // First, calculate geographic bounding box from polygon vertices
    double geo_min_x = std::numeric_limits<double>::max();
    double geo_min_y = std::numeric_limits<double>::max();
    double geo_max_x = std::numeric_limits<double>::lowest();
    double geo_max_y = std::numeric_limits<double>::lowest();

    for (const auto& polygon : polygons) {
        if (polygon.empty()) continue;

        // Process all rings (exterior + holes)
        for (const auto& ring : polygon.rings) {
            for (const auto& [x, y] : ring) {
                geo_min_x = std::min(geo_min_x, x);
                geo_max_x = std::max(geo_max_x, x);
                geo_min_y = std::min(geo_min_y, y);
                geo_max_y = std::max(geo_max_y, y);
            }
        }
    }

    // Add small margin if bounding box is degenerate
    if (geo_max_x - geo_min_x < 1e-6) {
        geo_min_x -= 0.5;
        geo_max_x += 0.5;
    }
    if (geo_max_y - geo_min_y < 1e-6) {
        geo_min_y -= 0.5;
        geo_max_y += 0.5;
    }

    // Convert geographic coordinates to pixel coordinates
    // This follows the same logic as RasterBuilder::geo_to_pixel

    double x_range = geo_bounds.max_x - geo_bounds.min_x;
    double y_range = geo_bounds.max_y - geo_bounds.min_y;

    // Calculate content area (accounting for margin)
    double content_width = width - 2 * options_.margin_px;
    double content_height = height - 2 * options_.margin_px;

    // Convert min point (bottom-left in geographic space)
    double norm_min_x = (geo_min_x - geo_bounds.min_x) / x_range;
    double norm_min_y = (geo_min_y - geo_bounds.min_y) / y_range;

    int pixel_min_x = static_cast<int>(options_.margin_px + norm_min_x * content_width);
    // Flip Y because raster Y increases downward, but geographic Y increases upward
    int pixel_max_y = static_cast<int>(options_.margin_px + (1.0 - norm_min_y) * content_height);

    // Convert max point (top-right in geographic space)
    double norm_max_x = (geo_max_x - geo_bounds.min_x) / x_range;
    double norm_max_y = (geo_max_y - geo_bounds.min_y) / y_range;

    int pixel_max_x = static_cast<int>(options_.margin_px + norm_max_x * content_width);
    // Flip Y
    int pixel_min_y = static_cast<int>(options_.margin_px + (1.0 - norm_max_y) * content_height);

    // Clamp to image bounds
    pixel_min_x = std::max(0, std::min(pixel_min_x, width - 1));
    pixel_max_x = std::max(0, std::min(pixel_max_x, width - 1));
    pixel_min_y = std::max(0, std::min(pixel_min_y, height - 1));
    pixel_max_y = std::max(0, std::min(pixel_max_y, height - 1));

    return {
        static_cast<double>(pixel_min_x),
        static_cast<double>(pixel_min_y),
        static_cast<double>(pixel_max_x),
        static_cast<double>(pixel_max_y)
    };
}

// Helper: Convert polygon geographic coordinates to pixel coordinates
std::vector<std::pair<int, int>> PNGExporter::get_polygon_pixels(
    const ContourLayer::PolygonData& polygon,
    const BoundingBox& geo_bounds,
    int width,
    int height) const {

    std::vector<std::pair<int, int>> pixels;
    if (polygon.empty()) return pixels;

    double x_range = geo_bounds.max_x - geo_bounds.min_x;
    double y_range = geo_bounds.max_y - geo_bounds.min_y;
    double content_width = width - 2 * options_.margin_px;
    double content_height = height - 2 * options_.margin_px;

    // Process exterior ring
    for (const auto& [geo_x, geo_y] : polygon.rings[0]) {
        double norm_x = (geo_x - geo_bounds.min_x) / x_range;
        double norm_y = (geo_y - geo_bounds.min_y) / y_range;

        int pixel_x = static_cast<int>(options_.margin_px + norm_x * content_width);
        int pixel_y = static_cast<int>(options_.margin_px + (1.0 - norm_y) * content_height);

        pixels.push_back({pixel_x, pixel_y});
    }

    return pixels;
}

// Helper: Find optimal rectangle for text placement in polygon
TextRect PNGExporter::find_optimal_text_rect(
    const std::vector<std::pair<int, int>>& polygon_pixels,
    int text_width_px,
    int text_height_px) const {

    if (polygon_pixels.empty()) {
        return {0, 0, 0, 0, false};
    }

    // Find bbox of polygon pixels
    int min_x = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::lowest();
    int min_y = std::numeric_limits<int>::max();
    int max_y = std::numeric_limits<int>::lowest();

    for (const auto& [x, y] : polygon_pixels) {
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }

    // Search for widest horizontal band that can fit text height
    int best_width = 0;
    int best_y = 0;

    for (int y = min_y; y <= max_y - text_height_px; ++y) {
        // Find horizontal extent at this y-band
        int band_min_x = std::numeric_limits<int>::max();
        int band_max_x = std::numeric_limits<int>::lowest();

        // Check all y positions within the text height band
        for (int dy = 0; dy < text_height_px; ++dy) {
            int check_y = y + dy;

            // Find polygon pixels at this y
            for (const auto& [px, py] : polygon_pixels) {
                if (py == check_y) {
                    band_min_x = std::min(band_min_x, px);
                    band_max_x = std::max(band_max_x, px);
                }
            }
        }

        if (band_max_x > band_min_x) {
            int band_width = band_max_x - band_min_x;
            if (band_width > best_width) {
                best_width = band_width;
                best_y = y + text_height_px / 2; // Center of band
            }
        }
    }

    // Check if text fits
    if (best_width < text_width_px) {
        return {0, 0, 0, 0, false};  // Doesn't fit
    }

    // Calculate center position
    int center_x = (min_x + max_x) / 2;  // Horizontal center of polygon

    return {center_x, best_y, best_width, text_height_px, true};
}

} // namespace topo
