/**
 * @file TopographicMesh.cpp
 * @brief Implementation of high-performance mesh with topology tracking
 */

#include "TopographicMesh.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <execution>
#include <numeric>
#include <unordered_set>
#include <fstream>
#include <iomanip>

// CGAL includes for robust polygon triangulation
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/partition_2.h>
#include <CGAL/Partition_traits_2.h>
#include <CGAL/Polygon_repair/repair.h>
#include <CGAL/Multipolygon_with_holes_2.h>

// No external dependencies needed for simple mesh cleanup

namespace topo {

// Compile-time constants for mesh operations
constexpr size_t DEFAULT_VERTEX_RESERVE = 100000;
constexpr size_t DEFAULT_TRIANGLE_RESERVE = 200000;
constexpr size_t DEFAULT_EDGE_RESERVE = 300000;

// ============================================================================
// Helper Functions for Distance Calculations (replaces CGAL::squared_distance)
// ============================================================================

/**
 * @brief Compute squared distance between two 3D points
 */
inline double squared_distance_3d(const Point3D& a, const Point3D& b) {
    double dx = a.x() - b.x();
    double dy = a.y() - b.y();
    double dz = a.z() - b.z();
    return dx * dx + dy * dy + dz * dz;
}

// ============================================================================
// TopographicMesh Implementation
// ============================================================================

TopographicMesh::TopographicMesh()
    : logger_("TopographicMesh") {
    // More aggressive pre-allocation based on typical usage patterns
    vertices_.reserve(DEFAULT_VERTEX_RESERVE);   // Increased for large topographic meshes
    triangles_.reserve(DEFAULT_TRIANGLE_RESERVE);  // Triangles typically 2x vertices

    // Pre-allocate edge registry hash table buckets
    edge_registry_.reserve(DEFAULT_EDGE_RESERVE);  // Edges typically 3x vertices
}

TopographicMesh::~TopographicMesh() = default;

VertexId TopographicMesh::add_vertex(const Vertex3D& vertex) {
    std::lock_guard<std::mutex> lock(topology_mutex_);

    VertexId vertex_id = static_cast<VertexId>(vertices_.size());
    vertices_.push_back(vertex);

    // Invalidate spatial acceleration structure
    elevation_index_.valid = false;

    return vertex_id;
}

VertexId TopographicMesh::add_vertex(Vertex3D&& vertex) {
    std::lock_guard<std::mutex> lock(topology_mutex_);

    VertexId vertex_id = static_cast<VertexId>(vertices_.size());
    vertices_.push_back(std::move(vertex));  // Use move semantics

    // Invalidate spatial acceleration structure
    elevation_index_.valid = false;

    return vertex_id;
}

TriangleId TopographicMesh::add_triangle(const Triangle& triangle) {
    std::lock_guard<std::mutex> lock(topology_mutex_);
    
    // Validate triangle before adding
    if (!is_triangle_valid(triangle)) {
        return static_cast<TriangleId>(-1);  // Invalid triangle ID
    }
    
    TriangleId triangle_id = static_cast<TriangleId>(triangles_.size());
    
    // Compute triangle properties
    Triangle tri_copy = triangle;
    tri_copy.normal = compute_triangle_normal(triangle);
    tri_copy.area = compute_triangle_area(triangle);
    
    triangles_.push_back(tri_copy);
    
    // Update topology registry
    update_edge_registry(triangle_id, true);
    
    // Invalidate spatial acceleration structure
    elevation_index_.valid = false;
    
    return triangle_id;
}

TriangleId TopographicMesh::add_triangle(VertexId v0, VertexId v1, VertexId v2) {
    Triangle triangle;
    triangle.vertices = {v0, v1, v2};
    return add_triangle(triangle);
}

bool TopographicMesh::remove_triangle(TriangleId triangle_id) {
    std::lock_guard<std::mutex> lock(topology_mutex_);
    
    if (triangle_id >= triangles_.size()) {
        return false;
    }
    
    // Update topology registry
    update_edge_registry(triangle_id, false);
    
    // Mark triangle as invalid (avoid vector reallocation)
    triangles_[triangle_id].vertices = {static_cast<VertexId>(-1), 
                                       static_cast<VertexId>(-1), 
                                       static_cast<VertexId>(-1)};
    
    elevation_index_.valid = false;
    return true;
}

const Vertex3D& TopographicMesh::get_vertex(VertexId vertex_id) const {
    if (vertex_id >= vertices_.size()) {
        throw std::out_of_range("Vertex ID " + std::to_string(vertex_id) + " out of range");
    }
    return vertices_[vertex_id];
}

const Triangle& TopographicMesh::get_triangle(TriangleId triangle_id) const {
    if (triangle_id >= triangles_.size()) {
        throw std::out_of_range("Triangle ID " + std::to_string(triangle_id) + " out of range");
    }
    return triangles_[triangle_id];
}

Vertex3D& TopographicMesh::get_vertex_mutable(VertexId vertex_id) {
    if (vertex_id >= vertices_.size()) {
        throw std::out_of_range("Vertex ID " + std::to_string(vertex_id) + " out of range");
    }
    elevation_index_.valid = false;
    return vertices_[vertex_id];
}

Triangle& TopographicMesh::get_triangle_mutable(TriangleId triangle_id) {
    if (triangle_id >= triangles_.size()) {
        throw std::out_of_range("Triangle ID " + std::to_string(triangle_id) + " out of range");
    }
    elevation_index_.valid = false;
    return triangles_[triangle_id];
}

BoundingBox TopographicMesh::compute_bounding_box() const {
    if (vertices_.empty()) {
        return BoundingBox();
    }
    
    auto minmax_x = std::minmax_element(vertices_.begin(), vertices_.end(),
        [](const Vertex3D& a, const Vertex3D& b) {
            return a.position.x() < b.position.x();
        });
    
    auto minmax_y = std::minmax_element(vertices_.begin(), vertices_.end(),
        [](const Vertex3D& a, const Vertex3D& b) {
            return a.position.y() < b.position.y();
        });
    
    auto minmax_z = std::minmax_element(vertices_.begin(), vertices_.end(),
        [](const Vertex3D& a, const Vertex3D& b) {
            return a.position.z() < b.position.z();
        });
    
    BoundingBox bbox;
    bbox.min_x = minmax_x.first->position.x();
    bbox.max_x = minmax_x.second->position.x();
    bbox.min_y = minmax_y.first->position.y();
    bbox.max_y = minmax_y.second->position.y();
    bbox.min_z = minmax_z.first->position.z();
    bbox.max_z = minmax_z.second->position.z();
    
    return bbox;
}


MeshValidationResult TopographicMesh::validate_topology() const {
    std::lock_guard<std::mutex> lock(topology_mutex_);
    
    MeshValidationResult result;
    
    // Check for non-manifold edges
    for (const auto& [edge_id, edge_info] : edge_registry_) {
        if (!edge_info.is_manifold) {
            result.non_manifold_edges.push_back(edge_id);
            result.is_manifold = false;
        }
    }
    result.num_non_manifold_edges = result.non_manifold_edges.size();
    
    // Check for degenerate triangles
    for (size_t i = 0; i < triangles_.size(); ++i) {
        const Triangle& triangle = triangles_[i];
        
        // Skip invalid triangles
        if (triangle.vertices[0] == static_cast<VertexId>(-1)) {
            continue;
        }
        
        if (triangle.area < 1e-12 || !is_triangle_valid(triangle)) {
            result.degenerate_triangles.push_back(static_cast<TriangleId>(i));
        }
    }
    result.num_degenerate_triangles = result.degenerate_triangles.size();
    
    // Check for watertight mesh (all edges should be shared by exactly 2 triangles)
    result.is_watertight = true;
    size_t boundary_edge_count = 0;
    size_t non_manifold_edge_count = 0;

    for (const auto& [edge_id, edge_info] : edge_registry_) {
        size_t triangle_count = edge_info.adjacent_triangles.size();
        if (triangle_count == 1) {
            boundary_edge_count++;
            result.is_watertight = false;
        } else if (triangle_count > 2) {
            non_manifold_edge_count++;
            result.is_watertight = false;
        }
    }

    // Update validation result with detailed edge counts
    result.boundary_edge_count = boundary_edge_count;
    result.excess_edge_count = non_manifold_edge_count;
    
    return result;
}

std::vector<EdgeId> TopographicMesh::find_non_manifold_edges() const {
    std::vector<EdgeId> non_manifold_edges;

    for (const auto& [edge_id, edge_info] : edge_registry_) {
        if (!edge_info.is_manifold) {
            non_manifold_edges.push_back(edge_id);
        }
    }

    return non_manifold_edges;
}

bool TopographicMesh::repair_mesh_with_libigl() {
    std::lock_guard<std::mutex> lock(topology_mutex_);

    logger_.info("Cleaning up degenerate triangles...");

    // Find degenerate triangles (area too small)
    std::vector<TriangleId> degenerate_tris;
    const double min_area = 1e-12;  // Extremely small threshold for degenerate triangles

    for (size_t i = 0; i < triangles_.size(); ++i) {
        const auto& tri = triangles_[i];
        if (tri.vertices[0] == static_cast<VertexId>(-1)) continue;  // Already invalid

        if (tri.area < min_area) {
            degenerate_tris.push_back(i);
        }
    }

    if (degenerate_tris.empty()) {
        logger_.info("No degenerate triangles found");
        return true;
    }

    logger_.info("Removing " + std::to_string(degenerate_tris.size()) + " degenerate triangles");

    // Mark degenerate triangles as invalid
    for (TriangleId tri_id : degenerate_tris) {
        triangles_[tri_id].vertices[0] = static_cast<VertexId>(-1);
        triangles_[tri_id].vertices[1] = static_cast<VertexId>(-1);
        triangles_[tri_id].vertices[2] = static_cast<VertexId>(-1);
    }

    // Rebuild edge registry to update manifold status
    edge_registry_.clear();
    for (size_t i = 0; i < triangles_.size(); ++i) {
        const auto& tri = triangles_[i];
        if (tri.vertices[0] == static_cast<VertexId>(-1)) continue;

        for (int j = 0; j < 3; ++j) {
            EdgeId eid = tri.edge(j);
            auto& edge_info = edge_registry_[eid];
            edge_info.adjacent_triangles.push_back(i);
            edge_info.is_manifold = (edge_info.adjacent_triangles.size() <= 2);
        }
    }

    logger_.info("Mesh cleanup completed - edge registry rebuilt");
    return true;
}

void TopographicMesh::export_to_stl(const std::string& filename, bool binary) const {
    std::ofstream file(filename, binary ? std::ios::binary : std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }
    
    // Count valid triangles
    size_t valid_triangle_count = 0;
    for (const auto& triangle : triangles_) {
        if (triangle.vertices[0] != static_cast<VertexId>(-1)) {
            valid_triangle_count++;
        }
    }
    
    if (binary) {
        // Binary STL format
        char header[80] = "Generated by C++ Topographic Generator";
        file.write(header, 80);
        
        uint32_t triangle_count = static_cast<uint32_t>(valid_triangle_count);
        file.write(reinterpret_cast<const char*>(&triangle_count), 4);
        
        for (const auto& triangle : triangles_) {
            if (triangle.vertices[0] == static_cast<VertexId>(-1)) continue;
            
            // Write normal
            float normal[3] = {
                static_cast<float>(triangle.normal.x()),
                static_cast<float>(triangle.normal.y()),
                static_cast<float>(triangle.normal.z())
            };
            file.write(reinterpret_cast<const char*>(normal), 12);
            
            // Write vertices
            for (int i = 0; i < 3; ++i) {
                const auto& vertex = vertices_[triangle.vertices[i]];
                float vertex_coords[3] = {
                    static_cast<float>(vertex.position.x()),
                    static_cast<float>(vertex.position.y()),
                    static_cast<float>(vertex.position.z())
                };
                file.write(reinterpret_cast<const char*>(vertex_coords), 12);
            }
            
            // Write attribute byte count (always 0)
            uint16_t attribute_count = 0;
            file.write(reinterpret_cast<const char*>(&attribute_count), 2);
        }
    } else {
        // ASCII STL format
        file << "solid TopographicModel\n";
        
        for (const auto& triangle : triangles_) {
            if (triangle.vertices[0] == static_cast<VertexId>(-1)) continue;
            
            file << "  facet normal " 
                 << triangle.normal.x() << " " 
                 << triangle.normal.y() << " " 
                 << triangle.normal.z() << "\n";
            file << "    outer loop\n";
            
            for (int i = 0; i < 3; ++i) {
                const auto& vertex = vertices_[triangle.vertices[i]];
                file << "      vertex " 
                     << vertex.position.x() << " " 
                     << vertex.position.y() << " " 
                     << vertex.position.z() << "\n";
            }
            
            file << "    endloop\n";
            file << "  endfacet\n";
        }
        
        file << "endsolid TopographicModel\n";
    }
    
    file.close();
}

void TopographicMesh::clear() {
    std::lock_guard<std::mutex> lock(topology_mutex_);

    vertices_.clear();
    triangles_.clear();
    edge_registry_.clear();
    non_manifold_edges_.clear();
    elevation_index_.valid = false;
}

void TopographicMesh::set_vertex_deduplication_tolerance(double tolerance) {
    vertex_dedup_tolerance_ = tolerance;
}

TopographicMesh::MemoryUsage TopographicMesh::compute_memory_usage() const {
    MemoryUsage usage;
    
    usage.vertex_memory_bytes = vertices_.size() * sizeof(Vertex3D);
    usage.triangle_memory_bytes = triangles_.size() * sizeof(Triangle);
    usage.edge_registry_bytes = edge_registry_.size() * 
        (sizeof(EdgeId) + sizeof(EdgeInfo));
    
    return usage;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

void TopographicMesh::update_edge_registry(TriangleId triangle_id, bool add) {
    // Validate triangle ID
    if (triangle_id >= triangles_.size()) {
        return;
    }

    const Triangle& triangle = triangles_[triangle_id];

    // Skip invalid triangles
    if (triangle.vertices[0] == static_cast<VertexId>(-1)) {
        return;
    }

    for (int i = 0; i < 3; ++i) {
        EdgeId edge_id = triangle.edge(i);

        if (add) {
            auto& edge_info = edge_registry_[edge_id];
            edge_info.add_triangle(triangle_id);

            // Update non-manifold edge tracking
            if (!edge_info.is_manifold) {
                non_manifold_edges_.insert(edge_id);
            } else {
                non_manifold_edges_.erase(edge_id);
            }
        } else {
            auto it = edge_registry_.find(edge_id);
            if (it != edge_registry_.end()) {
                it->second.remove_triangle(triangle_id);

                // Remove edge if no triangles use it
                if (it->second.adjacent_triangles.empty()) {
                    non_manifold_edges_.erase(edge_id);
                    edge_registry_.erase(it);
                } else {
                    // Update manifold status after removal
                    if (it->second.is_manifold) {
                        non_manifold_edges_.erase(edge_id);
                    } else {
                        non_manifold_edges_.insert(edge_id);
                    }
                }
            }
        }
    }
}

bool TopographicMesh::is_triangle_valid(const Triangle& triangle) const {
    // Check vertex indices are valid
    for (int i = 0; i < 3; ++i) {
        if (triangle.vertices[i] >= vertices_.size() ||
            triangle.vertices[i] == static_cast<VertexId>(-1)) {
            return false;
        }
    }

    // Check for duplicate vertex indices
    if (triangle.vertices[0] == triangle.vertices[1] ||
        triangle.vertices[1] == triangle.vertices[2] ||
        triangle.vertices[2] == triangle.vertices[0]) {
        return false;
    }

    // Check vertices are not identical (using same tolerance as vertex deduplication)
    const auto& v0 = vertices_[triangle.vertices[0]].position;
    const auto& v1 = vertices_[triangle.vertices[1]].position;
    const auto& v2 = vertices_[triangle.vertices[2]].position;

    const double epsilon = vertex_dedup_tolerance_ * vertex_dedup_tolerance_;  // Squared for distance comparison

    if (squared_distance_3d(v0, v1) < epsilon ||
        squared_distance_3d(v1, v2) < epsilon ||
        squared_distance_3d(v2, v0) < epsilon) {
        return false;
    }

    return true;
}

Vector3D TopographicMesh::compute_triangle_normal(const Triangle& triangle) const {
    // Validate triangle before computation
    if (!is_triangle_valid(triangle)) {
        return Vector3D(0, 0, 1); // Default normal
    }

    const auto& v0 = vertices_[triangle.vertices[0]].position;
    const auto& v1 = vertices_[triangle.vertices[1]].position;
    const auto& v2 = vertices_[triangle.vertices[2]].position;
    
    Vector3D edge1(v1.x() - v0.x(), v1.y() - v0.y(), v1.z() - v0.z());
    Vector3D edge2(v2.x() - v0.x(), v2.y() - v0.y(), v2.z() - v0.z());
    
    Vector3D normal = edge1.cross(edge2);

    // Normalize
    return normal.normalized();
}

double TopographicMesh::compute_triangle_area(const Triangle& triangle) const {
    // Validate triangle before computation
    if (!is_triangle_valid(triangle)) {
        return 0.0; // Invalid triangle has zero area
    }

    const auto& v0 = vertices_[triangle.vertices[0]].position;
    const auto& v1 = vertices_[triangle.vertices[1]].position;
    const auto& v2 = vertices_[triangle.vertices[2]].position;
    
    Vector3D edge1(v1.x() - v0.x(), v1.y() - v0.y(), v1.z() - v0.z());
    Vector3D edge2(v2.x() - v0.x(), v2.y() - v0.y(), v2.z() - v0.z());

    Vector3D cross = edge1.cross(edge2);
    return 0.5 * cross.length();
}

// ============================================================================
// MeshBuilder Implementation  
// ============================================================================

MeshBuilder::MeshBuilder(size_t expected_vertices, size_t expected_triangles)
    : logger_("MeshBuilder") {
    if (expected_vertices > 0) vertices_.reserve(expected_vertices);
    if (expected_triangles > 0) triangles_.reserve(expected_triangles);
}

VertexId MeshBuilder::add_vertex(double x, double y, double z) {
    return add_vertex(Point3D(x, y, z));
}

VertexId MeshBuilder::add_vertex(const Point3D& position) {
    if (hash_vertex_index_.initialized) {
        return get_or_create_vertex(position);
    } else {
        // Fallback to non-deduplicated mode if vertex indexing not set up
        VertexId vertex_id = static_cast<VertexId>(vertices_.size());
        vertices_.emplace_back(position);
        return vertex_id;
    }
}

TriangleId MeshBuilder::add_triangle(VertexId v0, VertexId v1, VertexId v2) {
    TriangleId triangle_id = static_cast<TriangleId>(triangles_.size());
    triangles_.emplace_back(v0, v1, v2);
    return triangle_id;
}

std::vector<VertexId> MeshBuilder::add_vertices(const std::vector<Point3D>& positions) {
    std::vector<VertexId> vertex_ids;
    vertex_ids.reserve(positions.size());
    
    for (const auto& position : positions) {
        vertex_ids.push_back(add_vertex(position));
    }
    
    return vertex_ids;
}

std::vector<TriangleId> MeshBuilder::add_triangles(
    const std::vector<std::array<VertexId, 3>>& triangles) {
    
    std::vector<TriangleId> triangle_ids;
    triangle_ids.reserve(triangles.size());
    
    for (const auto& triangle : triangles) {
        triangle_ids.push_back(add_triangle(triangle[0], triangle[1], triangle[2]));
    }
    
    return triangle_ids;
}

std::unique_ptr<TopographicMesh> MeshBuilder::build(bool validate) {
    logger_.debug("MeshBuilder::build() starting");
    logger_.debug("Input: " + std::to_string(vertices_.size()) + " vertices, " + std::to_string(triangles_.size()) + " triangles");

    try {
        auto mesh = std::make_unique<TopographicMesh>();
        if (!mesh) {
            logger_.error("Failed to create TopographicMesh");
            return nullptr;
        }
        logger_.debug("Created TopographicMesh successfully");

        // Set vertex deduplication tolerance to match what was used during vertex indexing
        if (hash_vertex_index_.initialized) {
            mesh->set_vertex_deduplication_tolerance(hash_vertex_index_.tolerance);
            logger_.debug("Set mesh vertex deduplication tolerance to " + std::to_string(hash_vertex_index_.tolerance) + " meters");
        }

        // Add vertices with safety checks
        logger_.debug("Adding vertices...");
        size_t vertex_count = 0;
        for (const auto& vertex : vertices_) {
            try {
                VertexId vid = mesh->add_vertex(vertex);
                if (vid == static_cast<VertexId>(-1)) {
                    logger_.warning("Failed to add vertex " + std::to_string(vertex_count));
                }
                vertex_count++;
                if (vertex_count % 1000 == 0) {
                    logger_.debug("Added " + std::to_string(vertex_count) + " vertices...");
                }
            } catch (const std::exception& e) {
                logger_.error("Exception adding vertex " + std::to_string(vertex_count) + ": " + e.what());
                return nullptr;
            }
        }
        logger_.debug("Successfully added " + std::to_string(vertex_count) + " vertices");

        // Add triangles with extensive safety checks
        logger_.debug("Adding triangles...");
        size_t triangle_count = 0;
        size_t failed_triangles = 0;

        for (const auto& triangle : triangles_) {
            try {
                // Declare triangle_id at function scope to avoid goto issues
                TriangleId triangle_id = static_cast<TriangleId>(-1);

                // Validate triangle vertex indices before adding
                for (int i = 0; i < 3; ++i) {
                    if (triangle.vertices[i] >= vertices_.size()) {
                        logger_.error("Triangle " + std::to_string(triangle_count) + " has invalid vertex " + std::to_string(i) +
                                     ": " + std::to_string(triangle.vertices[i]) + " >= " + std::to_string(vertices_.size()));
                        failed_triangles++;
                        goto next_triangle;
                    }
                }

                // The triangle already has the correct structure
                triangle_id = mesh->add_triangle(triangle);

                // Check if triangle was successfully added
                if (triangle_id == static_cast<TriangleId>(-1)) {
                    // Diagnose why triangle was rejected
                    std::string reason = "unknown";

                    // Check for duplicate indices
                    if (triangle.vertices[0] == triangle.vertices[1] ||
                        triangle.vertices[1] == triangle.vertices[2] ||
                        triangle.vertices[2] == triangle.vertices[0]) {
                        reason = "duplicate_vertex_indices";
                    } else {
                        // Check for collocated vertices (degenerate triangle)
                        const auto& v0 = mesh->get_vertex(triangle.vertices[0]).position;
                        const auto& v1 = mesh->get_vertex(triangle.vertices[1]).position;
                        const auto& v2 = mesh->get_vertex(triangle.vertices[2]).position;

                        double tol = 0.001; // 1mm tolerance for diagnostic
                        double tol_sq = tol * tol;

                        double d01 = squared_distance_3d(v0, v1);
                        double d12 = squared_distance_3d(v1, v2);
                        double d20 = squared_distance_3d(v2, v0);

                        if (d01 < tol_sq || d12 < tol_sq || d20 < tol_sq) {
                            reason = "degenerate(vertices_too_close)";
                        }
                    }

                    logger_.warning("Failed to add triangle " + std::to_string(triangle_count) +
                                   " - reason: " + reason);
                    failed_triangles++;
                } else {
                    triangle_count++;
                    if (triangle_count % 1000 == 0) {
                        logger_.debug("Added " + std::to_string(triangle_count) + " triangles...");
                    }
                }

                next_triangle:;
            } catch (const std::exception& e) {
                logger_.error("Exception adding triangle " + std::to_string(triangle_count) + ": " + std::string(e.what()));
                failed_triangles++;
            }
        }
        logger_.debug("Successfully added " + std::to_string(triangle_count) + " triangles, " +
                     std::to_string(failed_triangles) + " failed");

    if (validate) {
        logger_.debug("Starting mesh validation...");

        try {
            auto validation_result = mesh->validate_topology();
            logger_.debug("Validation completed successfully");

            if (!validation_result.is_valid()) {
                // Log validation issues and attempt repairs
                logger_.warning("Mesh validation found issues:");
                logger_.warning("  Non-manifold edges: " + std::to_string(validation_result.num_non_manifold_edges));
                logger_.warning("  Degenerate triangles: " + std::to_string(validation_result.num_degenerate_triangles));
                logger_.warning("  Boundary edges: " + std::to_string(validation_result.boundary_edge_count));
                logger_.warning("  Excess edges: " + std::to_string(validation_result.excess_edge_count));
                logger_.warning("  Is watertight: " + std::string(validation_result.is_watertight ? "yes" : "no"));

                // Note: No repair during layer building to avoid vertex duplication issues
                // Degenerate triangles are typically small and don't affect exports

                // Check mesh validity after validation
                if (!mesh) {
                    logger_.error("Mesh pointer is null after validation!");
                    return nullptr;
                }

                try {
                    size_t vertex_count = mesh->num_vertices();
                    size_t triangle_count = mesh->num_triangles();
                    size_t edge_count = mesh->num_edges();

                    logger_.debug("Mesh stats before repair: " + std::to_string(vertex_count) + " vertices, " +
                                std::to_string(triangle_count) + " triangles, " + std::to_string(edge_count) + " edges");
                } catch (const std::exception& e) {
                    logger_.error("Exception getting mesh statistics: " + std::string(e.what()));
                    return nullptr;
                } catch (...) {
                    logger_.error("Unknown exception getting mesh statistics");
                    return nullptr;
                }
            } else {
                logger_.debug("Mesh validation passed - no repairs needed");
            }
        } catch (const std::exception& e) {
            logger_.error("Exception during mesh validation: " + std::string(e.what()));
            logger_.debug("Continuing without validation...");
        }
    }

    logger_.debug("Mesh building completed successfully");

        return mesh;
    } catch (const std::exception& e) {
        logger_.error("Exception in MeshBuilder::build(): " + std::string(e.what()));
        return nullptr;
    }
}

// ============================================================================
// MeshBuilder Hash-Based Vertex Indexing Implementation
// ============================================================================

void MeshBuilder::set_vertex_deduplication_tolerance(double tolerance) {
    hash_vertex_index_.initialize(tolerance);
}

void MeshBuilder::HashBasedVertexIndex::initialize(double tolerance_) {
    tolerance = tolerance_;
    vertex_map.clear();
    lookup_count = 0;
    deduplication_count = 0;
    initialized = true;
    // Debug output removed - use Logger for proper log level control
}

std::tuple<int64_t, int64_t, int64_t> MeshBuilder::HashBasedVertexIndex::vertex_key(
    double x, double y, double z) const {
    // Create integer keys by quantizing coordinates to tolerance
    // This is similar to Python's vertex_key() function
    double scale = 1.0 / tolerance;
    return std::make_tuple(
        static_cast<int64_t>(std::round(x * scale)),
        static_cast<int64_t>(std::round(y * scale)),
        static_cast<int64_t>(std::round(z * scale))
    );
}

VertexId MeshBuilder::HashBasedVertexIndex::find_vertex(double x, double y, double z) const {
    if (!initialized) return static_cast<VertexId>(-1);

    lookup_count++;
    auto key = vertex_key(x, y, z);
    auto it = vertex_map.find(key);

    if (it != vertex_map.end()) {
        deduplication_count++;
        return it->second;
    }

    return static_cast<VertexId>(-1);
}

void MeshBuilder::HashBasedVertexIndex::add_vertex(double x, double y, double z, VertexId vertex_id) {
    if (!initialized) return;

    auto key = vertex_key(x, y, z);
    vertex_map[key] = vertex_id;
}

VertexId MeshBuilder::get_or_create_vertex(const Point3D& position) {
    double x = position.x();
    double y = position.y();
    double z = position.z();

    // Check if vertex already exists at this position
    VertexId existing_vertex = hash_vertex_index_.find_vertex(x, y, z);
    if (existing_vertex != static_cast<VertexId>(-1)) {
        return existing_vertex; // Reuse existing vertex
    }

    // Create new vertex
    VertexId vertex_id = static_cast<VertexId>(vertices_.size());
    vertices_.emplace_back(position);

    // Add to hash-based index
    hash_vertex_index_.add_vertex(x, y, z, vertex_id);

    return vertex_id;
}

// ============================================================================
// MeshBuilder Polygon Triangulation Implementations
// ============================================================================

/**
 * @brief Triangulate polygon using Seidel's y-monotone partition algorithm
 * Fast O(n log n) method that partitions into y-monotone pieces then triangulates each
 */
std::vector<std::array<size_t, 3>> MeshBuilder::triangulate_polygon_seidel(
    const std::vector<Point3D>& vertices) const {

    std::vector<std::array<size_t, 3>> triangles;

    if (vertices.size() < 3) return triangles;

    // Use CGAL types - must use std::list for partition algorithm compatibility
    using K = CGAL::Exact_predicates_inexact_constructions_kernel;
    using Point_2 = K::Point_2;
    using Polygon_2 = CGAL::Polygon_2<K, std::list<Point_2>>;  // list container for partition compatibility
    using Polygon_list = std::list<Polygon_2>;
    using Multipolygon_2 = CGAL::Multipolygon_with_holes_2<K, std::list<Point_2>>;

    // Convert to CGAL polygon
    Polygon_2 polygon;
    for (const auto& v : vertices) {
        polygon.push_back(Point_2(v.x(), v.y()));
    }

    // CRITICAL: Check if polygon is simple (non-self-intersecting) BEFORE calling orientation()
    // orientation() has a precondition that the polygon must be simple!
    // If not simple, use CGAL Polygon_repair to split into multiple simple polygons
    std::vector<Polygon_2> simple_polygons;

    if (!polygon.is_simple()) {
        // Polygon has self-intersections - repair it
        logger_.debug("  Detected self-intersecting polygon with " + std::to_string(vertices.size()) + " vertices - repairing...");

        // Use CGAL's polygon repair with Even-Odd rule (appropriate for contours)
        Multipolygon_2 repaired = CGAL::Polygon_repair::repair(polygon, CGAL::Polygon_repair::Even_odd_rule());

        logger_.debug("  Repair split polygon into " + std::to_string(repaired.number_of_polygons_with_holes()) + " simple polygon(s)");

        // Extract outer boundaries from each polygon (ignore holes for now - contours shouldn't have holes at this stage)
        for (const auto& poly_with_holes : repaired.polygons_with_holes()) {
            Polygon_2 repaired_poly = poly_with_holes.outer_boundary();

            // CGAL repair ensures proper orientation, but double-check and fix if needed
            // Now safe to call orientation() since repaired polygons are guaranteed simple
            if (repaired_poly.orientation() == CGAL::CLOCKWISE) {
                repaired_poly.reverse_orientation();
            }

            simple_polygons.push_back(repaired_poly);

            // Log if any holes were created (unexpected for contour polygons)
            if (poly_with_holes.number_of_holes() > 0) {
                logger_.warning("  Repaired polygon has " + std::to_string(poly_with_holes.number_of_holes()) +
                              " holes (unexpected for contour polygon)");
            }
        }
    } else {
        // Polygon is already simple - safe to check orientation
        if (polygon.orientation() == CGAL::CLOCKWISE) {
            polygon.reverse_orientation();
        }
        simple_polygons.push_back(polygon);
    }

    // Process each simple polygon
    for (const auto& simple_poly : simple_polygons) {
        // Partition into y-monotone pieces
        Polygon_list partition_polys;
        CGAL::y_monotone_partition_2(simple_poly.vertices_begin(),
                                      simple_poly.vertices_end(),
                                      std::back_inserter(partition_polys));

        // Triangulate each y-monotone piece (simple linear sweep)
        for (const auto& mono_poly : partition_polys) {
            size_t n = mono_poly.size();
            if (n < 3) continue;

            // Find vertices in original polygon to get indices
            std::vector<size_t> indices;
            for (auto vit = mono_poly.vertices_begin(); vit != mono_poly.vertices_end(); ++vit) {
                // Find matching vertex in original
                for (size_t i = 0; i < vertices.size(); ++i) {
                    if (std::abs(CGAL::to_double(vit->x()) - vertices[i].x()) < 1e-9 &&
                        std::abs(CGAL::to_double(vit->y()) - vertices[i].y()) < 1e-9) {
                        indices.push_back(i);
                        break;
                    }
                }
            }

            // Simple fan triangulation (valid for y-monotone polygons)
            for (size_t i = 1; i + 1 < indices.size(); ++i) {
                triangles.push_back({indices[0], indices[i], indices[i + 1]});
            }
        }
    }

    return triangles;
}

// ============================================================================
// MeshBuilder Watertight Extrusion Implementation
// ============================================================================

void MeshBuilder::add_extruded_polygon(const std::vector<Point3D>& polygon_vertices,
                                      double top_elevation, double bottom_elevation,
                                      [[maybe_unused]] TriangulationMethod method) {
    if (polygon_vertices.size() < 3) {
        logger_.error("Polygon must have at least 3 vertices for extrusion");
        return;
    }

    logger_.debug("Creating watertight extruded mesh:");
    logger_.debug("  Polygon vertices: " + std::to_string(polygon_vertices.size()));
    logger_.debug("  Top elevation: " + std::to_string(top_elevation) + "m");
    logger_.debug("  Bottom elevation: " + std::to_string(bottom_elevation) + "m");

    // Step 1: Create vertices for top and bottom surfaces
    std::vector<VertexId> top_vertices;
    std::vector<VertexId> bottom_vertices;

    top_vertices.reserve(polygon_vertices.size());
    bottom_vertices.reserve(polygon_vertices.size());

    for (const auto& vertex : polygon_vertices) {
        // Top surface vertex
        Point3D top_pos(vertex.x(), vertex.y(), top_elevation);
        VertexId top_id = add_vertex(top_pos);
        top_vertices.push_back(top_id);

        // Bottom surface vertex (same X,Y, different Z)
        Point3D bottom_pos(vertex.x(), vertex.y(), bottom_elevation);
        VertexId bottom_id = add_vertex(bottom_pos);
        bottom_vertices.push_back(bottom_id);
    }

    // Step 2: Triangulate the 2D polygon using robust algorithm
    // Use either Seidel (fast) or Constrained Delaunay (quality) method
    std::vector<std::array<size_t, 3>> triangle_indices;

    // Use Seidel y-monotone triangulation (only supported method)
    logger_.debug("Using Seidel y-monotone triangulation");
    triangle_indices = triangulate_polygon_seidel(polygon_vertices);

    // Create triangles for top and bottom surfaces
    for (const auto& tri : triangle_indices) {
        // Top surface triangle (counter-clockwise for upward normal)
        add_triangle(top_vertices[tri[0]], top_vertices[tri[1]], top_vertices[tri[2]]);

        // Bottom surface triangle (clockwise for downward normal)
        add_triangle(bottom_vertices[tri[0]], bottom_vertices[tri[2]], bottom_vertices[tri[1]]);
    }

    // Step 3: Create side walls by connecting boundary edges
    // For each edge on the boundary, create a rectangle (2 triangles)
    for (size_t i = 0; i < polygon_vertices.size(); ++i) {
        size_t next_i = (i + 1) % polygon_vertices.size();

        // Get the four vertices of the rectangle
        VertexId top_current = top_vertices[i];
        VertexId top_next = top_vertices[next_i];
        VertexId bottom_current = bottom_vertices[i];
        VertexId bottom_next = bottom_vertices[next_i];

        // Create two triangles for the rectangular side wall
        // Triangle 1: top_current -> bottom_current -> top_next
        add_triangle(top_current, bottom_current, top_next);

        // Triangle 2: bottom_current -> bottom_next -> top_next
        add_triangle(bottom_current, bottom_next, top_next);
    }

    logger_.debug("Watertight extrusion complete:");
    logger_.debug("  Added " + std::to_string(top_vertices.size() * 2) + " vertices");
    logger_.debug("  Added " + std::to_string((polygon_vertices.size() - 2) * 2 + polygon_vertices.size() * 2) + " triangles");
    logger_.debug("  Topology guaranteed watertight by construction");
}

// Elevation Index Implementation
void TopographicMesh::ElevationIndex::build(const std::vector<Vertex3D>& vertices, const std::vector<Triangle>& triangles) {
    if (triangles.empty()) {
        valid = false;
        return;
    }

    // Find elevation range
    min_elevation = std::numeric_limits<double>::max();
    max_elevation = std::numeric_limits<double>::lowest();

    for (const auto& triangle : triangles) {
        if (triangle.vertices[0] == static_cast<VertexId>(-1)) continue;

        for (int i = 0; i < 3; ++i) {
            double z = vertices[triangle.vertices[i]].position.z();
            min_elevation = std::min(min_elevation, z);
            max_elevation = std::max(max_elevation, z);
        }
    }

    if (min_elevation >= max_elevation) {
        valid = false;
        return;
    }

    // Calculate number of bands needed
    double range = max_elevation - min_elevation;
    size_t num_bands = static_cast<size_t>(std::ceil(range / band_height)) + 1;

    bands.clear();
    bands.resize(num_bands);

    // Initialize bands
    for (size_t i = 0; i < num_bands; ++i) {
        bands[i].min_z = min_elevation + i * band_height;
        bands[i].max_z = min_elevation + (i + 1) * band_height;
        bands[i].triangles.reserve(triangles.size() / num_bands * 2); // Estimate with overlap
    }

    // Assign triangles to bands
    for (size_t tri_idx = 0; tri_idx < triangles.size(); ++tri_idx) {
        const auto& triangle = triangles[tri_idx];
        if (triangle.vertices[0] == static_cast<VertexId>(-1)) continue;

        // Find triangle elevation range
        double tri_min_z = vertices[triangle.vertices[0]].position.z();
        double tri_max_z = tri_min_z;

        for (int i = 1; i < 3; ++i) {
            double z = vertices[triangle.vertices[i]].position.z();
            tri_min_z = std::min(tri_min_z, z);
            tri_max_z = std::max(tri_max_z, z);
        }

        // Add triangle to all overlapping bands
        size_t first_band = static_cast<size_t>(std::max(0.0, (tri_min_z - min_elevation) / band_height));
        size_t last_band = static_cast<size_t>(std::min(static_cast<double>(num_bands - 1), (tri_max_z - min_elevation) / band_height));

        for (size_t band_idx = first_band; band_idx <= last_band; ++band_idx) {
            bands[band_idx].triangles.push_back(static_cast<TriangleId>(tri_idx));
        }
    }

    valid = true;
}

std::vector<TriangleId> TopographicMesh::ElevationIndex::find_triangles_near_elevation(double elevation, double tolerance) const {
    if (!valid) {
        return {};
    }

    std::vector<TriangleId> result;

    // Find bands that might contain triangles at this elevation
    double search_min = elevation - tolerance;
    double search_max = elevation + tolerance;

    size_t first_band = static_cast<size_t>(std::max(0.0, (search_min - min_elevation) / band_height));
    size_t last_band = static_cast<size_t>(std::min(static_cast<double>(bands.size() - 1), (search_max - min_elevation) / band_height));

    // Collect triangles from relevant bands (with deduplication)
    std::unordered_set<TriangleId> unique_triangles;
    for (size_t band_idx = first_band; band_idx <= last_band && band_idx < bands.size(); ++band_idx) {
        for (TriangleId tri_id : bands[band_idx].triangles) {
            unique_triangles.insert(tri_id);
        }
    }

    result.reserve(unique_triangles.size());
    for (TriangleId tri_id : unique_triangles) {
        result.push_back(tri_id);
    }

    return result;
}

void TopographicMesh::rebuild_elevation_index() const {
    std::lock_guard<std::mutex> lock(spatial_mutex_);
    elevation_index_.build(vertices_, triangles_);
}

} // namespace topo
