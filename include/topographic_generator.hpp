#pragma once

/**
 * @file topographic_generator.hpp
 * @brief Main header for the C++ Topographic Generator
 * 
 * High-performance topographic model generator using professional
 * geometry libraries and algorithms from Bambu Slicer (libslic3r).
 * 
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include <optional>
#include <cmath>

// Forward declarations
namespace topo {
    class OutputTracker;
    struct ContourLayer;
}

// Linear algebra
#include <Eigen/Dense>
#include <Eigen/Sparse>

// Parallel processing
#ifdef HAVE_TBB
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/task_arena.h>
// Note: task_scheduler_init.h removed in TBB 2021+, use global_control instead
#endif

#ifdef HAVE_OPENMP
    #include <omp.h>
#endif

// Optional libraries
#ifdef HAVE_LIBIGL
#include <igl/readSTL.h>
#include <igl/writeSTL.h>
#include <igl/writeOBJ.h>
#endif

#ifdef HAVE_HDF5
#include <H5Cpp.h>
#endif

namespace topo {

// ============================================================================
// Custom Geometry Types (CGAL-free implementation)
// ============================================================================

/**
 * @brief 3D point with x, y, z coordinates
 */
struct Point3D {
    double x_, y_, z_;

    Point3D() : x_(0), y_(0), z_(0) {}
    Point3D(double x, double y, double z) : x_(x), y_(y), z_(z) {}
    Point3D(const Point3D& other) = default;  // Explicit copy constructor

    double x() const { return x_; }
    double y() const { return y_; }
    double z() const { return z_; }

    Point3D& operator=(const Point3D& other) = default;

    bool operator==(const Point3D& other) const {
        return x_ == other.x_ && y_ == other.y_ && z_ == other.z_;
    }
};

/**
 * @brief 2D point with x, y coordinates
 */
struct Point2D {
    double x_, y_;

    Point2D() : x_(0), y_(0) {}
    Point2D(double x, double y) : x_(x), y_(y) {}

    double x() const { return x_; }
    double y() const { return y_; }

    Point2D& operator=(const Point2D& other) = default;

    bool operator==(const Point2D& other) const {
        return x_ == other.x_ && y_ == other.y_;
    }
};

/**
 * @brief 3D vector for normals and directions
 */
struct Vector3D {
    double x_, y_, z_;

    Vector3D() : x_(0), y_(0), z_(1) {}  // Default to up vector
    Vector3D(double x, double y, double z) : x_(x), y_(y), z_(z) {}
    Vector3D(const Vector3D& other) = default;  // Explicit copy constructor

    double x() const { return x_; }
    double y() const { return y_; }
    double z() const { return z_; }

    Vector3D& operator=(const Vector3D& other) = default;

    // Basic vector operations
    Vector3D operator+(const Vector3D& other) const {
        return Vector3D(x_ + other.x_, y_ + other.y_, z_ + other.z_);
    }

    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x_ - other.x_, y_ - other.y_, z_ - other.z_);
    }

    Vector3D operator*(double scalar) const {
        return Vector3D(x_ * scalar, y_ * scalar, z_ * scalar);
    }

    double dot(const Vector3D& other) const {
        return x_ * other.x_ + y_ * other.y_ + z_ * other.z_;
    }

    Vector3D cross(const Vector3D& other) const {
        return Vector3D(
            y_ * other.z_ - z_ * other.y_,
            z_ * other.x_ - x_ * other.z_,
            x_ * other.y_ - y_ * other.x_
        );
    }

    double length() const {
        return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_);
    }

    Vector3D normalized() const {
        double len = length();
        return len > 0 ? Vector3D(x_ / len, y_ / len, z_ / len) : Vector3D(0, 0, 1);
    }
};

// Forward declarations
class TopographicMesh;
class ElevationProcessor;
class MeshTopologyManager;
class ParallelLayerGenerator;
class MultiFormatExporter;

/**
 * @brief Unique identifiers for mesh components
 */
