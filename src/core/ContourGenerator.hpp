/**
 * @file ContourGenerator.hpp
 * @brief Contour polygon generation from elevation data
 *
 * Port of Python contour_ops.py functionality
 * Generates contour polygons using GDAL/OGR for topographic modeling
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "topographic_generator.hpp"
#include "Logger.hpp"
#include "MemoryMonitor.hpp"
#include "StackGuard.hpp"
#include "CrashHandler.hpp"
// IncrementalProcessor removed - CGAL-dependent
#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace topo {

// Forward declarations
struct ContourLayer;

// ContourStrategy enum moved to topographic_generator.hpp to avoid duplicate definitions
// BaseLayerMethod enum also defined in topographic_generator.hpp

/**
 * @brief Configuration for contour generation
 */
struct ContourConfig {
    double interval = 100.0;                    ///< Contour interval in meters
    bool vertical_contour_relief = true;        ///< True for vertical bands, false for terrain-following
    bool outer_boundaries_only = false;        ///< Only extract outer boundaries, ignore holes
    bool remove_holes = true;                  ///< Remove polygon holes from SVG output (simpler laser cutting)
    bool force_all_layers = false;             ///< Include empty layers for consistent count

    // Inset options for layer stacking optimization
    bool inset_upper_layers = false;           ///< Cut holes where next layer sits (reduces material)
    double inset_offset_mm = 1.0;              ///< Size of nesting lip in millimeters
    // convert_to_meters removed - using WGS84 coordinates throughout
    double simplify_tolerance = 0.0;           ///< Simplification tolerance in meters
    double vertex_dedup_tolerance = 1e-6;      ///< Vertex deduplication tolerance (meters, default 1 micron)
    ContourStrategy strategy = ContourStrategy::UNIFORM; ///< Level generation strategy
    bool simplified_visibility_filtering = true; ///< Use simplified layer filtering for watertightness
    
    // Elevation filtering (only applied if user explicitly specifies them)
    std::optional<double> min_elevation;       ///< Minimum elevation to include (optional)
    std::optional<double> max_elevation;       ///< Maximum elevation to include (optional)
    double elevation_threshold = 0.0;          ///< Height/depth threshold from extremes
    
    // Water body support
    double fixed_elevation = -1e9;             ///< Fixed elevation for water body insertion
    bool has_water_polygon = false;            ///< Whether water polygon is provided
    
    // Output options
    std::string debug_image_path = "";         ///< Debug image output directory
    bool verbose = false;                      ///< Verbose logging
    std::string output_directory = "output";   ///< Output directory path
    std::string base_filename = "";            ///< Base filename for output files (e.g., "McKinley")
};

/**
 * @brief A single contour layer with elevation and geometry
 *
 * Uses simple coordinate-based storage (OGR/GDAL compatible) rather than CGAL types.
 * Each polygon consists of multiple rings (exterior boundary + holes).
 */
struct ContourLayer {
    double elevation;                          ///< Elevation level in meters
    int layer_number = -1;                     ///< Layer number (0=bottom, -1=unassigned)
    int level_index = -1;                      ///< Original index in contour_levels array (-1=unassigned)

    /**
     * @brief Simple polygon representation using coordinate rings
     *
     * First ring is exterior boundary, subsequent rings are holes.
     * Each ring is a closed loop of (x,y) coordinate pairs in projected meters.
     */
    struct PolygonData {
        std::vector<std::vector<std::pair<double, double>>> rings;

        bool empty() const { return rings.empty() || rings[0].empty(); }

        // Get exterior ring
        const std::vector<std::pair<double, double>>& exterior() const {
            return rings[0];
        }

        // Get holes (all rings except first)
        std::vector<std::vector<std::pair<double, double>>> holes() const {
            if (rings.size() <= 1) return {};
            return std::vector<std::vector<std::pair<double, double>>>(
                rings.begin() + 1, rings.end()
            );
        }

        size_t num_holes() const {
            return rings.size() > 0 ? rings.size() - 1 : 0;
        }
    };

    std::vector<PolygonData> polygons;         ///< Polygons for this layer (GDAL/OGR format)
    bool is_closed = true;                     ///< Whether contours are closed polygons
    double area = 0.0;                         ///< Total area of all polygons in m²

    ContourLayer(double elev) : elevation(elev), layer_number(-1), level_index(-1) {}
    ContourLayer(double elev, int num) : elevation(elev), layer_number(num), level_index(-1) {}
    ContourLayer(double elev, int num, int idx) : elevation(elev), layer_number(num), level_index(idx) {}

    /**
     * @brief Check if layer is empty
     */
    bool empty() const {
        return polygons.empty() || area < 1e-12;
    }

    /**
     * @brief Calculate total area of all polygons
     */
    void calculate_area();
};

// CoordinateSystem enum removed - using WGS84 coordinates throughout

/**
 * @brief Contour polygon generator
 *
 * Handles generating contour bands from elevation data using GDAL/OGR.
 * Ports functionality from Python contour_ops.py with high performance C++.
 */
class ContourGenerator {
public:
    /**
     * @brief Constructor
     * @param config Configuration for contour generation
     */
    explicit ContourGenerator(const ContourConfig& config = ContourConfig{});

