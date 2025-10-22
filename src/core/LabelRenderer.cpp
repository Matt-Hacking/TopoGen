/**
 * @file LabelRenderer.cpp
 * @brief Implementation of label rendering and pattern substitution
 */

#include "LabelRenderer.hpp"
#include "TextPathGenerator.hpp"
#include "ContourGenerator.hpp"  // For ContourLayer::PolygonData implementation
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <regex>
#include <map>

namespace topo {

LabelRenderer::LabelRenderer(const LabelConfig& config)
    : config_(config) {
}

std::string LabelRenderer::substitute_patterns(const std::string& template_text, const LabelContext& context) const {
    if (template_text.empty()) {
        return "";
    }

    // First, handle escaping: %%{...} → %{...} (temporary placeholder)
    std::string result = template_text;
    const std::string escape_placeholder = "\x01ESCAPED_PERCENT\x01";  // Unlikely to appear in user text
    size_t pos = 0;
    while ((pos = result.find("%%{", pos)) != std::string::npos) {
        result.replace(pos, 2, escape_placeholder);
        pos += escape_placeholder.length();
    }

    // Build pattern substitution map
    std::map<std::string, std::string> pattern_map;

    const auto& bounds = context.geographic_bounds;
    double center_lon = (bounds.min_x + bounds.max_x) / 2.0;
    double center_lat = (bounds.min_y + bounds.max_y) / 2.0;
    double area_width_m = (bounds.max_x - bounds.min_x);
    double area_height_m = (bounds.max_y - bounds.min_y);

    // Single-letter patterns
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << context.scale_ratio;
        pattern_map["s"] = oss.str();  // Scale ratio
    }
    pattern_map["c"] = format_distance(context.contour_height_m, false);  // Contour height
    if (context.layer_number > 0) {
        pattern_map["n"] = std::to_string(context.layer_number);  // Layer number
    }

    // Layer and elevation patterns (for label text)
    {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << context.layer_number;
        pattern_map["l"] = oss.str();  // Layer number (zero-padded)
    }
    pattern_map["e"] = std::to_string(static_cast<int>(std::round(context.elevation_m)));  // Elevation in meters
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << center_lon;
        pattern_map["x"] = oss.str();  // Center longitude
    }
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4) << center_lat;
        pattern_map["y"] = oss.str();  // Center latitude
    }
    pattern_map["w"] = format_distance(area_width_m, false);  // Area width
    pattern_map["h"] = format_distance(area_height_m, false);  // Area height
    if (context.substrate_size_mm > 0) {
        pattern_map["W"] = format_distance(context.substrate_size_mm / 1000.0, true);  // Substrate width (mm→m→formatted)
        pattern_map["H"] = format_distance(context.substrate_size_mm / 1000.0, true);  // Substrate height (square)
    }

    // Multi-letter coordinate patterns
    pattern_map["C"] = format_coordinate(center_lat, center_lon);  // Center full coordinate
    pattern_map["UL"] = format_coordinate(bounds.max_y, bounds.min_x);  // Upper-left
    pattern_map["UR"] = format_coordinate(bounds.max_y, bounds.max_x);  // Upper-right
    pattern_map["LL"] = format_coordinate(bounds.min_y, bounds.min_x);  // Lower-left
    pattern_map["LR"] = format_coordinate(bounds.min_y, bounds.max_x);  // Lower-right

    // Regex to match %{identifier} where identifier is one or more letters
    std::regex pattern_regex(R"(%\{([a-zA-Z]+)\})");
    std::smatch match;
    std::string::const_iterator search_start(result.cbegin());

    // Replace all patterns
    std::string output;
    while (std::regex_search(search_start, result.cend(), match, pattern_regex)) {
        // Append text before match
        output.append(search_start, match[0].first);

        // Get pattern identifier (without %{ and })
        std::string identifier = match[1].str();

        // Look up replacement
        auto it = pattern_map.find(identifier);
        if (it != pattern_map.end()) {
            output.append(it->second);
        } else {
            // Unknown pattern - keep original
            output.append(match[0].str());
        }

        // Move search start past this match
        search_start = match[0].second;
    }
    // Append remaining text
    output.append(search_start, result.cend());

    // Restore escaped patterns: placeholder → %{
    result = output;
    pos = 0;
    while ((pos = result.find(escape_placeholder, pos)) != std::string::npos) {
        result.replace(pos, escape_placeholder.length(), "%{");
        pos += 2;
    }

    return result;
}