using VertexId = std::uint32_t;
using TriangleId = std::uint32_t;
using EdgeId = std::uint64_t; // Combined vertex IDs

/**
 * @brief 3D vertex with position and optional attributes
 */
struct Vertex3D {
    Point3D position;
    Vector3D normal;
    std::optional<double> elevation;
    std::optional<std::array<float, 3>> color;
    
    Vertex3D() = default;
    Vertex3D(const Point3D& pos) : position(pos) {}
    Vertex3D(double x, double y, double z) : position(x, y, z) {}
};

/**
 * @brief Triangle defined by three vertex indices
 */
struct Triangle {
    std::array<VertexId, 3> vertices;
    Vector3D normal;
    double area = 0.0;
    
    Triangle() = default;
    Triangle(VertexId v0, VertexId v1, VertexId v2) : vertices{v0, v1, v2} {}
    
    // Edge access
    EdgeId edge(int i) const {
        int next = (i + 1) % 3;
        return make_edge_id(vertices[i], vertices[next]);
    }
    
private:
    static EdgeId make_edge_id(VertexId a, VertexId b) {
        if (a > b) std::swap(a, b);
        return (static_cast<EdgeId>(a) << 32) | b;
    }
};

/**
 * @brief Bounding box for spatial queries
 */
struct BoundingBox {
    double min_x, min_y, max_x, max_y;
    std::optional<double> min_z, max_z;

    BoundingBox() : min_x(0.0), min_y(0.0), max_x(0.0), max_y(0.0) {}
    BoundingBox(double minx, double miny, double maxx, double maxy)
        : min_x(minx), min_y(miny), max_x(maxx), max_y(maxy) {}
        
    bool contains(const Point2D& point) const {
        return point.x() >= min_x && point.x() <= max_x &&
               point.y() >= min_y && point.y() <= max_y;
    }
    
    double width() const { return max_x - min_x; }
    double height() const { return max_y - min_y; }
};

/**
 * @brief Strategy for contour level generation
 */
enum class ContourStrategy {
    UNIFORM,     ///< Uniform spacing between levels
    LOGARITHMIC, ///< Logarithmic spacing (good for large elevation ranges)
    EXPONENTIAL  ///< Exponential spacing (emphasizes lower elevations)
};

/**
 * @brief Configuration for topographic generation
 */
struct TopographicConfig {
    // Geographic bounds (matching Python: upper_left_lat/lon, lower_right_lat/lon)
    BoundingBox bounds{-151.1847, 62.9887, -150.8293, 63.1497}; // Default Mount Denali bounds (minx, miny, maxx, maxy)
    double upper_left_lat = 63.1497;
    double upper_left_lon = -151.1847;
    double lower_right_lat = 62.9887;
    double lower_right_lon = -150.8293;
    
    // Output configuration (matching Python defaults)
    std::string base_name = "mount_denali";
    std::string output_directory = "output";
    std::string cache_directory = "cache";  // Directory for downloaded tiles and temporary data
    std::string filename_pattern = "%{b}-%{l}";  // Filename pattern with substitutions
    std::vector<std::string> output_formats = {"stl"};
    bool output_layers = true;
    bool output_stacked = false;
    
    // Layer configuration (matching Python defaults)
    int num_layers = 7;
    double height_per_layer = 21.43;  // meters
    double contour_interval = 100.0;  // meters - contour spacing for SVG export
    double layer_thickness_mm = 3.0;
    double substrate_size_mm = 200.0;
    std::optional<double> substrate_depth_mm;
    std::optional<double> cutting_bed_size_mm;
    std::optional<double> cutting_bed_x_mm;
    std::optional<double> cutting_bed_y_mm;
    
    // Scaling configuration
    enum class ScalingMethod {
        AUTO,               // Choose based on available parameters
        BED_SIZE,          // Fit to bed dimensions
        MATERIAL_THICKNESS, // Calculate from material thickness (2D only)
        LAYERS,            // Fit N layers to substrate
        PRINT_HEIGHT,      // Fit to maximum Z height (3D only)
        UNIFORM_XYZ,       // Same scale for all axes (3D only)
        EXPLICIT           // Use explicitly provided scale factor
    };

