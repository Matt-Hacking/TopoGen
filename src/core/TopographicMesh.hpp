#pragma once

/**
 * @file TopographicMesh.hpp
 * @brief High-performance mesh representation with topology tracking
 * 
 * This class provides a robust mesh data structure optimized for
 * topographic model generation, with comprehensive topology management
 * and triangle-plane intersection capabilities adapted from libslic3r.
 */

#include "topographic_generator.hpp"
#include "Logger.hpp"
#include <unordered_set>
#include <mutex>

namespace topo {

/**
 * @brief Edge hash function for unordered containers
 */
struct EdgeHash {
    std::size_t operator()(const EdgeId& edge_id) const noexcept {
        return std::hash<EdgeId>{}(edge_id);
    }
};

/**
 * @brief Information about mesh edges for topology tracking
 */
struct EdgeInfo {
    std::vector<TriangleId> adjacent_triangles;
    bool is_boundary = false;
    bool is_manifold = true;
    
    void add_triangle(TriangleId triangle_id) {
        adjacent_triangles.push_back(triangle_id);
        is_manifold = adjacent_triangles.size() <= 2;
        is_boundary = adjacent_triangles.size() == 1;
    }
    
    void remove_triangle(TriangleId triangle_id) {
        auto it = std::find(adjacent_triangles.begin(), 
                           adjacent_triangles.end(), triangle_id);
        if (it != adjacent_triangles.end()) {
            adjacent_triangles.erase(it);
            is_manifold = adjacent_triangles.size() <= 2;
            is_boundary = adjacent_triangles.size() == 1;
        }
    }
};

/**
 * @brief High-performance topographic mesh with topology tracking
 * 
 * Features:
 * - Shared vertex storage for memory efficiency
 * - Real-time topology validation
 * - CGAL AABB tree for fast spatial queries
 * - Triangle-plane intersection using libslic3r algorithms
 * - Parallel mesh operations
 */
class TopographicMesh {
public:
    TopographicMesh();
    ~TopographicMesh();

    // Add move constructor and move assignment operator
    TopographicMesh(TopographicMesh&& other) noexcept
        : vertices_(std::move(other.vertices_))
        , triangles_(std::move(other.triangles_))
        , edge_registry_(std::move(other.edge_registry_))
        , non_manifold_edges_(std::move(other.non_manifold_edges_))
        , elevation_index_(std::move(other.elevation_index_)) {}

    TopographicMesh& operator=(TopographicMesh&& other) noexcept {
        if (this != &other) {
            vertices_ = std::move(other.vertices_);
            triangles_ = std::move(other.triangles_);
            edge_registry_ = std::move(other.edge_registry_);
            non_manifold_edges_ = std::move(other.non_manifold_edges_);
            elevation_index_ = std::move(other.elevation_index_);
        }
        return *this;
    }

    // Delete copy constructor and copy assignment (large objects should be moved)
    TopographicMesh(const TopographicMesh&) = delete;
    TopographicMesh& operator=(const TopographicMesh&) = delete;

    // Mesh construction
    VertexId add_vertex(const Vertex3D& vertex);
    VertexId add_vertex(Vertex3D&& vertex);  // Move overload
    TriangleId add_triangle(const Triangle& triangle);
    TriangleId add_triangle(VertexId v0, VertexId v1, VertexId v2);
    
    bool remove_vertex(VertexId vertex_id);
    bool remove_triangle(TriangleId triangle_id);
    
    // Accessors
    const Vertex3D& get_vertex(VertexId vertex_id) const;
    const Triangle& get_triangle(TriangleId triangle_id) const;
    
    Vertex3D& get_vertex_mutable(VertexId vertex_id);
    Triangle& get_triangle_mutable(TriangleId triangle_id);
    
    size_t num_vertices() const { return vertices_.size(); }
    size_t num_triangles() const { return triangles_.size(); }
    size_t num_edges() const { return edge_registry_.size(); }
    
    // Iteration
    auto vertices_begin() const { return vertices_.cbegin(); }
    auto vertices_end() const { return vertices_.cend(); }
    auto triangles_begin() const { return triangles_.cbegin(); }
    auto triangles_end() const { return triangles_.cend(); }
    
    // Spatial queries
    BoundingBox compute_bounding_box() const;

    // Topology operations
    MeshValidationResult validate_topology() const;
    bool repair_mesh_with_libigl();

    std::vector<EdgeId> find_non_manifold_edges() const;
    std::vector<TriangleId> find_degenerate_triangles() const;
    std::vector<VertexId> find_duplicate_vertices(double tolerance = 1e-6) const;
    
    // Mesh quality metrics
    double compute_average_edge_length() const;
    double compute_min_triangle_area() const;
    double compute_max_triangle_area() const;
    std::pair<double, double> compute_aspect_ratio_range() const;
    
    // Export support
    void export_to_stl(const std::string& filename, bool binary = true) const;
    void export_to_obj(const std::string& filename, bool with_materials = false) const;
    void export_to_ply(const std::string& filename) const;
    
    // Memory management
    void reserve_vertices(size_t count) { vertices_.reserve(count); }
    void reserve_triangles(size_t count) { triangles_.reserve(count); }
    void clear();

