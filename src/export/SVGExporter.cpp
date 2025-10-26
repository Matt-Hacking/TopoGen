/**
 * @file SVGExporter.cpp
 * @brief Implementation of SVG export for laser cutting
 *
 * High-performance C++ port of Python svg_export.py functionality
 * Generates optimized SVG files for laser cutting topographic models
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "SVGExporter.hpp"
#include "LabelRenderer.hpp"
#include "TextFitter.hpp"
#include "../core/Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <limits>

namespace topo {

// ============================================================================
// SVGExporter Implementation
// ============================================================================

SVGExporter::SVGExporter(const SVGConfig& config) : config_(config), logger_("SVGExporter") {
}

std::vector<std::string> SVGExporter::export_layers(
    const std::vector<ContourLayer>& layers,
    bool individual_files,
    double override_min_elev,
    double override_max_elev,
    const BoundingBox* geographic_bounds) {

    if (layers.empty()) {
        if (config_.verbose) {
            logger_.warning("No layers to export");
        }
        return {};
    }

    ensure_output_directory();

    // Determine elevation range - use override if provided, otherwise calculate from layers
    double global_min_elev, global_max_elev;
    if (std::isnan(override_min_elev) || std::isnan(override_max_elev)) {
        // Auto-calculate from layers
        global_min_elev = std::numeric_limits<double>::max();
        global_max_elev = std::numeric_limits<double>::lowest();
        for (const auto& layer : layers) {
            global_min_elev = std::min(global_min_elev, layer.elevation);
            global_max_elev = std::max(global_max_elev, layer.elevation);
        }
    } else {
        // Use provided global range
        global_min_elev = override_min_elev;
        global_max_elev = override_max_elev;
    }

    std::vector<std::string> generated_files;

    if (individual_files) {
        // Export each layer to separate file
        for (size_t i = 0; i < layers.size(); ++i) {
            const auto& layer = layers[i];

            if (layer.empty() && !config_.force_all_layers) {
                continue; // Skip empty layers unless explicitly requested
            }

            // Generate filename using pattern
            std::string filename_no_ext;
            if (config_.filename_pattern.empty()) {
                // Use default format
                std::ostringstream oss;
                oss << config_.base_filename << "_layer_"
                    << std::setfill('0') << std::setw(2) << (i + 1)
                    << "_elev_" << static_cast<int>(std::round(layer.elevation)) << "m";
                filename_no_ext = oss.str();
            } else {
                // Use pattern substitution
                filename_no_ext = LabelRenderer::substitute_filename_pattern(
                    config_.filename_pattern,
                    config_.base_filename,
                    static_cast<int>(i + 1),
                    layer.elevation
                );
            }
            std::string filename = filename_no_ext;

            // Pass layer index: -1 for base layer (first layer), 0 for all other layers
            // In export_single_layer(), svg_layers[0] is the current layer, so non-base layers use index 0
            int layer_idx = (i == 0) ? -1 : 0;

            // Pass next layer for hidden label placement (nullptr if this is the top layer)
            const ContourLayer* next_layer = (i + 1 < layers.size()) ? &layers[i + 1] : nullptr;

            // Pass i+1 as the global layer number (1-based: layer 1, 2, 3, ...)
            std::string svg_file = export_single_layer(layer, filename,
                                                       global_min_elev, global_max_elev,
                                                       geographic_bounds,
                                                       layer_idx,
                                                       next_layer,
                                                       static_cast<int>(i + 1));
            if (!svg_file.empty()) {
                generated_files.push_back(svg_file);
            }
        }

        if (config_.verbose) {
            logger_.info("Generated " + std::to_string(generated_files.size()) + " individual SVG files");
        }
    } else {
        // Export all layers to combined file
        std::string filename = config_.base_filename + "_combined";
        std::string svg_file = export_combined_layers(layers, filename, geographic_bounds);
        if (!svg_file.empty()) {
            generated_files.push_back(svg_file);
        }

        if (config_.verbose) {
            logger_.info("Generated combined SVG file: " + svg_file);
        }
    }

    return generated_files;
}

std::string SVGExporter::export_single_layer(
    const ContourLayer& layer,
    const std::string& filename,
    double global_min_elev,
    double global_max_elev,
    const BoundingBox* geographic_bounds,
    int layer_index,
    const ContourLayer* next_layer,
    int global_layer_number) {

    if (config_.verbose) {
        logger_.info("Exporting layer at " + std::to_string(layer.elevation) + "m...");
    }

    std::string svg_path = get_output_path(filename + ".svg");
    std::ofstream file(svg_path);

    if (!file.is_open()) {
        logger_.error("Could not create SVG file: " + svg_path);
        return "";
    }

    // Convert to SVG layer format
    std::vector<ContourLayer> single_layer = {layer};

    // Use geographic bounds if provided, otherwise calculate from polygons
    BoundingBox bbox = (geographic_bounds != nullptr) ?
        *geographic_bounds :
        calculate_bounding_box(single_layer);

    // Calculate actual canvas dimensions based on geographic aspect ratio
    double actual_width_mm = config_.width_mm;
    double actual_height_mm = config_.height_mm;

    if (config_.auto_scale) {
        // Adjust canvas height to match geographic aspect ratio
        double geo_aspect = (bbox.max_y - bbox.min_y) / (bbox.max_x - bbox.min_x);
        double available_width = config_.width_mm - 2 * config_.margin_mm;
        double available_height = available_width * geo_aspect;
        actual_height_mm = available_height + 2 * config_.margin_mm;
    }

    double scale_factor = config_.auto_scale ? calculate_scale_factor(bbox, actual_width_mm, actual_height_mm) : config_.scale_factor;

    std::vector<SVGLayer> svg_layers;
    svg_layers.emplace_back(generate_layer_id(layer.elevation, 0), layer.elevation);
    svg_layers[0].polygons = layer.polygons;
    svg_layers[0].stroke_color = config_.stroke_color;
    svg_layers[0].stroke_width = config_.cut_stroke_width;

    // If next layer exists, add it to svg_layers for hidden label placement calculation
    if (next_layer != nullptr) {
        svg_layers.emplace_back(generate_layer_id(next_layer->elevation, 1), next_layer->elevation);
        svg_layers[1].polygons = next_layer->polygons;
    }

    // Transform coordinates using actual canvas dimensions
    transform_coordinates(svg_layers, bbox, scale_factor, actual_width_mm, actual_height_mm);

    // Simplify for laser cutting
    if (config_.simplify_tolerance > 0) {
        simplify_polygons(svg_layers);
    }

    // Write SVG content with actual calculated dimensions
    write_svg_header(file, actual_width_mm, actual_height_mm);
    write_background(file, actual_width_mm, actual_height_mm);
    write_layer_definitions(file);

    // Use global elevation range for proper color mapping
    // (passed from export_layers() which calculated it across all layers)

    // Write layer content (only first layer - second layer is just for hidden bbox calculation)
    if (!svg_layers.empty()) {
        const auto& svg_layer = svg_layers[0];
        file << "  <g id=\"" << svg_layer.layer_id << "\" inkscape:label=\"Layer "
             << svg_layer.elevation << "m\">\n";

        for (const auto& polygon : svg_layer.polygons) {
            write_polygon(file, polygon, svg_layer.layer_id,
                         svg_layer.stroke_color, svg_layer.stroke_width,
                         svg_layer.elevation, global_min_elev, global_max_elev);
        }

        file << "  </g>\n";
    }

    // Write additional elements
    if (config_.include_alignment_marks) {
        write_alignment_marks(file, actual_width_mm, actual_height_mm);
    }

    if (config_.include_elevation_labels) {
        // Only write label for first layer (second layer is for hidden bbox calculation only)
        std::vector<SVGLayer> single_layer_for_label;
        if (!svg_layers.empty()) {
            single_layer_for_label.push_back(svg_layers[0]);
        }
        write_elevation_labels(file, single_layer_for_label);
    }

    // Write comprehensive labels if any label templates are configured
    bool has_label_templates = !config_.base_label_visible.empty() ||
                               !config_.base_label_hidden.empty() ||
                               !config_.layer_label_visible.empty() ||
                               !config_.layer_label_hidden.empty();
    if (has_label_templates) {
        write_comprehensive_labels(file, svg_layers, layer_index, actual_width_mm, actual_height_mm, global_layer_number);
    }

    if (config_.include_cut_guidelines) {
        write_cutting_guidelines(file, actual_width_mm, actual_height_mm);
    }

    write_svg_footer(file);
    file.close();

    if (config_.verbose) {
        logger_.info("  Created: " + svg_path);
    }

    return svg_path;
}

std::string SVGExporter::export_combined_layers(
    const std::vector<ContourLayer>& layers,
    const std::string& filename,
    const BoundingBox* geographic_bounds) {

    if (config_.verbose) {
        logger_.info("Exporting combined layers (" + std::to_string(layers.size()) + " layers)...");
    }

    std::string svg_path = get_output_path(filename + ".svg");
    std::ofstream file(svg_path);

    if (!file.is_open()) {
        logger_.error("Could not create SVG file: " + svg_path);
        return "";
    }
    
    // Calculate overall bounding box and scale
    // Use geographic bounds if provided, otherwise calculate from polygons
    BoundingBox bbox = (geographic_bounds != nullptr) ?
        *geographic_bounds :
        calculate_bounding_box(layers);

    // Calculate actual canvas dimensions based on geographic aspect ratio
    double actual_width_mm = config_.width_mm;
    double actual_height_mm = config_.height_mm;

    if (config_.auto_scale) {
        // Adjust canvas height to match geographic aspect ratio
        double geo_aspect = (bbox.max_y - bbox.min_y) / (bbox.max_x - bbox.min_x);
        double available_width = config_.width_mm - 2 * config_.margin_mm;
        double available_height = available_width * geo_aspect;
        actual_height_mm = available_height + 2 * config_.margin_mm;
    }

    double scale_factor = config_.auto_scale ? calculate_scale_factor(bbox, actual_width_mm, actual_height_mm) : config_.scale_factor;
    
    // Convert to SVG layers
    std::vector<SVGLayer> svg_layers;
    for (size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];
        
        svg_layers.emplace_back(generate_layer_id(layer.elevation, i), layer.elevation);
        svg_layers.back().polygons = layer.polygons;
        
        // Color coding by elevation (simple gradient)
        double t = static_cast<double>(i) / std::max(1.0, static_cast<double>(layers.size() - 1));
        int red = static_cast<int>(255 * (1.0 - t));
        int blue = static_cast<int>(255 * t);
        
        std::ostringstream color;
        color << "#" << std::hex << std::setfill('0') << std::setw(2) << red
              << "00" << std::setw(2) << blue;
        svg_layers.back().stroke_color = color.str();
        svg_layers.back().stroke_width = config_.cut_stroke_width;
    }

    // Transform coordinates using actual canvas dimensions
    transform_coordinates(svg_layers, bbox, scale_factor, actual_width_mm, actual_height_mm);
    
    // Simplify for laser cutting
    if (config_.simplify_tolerance > 0) {
        simplify_polygons(svg_layers);
    }

    // Write SVG content with actual calculated dimensions
    write_svg_header(file, actual_width_mm, actual_height_mm);
    write_background(file, actual_width_mm, actual_height_mm);
    write_layer_definitions(file);

    // Calculate elevation range for render modes
    double min_elev = std::numeric_limits<double>::max();
    double max_elev = std::numeric_limits<double>::lowest();
    for (const auto& svg_layer : svg_layers) {
        min_elev = std::min(min_elev, svg_layer.elevation);
        max_elev = std::max(max_elev, svg_layer.elevation);
    }

    // Write each layer
    for (const auto& svg_layer : svg_layers) {
        file << "  <g id=\"" << svg_layer.layer_id << "\" inkscape:label=\"Layer "
             << svg_layer.elevation << "m\"";

        if (!svg_layer.visible) {
            file << " style=\"display:none\"";
        }

        file << ">\n";

        for (const auto& polygon : svg_layer.polygons) {
            write_polygon(file, polygon, svg_layer.layer_id,
                         svg_layer.stroke_color, svg_layer.stroke_width,
                         svg_layer.elevation, min_elev, max_elev);
        }

        file << "  </g>\n";
    }
    
    // Write additional elements
    if (config_.include_alignment_marks) {
        write_alignment_marks(file, actual_width_mm, actual_height_mm);
    }

    if (config_.include_elevation_labels) {
        write_elevation_labels(file, svg_layers);
    }

    // Write comprehensive labels if any label templates are configured
    // For combined exports, treat as base layer (-1)
    bool has_label_templates = !config_.base_label_visible.empty() ||
                               !config_.base_label_hidden.empty() ||
                               !config_.layer_label_visible.empty() ||
                               !config_.layer_label_hidden.empty();
    if (has_label_templates) {
        write_comprehensive_labels(file, svg_layers, -1, actual_width_mm, actual_height_mm);
    }

    if (config_.include_cut_guidelines) {
        write_cutting_guidelines(file, actual_width_mm, actual_height_mm);
    }

    write_svg_footer(file);
    file.close();

    if (config_.verbose) {
        logger_.info("  Created: " + svg_path);
    }

    return svg_path;
}

BoundingBox SVGExporter::calculate_bounding_box(const std::vector<ContourLayer>& layers) const {
    if (layers.empty()) {
        return {0, 0, 0, 0};
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();

    for (const auto& layer : layers) {
        for (const auto& polygon : layer.polygons) {
            if (polygon.empty()) continue;

            // Process all rings (exterior + holes)
            for (const auto& ring : polygon.rings) {
                for (const auto& [x, y] : ring) {
                    min_x = std::min(min_x, x);
                    max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y);
                    max_y = std::max(max_y, y);
                }
            }
        }
    }

    // Add small margin if bounding box is degenerate
    if (max_x - min_x < 1e-6) {
        min_x -= 0.5;
        max_x += 0.5;
    }
    if (max_y - min_y < 1e-6) {
        min_y -= 0.5;
        max_y += 0.5;
    }

    return {min_x, min_y, max_x, max_y};
}

BoundingBox SVGExporter::calculate_svg_polygon_bbox(const std::vector<ContourLayer::PolygonData>& polygons) const {
    if (polygons.empty()) {
        return {0, 0, 0, 0};
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();

    for (const auto& polygon : polygons) {
        if (polygon.empty()) continue;

        // Process all rings (exterior + holes)
        for (const auto& ring : polygon.rings) {
            for (const auto& [x, y] : ring) {
                min_x = std::min(min_x, x);
                max_x = std::max(max_x, x);
                min_y = std::min(min_y, y);
                max_y = std::max(max_y, y);
            }
        }
    }

    // Add small margin if bounding box is degenerate
    if (max_x - min_x < 1e-6) {
        min_x -= 0.5;
        max_x += 0.5;
    }
    if (max_y - min_y < 1e-6) {
        min_y -= 0.5;
        max_y += 0.5;
    }

    return {min_x, min_y, max_x, max_y};
}

double SVGExporter::calculate_scale_factor(const BoundingBox& bbox) const {
    return calculate_scale_factor(bbox, config_.width_mm, config_.height_mm);
}

double SVGExporter::calculate_scale_factor(const BoundingBox& bbox, double canvas_width_mm, double canvas_height_mm) const {
    double content_width = bbox.max_x - bbox.min_x;
    double content_height = bbox.max_y - bbox.min_y;

    // Available space considering margins
    double available_width = canvas_width_mm - 2 * config_.margin_mm;
    double available_height = canvas_height_mm - 2 * config_.margin_mm;

    // Calculate scale to fit within available space
    double scale_x = available_width / content_width;
    double scale_y = available_height / content_height;

    return std::min(scale_x, scale_y);
}

void SVGExporter::transform_coordinates(
    std::vector<SVGLayer>& svg_layers,
    const BoundingBox& bbox,
    double scale_factor,
    double actual_width_mm,
    double actual_height_mm) const {

    double content_width = (bbox.max_x - bbox.min_x) * scale_factor;
    double content_height = (bbox.max_y - bbox.min_y) * scale_factor;

    // Calculate offsets for centering using actual canvas dimensions
    double offset_x = config_.center_content ?
        (actual_width_mm - content_width) / 2.0 : config_.margin_mm;
    double offset_y = config_.center_content ?
        (actual_height_mm - content_height) / 2.0 : config_.margin_mm;
    
    for (auto& svg_layer : svg_layers) {
        std::vector<ContourLayer::PolygonData> transformed_polygons;

        for (const auto& polygon : svg_layer.polygons) {
            ContourLayer::PolygonData transformed_poly;

            // Transform all rings (exterior + holes)
            for (const auto& ring : polygon.rings) {
                std::vector<std::pair<double, double>> transformed_ring;

                for (const auto& [x, y] : ring) {
                    // Scale and translate
                    double svg_x = (x - bbox.min_x) * scale_factor + offset_x;
                    // BUGFIX: Use actual_height_mm instead of config_.height_mm for correct Y-flip
                    // when auto_scale adjusts canvas dimensions based on geographic aspect ratio
                    double svg_y = actual_height_mm - ((y - bbox.min_y) * scale_factor + offset_y); // Flip Y

                    transformed_ring.emplace_back(svg_x, svg_y);
                }

                if (transformed_ring.size() >= 3) {
                    transformed_poly.rings.push_back(std::move(transformed_ring));
                }
            }

            if (!transformed_poly.empty()) {
                transformed_polygons.push_back(std::move(transformed_poly));
            }
        }

        svg_layer.polygons = std::move(transformed_polygons);
    }
}

void SVGExporter::simplify_polygons(std::vector<SVGLayer>& svg_layers) const {
    if (config_.simplify_tolerance <= 0) return;

    for (auto& svg_layer : svg_layers) {
        std::vector<ContourLayer::PolygonData> simplified_polygons;

        for (const auto& polygon : svg_layer.polygons) {
            if (polygon.empty()) continue;

            ContourLayer::PolygonData simplified_poly;

            // Simplify all rings (exterior + holes)
            for (const auto& ring : polygon.rings) {
                if (ring.size() < 3) continue;

                // Simple vertex decimation based on distance threshold
                std::vector<std::pair<double, double>> simplified_ring;
                simplified_ring.push_back(ring[0]);

                for (size_t i = 1; i < ring.size(); ++i) {
                    const auto& prev = simplified_ring.back();
                    const auto& curr = ring[i];

                    double dx = curr.first - prev.first;
                    double dy = curr.second - prev.second;
                    double dist = std::sqrt(dx*dx + dy*dy);

                    if (dist >= config_.simplify_tolerance) {
                        simplified_ring.push_back(curr);
                    }
                }

                if (simplified_ring.size() >= 3) {
                    simplified_poly.rings.push_back(std::move(simplified_ring));
                }
            }

            if (!simplified_poly.empty()) {
                simplified_polygons.push_back(std::move(simplified_poly));
            }
        }

        svg_layer.polygons = std::move(simplified_polygons);
    }
}

void SVGExporter::write_svg_header(std::ofstream& file) const {
    write_svg_header(file, config_.width_mm, config_.height_mm);
}

void SVGExporter::write_svg_header(std::ofstream& file, double width_mm, double height_mm) const {
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    file << "<svg\n";
    file << "  width=\"" << width_mm << "mm\"\n";
    file << "  height=\"" << height_mm << "mm\"\n";
    file << "  viewBox=\"0 0 " << width_mm << " " << height_mm << "\"\n";
    file << "  version=\"1.1\"\n";
    file << "  xmlns=\"http://www.w3.org/2000/svg\"\n";
    file << "  xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\">\n";
    file << "  <title>Topographic Layers - Generated by TopographicGenerator</title>\n";
    file << "  <defs>\n";
    file << "    <style>\n";
    file << "      .cut-line { stroke-width: " << config_.cut_stroke_width << "mm; }\n";
    file << "      .alignment-mark { fill: none; stroke-width: " << config_.alignment_stroke_width << "mm; }\n";
    file << "      .elevation-label { font-family: Arial, sans-serif; font-size: 3mm; }\n";
    file << "    </style>\n";
    file << "  </defs>\n";
}

void SVGExporter::write_svg_footer(std::ofstream& file) const {
    file << "</svg>\n";
}

void SVGExporter::write_background(std::ofstream& file) const {
    write_background(file, config_.width_mm, config_.height_mm);
}

void SVGExporter::write_background(std::ofstream& file, double width_mm, double height_mm) const {
    // Add full-canvas background rectangle
    file << "  <rect\n";
    file << "    x=\"0\"\n";
    file << "    y=\"0\"\n";
    file << "    width=\"" << width_mm << "\"\n";
    file << "    height=\"" << height_mm << "\"\n";
    file << "    fill=\"" << config_.background_color << "\"\n";
    file << "    inkscape:label=\"Background\"\n";
    file << "  />\n";
}

void SVGExporter::write_layer_definitions([[maybe_unused]] std::ofstream& file) const {
    // Layer definitions are handled inline for simplicity
}

std::string SVGExporter::get_fill_color(double elevation, double min_elevation, double max_elevation) const {
    // Convert ColorScheme enum to ColorMapper::Scheme
    ColorMapper::Scheme scheme;
    switch (config_.color_scheme) {
        case TopographicConfig::ColorScheme::GRAYSCALE:
            scheme = ColorMapper::Scheme::GRAYSCALE;
            break;
        case TopographicConfig::ColorScheme::TERRAIN:
            scheme = ColorMapper::Scheme::TERRAIN;
            break;
        case TopographicConfig::ColorScheme::RAINBOW:
            scheme = ColorMapper::Scheme::RAINBOW;
            break;
        case TopographicConfig::ColorScheme::TOPOGRAPHIC:
            scheme = ColorMapper::Scheme::TOPOGRAPHIC;
            break;
        case TopographicConfig::ColorScheme::HYPSOMETRIC:
            scheme = ColorMapper::Scheme::HYPSOMETRIC;
            break;
        default:
            scheme = ColorMapper::Scheme::TERRAIN;
            break;
    }

    ColorMapper color_mapper(scheme);
    auto rgb = color_mapper.map_elevation_to_color(elevation, min_elevation, max_elevation);

    // Convert RGB float [0-1] to hex color string
    int r = static_cast<int>(rgb[0] * 255);
    int g = static_cast<int>(rgb[1] * 255);
    int b = static_cast<int>(rgb[2] * 255);

    std::ostringstream color_str;
    color_str << "#" << std::hex << std::setfill('0')
              << std::setw(2) << r
              << std::setw(2) << g
              << std::setw(2) << b;

    return color_str.str();
}

void SVGExporter::write_polygon(
    std::ofstream& file,
    const ContourLayer::PolygonData& polygon,
    const std::string& layer_id,
    const std::string& stroke_color,
    double stroke_width,
    double elevation,
    double min_elevation,
    double max_elevation) const {

    if (polygon.empty()) return;

    std::string path_data = polygon_to_path_data(polygon);
    if (path_data.empty()) return;

    // Determine fill based on render mode
    std::string fill_value = "none";
    std::string fill_opacity = "1.0";

    switch (config_.render_mode) {
        case TopographicConfig::RenderMode::FULL_COLOR:
            // FULL_COLOR means fill with colors according to ColorScheme
            fill_value = get_fill_color(elevation, min_elevation, max_elevation);
            fill_opacity = "0.85";  // Slightly transparent for better layering
            break;

        case TopographicConfig::RenderMode::GRAYSCALE:
            // GRAYSCALE is just an alias for ColorScheme::GRAYSCALE
            // Use the ColorMapper with grayscale scheme
            fill_value = get_fill_color(elevation, min_elevation, max_elevation);
            fill_opacity = "0.7";  // Slightly transparent for better layering
            break;

        case TopographicConfig::RenderMode::MONOCHROME:
            fill_value = "none";  // Outline only, no fills
            break;
    }

    file << "    <path\n";
    file << "      d=\"" << path_data << "\"\n";
    file << "      class=\"cut-line\"\n";
    file << "      stroke=\"" << stroke_color << "\"\n";
    file << "      stroke-width=\"" << stroke_width << "mm\"\n";
    file << "      fill=\"" << fill_value << "\"\n";
    if (fill_value != "none") {
        file << "      fill-opacity=\"" << fill_opacity << "\"\n";
    }
    file << "      inkscape:label=\"Elevation " << layer_id << "\"\n";
    file << "    />\n";
}

void SVGExporter::write_alignment_marks(std::ofstream& file) const {
    write_alignment_marks(file, config_.width_mm, config_.height_mm);
}

void SVGExporter::write_alignment_marks(std::ofstream& file, double width_mm, double height_mm) const {
    file << "  <g id=\"alignment-marks\" inkscape:label=\"Alignment Marks\">\n";

    double mark_size = 5.0; // mm
    double margin = config_.margin_mm / 2.0;

    // Corner alignment marks
    std::vector<std::pair<double, double>> corners = {
        {margin, margin}, // bottom-left
        {width_mm - margin, margin}, // bottom-right
        {width_mm - margin, height_mm - margin}, // top-right
        {margin, height_mm - margin} // top-left
    };

    for (const auto& corner : corners) {
        double x = corner.first;
        double y = corner.second;

        // Cross mark
        file << "    <path\n";
        file << "      d=\"M " << (x - mark_size/2) << " " << y << " L " << (x + mark_size/2) << " " << y << " M ";
        file << x << " " << (y - mark_size/2) << " L " << x << " " << (y + mark_size/2) << "\"\n";
        file << "      class=\"alignment-mark\"\n";
        file << "      stroke=\"" << config_.alignment_color << "\"\n";
        file << "    />\n";
    }

    file << "  </g>\n";
}

void SVGExporter::write_elevation_labels(
    std::ofstream& file,
    const std::vector<SVGLayer>& svg_layers) const {
    
    file << "  <g id=\"elevation-labels\" inkscape:label=\"Elevation Labels\">\n";
    
    double label_x = config_.margin_mm;
    double label_y_start = config_.margin_mm + 5.0;
    double label_spacing = 5.0; // mm
    
    for (size_t i = 0; i < svg_layers.size(); ++i) {
        const auto& svg_layer = svg_layers[i];
        double y = label_y_start + i * label_spacing;
        
        file << "    <text\n";
        file << "      x=\"" << label_x << "\"\n";
        file << "      y=\"" << y << "\"\n";
        file << "      class=\"elevation-label\"\n";
        file << "      fill=\"" << config_.text_color << "\"\n";
        file << "    >" << static_cast<int>(std::round(svg_layer.elevation)) << "m</text>\n";
    }
    
    file << "  </g>\n";
}

void SVGExporter::write_comprehensive_labels(
    std::ofstream& file,
    const std::vector<SVGLayer>& svg_layers,
    int layer_index) const {
    write_comprehensive_labels(file, svg_layers, layer_index, config_.width_mm, config_.height_mm);
}

void SVGExporter::write_comprehensive_labels(
    std::ofstream& file,
    const std::vector<SVGLayer>& svg_layers,
    int layer_index,
    double width_mm,
    double height_mm,
    int global_layer_number) const {

    // Create LabelConfig from SVGConfig
    LabelConfig label_config;
    label_config.base_label_visible = config_.base_label_visible;
    label_config.base_label_hidden = config_.base_label_hidden;
    label_config.layer_label_visible = config_.layer_label_visible;
    label_config.layer_label_hidden = config_.layer_label_hidden;
    label_config.visible_label_color = config_.visible_label_color;
    label_config.hidden_label_color = config_.hidden_label_color;
    label_config.base_font_size_mm = config_.base_font_size_mm;
    label_config.layer_font_size_mm = config_.layer_font_size_mm;
    label_config.label_units = config_.label_units;
    label_config.print_units = config_.print_units;
    label_config.land_units = config_.land_units;

    // Create TextFitter for adaptive fitting
    TextFitter::Config fitter_config;
    fitter_config.max_bend_angle_deg = 15.0;
    fitter_config.min_scale_factor = 0.5;
    fitter_config.max_split_parts = 3;
    fitter_config.min_legible_size_mm = 1.5;
    fitter_config.margin_mm = 0.5;
    TextFitter text_fitter(fitter_config);

    // Create LabelRenderer
    LabelRenderer label_renderer(label_config);

    // Create LabelContext
    LabelContext context;
    context.geographic_bounds = config_.geographic_bounds;
    context.scale_ratio = config_.scale_ratio;
    context.contour_height_m = config_.contour_height_m;
    context.substrate_size_mm = config_.substrate_size_mm;

    // Content bounding box (area within margins) - use actual canvas dimensions
    context.content_bbox.min_x = config_.margin_mm;
    context.content_bbox.max_x = width_mm - config_.margin_mm;
    context.content_bbox.min_y = config_.margin_mm;
    context.content_bbox.max_y = height_mm - config_.margin_mm;

    // Hidden area bbox (will be covered by next layer)
    // If next layer exists in svg_layers[1], use its geometry; otherwise use center 60% heuristic
    if (svg_layers.size() > 1 && !svg_layers[1].polygons.empty()) {
        // Calculate bbox from next layer's actual geometry
        context.hidden_bbox = calculate_svg_polygon_bbox(svg_layers[1].polygons);
        // Pass polygon data for curved text path generation
        context.next_layer_polygons = &svg_layers[1].polygons;
    } else {
        // Fallback: use center 60% of content area
        double content_width = context.content_bbox.max_x - context.content_bbox.min_x;
        double content_height = context.content_bbox.max_y - context.content_bbox.min_y;
        double hidden_margin_x = content_width * 0.2;  // 20% margin on each side
        double hidden_margin_y = content_height * 0.2;
        context.hidden_bbox.min_x = context.content_bbox.min_x + hidden_margin_x;
        context.hidden_bbox.max_x = context.content_bbox.max_x - hidden_margin_x;
        context.hidden_bbox.min_y = context.content_bbox.min_y + hidden_margin_y;
        context.hidden_bbox.max_y = context.content_bbox.max_y - hidden_margin_y;
        context.next_layer_polygons = nullptr;
    }

    bool is_base = (layer_index == -1);

    // Determine layer number: use global_layer_number if provided, otherwise calculate from layer_index
    if (is_base) {
        context.layer_number = (global_layer_number >= 0) ? global_layer_number : 0;
        context.elevation_m = !svg_layers.empty() ? svg_layers[0].elevation : 0.0;
    } else if (layer_index >= 0 && layer_index < static_cast<int>(svg_layers.size())) {
        // Use global_layer_number if provided, otherwise fall back to layer_index + 1
        context.layer_number = (global_layer_number >= 0) ? global_layer_number : (layer_index + 1);
        context.elevation_m = svg_layers[layer_index].elevation;
    } else {
        return; // Invalid layer index
    }

    // Generate labels
    std::optional<PlacedLabel> visible_label;
    std::optional<PlacedLabel> hidden_label;

    if (is_base) {
        visible_label = label_renderer.generate_base_visible_label(context);
        hidden_label = label_renderer.generate_base_hidden_label(context);
    } else {
        visible_label = label_renderer.generate_layer_visible_label(context);
        hidden_label = label_renderer.generate_layer_hidden_label(context);
    }

    file << "  <g id=\"comprehensive-labels\" inkscape:label=\"Comprehensive Labels\">\n";

    // Write visible label if present
    if (visible_label) {
        BoundingBox visible_bbox = context.content_bbox;

        FittedText fitted = text_fitter.fit_text(
            visible_label->text,
            visible_label->x,
            visible_label->y,
            visible_label->font_size_mm,
            visible_bbox,
            visible_label->anchor
        );

        if (config_.verbose && !fitted.warning.empty()) {
            logger_.debug("  Visible label: " + fitted.warning);
        }

        // Write visible label (could be split into multiple parts)
        if (fitted.was_split) {
            for (size_t i = 0; i < fitted.split_parts.size(); ++i) {
                file << "    <text\n";
                file << "      x=\"" << fitted.split_positions[i].first << "\"\n";
                file << "      y=\"" << fitted.split_positions[i].second << "\"\n";
                file << "      text-anchor=\"" << visible_label->anchor << "\"\n";
                file << "      font-size=\"" << fitted.font_size_mm << "mm\"\n";
                file << "      fill=\"" << visible_label->color << "\"\n";
                file << "      font-family=\"Arial, sans-serif\"\n";
                file << "    >" << fitted.split_parts[i] << "</text>\n";
            }
        } else {
            file << "    <text\n";
            file << "      x=\"" << fitted.x << "\"\n";
            file << "      y=\"" << fitted.y << "\"\n";
            file << "      text-anchor=\"" << visible_label->anchor << "\"\n";
            file << "      font-size=\"" << fitted.font_size_mm << "mm\"\n";
            file << "      fill=\"" << visible_label->color << "\"\n";
            file << "      font-family=\"Arial, sans-serif\"\n";
            file << "    >" << fitted.text << "</text>\n";
        }
    }

    // Write hidden label if present
    if (hidden_label) {
        // Check if label has curved path data
        if (hidden_label->has_curved_path && !hidden_label->svg_path_d.empty()) {
            // Render curved text using SVG <textPath>

            if (config_.verbose && !hidden_label->modification_warning.empty()) {
                logger_.debug("  Hidden label: " + hidden_label->modification_warning);
            }

            // Write path definition in <defs> section
            file << "    <defs>\n";
            file << "      <path id=\"" << hidden_label->svg_path_id << "\"\n";
            file << "            d=\"" << hidden_label->svg_path_d << "\"/>\n";
            file << "    </defs>\n";

            // Write text on path
            file << "    <text\n";
            file << "      text-anchor=\"" << hidden_label->anchor << "\"\n";
            file << "      font-size=\"" << hidden_label->font_size_mm << "mm\"\n";
            file << "      fill=\"" << hidden_label->color << "\"\n";
            file << "      font-family=\"Arial, sans-serif\"\n";
            file << "      opacity=\"0.7\"\n";
            file << "    >\n";
            file << "      <textPath href=\"#" << hidden_label->svg_path_id << "\" startOffset=\"50%\">\n";
            file << "        " << hidden_label->text << "\n";
            file << "      </textPath>\n";
            file << "    </text>\n";
        } else {
            // Fallback to straight text with TextFitter
            BoundingBox hidden_bbox = context.hidden_bbox;

            FittedText fitted = text_fitter.fit_text(
                hidden_label->text,
                hidden_label->x,
                hidden_label->y,
                hidden_label->font_size_mm,
                hidden_bbox,
                hidden_label->anchor
            );

            if (config_.verbose && !fitted.warning.empty()) {
                logger_.debug("  Hidden label: " + fitted.warning);
            }

            // Write hidden label (could be split into multiple parts)
            if (fitted.was_split) {
                for (size_t i = 0; i < fitted.split_parts.size(); ++i) {
                    file << "    <text\n";
                    file << "      x=\"" << fitted.split_positions[i].first << "\"\n";
                    file << "      y=\"" << fitted.split_positions[i].second << "\"\n";
                    file << "      text-anchor=\"" << hidden_label->anchor << "\"\n";
                    file << "      font-size=\"" << fitted.font_size_mm << "mm\"\n";
                    file << "      fill=\"" << hidden_label->color << "\"\n";
                    file << "      font-family=\"Arial, sans-serif\"\n";
                    file << "      opacity=\"0.7\"\n";
                    file << "    >" << fitted.split_parts[i] << "</text>\n";
                }
            } else {
                file << "    <text\n";
                file << "      x=\"" << fitted.x << "\"\n";
                file << "      y=\"" << fitted.y << "\"\n";
                file << "      text-anchor=\"" << hidden_label->anchor << "\"\n";
                file << "      font-size=\"" << fitted.font_size_mm << "mm\"\n";
                file << "      fill=\"" << hidden_label->color << "\"\n";
                file << "      font-family=\"Arial, sans-serif\"\n";
                file << "      opacity=\"0.7\"\n";
                file << "    >" << fitted.text << "</text>\n";
            }
        }
    }

    file << "  </g>\n";
}

void SVGExporter::write_cutting_guidelines(std::ofstream& file) const {
    write_cutting_guidelines(file, config_.width_mm, config_.height_mm);
}

void SVGExporter::write_cutting_guidelines(std::ofstream& file, double width_mm, double height_mm) const {
    file << "  <g id=\"cutting-guidelines\" inkscape:label=\"Cutting Guidelines\">\n";

    // Page border
    file << "    <rect\n";
    file << "      x=\"" << config_.margin_mm << "\"\n";
    file << "      y=\"" << config_.margin_mm << "\"\n";
    file << "      width=\"" << (width_mm - 2 * config_.margin_mm) << "\"\n";
    file << "      height=\"" << (height_mm - 2 * config_.margin_mm) << "\"\n";
    file << "      fill=\"none\"\n";
    file << "      stroke=\"" << config_.alignment_color << "\"\n";
    file << "      stroke-width=\"0.1mm\"\n";
    file << "      stroke-dasharray=\"1,1\"\n";
    file << "    />\n";

    file << "  </g>\n";
}

std::string SVGExporter::polygon_to_path_data(const ContourLayer::PolygonData& polygon) const {
    if (polygon.rings.empty() || polygon.rings[0].empty()) return "";

    std::ostringstream path;

    // Outer boundary (first ring)
    const auto& exterior = polygon.rings[0];
    if (!exterior.empty()) {
        path << "M " << exterior[0].first << " " << exterior[0].second;

        for (size_t i = 1; i < exterior.size(); ++i) {
            path << " L " << exterior[i].first << " " << exterior[i].second;
        }

        path << " Z"; // Close path
    }

    // Handle holes if not removing them (subsequent rings)
    if (!config_.remove_holes && polygon.rings.size() > 1) {
        for (size_t ring_idx = 1; ring_idx < polygon.rings.size(); ++ring_idx) {
            const auto& hole = polygon.rings[ring_idx];

            if (!hole.empty()) {
                path << " M " << hole[0].first << " " << hole[0].second;

                for (size_t i = 1; i < hole.size(); ++i) {
                    path << " L " << hole[i].first << " " << hole[i].second;
                }

                path << " Z"; // Close hole
            }
        }
    }

    return path.str();
}

std::string SVGExporter::generate_layer_id(double elevation, int index) const {
    std::ostringstream id;
    id << "layer_" << std::setfill('0') << std::setw(2) << index
       << "_elev_" << static_cast<int>(std::round(elevation));
    return id.str();
}

void SVGExporter::ensure_output_directory() const {
    std::filesystem::create_directories(config_.output_directory);
}

std::string SVGExporter::get_output_path(const std::string& filename) const {
    return (std::filesystem::path(config_.output_directory) / filename).string();
}

} // namespace topo