std::string LabelRenderer::format_coordinate(double lat, double lon) const {
    std::ostringstream oss;

    if (config_.label_units == TopographicConfig::Units::IMPERIAL) {
        // Use degrees, minutes, seconds for imperial
        auto [lat_d, lat_m, lat_s] = to_dms(lat);
        auto [lon_d, lon_m, lon_s] = to_dms(lon);

        char lat_dir = (lat >= 0) ? 'N' : 'S';
        char lon_dir = (lon >= 0) ? 'E' : 'W';

        oss << lat_d << "°" << lat_m << "'" << std::fixed << std::setprecision(1) << lat_s << "\"" << lat_dir
            << ", " << lon_d << "°" << lon_m << "'" << lon_s << "\"" << lon_dir;
    } else {
        // Use decimal degrees for metric
        oss << std::fixed << std::setprecision(4) << lat << "°, " << lon << "°";
    }

    return oss.str();
}

std::tuple<int, int, double> LabelRenderer::to_dms(double decimal) const {
    int degrees = static_cast<int>(std::abs(decimal));
    double remainder = (std::abs(decimal) - degrees) * 60.0;
    int minutes = static_cast<int>(remainder);
    double seconds = (remainder - minutes) * 60.0;

    return {degrees, minutes, seconds};
}

std::string LabelRenderer::format_distance(double value_meters, bool is_print_units) const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (is_print_units) {
        // Format for substrate/print dimensions
        if (config_.print_units == TopographicConfig::PrintUnits::INCHES) {
            double value_inches = value_meters * 1000.0 / 25.4;  // meters -> mm -> inches
            oss << value_inches << "in";
        } else {
            oss << (value_meters * 1000.0) << "mm";
        }
    } else {
        // Format for elevation/terrain dimensions
        if (config_.land_units == TopographicConfig::LandUnits::FEET) {
            double value_feet = value_meters * 3.28084;
            oss << value_feet << "ft";
        } else {
            oss << value_meters << "m";
        }
    }

    return oss.str();
}

std::optional<PlacedLabel> LabelRenderer::generate_base_visible_label(const LabelContext& context) const {
    if (config_.base_label_visible.empty()) {
        return std::nullopt;
    }

    std::string text = substitute_patterns(config_.base_label_visible, context);
    if (text.empty()) {
        return std::nullopt;
    }

    return place_visible_label(text, context, true);
}

std::optional<PlacedLabel> LabelRenderer::generate_base_hidden_label(const LabelContext& context) const {
    if (config_.base_label_hidden.empty()) {
        return std::nullopt;
    }

    std::string text = substitute_patterns(config_.base_label_hidden, context);
    if (text.empty()) {
        return std::nullopt;
    }

    return place_hidden_label(text, context, true);
}

std::optional<PlacedLabel> LabelRenderer::generate_layer_visible_label(const LabelContext& context) const {
    if (config_.layer_label_visible.empty()) {
        return std::nullopt;
    }

    std::string text = substitute_patterns(config_.layer_label_visible, context);
    if (text.empty()) {
        return std::nullopt;
    }

    return place_visible_label(text, context, false);
}

std::optional<PlacedLabel> LabelRenderer::generate_layer_hidden_label(const LabelContext& context) const {
    if (config_.layer_label_hidden.empty()) {
        return std::nullopt;
    }

    std::string text = substitute_patterns(config_.layer_label_hidden, context);
    if (text.empty()) {
        return std::nullopt;
    }

    return place_hidden_label(text, context, false);
}

