#include "UnitParser.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace topo {

// ============================================================================
// UNIT PREFERENCES
// ============================================================================

void UnitPreferences::set_land_units(const std::string& unit) {
    std::string lower_unit = unit;
    std::transform(lower_unit.begin(), lower_unit.end(), lower_unit.begin(), ::tolower);

    if (lower_unit == "meters" || lower_unit == "m") {
        land_distance = DistanceUnit::METERS;
    } else if (lower_unit == "kilometers" || lower_unit == "km") {
        land_distance = DistanceUnit::KILOMETERS;
    } else if (lower_unit == "feet" || lower_unit == "ft") {
        land_distance = DistanceUnit::FEET;
    } else if (lower_unit == "miles" || lower_unit == "mi") {
        land_distance = DistanceUnit::MILES;
    } else {
        throw UnitParseError("Invalid land units: " + unit +
                           ". Use: meters, km, feet, or miles");
    }
}

void UnitPreferences::set_print_units(const std::string& unit) {
    std::string lower_unit = unit;
    std::transform(lower_unit.begin(), lower_unit.end(), lower_unit.begin(), ::tolower);

    if (lower_unit == "mm" || lower_unit == "millimeters") {
        print_distance = DistanceUnit::MILLIMETERS;
    } else if (lower_unit == "in" || lower_unit == "inches") {
        print_distance = DistanceUnit::INCHES;
    } else {
        throw UnitParseError("Invalid print units: " + unit +
                           ". Use: mm or inches");
    }
}

// ============================================================================
// UNIT CONVERSION
// ============================================================================

double UnitParser::to_meters_factor(DistanceUnit unit) {
    switch (unit) {
        case DistanceUnit::METERS:      return 1.0;
        case DistanceUnit::KILOMETERS:  return 1000.0;
        case DistanceUnit::FEET:        return 0.3048;
        case DistanceUnit::MILES:       return 1609.34;
        case DistanceUnit::MILLIMETERS: return 0.001;
        case DistanceUnit::INCHES:      return 0.0254;
        case DistanceUnit::PIXELS:
            throw UnitParseError("PIXELS unit requires DPI for conversion - use pixels_to_mm() instead");
    }
    throw UnitParseError("Unknown distance unit in to_meters_factor");
}

double UnitParser::to_millimeters_factor(DistanceUnit unit) {
    switch (unit) {
        case DistanceUnit::METERS:      return 1000.0;
        case DistanceUnit::KILOMETERS:  return 1000000.0;
        case DistanceUnit::FEET:        return 304.8;
        case DistanceUnit::MILES:       return 1609344.0;
        case DistanceUnit::MILLIMETERS: return 1.0;
        case DistanceUnit::INCHES:      return 25.4;
        case DistanceUnit::PIXELS:
            throw UnitParseError("PIXELS unit requires DPI for conversion - use pixels_to_mm() instead");
    }
    throw UnitParseError("Unknown distance unit in to_millimeters_factor");
}

double UnitParser::convert_distance(double value, DistanceUnit from_unit, DistanceUnit to_unit) {
    if (from_unit == to_unit) {
        return value;
    }

    // Convert to meters first, then to target unit
    double in_meters = value * to_meters_factor(from_unit);
    return in_meters / to_meters_factor(to_unit);
}

DistanceUnit UnitParser::parse_unit_string(const std::string& unit_str) {
    std::string lower_unit = unit_str;
    std::transform(lower_unit.begin(), lower_unit.end(), lower_unit.begin(), ::tolower);

    if (lower_unit == "m" || lower_unit == "meters") {
        return DistanceUnit::METERS;
    } else if (lower_unit == "km" || lower_unit == "kilometers") {
        return DistanceUnit::KILOMETERS;
    } else if (lower_unit == "ft" || lower_unit == "feet") {
        return DistanceUnit::FEET;
    } else if (lower_unit == "mi" || lower_unit == "miles") {
        return DistanceUnit::MILES;
    } else if (lower_unit == "mm" || lower_unit == "millimeters") {
        return DistanceUnit::MILLIMETERS;
    } else if (lower_unit == "in" || lower_unit == "inches" || lower_unit == "\"") {
        return DistanceUnit::INCHES;
    } else if (lower_unit == "px" || lower_unit == "pixels") {
        return DistanceUnit::PIXELS;
    }

    throw UnitParseError("Unrecognized unit: '" + unit_str + "'. " +
                       "Supported units: m, km, ft, mi, mm, in, px");
}

std::string UnitParser::unit_to_string(DistanceUnit unit) {
    switch (unit) {
        case DistanceUnit::METERS:      return "m";
        case DistanceUnit::KILOMETERS:  return "km";
        case DistanceUnit::FEET:        return "ft";
        case DistanceUnit::MILES:       return "mi";
        case DistanceUnit::MILLIMETERS: return "mm";
        case DistanceUnit::INCHES:      return "in";
        case DistanceUnit::PIXELS:      return "px";
    }
    return "unknown";
}

