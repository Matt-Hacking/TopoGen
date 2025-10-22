#pragma once

/**
 * @file ElevationProcessor.hpp
 * @brief High-performance elevation data processing using GDAL
 */

#include "topographic_generator.hpp"
#include "ExecutionPolicies.hpp"
#include <memory>

namespace topo {

/**
 * @brief Sampling grid parameters for elevation data
 */
struct Grid {
    double min_x, max_x, min_y, max_y;
    size_t width, height;

    Grid(double minx, double miny, double maxx, double maxy, size_t w, size_t h)
        : min_x(minx), max_x(maxx), min_y(miny), max_y(maxy), width(w), height(h) {}
};

// ContourStrategy enum moved to topographic_generator.hpp to avoid duplicate definitions


/**
 * @brief High-performance elevation data processor
 * 
 * This class handles loading, processing, and sampling of elevation data
 * from various sources (SRTM tiles, DEM files) using GDAL for optimal
 * performance and format support.
 */
class ElevationProcessor {
public:
    ElevationProcessor();
    ~ElevationProcessor();
    
    // Data loading
    bool load_elevation_tiles(const BoundingBox& bbox);
    bool load_elevation_file(const std::string& filename);
    
    // Elevation sampling
    std::vector<Point3D> sample_elevation_points(const Grid& sampling_grid) const;
    
    template<typename ExecutionPolicy>
    std::vector<Point3D> sample_elevation_points_parallel(
        ExecutionPolicy&& policy, const Grid& sampling_grid) const;
    
    // Elevation queries
    double interpolate_elevation(double x, double y) const;
    std::pair<double, double> get_elevation_range() const;
    
    // Contour level generation
    std::vector<double> generate_contour_levels(
        int num_layers, ContourStrategy strategy = ContourStrategy::UNIFORM) const;

    // Raw elevation data access for contour generation
    const float* get_elevation_data() const;
    std::pair<size_t, size_t> get_elevation_dimensions() const;
    std::vector<double> get_geotransform() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace topo