    ScalingMethod scaling_2d_method = ScalingMethod::AUTO;
    ScalingMethod scaling_3d_method = ScalingMethod::AUTO;
    bool use_2d_scaling_for_3d = false;  // Force 2D scaling for 3D outputs
    bool use_3d_scaling_for_2d = false;  // Force 3D scaling for 2D outputs
    std::optional<double> explicit_2d_scale_factor;  // mm/m
    std::optional<double> explicit_3d_scale_factor;  // mm/m

    // Processing options (matching Python defaults)
    double simplification_tolerance = 5.0;  // meters
    double vertex_dedup_tolerance = 1e-6;   // meters (default 1 micron for mesh vertex deduplication)
    int smoothing_iterations = 1;
    double min_feature_area = 100.0;  // m²
    double min_feature_width_mm = 2.0;
    double print_resolution_dpi = 600.0;

    // Color options for 2D output
    std::string stroke_color = "FF0000";      // RGB hex for stroke/border color (default red)
    std::string background_color = "FFFFFF";   // RGB hex for background color (default white)
    double stroke_width = 0.2;                 // Stroke width in millimeters (for SVG laser cutting) or scaled for PNG

    // Elevation filtering (matching Python defaults)
    std::optional<double> elevation_threshold;
    std::optional<double> min_elevation;
    std::optional<double> max_elevation;
    
    // Print/cut direction (matching Python defaults)
    bool upside_down_printing = false;
    
    // Contour method (matching Python defaults)
    bool vertical_contour_relief = true;
    bool outer_boundaries_only = false;
    bool remove_holes = true;              // Remove polygon holes from SVG output (simpler laser cutting)

    // Inset options for 2D outputs (layer stacking optimization)
    bool inset_upper_layers = false;       // Cut holes where next layer sits (reduces material, creates nesting lips)
    double inset_offset_mm = 1.0;          // Size of lip to leave when insetting (mm)

    // Feature inclusion (matching Python defaults)
    bool include_roads = false;
    bool include_buildings = false;
    bool include_waterways = false;
    
    // Layer generation options (matching Python defaults)
    bool force_all_layers = false;
    bool include_layer_numbers = true;
    bool add_registration_marks = true;
    bool add_physical_registration = false;
    double physical_registration_hole_diameter = 1.0;
    bool add_base_layer = false;
    
    // Labeling options (matching Python defaults)
    std::string base_label_visible = "";
    std::string base_label_hidden = "";
    std::string layer_label_visible = "";
    std::string layer_label_hidden = "";

    // Label styling
    std::string visible_label_color = "#000000";  // Black
    std::string hidden_label_color = "#666666";   // Gray
    double base_font_size_mm = 4.0;
    double layer_font_size_mm = 3.0;

    // Font configuration for raster text rendering
    std::string font_path = "";           // Empty = auto-detect system font
    std::string font_face = "Arial";      // Preferred font face name

    // Unit preferences (matching Python defaults)
    enum class Units { METRIC, IMPERIAL };
    enum class PrintUnits { MM, INCHES };
    enum class LandUnits { METERS, FEET };
    Units label_units = Units::METRIC;
    PrintUnits print_units = PrintUnits::MM;
    LandUnits land_units = LandUnits::METERS;
    
    // Processing options
    bool parallel_processing = true;  // Enable by default as requested
    int num_threads = 0; // auto-detect
    
    // Quality settings
    enum class MeshQuality { DRAFT, MEDIUM, HIGH, ULTRA };
    MeshQuality quality = MeshQuality::MEDIUM;

    // Triangulation method
    enum class TriangulationMethod { DELAUNAY, SEIDEL };
    TriangulationMethod triangulation_method = TriangulationMethod::SEIDEL;

    // NURBS-specific settings
    enum class NURBSQuality { LOW, MEDIUM, HIGH };
    NURBSQuality nurbs_quality = NURBSQuality::MEDIUM;
    