    // Configuration
    void set_vertex_deduplication_tolerance(double tolerance);

    // Statistics
    struct MemoryUsage {
        size_t vertex_memory_bytes;
        size_t triangle_memory_bytes;
        size_t edge_registry_bytes;
        size_t total_bytes() const {
            return vertex_memory_bytes + triangle_memory_bytes + edge_registry_bytes;
        }
    };
    MemoryUsage compute_memory_usage() const;

private:
    // Core data storage
    std::vector<Vertex3D> vertices_;
    std::vector<Triangle> triangles_;
    
    // Topology tracking
    std::unordered_map<EdgeId, EdgeInfo, EdgeHash> edge_registry_;
    std::unordered_set<EdgeId, EdgeHash> non_manifold_edges_;
    
    // Spatial acceleration structure for elevation-based queries
    struct ElevationIndex {
        struct ElevationBand {
            double min_z, max_z;
            std::vector<TriangleId> triangles;
        };

        std::vector<ElevationBand> bands;
        static constexpr double band_height = 10.0;  // Default 10m elevation bands
        double min_elevation = 0.0;
        double max_elevation = 0.0;
        bool valid = false;

        void build(const std::vector<Vertex3D>& vertices, const std::vector<Triangle>& triangles);
        std::vector<TriangleId> find_triangles_near_elevation(double elevation, double tolerance = 1.0) const;
    };

    mutable ElevationIndex elevation_index_;

    // Thread safety
    mutable std::mutex topology_mutex_;
    mutable std::mutex spatial_mutex_;

    // Vertex deduplication tolerance (used for both vertex merging and triangle validation)
    double vertex_dedup_tolerance_ = 1e-6;  // Default 1 micron

    // Logger for debugging and error reporting
    mutable Logger logger_;

    // Helper methods
    void update_edge_registry(TriangleId triangle_id, bool add);
    void rebuild_elevation_index() const;
    bool is_triangle_valid(const Triangle& triangle) const;
    Vector3D compute_triangle_normal(const Triangle& triangle) const;
    double compute_triangle_area(const Triangle& triangle) const;
};

/**
 * @brief Triangulation method for polygon surfaces
 */
enum class TriangulationMethod {
    SEIDEL  ///< Seidel's y-monotone partition (fast, O(n log n), produces watertight meshes)
};

/**
 * @brief Builder class for constructing meshes efficiently
 */
class MeshBuilder {
public:
    explicit MeshBuilder(size_t expected_vertices = 0, size_t expected_triangles = 0);

    VertexId add_vertex(double x, double y, double z);
    VertexId add_vertex(const Point3D& position);
    TriangleId add_triangle(VertexId v0, VertexId v1, VertexId v2);

    // Batch operations for better performance
    std::vector<VertexId> add_vertices(const std::vector<Point3D>& positions);
    std::vector<TriangleId> add_triangles(const std::vector<std::array<VertexId, 3>>& triangles);

    // Build final mesh with validation
    std::unique_ptr<TopographicMesh> build(bool validate = true);

    // Create watertight extruded mesh from 2D polygon
    void add_extruded_polygon(const std::vector<Point3D>& polygon_vertices,
                             double top_elevation, double bottom_elevation,
                             TriangulationMethod method = TriangulationMethod::SEIDEL);

    // Statistics
    size_t vertex_count() const { return vertices_.size(); }
    size_t triangle_count() const { return triangles_.size(); }

    // Set vertex deduplication tolerance (call before adding vertices)
    void set_vertex_deduplication_tolerance(double tolerance);

private:
    // Polygon triangulation helper functions
    std::vector<std::array<size_t, 3>> triangulate_polygon_seidel(
        const std::vector<Point3D>& vertices) const;

    std::vector<Vertex3D> vertices_;
    std::vector<Triangle> triangles_;

    // Hash-based vertex deduplication (replaces memory-explosive 3D grid)
    struct VertexKeyHash {
        std::size_t operator()(const std::tuple<int64_t, int64_t, int64_t>& key) const {
            auto h1 = std::hash<int64_t>{}(std::get<0>(key));
            auto h2 = std::hash<int64_t>{}(std::get<1>(key));
            auto h3 = std::hash<int64_t>{}(std::get<2>(key));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    struct HashBasedVertexIndex {
        double tolerance;
        std::unordered_map<std::tuple<int64_t, int64_t, int64_t>, VertexId, VertexKeyHash> vertex_map;
        bool initialized = false;

        // Statistics for monitoring (mutable to allow updates in const methods)
        mutable size_t lookup_count = 0;
        mutable size_t deduplication_count = 0;

        void initialize(double tolerance_);
        VertexId find_vertex(double x, double y, double z) const;
        void add_vertex(double x, double y, double z, VertexId vertex_id);

    private:
        std::tuple<int64_t, int64_t, int64_t> vertex_key(double x, double y, double z) const;
    } hash_vertex_index_;

    // Helper to get or create vertex using spatial indexing
    VertexId get_or_create_vertex(const Point3D& position);

    // Logger for debugging and error reporting
    mutable Logger logger_;
};

} // namespace topo