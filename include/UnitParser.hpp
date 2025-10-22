#pragma once

#include <string>
#include <optional>
#include <stdexcept>
#include <regex>
#include <cmath>

namespace topo {

/**
 * @brief Exception thrown when unit parsing fails
 */
class UnitParseError : public std::runtime_error {
public:
    explicit UnitParseError(const std::string& message)
        : std::runtime_error("Unit parsing error: " + message) {}
};

/**
 * @brief Unit types for distance measurements
 */
enum class DistanceUnit {
    METERS,      // m
    KILOMETERS,  // km
    FEET,        // ft
    MILES,       // mi
    MILLIMETERS, // mm
    INCHES,      // in
    PIXELS       // px (requires DPI for conversion)
};

/**
 * @brief Unit types for coordinate formats
 */
enum class CoordinateFormat {
    DECIMAL_DEGREES,  // Standard decimal notation (45.5231)
    DMS               // Degrees/Minutes/Seconds (45°31'23"N)
};

/**
 * @brief Global unit preferences
 */
struct UnitPreferences {
    DistanceUnit land_distance = DistanceUnit::METERS;      // For elevation, terrain
    DistanceUnit print_distance = DistanceUnit::MILLIMETERS; // For substrate, thickness
    CoordinateFormat coord_format = CoordinateFormat::DECIMAL_DEGREES;

    // Constructor with string-based initialization
    UnitPreferences() = default;

    void set_land_units(const std::string& unit);
    void set_print_units(const std::string& unit);
};

/**
 * @brief Result of parsing a value with units
 */
struct ParsedValue {
    double value;              // Numeric value in canonical units (meters or mm)
    DistanceUnit original_unit; // Unit that was parsed
    bool had_explicit_unit;    // Whether unit was explicitly specified

    ParsedValue(double v, DistanceUnit u, bool explicit_unit = false)
        : value(v), original_unit(u), had_explicit_unit(explicit_unit) {}
};

/**
 * @brief Parses numeric values with optional unit suffixes
 *
 * Supports flexible unit specification with per-parameter overrides:
 * - Distance: "200m", "5km", "10mi", "500ft", "300mm", "12in"
 * - Coordinates: "45.5231" (decimal degrees) or "45°31'23"N" (DMS)
 *
 * Examples:
 *   --land-units meters --height-per-layer 200ft
 *   --substrate-size-mm 300mm
 *   --upper-left 63°07'29"N,151°11'05"W
 */
class UnitParser {
public:
    /**
     * @brief Construct parser with default preferences
     */
    UnitParser() = default;

    /**
     * @brief Construct parser with specific preferences
     */
    explicit UnitParser(const UnitPreferences& prefs) : preferences_(prefs) {}

    /**
     * @brief Set global unit preferences
     */
    void set_preferences(const UnitPreferences& prefs) { preferences_ = prefs; }
    const UnitPreferences& get_preferences() const { return preferences_; }

    // ========================================================================
    // DISTANCE PARSING
    // ========================================================================

    /**
     * @brief Parse a distance value with optional unit suffix
     *
     * @param input String to parse (e.g., "200", "5km", "10mi")
     * @param default_unit Unit to use if no suffix is provided
     * @param canonical_unit Canonical unit for output (METERS or MILLIMETERS)
     * @return Parsed value in canonical units
     *
     * Examples:
     *   parse_distance("200", METERS, METERS) -> 200.0
     *   parse_distance("5km", METERS, METERS) -> 5000.0
     *   parse_distance("10mi", METERS, METERS) -> 16093.4
     *   parse_distance("300mm", MILLIMETERS, MILLIMETERS) -> 300.0
     *   parse_distance("12in", MILLIMETERS, MILLIMETERS) -> 304.8
     */
    ParsedValue parse_distance(const std::string& input,
                               DistanceUnit default_unit,
                               DistanceUnit canonical_unit) const;

    /**
     * @brief Parse a distance value using global land distance preference
     */
    ParsedValue parse_land_distance(const std::string& input) const {
        return parse_distance(input, preferences_.land_distance, DistanceUnit::METERS);
    }

    /**
     * @brief Parse a distance value using global print distance preference
     */
    ParsedValue parse_print_distance(const std::string& input) const {
        return parse_distance(input, preferences_.print_distance, DistanceUnit::MILLIMETERS);
    }

