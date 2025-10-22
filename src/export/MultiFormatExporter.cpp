/**
 * @file MultiFormatExporter.cpp
 * @brief Implementation of multi-format export system
 */

#include "MultiFormatExporter.hpp"
#include "../core/TopographicMesh.hpp"
#include "SVGExporter.hpp"
#include "../core/ContourGenerator.hpp"
#include "LabelRenderer.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace topo {

// ============================================================================
// ColorMapper Implementation
// ============================================================================

ColorMapper::ColorMapper(Scheme scheme) : scheme_(scheme) {
    set_scheme(scheme);
}

ColorMapper::ColorMapper(const std::vector<ColorStop>& custom_stops) 
    : scheme_(Scheme::CUSTOM), color_stops_(custom_stops) {
    
    // Sort color stops by elevation
    std::sort(color_stops_.begin(), color_stops_.end(),
              [](const ColorStop& a, const ColorStop& b) {
                  return a.elevation < b.elevation;
              });
}

std::vector<Material> ColorMapper::generate_elevation_materials(
    double min_elevation, double max_elevation, int num_bands) const {
    
    std::vector<Material> materials;
    materials.reserve(num_bands);
    
    for (int i = 0; i < num_bands; ++i) {
        double elevation = min_elevation + (max_elevation - min_elevation) * i / (num_bands - 1);
        auto color = map_elevation_to_color(elevation, min_elevation, max_elevation);
        
        std::string material_name = "elevation_" + std::to_string(static_cast<int>(elevation)) + "m";
        Material material(material_name);
        
        // Set diffuse color from elevation mapping
        material.diffuse[0] = color[0];
        material.diffuse[1] = color[1];
        material.diffuse[2] = color[2];
        
        // Set ambient to slightly darker version of diffuse
        material.ambient[0] = color[0] * 0.3f;
        material.ambient[1] = color[1] * 0.3f;
        material.ambient[2] = color[2] * 0.3f;
        
        // Set specular to white with low intensity
        material.specular[0] = 0.1f;
        material.specular[1] = 0.1f;
        material.specular[2] = 0.1f;
        
        // Low shininess for terrain
        material.shininess = 10.0f;
        
        materials.push_back(material);
    }
    
    return materials;
}

std::array<float, 3> ColorMapper::map_elevation_to_color(double elevation,
                                                        double min_elevation,
                                                        double max_elevation) const {
    if (color_stops_.empty()) {
        // Default grayscale
        float intensity = static_cast<float>((elevation - min_elevation) / (max_elevation - min_elevation));
        intensity = std::clamp(intensity, 0.0f, 1.0f);
        return {intensity, intensity, intensity};
    }

    // Normalize elevation to [0, 1] range to match color stop positions
    double normalized_elev = (elevation - min_elevation) / (max_elevation - min_elevation);
    normalized_elev = std::clamp(normalized_elev, 0.0, 1.0);

    // Find surrounding color stops based on normalized elevation
    for (size_t i = 0; i < color_stops_.size() - 1; ++i) {
        if (normalized_elev >= color_stops_[i].elevation && normalized_elev <= color_stops_[i + 1].elevation) {
            double t = (normalized_elev - color_stops_[i].elevation) /
                      (color_stops_[i + 1].elevation - color_stops_[i].elevation);

            // Linear interpolation between colors
            std::array<float, 3> result;
            for (int c = 0; c < 3; ++c) {
                result[c] = static_cast<float>(
                    color_stops_[i].color[c] * (1.0 - t) + color_stops_[i + 1].color[c] * t
                );
            }
            return result;
        }
    }

    // Fallback: use closest color
    if (normalized_elev <= color_stops_[0].elevation) {
        return color_stops_[0].color;
    } else {
        return color_stops_.back().color;
    }
}