PlacedLabel LabelRenderer::place_visible_label(const std::string& text, const LabelContext& context, bool is_base) const {
    PlacedLabel label;
    label.text = text;
    label.color = config_.visible_label_color;
    label.font_size_mm = is_base ? config_.base_font_size_mm : config_.layer_font_size_mm;
    label.is_hidden = false;

    // Place in a corner that won't be covered by next layer
    // Base layer: lower-left corner (10% in from edges)
    // Other layers: lower-right corner (10% in from edges)
    const auto& bbox = context.content_bbox;

    if (is_base) {
        // Base layer visible label: lower-left
        label.x = bbox.min_x + (bbox.max_x - bbox.min_x) * 0.1;
        label.y = bbox.min_y + (bbox.max_y - bbox.min_y) * 0.1;
        label.anchor = "start";  // Left-aligned
    } else {
        // Layer visible label: lower-right
        label.x = bbox.max_x - (bbox.max_x - bbox.min_x) * 0.1;
        label.y = bbox.min_y + (bbox.max_y - bbox.min_y) * 0.1;
        label.anchor = "end";  // Right-aligned
    }

    return label;
}

PlacedLabel LabelRenderer::place_hidden_label(const std::string& text, const LabelContext& context, bool is_base) const {
    PlacedLabel label;
    label.text = text;
    label.color = config_.hidden_label_color;
    label.font_size_mm = is_base ? config_.base_font_size_mm * 0.9 : config_.layer_font_size_mm * 0.9;  // Slightly smaller
    label.is_hidden = true;

    // Place in center of hidden area (will be covered by next layer)
    const auto& bbox = context.hidden_bbox;

    // Calculate available space in the bounding box
    double bbox_width = bbox.max_x - bbox.min_x;
    // double bbox_height = bbox.max_y - bbox.min_y;  // Reserved for future vertical fitting

    // Estimate text width using heuristic: font_size * text_length * 0.6
    // (0.6 is approximate average character width ratio for proportional fonts)
    double estimated_text_width = label.font_size_mm * text.length() * 0.6;

    // Calculate center position for text
    double center_x = (bbox.min_x + bbox.max_x) / 2.0;
    double center_y = (bbox.min_y + bbox.max_y) / 2.0;

    // Try to generate curved text path if enabled and polygon data available
    // Skip curved text if text already fits comfortably in the bounding box
    bool text_fits_straight = (estimated_text_width * 1.1 < bbox_width);  // 10% margin

    if (config_.enable_curved_text && context.next_layer_polygons != nullptr && !text_fits_straight) {
        // Cast void* back to proper type
        const auto* polygons = static_cast<const std::vector<ContourLayer::PolygonData>*>(context.next_layer_polygons);

        if (!polygons->empty()) {
            // Configure text path generator
            TextPathConfig path_config;
            path_config.inset_ratio = config_.text_path_inset_ratio;
            path_config.min_path_length_ratio = config_.min_path_length_ratio;
            path_config.spline_sample_points = config_.spline_sample_points;

            TextPathGenerator path_generator(path_config);

            // Try to generate curved path
            auto text_path_opt = path_generator.generate_path_from_polygons(
                *polygons,
                estimated_text_width,
                label.font_size_mm,
                center_x,
                center_y
            );

        if (text_path_opt) {
            // Successfully generated curved path!
            const auto& text_path = *text_path_opt;

            label.has_curved_path = true;
            label.path_points = text_path.control_points;
            label.char_positions = text_path.sample_points;
            label.char_rotations = text_path.tangent_angles;
            label.svg_path_d = text_path.svg_path_d;

            // Generate unique ID for SVG path
            static int path_id_counter = 0;
            label.svg_path_id = "text-path-" + std::to_string(++path_id_counter);

            label.was_bent = true;
            label.modification_warning = "Text bent along polygon contour";

            // Check if text needs scaling to fit path
            if (estimated_text_width > text_path.total_length) {
                double scale_ratio = text_path.total_length / estimated_text_width;
                label.font_size_mm *= scale_ratio * 0.95;  // 0.95 for safety margin
                label.was_scaled = true;
                label.modification_warning += ", scaled to fit path";
            }

            // Use path center for fallback position
            if (!text_path.sample_points.empty()) {
                size_t mid_idx = text_path.sample_points.size() / 2;
                label.x = text_path.sample_points[mid_idx].first;
                label.y = text_path.sample_points[mid_idx].second;
            } else {
                label.x = center_x;
                label.y = center_y;
            }
            label.anchor = "middle";

            return label;
        }
        // If path generation failed, fall through to straight text logic
        }
    }

    // Fallback: Straight text adaptive fitting
    // Adaptive fitting: shrink, bent, truncate (in that order)
    if (estimated_text_width > bbox_width) {
        // Strategy 1: Try shrinking the text
        double shrink_ratio = bbox_width / estimated_text_width;
        const double min_shrink = 0.5;  // Don't shrink below 50% of original size

        if (shrink_ratio >= min_shrink) {
            // Shrink the font to fit
            label.font_size_mm *= shrink_ratio * 0.95;  // 0.95 for safety margin
            label.was_scaled = true;
            label.modification_warning = "Text scaled to fit";
        } else {
            // Strategy 2: Bent text (heuristic, no actual curve)
            label.font_size_mm *= min_shrink;
            label.was_bent = true;
            label.was_scaled = true;
            label.modification_warning = "Text bent and scaled to fit";

            // Recalculate estimated width after shrinking
            estimated_text_width = label.font_size_mm * text.length() * 0.6;

            // Strategy 3: If still doesn't fit after bending, truncate
            if (estimated_text_width > bbox_width) {
                // Calculate how many characters will fit
                size_t max_chars = static_cast<size_t>(bbox_width / (label.font_size_mm * 0.6));
                if (max_chars < text.length() && max_chars > 3) {
                    // Truncate and add ellipsis
                    label.text = text.substr(0, max_chars - 3) + "...";
                    label.was_truncated = true;
                    label.modification_warning = "Text truncated to fit";
                }
            }
        }
    }

    // Use center point of hidden area
    label.x = center_x;
    label.y = center_y;
    label.anchor = "middle";  // Center-aligned

    return label;
}