    // OBJ-specific settings
    bool obj_include_materials = true;
    bool obj_elevation_colors = true;
    enum class ColorScheme { GRAYSCALE, TERRAIN, RAINBOW, TOPOGRAPHIC, HYPSOMETRIC, CUSTOM };
    ColorScheme color_scheme = ColorScheme::TERRAIN;
    int elevation_bands = 10;

    // Render mode for 2D vector outputs (SVG, PNG)
    enum class RenderMode {
        FULL_COLOR,    ///< Full color rendering with fills and strokes
        GRAYSCALE,     ///< Grayscale shading over land areas (elevation-based)
        MONOCHROME     ///< Boundaries only, no fills (outline mode)
    };
    RenderMode render_mode = RenderMode::FULL_COLOR;

    // Terrain following mode
    bool terrain_following = false;
    
    // Config file support
    std::optional<std::string> config_file;
    bool create_config = false;
    std::string create_config_path = "";
    
    // Layer list support
    std::optional<std::string> layer_list;
    std::vector<int> specific_layers;
    
    // Logging options
    int log_level = 3;  // 1-6 scale matching Python (1=CRITICAL, 2=ERROR, 3=WARNING, 4=INFO, 5=DEBUG, 6=ALL)
    std::optional<std::string> log_file;

    // Geometry debugging options
    bool debug_geometry = false;
    std::string debug_image_path = "debug";
    bool debug_show_legend = false;
};

/**
 * @brief Results from mesh validation operations
 */
struct MeshValidationResult {
    bool is_manifold = true;
    bool is_watertight = true;
    size_t num_non_manifold_edges = 0;
    size_t num_degenerate_triangles = 0;
    size_t num_duplicate_vertices = 0;
    size_t boundary_edge_count = 0;
    size_t excess_edge_count = 0;
    std::vector<EdgeId> non_manifold_edges;
    std::vector<TriangleId> degenerate_triangles;

    bool is_valid() const {
        return is_manifold && is_watertight &&
               num_degenerate_triangles == 0;
    }
};

/**
 * @brief Performance metrics for operations
 */
struct PerformanceMetrics {
    std::chrono::milliseconds elevation_loading_time{0};
    std::chrono::milliseconds contour_generation_time{0};
    std::chrono::milliseconds mesh_generation_time{0};
    std::chrono::milliseconds slicing_time{0};
    std::chrono::milliseconds export_time{0};
    std::chrono::milliseconds total_time{0};
    
    size_t triangles_generated = 0;
    size_t vertices_generated = 0;
    size_t memory_peak_mb = 0;
};

/**
 * @brief Main interface for topographic model generation
 * 
 * This class coordinates all components to generate high-quality
 * topographic models from elevation data using professional
 * algorithms adapted from Bambu Slicer.
 */
class TopographicGenerator {
public:
    explicit TopographicGenerator(const TopographicConfig& config);
    ~TopographicGenerator();
    
    // Main generation pipeline
    bool generate_model();
    
    // Individual pipeline stages
    bool load_elevation_data();
    bool generate_mesh();
    bool validate_mesh();
    bool export_models();
    
    // Accessors
    const TopographicMesh& get_mesh() const;
    const std::vector<ContourLayer>& get_contour_layers() const;
    const PerformanceMetrics& get_metrics() const;
    const MeshValidationResult& get_validation_result() const;
    const OutputTracker& get_output_tracker() const;
    
    // Configuration
    void update_config(const TopographicConfig& config);
    const TopographicConfig& get_config() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Factory function for creating generators with optimal settings
 */
std::unique_ptr<TopographicGenerator> create_generator(
    const BoundingBox& bounds,
    const std::vector<std::string>& output_formats = {"stl"},
    const TopographicConfig::MeshQuality quality = TopographicConfig::MeshQuality::MEDIUM
);

/**
 * @brief Utility function for quick model generation
 */
bool generate_topographic_model(
    const BoundingBox& bounds,
    const std::string& output_directory,
    const std::vector<std::string>& formats = {"stl"},
    int num_layers = 10
);

} // namespace topo