void ColorMapper::set_scheme(Scheme scheme) {
    scheme_ = scheme;
    color_stops_.clear();
    
    switch (scheme) {
        case Scheme::TERRAIN:
            // Brown (low) -> Green -> White (high)
            color_stops_ = {
                {0.0, {0.4f, 0.2f, 0.1f}},     // Brown
                {0.3, {0.2f, 0.6f, 0.1f}},     // Green
                {0.7, {0.8f, 0.8f, 0.6f}},     // Beige
                {1.0, {1.0f, 1.0f, 1.0f}}      // White
            };
            break;
            
        case Scheme::RAINBOW:
            // Full spectrum
            color_stops_ = {
                {0.0, {0.5f, 0.0f, 1.0f}},     // Purple
                {0.2, {0.0f, 0.0f, 1.0f}},     // Blue
                {0.4, {0.0f, 1.0f, 1.0f}},     // Cyan
                {0.6, {0.0f, 1.0f, 0.0f}},     // Green
                {0.8, {1.0f, 1.0f, 0.0f}},     // Yellow
                {1.0, {1.0f, 0.0f, 0.0f}}      // Red
            };
            break;
            
        case Scheme::GRAYSCALE:
            color_stops_ = {
                {0.0, {0.0f, 0.0f, 0.0f}},
                {1.0, {1.0f, 1.0f, 1.0f}}
            };
            break;
            
        default:
            // Default to grayscale
            set_scheme(Scheme::GRAYSCALE);
            break;
    }
}

// ============================================================================
// STLExporter Implementation  
// ============================================================================

bool STLExporter::export_stl(const TopographicMesh& mesh, const std::string& filename) {
    if (options_.binary_format) {
        return write_binary_stl(mesh, filename);
    } else {
        return write_ascii_stl(mesh, filename);
    }
}

bool STLExporter::write_ascii_stl(const TopographicMesh& mesh, const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << std::fixed << std::setprecision(6);
    file << "solid " << (options_.solid_name.empty() ? "topographic_model" : options_.solid_name) << std::endl;

    // Calculate scaling for coordinate transformation
    double scale_factor = calculate_scale_factor(mesh);

    // Calculate mesh bounding box for centering
    BoundingBox bbox;
    if (mesh.num_vertices() > 0) {
        const auto& first_vertex = mesh.get_vertex(0);
        bbox.min_x = bbox.max_x = first_vertex.position.x();
        bbox.min_y = bbox.max_y = first_vertex.position.y();
        bbox.min_z = bbox.max_z = first_vertex.position.z();

        for (size_t i = 1; i < mesh.num_vertices(); ++i) {
            const auto& vertex = mesh.get_vertex(static_cast<VertexId>(i));
            bbox.min_x = std::min(bbox.min_x, vertex.position.x());
            bbox.max_x = std::max(bbox.max_x, vertex.position.x());
            bbox.min_y = std::min(bbox.min_y, vertex.position.y());
            bbox.max_y = std::max(bbox.max_y, vertex.position.y());
            bbox.min_z = std::min(*bbox.min_z, vertex.position.z());
            bbox.max_z = std::max(*bbox.max_z, vertex.position.z());
        }
    }

    for (size_t i = 0; i < mesh.num_triangles(); ++i) {
        const auto& triangle = mesh.get_triangle(static_cast<TriangleId>(i));

        // Skip invalid triangles
        if (triangle.vertices[0] == static_cast<VertexId>(-1)) {
            continue;
        }

        // Validate vertex indices before accessing vertices
        if (triangle.vertices[0] >= mesh.num_vertices() ||
            triangle.vertices[1] >= mesh.num_vertices() ||
            triangle.vertices[2] >= mesh.num_vertices()) {
            continue; // Skip invalid triangle
        }

        const auto& v0 = mesh.get_vertex(triangle.vertices[0]);
        const auto& v1 = mesh.get_vertex(triangle.vertices[1]);
        const auto& v2 = mesh.get_vertex(triangle.vertices[2]);

        // Apply scaling and centering to vertices
        Point3D scaled_v0 = scale_vertex(v0.position, bbox, scale_factor);
        Point3D scaled_v1 = scale_vertex(v1.position, bbox, scale_factor);
        Point3D scaled_v2 = scale_vertex(v2.position, bbox, scale_factor);

        // Calculate normal
        auto edge1 = Point3D(
            v1.position.x() - v0.position.x(),
            v1.position.y() - v0.position.y(),
            v1.position.z() - v0.position.z()
        );
        auto edge2 = Point3D(
            v2.position.x() - v0.position.x(),
            v2.position.y() - v0.position.y(),
            v2.position.z() - v0.position.z()
        );
        
        // Cross product for normal
        Point3D normal(
            edge1.y() * edge2.z() - edge1.z() * edge2.y(),
            edge1.z() * edge2.x() - edge1.x() * edge2.z(),
            edge1.x() * edge2.y() - edge1.y() * edge2.x()
        );
        
        // Normalize
        double length = std::sqrt(normal.x()*normal.x() + normal.y()*normal.y() + normal.z()*normal.z());
        if (length > 1e-12) {
            normal = Point3D(normal.x()/length, normal.y()/length, normal.z()/length);
        }
        
        file << "  facet normal " << normal.x() << " " << normal.y() << " " << normal.z() << std::endl;
        file << "    outer loop" << std::endl;
        file << "      vertex " << scaled_v0.x() << " " << scaled_v0.y() << " " << scaled_v0.z() << std::endl;
        file << "      vertex " << scaled_v1.x() << " " << scaled_v1.y() << " " << scaled_v1.z() << std::endl;
        file << "      vertex " << scaled_v2.x() << " " << scaled_v2.y() << " " << scaled_v2.z() << std::endl;
        file << "    endloop" << std::endl;
        file << "  endfacet" << std::endl;
    }
    
    file << "endsolid " << (options_.solid_name.empty() ? "topographic_model" : options_.solid_name) << std::endl;
    return true;
}