    /**
     * @brief Destructor - flushes logger buffers
     */
    ~ContourGenerator();
    
    /**
     * @brief Generate contour layers from elevation data
     * 
     * @param elevation_data Raw elevation values as 2D array (row-major)
     * @param width Width of elevation data
     * @param height Height of elevation data
     * @param geotransform GDAL geotransform array [6 elements]
     * @param num_layers Number of layers to generate (overrides interval if > 0)
     * @param center_lon Center longitude for coordinate conversion
     * @param center_lat Center latitude for coordinate conversion
     * @return Vector of contour layers sorted from lowest to highest elevation
     */
    std::vector<ContourLayer> generate_contours(
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform,
        int num_layers = 0,
        double center_lon = 0.0,
        double center_lat = 0.0
    );
    
    /**
     * @brief Generate contour levels for given elevation range
     * 
     * @param min_elevation Minimum elevation in data
     * @param max_elevation Maximum elevation in data  
     * @param num_layers Number of layers (if 0, uses config interval)
     * @return Vector of elevation levels
     */
    std::vector<double> generate_contour_levels(
        double min_elevation,
        double max_elevation,
        int num_layers = 0
    ) const;
    
    /**
     * @brief Get current configuration
     */
    const ContourConfig& get_config() const { return config_; }
    
    /**
     * @brief Update configuration
     */
    void set_config(const ContourConfig& config) { config_ = config; }

    /**
     * @brief Set the logger for centralized logging
     * @param logger Logger instance to use
     */
    void set_logger(const Logger& logger) {
        // Can't assign Logger due to mutex - set level and file instead
        logger_.setLogLevel(logger.getLogLevel());
    }

    /**
     * @brief Get the logger instance for helper functions
     * @return Reference to the logger
     */
    const Logger& get_logger() const { return logger_; }

    /**
     * @brief Initialize monitoring infrastructure
     * @param enable_crash_handlers Enable crash signal handlers
     */
    void initialize_monitoring(bool enable_crash_handlers = true);

    /**
     * @brief Get current memory usage statistics
     */
    MemoryStats get_memory_stats() const { return memory_monitor_.get_current_stats(); }

    /**
     * @brief Get current stack usage statistics
     */
    StackStats get_stack_stats() const { return stack_guard_.get_current_stats(); }

    /**
     * @brief Check if system is under memory pressure
     */
    bool is_memory_pressure() const {
        auto stats = memory_monitor_.get_current_stats();
        return stats.is_warning_level() || stats.is_critical_level();
    }

private:
    ContourConfig config_;
    Logger logger_;  // Centralized logging system

    // Memory and crash monitoring infrastructure (mutable for use in const methods)
    mutable MemoryMonitor memory_monitor_;
    mutable StackGuard stack_guard_;
    mutable CrashHandler crash_handler_;

    /**
     * @brief Create meshgrid for elevation data coordinates
     */
    void create_meshgrid(
        size_t width, size_t height, 
        const double* geotransform,
        std::vector<double>& lon, 
        std::vector<double>& lat
    ) const;
    


    /**
     * @brief Export GDAL/OGR features directly to SVG (bypasses CGAL conversion)
     *
     * Used for binary mask mode to avoid memory exhaustion from CGAL conversion.
     * Reads OGR polygon features and writes them directly as SVG paths.
     *
     * @param layer OGR layer containing polygon features
     * @param field_index Index of field containing elevation values
     * @param total_features Total number of features to process
     * @param geotransform GDAL geotransform for coordinate conversion
     * @param output_path_base Base path for output files (without extension)
     * @param substrate_size_mm Size of substrate in millimeters
     * @param output_individual_layers If true, create one file per elevation level; if false, one combined file
     * @return true if export succeeded, false otherwise
     */
    bool export_ogr_features_to_svg(
        void* layer,  // OGRLayerH
        int field_index,
        int total_features,
        const double* geotransform,
        const std::string& output_path_base,
        double substrate_size_mm = 200.0,
        bool output_individual_layers = false
    ) const;

    // CGAL-dependent geometry processing functions removed
    // Now using direct GDAL/OGR geometry handling

    /**
     * @brief GDAL contour helper methods
     */
    bool write_temporary_geotiff(
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform,
        const std::string& filename) const;

    bool run_gdal_contour(
        const std::string& input_tiff,
        const std::string& output_geojson,
        const std::vector<double>& levels) const;

    /**
     * @brief Helper to convert OGR polygon to ContourLayer::PolygonData
     * @param ogr_geometry OGR geometry handle (must be wkbPolygon type)
     * @param polygon_data Output polygon data structure
     * @return true if conversion succeeded
     */
    bool ogr_polygon_to_polygon_data(
        void* ogr_geometry,
        ContourLayer::PolygonData& polygon_data) const;

    /**
     * @brief Generate contours from elevation data using GDAL Contour API
     * @param elevation_data Raw elevation values
     * @param width Width of elevation data
     * @param height Height of elevation data
     * @param geotransform GDAL geotransform array [6 elements]
     * @param levels Contour elevation levels to generate
     * @return Vector of contour layers with PolygonData geometries
     */
    std::vector<ContourLayer> generate_contours_with_gdal_api(
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform,
        const std::vector<double>& levels) const;

};

} // namespace topo