std::string LabelRenderer::substitute_filename_pattern(
    const std::string& pattern,
    const std::string& basename,
    int layer_number,
    double elevation_m) {

    std::string result = pattern;

    // Handle escape sequences (%%{ → placeholder)
    const std::string escape_placeholder = "\x01ESCAPED_PERCENT\x01";
    size_t pos = 0;
    while ((pos = result.find("%%{", pos)) != std::string::npos) {
        result.replace(pos, 2, escape_placeholder);
        pos += escape_placeholder.length();
    }

    // Build pattern map with filename-specific patterns
    std::unordered_map<std::string, std::string> pattern_map;

    // %{b}: basename
    pattern_map["b"] = basename;

    // %{l}: layer number (zero-padded to 2 digits)
    std::ostringstream layer_ss;
    layer_ss << std::setfill('0') << std::setw(2) << layer_number;
    pattern_map["l"] = layer_ss.str();

    // %{e}: elevation in meters (integer)
    pattern_map["e"] = std::to_string(static_cast<int>(std::round(elevation_m)));

    // %{n}: layer number (alias for %{l})
    pattern_map["n"] = pattern_map["l"];

    // Replace patterns using regex
    std::regex pattern_regex(R"(%\{([a-zA-Z]+)\})");
    std::smatch match;
    std::string::const_iterator search_start(result.cbegin());

    std::string output;
    while (std::regex_search(search_start, result.cend(), match, pattern_regex)) {
        // Append text before match
        output.append(search_start, search_start + match.position());

        // Get the identifier (what's inside braces)
        std::string identifier = match[1].str();

        // Replace with value if exists, otherwise keep original
        auto it = pattern_map.find(identifier);
        if (it != pattern_map.end()) {
            output.append(it->second);
        } else {
            output.append(match[0].str());  // Keep original if not found
        }

        // Move search position forward
        search_start += match.position() + match.length();
    }

    // Append remaining text
    output.append(search_start, result.cend());

    // Restore escaped sequences (placeholder → %{)
    pos = 0;
    while ((pos = output.find(escape_placeholder, pos)) != std::string::npos) {
        output.replace(pos, escape_placeholder.length(), "%{");
        pos += 2;
    }

    return output;
}

} // namespace topo