bool STLExporter::write_binary_stl(const TopographicMesh& mesh, const std::string& filename) const {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Write 80-byte header
    char header[80] = {0};
    std::string header_text = "Binary STL - Topographic Generator";
    std::strncpy(header, header_text.c_str(), std::min(header_text.length(), size_t(79)));
    file.write(header, 80);
    
    // Count valid triangles
    uint32_t triangle_count = 0;
    for (size_t i = 0; i < mesh.num_triangles(); ++i) {
        const auto& triangle = mesh.get_triangle(static_cast<TriangleId>(i));
        if (triangle.vertices[0] != static_cast<VertexId>(-1)) {
            triangle_count++;
        }
    }
    
    // Write triangle count
    file.write(reinterpret_cast<const char*>(&triangle_count), sizeof(uint32_t));

    // Calculate scaling for coordinate transformation
    double scale_factor = calculate_scale_factor(mesh);

    // Calculate mesh bounding box for centering
    BoundingBox bbox;
    if (mesh.num_vertices() > 0) {
        const auto& first_vertex = mesh.get_vertex(0);
        bbox.min_x = bbox.max_x = first_vertex.position.x();
        bbox.min_y = bbox.max_y = first_vertex.position.y();
        bbox.min_z = bbox.max_z = first_vertex.position.z();

        for (size_t i = 1; i < mesh.num_vertices(); ++i) {
            const auto& vertex = mesh.get_vertex(static_cast<VertexId>(i));
            bbox.min_x = std::min(bbox.min_x, vertex.position.x());
            bbox.max_x = std::max(bbox.max_x, vertex.position.x());
            bbox.min_y = std::min(bbox.min_y, vertex.position.y());
            bbox.max_y = std::max(bbox.max_y, vertex.position.y());
            bbox.min_z = std::min(*bbox.min_z, vertex.position.z());
            bbox.max_z = std::max(*bbox.max_z, vertex.position.z());
        }
    }

    // Write triangles
    for (size_t i = 0; i < mesh.num_triangles(); ++i) {
        const auto& triangle = mesh.get_triangle(static_cast<TriangleId>(i));

        // Skip invalid triangles
        if (triangle.vertices[0] == static_cast<VertexId>(-1)) {
            continue;
        }

        // Validate vertex indices before accessing vertices
        if (triangle.vertices[0] >= mesh.num_vertices() ||
            triangle.vertices[1] >= mesh.num_vertices() ||
            triangle.vertices[2] >= mesh.num_vertices()) {
            continue; // Skip invalid triangle
        }

        const auto& v0 = mesh.get_vertex(triangle.vertices[0]);
        const auto& v1 = mesh.get_vertex(triangle.vertices[1]);
        const auto& v2 = mesh.get_vertex(triangle.vertices[2]);

        // Apply scaling and centering to vertices
        Point3D scaled_v0 = scale_vertex(v0.position, bbox, scale_factor);
        Point3D scaled_v1 = scale_vertex(v1.position, bbox, scale_factor);
        Point3D scaled_v2 = scale_vertex(v2.position, bbox, scale_factor);

        // Calculate and write normal (12 bytes)
        float normal[3] = {0.0f, 0.0f, 1.0f};  // Simplified normal
        file.write(reinterpret_cast<const char*>(normal), 12);

        // Write scaled vertices (36 bytes)
        float vertices[9] = {
            static_cast<float>(scaled_v0.x()), static_cast<float>(scaled_v0.y()), static_cast<float>(scaled_v0.z()),
            static_cast<float>(scaled_v1.x()), static_cast<float>(scaled_v1.y()), static_cast<float>(scaled_v1.z()),
            static_cast<float>(scaled_v2.x()), static_cast<float>(scaled_v2.y()), static_cast<float>(scaled_v2.z())
        };
        file.write(reinterpret_cast<const char*>(vertices), 36);
        
        // Write attribute byte count (2 bytes)
        uint16_t attributes = 0;
        file.write(reinterpret_cast<const char*>(&attributes), 2);
    }
    
    return true;
}