// ============================================================================
// VALUE AND UNIT SPLITTING
// ============================================================================

std::pair<std::string, std::string> UnitParser::split_value_and_unit(const std::string& input) const {
    // Trim whitespace
    std::string trimmed = input;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        throw UnitParseError("Empty input string");
    }

    // Find where numeric part ends
    size_t num_end = 0;
    bool found_decimal = false;
    bool found_digit = false;

    for (size_t i = 0; i < trimmed.size(); ++i) {
        char c = trimmed[i];

        if (std::isdigit(c)) {
            found_digit = true;
            num_end = i + 1;
        } else if (c == '.' && !found_decimal && found_digit) {
            found_decimal = true;
            num_end = i + 1;
        } else if ((c == '-' || c == '+') && i == 0) {
            // Allow leading sign
            num_end = i + 1;
        } else if (c == ' ' && found_digit) {
            // Space between number and unit
            break;
        } else if (std::isalpha(c) || c == '"') {
            // Start of unit suffix
            break;
        } else {
            // Invalid character in numeric part
            break;
        }
    }

    if (!found_digit) {
        throw UnitParseError("No numeric value found in: '" + input + "'");
    }

    std::string value_str = trimmed.substr(0, num_end);
    std::string unit_str = trimmed.substr(num_end);

    // Trim unit string
    unit_str.erase(0, unit_str.find_first_not_of(" \t"));
    unit_str.erase(unit_str.find_last_not_of(" \t") + 1);

    return {value_str, unit_str};
}

// ============================================================================
// DISTANCE PARSING
// ============================================================================

ParsedValue UnitParser::parse_distance(const std::string& input,
                                       DistanceUnit default_unit,
                                       DistanceUnit canonical_unit) const {
    auto [value_str, unit_str] = split_value_and_unit(input);

    // Parse numeric value
    double value;
    try {
        value = std::stod(value_str);
    } catch (const std::exception& e) {
        throw UnitParseError("Invalid numeric value: '" + value_str + "'");
    }

    // Determine unit
    DistanceUnit source_unit = default_unit;
    bool had_explicit_unit = !unit_str.empty();

    if (had_explicit_unit) {
        source_unit = parse_unit_string(unit_str);
    }

    // Convert to canonical unit
    double converted_value = convert_distance(value, source_unit, canonical_unit);

    return ParsedValue(converted_value, source_unit, had_explicit_unit);
}

// ============================================================================
// DMS COORDINATE PARSING
// ============================================================================

bool UnitParser::is_dms_format(const std::string& input) const {
    // Check for DMS indicators: °, d, ', ", N, S, E, W
    return input.find("°") != std::string::npos ||
           input.find('d') != std::string::npos ||
           input.find('\'') != std::string::npos ||
           input.find('"') != std::string::npos ||
           input.find('N') != std::string::npos ||
           input.find('S') != std::string::npos ||
           input.find('E') != std::string::npos ||
           input.find('W') != std::string::npos;
}