    /**
     * @brief Parse stroke width with pixel support and DPI-based conversion
     *
     * @param input String to parse (e.g., "3", "3px", "0.2mm")
     * @param dpi DPI for pixel-to-mm conversion (default 600)
     * @return Parsed value in millimeters
     *
     * Examples:
     *   parse_stroke_width("3", 600) -> 0.127mm (3 pixels at 600 DPI)
     *   parse_stroke_width("3px", 600) -> 0.127mm
     *   parse_stroke_width("0.2mm", 600) -> 0.2mm
     */
    ParsedValue parse_stroke_width(const std::string& input, double dpi = 600.0) const;

    // ========================================================================
    // DPI CONVERSION
    // ========================================================================

    /**
     * @brief Convert pixels to millimeters using DPI
     * @param pixels Number of pixels
     * @param dpi Dots per inch
     * @return Distance in millimeters
     */
    static double pixels_to_mm(double pixels, double dpi);

    /**
     * @brief Convert millimeters to pixels using DPI
     * @param mm Distance in millimeters
     * @param dpi Dots per inch
     * @return Number of pixels
     */
    static double mm_to_pixels(double mm, double dpi);

    // ========================================================================
    // COORDINATE PARSING
    // ========================================================================

    /**
     * @brief Parse a latitude coordinate (decimal or DMS)
     *
     * @param input String to parse
     * @return Latitude in decimal degrees (-90 to +90)
     *
     * Decimal examples:
     *   "63.1497" -> 63.1497
     *   "-45.5" -> -45.5
     *
     * DMS examples:
     *   "63°07'29"N" -> 63.124722
     *   "45°31'23"S" -> -45.523056
     *   "63d07m29sN" -> 63.124722
     */
    double parse_latitude(const std::string& input) const;

    /**
     * @brief Parse a longitude coordinate (decimal or DMS)
     *
     * @param input String to parse
     * @return Longitude in decimal degrees (-180 to +180)
     *
     * Decimal examples:
     *   "-151.1847" -> -151.1847
     *   "122.5" -> 122.5
     *
     * DMS examples:
     *   "151°11'05"W" -> -151.184722
     *   "122°30'00"E" -> 122.5
     *   "151d11m05sW" -> -151.184722
     */
    double parse_longitude(const std::string& input) const;

    /**
     * @brief Parse a coordinate pair (lat,lon)
     *
     * @param input String to parse (e.g., "63.1497,-151.1847")
     * @return Pair of (latitude, longitude) in decimal degrees
     *
     * Examples:
     *   "63.1497,-151.1847" -> (63.1497, -151.1847)
     *   "63°07'29"N,151°11'05"W" -> (63.124722, -151.184722)
     */
    std::pair<double, double> parse_coordinate_pair(const std::string& input) const;

    // ========================================================================
    // UNIT CONVERSION
    // ========================================================================

    /**
     * @brief Convert distance between units
     *
     * @param value Value to convert
     * @param from_unit Source unit
     * @param to_unit Target unit
     * @return Converted value
     */
    static double convert_distance(double value, DistanceUnit from_unit, DistanceUnit to_unit);

    /**
     * @brief Get conversion factor to meters
     */
    static double to_meters_factor(DistanceUnit unit);

    /**
     * @brief Get conversion factor to millimeters
     */
    static double to_millimeters_factor(DistanceUnit unit);

    /**
     * @brief Parse unit string to DistanceUnit enum
     *
     * @param unit_str Unit string (e.g., "m", "km", "ft", "mi", "mm", "in")
     * @return Corresponding DistanceUnit enum value
     * @throws UnitParseError if unit string is not recognized
     */
    static DistanceUnit parse_unit_string(const std::string& unit_str);

    /**
     * @brief Convert DistanceUnit enum to string representation
     */
    static std::string unit_to_string(DistanceUnit unit);

private:
    UnitPreferences preferences_;

    /**
     * @brief Split numeric value and unit suffix
     *
     * @param input Input string (e.g., "200m", "5.5km")
     * @return Pair of (numeric_value, unit_suffix)
     *
     * Examples:
     *   "200" -> ("200", "")
     *   "5km" -> ("5", "km")
     *   "10.5mi" -> ("10.5", "mi")
     */
    std::pair<std::string, std::string> split_value_and_unit(const std::string& input) const;

    /**
     * @brief Parse DMS (degrees/minutes/seconds) format
     *
     * @param input DMS string
     * @param is_latitude True for latitude (N/S), false for longitude (E/W)
     * @return Decimal degrees
     *
     * Supported formats:
     *   - 63°07'29"N (Unicode degree symbol)
     *   - 63d07m29sN (ASCII letters)
     *   - 63 07 29 N (space-separated)
     */
    double parse_dms(const std::string& input, bool is_latitude) const;

    /**
     * @brief Check if string appears to be DMS format
     */
    bool is_dms_format(const std::string& input) const;
};

} // namespace topo