// ============================================================================
// STLExporter Scaling Methods Implementation
// ============================================================================

double STLExporter::calculate_scale_factor(const TopographicMesh& mesh) const {
    if (!options_.auto_scale) {
        return options_.scale_factor;
    }

    // Calculate mesh bounding box
    if (mesh.num_vertices() == 0) {
        return 1.0;
    }

    const auto& first_vertex = mesh.get_vertex(0);
    double min_x = first_vertex.position.x();
    double max_x = min_x;
    double min_y = first_vertex.position.y();
    double max_y = min_y;
    double min_z = first_vertex.position.z();
    double max_z = min_z;

    for (size_t i = 1; i < mesh.num_vertices(); ++i) {
        const auto& vertex = mesh.get_vertex(static_cast<VertexId>(i));
        min_x = std::min(min_x, vertex.position.x());
        max_x = std::max(max_x, vertex.position.x());
        min_y = std::min(min_y, vertex.position.y());
        max_y = std::max(max_y, vertex.position.y());
        min_z = std::min(min_z, vertex.position.z());
        max_z = std::max(max_z, vertex.position.z());
    }

    // Calculate the largest dimension in the XY plane
    double range_x = max_x - min_x;
    double range_y = max_y - min_y;
    double max_range = std::max(range_x, range_y);

    // Avoid division by zero
    if (max_range <= 0.0) {
        return 1.0;
    }

    // Scale to fit within 90% of the target bed size (leaving 10% margin)
    double target_size = options_.target_bed_size_mm * 0.9;
    return target_size / max_range;
}

Point3D STLExporter::scale_vertex(const Point3D& vertex, const BoundingBox& bbox, double scale_factor) const {
    // Center the vertex relative to the bounding box center
    double center_x = (bbox.min_x + bbox.max_x) / 2.0;
    double center_y = (bbox.min_y + bbox.max_y) / 2.0;
    double center_z = bbox.min_z.has_value() && bbox.max_z.has_value() ?
                      (*bbox.min_z + *bbox.max_z) / 2.0 : 0.0;

    // Apply uniform scaling to X, Y, and Z coordinates
    // This ensures no vertical exaggeration - Z is scaled the same as XY
    // XYZ coordinates from mesh generation are all in meters (UTM projection)
    double scaled_x = (vertex.x() - center_x) * scale_factor;
    double scaled_y = (vertex.y() - center_y) * scale_factor;
    double scaled_z = (vertex.z() - center_z) * scale_factor;

    return Point3D(scaled_x, scaled_y, scaled_z);
}

// ============================================================================
// MultiFormatExporter Implementation
// ============================================================================