double UnitParser::parse_dms(const std::string& input, bool is_latitude) const {
    std::string work = input;

    // Trim whitespace
    work.erase(0, work.find_first_not_of(" \t\n\r"));
    work.erase(work.find_last_not_of(" \t\n\r") + 1);

    // Extract hemisphere (N/S/E/W) from end
    char hemisphere = '\0';
    if (!work.empty()) {
        char last_char = std::toupper(work.back());
        if (last_char == 'N' || last_char == 'S' || last_char == 'E' || last_char == 'W') {
            hemisphere = last_char;
            work.pop_back();
        }
    }

    // Normalize separators: replace °, d, ', " with spaces
    // Note: degree symbol is multi-byte UTF-8, use string replacement
    auto replace_all = [](std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replace_all(work, "°", " ");  // Unicode degree symbol
    for (char& c : work) {
        if (c == 'd' || c == '\'' || c == '"' || c == 'm' || c == 's') {
            c = ' ';
        }
    }

    // Parse degrees, minutes, seconds
    std::istringstream iss(work);
    double degrees = 0.0, minutes = 0.0, seconds = 0.0;

    iss >> degrees;
    if (iss.fail()) {
        throw UnitParseError("Invalid DMS format: '" + input + "'");
    }

    // Try to read minutes
    if (iss >> minutes) {
        // Try to read seconds
        iss >> seconds;
    }

    // Validate ranges
    if (minutes < 0 || minutes >= 60) {
        throw UnitParseError("Invalid minutes value: " + std::to_string(minutes));
    }
    if (seconds < 0 || seconds >= 60) {
        throw UnitParseError("Invalid seconds value: " + std::to_string(seconds));
    }

    // Convert to decimal degrees
    double decimal = std::abs(degrees) + minutes / 60.0 + seconds / 3600.0;

    // Apply sign based on degrees or hemisphere
    bool is_negative = (degrees < 0);
    if (hemisphere == 'S' || hemisphere == 'W') {
        is_negative = true;
    } else if (hemisphere == 'N' || hemisphere == 'E') {
        is_negative = false;
    }

    if (is_negative) {
        decimal = -decimal;
    }

    // Validate range
    if (is_latitude) {
        if (decimal < -90.0 || decimal > 90.0) {
            throw UnitParseError("Latitude out of range [-90, 90]: " + std::to_string(decimal));
        }
    } else {
        if (decimal < -180.0 || decimal > 180.0) {
            throw UnitParseError("Longitude out of range [-180, 180]: " + std::to_string(decimal));
        }
    }

    return decimal;
}

// ============================================================================
// COORDINATE PARSING
// ============================================================================

double UnitParser::parse_latitude(const std::string& input) const {
    if (is_dms_format(input)) {
        return parse_dms(input, true);
    }

    // Parse as decimal degrees
    try {
        double lat = std::stod(input);
        if (lat < -90.0 || lat > 90.0) {
            throw UnitParseError("Latitude out of range [-90, 90]: " + std::to_string(lat));
        }
        return lat;
    } catch (const std::invalid_argument& e) {
        throw UnitParseError("Invalid latitude format: '" + input + "'");
    } catch (const std::out_of_range& e) {
        throw UnitParseError("Latitude value out of range: '" + input + "'");
    }
}

double UnitParser::parse_longitude(const std::string& input) const {
    if (is_dms_format(input)) {
        return parse_dms(input, false);
    }

    // Parse as decimal degrees
    try {
        double lon = std::stod(input);
        if (lon < -180.0 || lon > 180.0) {
            throw UnitParseError("Longitude out of range [-180, 180]: " + std::to_string(lon));
        }
        return lon;
    } catch (const std::invalid_argument& e) {
        throw UnitParseError("Invalid longitude format: '" + input + "'");
    } catch (const std::out_of_range& e) {
        throw UnitParseError("Longitude value out of range: '" + input + "'");
    }
}

std::pair<double, double> UnitParser::parse_coordinate_pair(const std::string& input) const {
    // Find comma separator
    size_t comma_pos = input.find(',');
    if (comma_pos == std::string::npos) {
        throw UnitParseError("Coordinate pair must be separated by comma: '" + input + "'");
    }

    std::string lat_str = input.substr(0, comma_pos);
    std::string lon_str = input.substr(comma_pos + 1);

    double lat = parse_latitude(lat_str);
    double lon = parse_longitude(lon_str);

    return {lat, lon};
}

// ============================================================================
// DPI CONVERSION
// ============================================================================

double UnitParser::pixels_to_mm(double pixels, double dpi) {
    // DPI = dots per inch
    // 1 inch = 25.4 mm
    // 1 pixel = 25.4 / DPI mm
    if (dpi <= 0) {
        throw UnitParseError("DPI must be positive, got: " + std::to_string(dpi));
    }
    return pixels * 25.4 / dpi;
}

double UnitParser::mm_to_pixels(double mm, double dpi) {
    // DPI = dots per inch
    // 1 inch = 25.4 mm
    // 1 mm = DPI / 25.4 pixels
    if (dpi <= 0) {
        throw UnitParseError("DPI must be positive, got: " + std::to_string(dpi));
    }
    return mm * dpi / 25.4;
}

// ============================================================================
// STROKE WIDTH PARSING
// ============================================================================

ParsedValue UnitParser::parse_stroke_width(const std::string& input, double dpi) const {
    auto [value_str, unit_str] = split_value_and_unit(input);

    // Parse numeric value
    double value;
    try {
        value = std::stod(value_str);
    } catch (const std::exception& e) {
        throw UnitParseError("Invalid numeric value: '" + value_str + "'");
    }

    // Determine unit - default to pixels if no unit specified
    DistanceUnit source_unit = DistanceUnit::PIXELS;
    bool had_explicit_unit = !unit_str.empty();

    if (had_explicit_unit) {
        source_unit = parse_unit_string(unit_str);
    }

    // Convert to millimeters
    double converted_value;
    if (source_unit == DistanceUnit::PIXELS) {
        // Use DPI-based conversion for pixels
        converted_value = pixels_to_mm(value, dpi);
    } else {
        // Use standard unit conversion for other units
        converted_value = convert_distance(value, source_unit, DistanceUnit::MILLIMETERS);
    }

    return ParsedValue(converted_value, source_unit, had_explicit_unit);
}

} // namespace topo