bool MultiFormatExporter::export_all_formats(const TopographicMesh& mesh, 
                                            const std::vector<std::string>& formats) {
    bool success = true;
    
    // Ensure output directory exists
    std::filesystem::create_directories(global_options_.output_directory);
    
    // Process formats in order of complexity: SVG (simple) → STL/OBJ/PLY (complex)
    // This ensures useful output files are generated before potential crashes
    std::vector<std::string> ordered_formats;

    // Add SVG first (simplest, least likely to crash)
    for (const auto& format : formats) {
        if (format == "svg") {
            ordered_formats.push_back(format);
        }
    }

    // Add 3D formats in order of complexity
    for (const auto& format : formats) {
        if (format == "stl" || format == "obj" || format == "ply") {
            ordered_formats.push_back(format);
        }
    }

    // Add any other formats not handled above
    for (const auto& format : formats) {
        if (format != "svg" && format != "stl" && format != "obj" && format != "ply") {
            ordered_formats.push_back(format);
        }
    }

    for (const auto& format : ordered_formats) {
        if (format == "svg") {
            // SVG format requires contour data which is not available at this level
            // SVG export should be handled by TopographicGenerator with contour layers
            if (global_options_.verbose) {
                std::cerr << "Warning: SVG export skipped - requires contour data from TopographicGenerator" << std::endl;
            }
            // Return success but don't create placeholder files

        } else if (format == "stl") {
            // Generate filename (for stacked 3D files, no layer number)
            std::string filename_no_ext = global_options_.filename_pattern.empty() ?
                global_options_.base_filename :
                LabelRenderer::substitute_filename_pattern(
                    global_options_.filename_pattern,
                    global_options_.base_filename,
                    0,  // No layer number for stacked files
                    0.0 // No specific elevation for stacked files
                );
            std::string filename = global_options_.output_directory + "/" + filename_no_ext + ".stl";
            STLExporter stl_exporter(stl_options_);
            success &= stl_exporter.export_stl(mesh, filename);

        } else if (format == "obj") {
            std::string filename = global_options_.output_directory + "/" +
                                  global_options_.base_filename + ".obj";
            OBJExporter obj_exporter;
            success &= obj_exporter.export_obj(mesh, filename);

        } else if (format == "ply") {
            std::string filename = global_options_.output_directory + "/" +
                                  global_options_.base_filename + ".ply";
            PLYExporter ply_exporter;
            success &= ply_exporter.export_mesh(mesh, filename);

        // } else if (format == "nurbs") {
        //     std::string base_path = global_options_.output_directory + "/" +
        //                            global_options_.base_filename;
        //     NURBSExporter nurbs_exporter;
        //     success &= nurbs_exporter.convert_mesh_to_nurbs(mesh);
        //     success &= nurbs_exporter.export_all_formats(base_path);

        } else if (format == "png" || format == "geojson" || format == "geotiff" || format == "shapefile") {
            // These formats require contour layers, not mesh data
            // They must be handled by TopographicGenerator with contour data
            if (global_options_.verbose) {
                std::cerr << "Note: " << format << " export requires contour data - handled separately by TopographicGenerator" << std::endl;
            }
            // Don't mark as failure - just skip (will be handled elsewhere)
        } else {
            std::cerr << "Unknown format: " << format << std::endl;
            success = false;
        }
    }
    
    return success;
}

// ============================================================================
// MultiFormatExporter Implementation
// ============================================================================

MultiFormatExporter::MultiFormatExporter(const GlobalOptions& opts) 
    : global_options_(opts), last_report_{} {
    // Initialize default export options for all formats
    stl_options_ = STLExporter::Options();
    obj_options_ = OBJExporter::Options(); 
    ply_options_ = PLYExporter::Options();
    nurbs_options_ = NURBSExporter::Options();
}

// ============================================================================
// STLExporter Implementation
// ============================================================================

bool STLExporter::export_mesh(const TopographicMesh& mesh, const std::string& filename) const {
    if (options_.binary_format) {
        return write_binary_stl(mesh, filename);
    } else {
        return write_ascii_stl(mesh, filename);
    }
}

// ============================================================================
// Legacy compatibility method implementations
// ============================================================================

// STLExporter legacy methods (non-const versions that call const implementations)
bool STLExporter::write_ascii_stl(const TopographicMesh& mesh, const std::string& filename) {
    // Call the const implementation
    const STLExporter* const_this = this;
    return const_this->write_ascii_stl(mesh, filename);
}

bool STLExporter::write_binary_stl(const TopographicMesh& mesh, const std::string& filename) {
    // Call the const implementation
    const STLExporter* const_this = this;
    return const_this->write_binary_stl(mesh, filename);
}

// OBJExporter legacy methods
bool OBJExporter::export_obj(const TopographicMesh& mesh, const std::string& filename) const {
    return export_mesh(mesh, filename);
}

// MultiFormatExporter const version implementation
bool MultiFormatExporter::export_all_formats(const TopographicMesh& mesh,
                                            const std::vector<std::string>& requested_formats) const {
    // This is the const version - delegate to non-const version via const_cast
    MultiFormatExporter* non_const_this = const_cast<MultiFormatExporter*>(this);
    return non_const_this->export_all_formats(mesh, requested_formats);
}

} // namespace topo