/**
 * @file HeightmapTriangulator.hpp
 * @brief Direct heightmap-to-mesh triangulation without intermediate polygon conversion
 *
 * Implements grid-based triangulation directly from raster elevation data using
 * GDAL geotransform for coordinate mapping. This approach eliminates the need for
 * CGAL polygon boolean operations and provides a simpler, more robust pipeline.
 *
 * Algorithm inspired by:
 * - phstl.py by Jim Newsom (https://github.com/anoved/phstl)
 * - dem2stl.py by cvr (https://github.com/cvr/dem2stl)
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#pragma once

#include "TopographicMesh.hpp"
#include "Logger.hpp"
#include <gdal_priv.h>
#include <vector>
#include <optional>

namespace topo {

/**
 * @brief Configuration for heightmap triangulation
 */
struct HeightmapTriangulationConfig {
    double base_height_mm = 5.0;                  ///< Base platform thickness in millimeters
    double vertical_scale = 1.0;                  ///< Z-axis vertical exaggeration factor
    double nodata_value = -32768.0;               ///< NoData value to skip
    bool create_base_platform = true;             ///< Add base platform triangles at z=0
    bool create_side_walls = true;                ///< Add vertical wall triangles around edges
    bool flip_normals = false;                    ///< Flip triangle winding for inside-out models
    std::optional<double> min_elevation;          ///< Clip elevations below this value
    std::optional<double> max_elevation;          ///< Clip elevations above this value
    bool verbose = false;                         ///< Verbose logging output
    std::optional<double> center_lon;             ///< Center longitude for geographic-to-meters projection
    std::optional<double> center_lat;             ///< Center latitude for geographic-to-meters projection
    bool contour_mode = true;                     ///< True for flat contour layers (laser cutting), false for terrain-following (3D printing)
};

/**
 * @brief Statistics from heightmap triangulation operation
 */
struct HeightmapTriangulationStats {
    size_t grid_width = 0;                        ///< Width of elevation grid
    size_t grid_height = 0;                       ///< Height of elevation grid
    size_t surface_triangles = 0;                 ///< Number of surface triangles created
    size_t base_triangles = 0;                    ///< Number of base platform triangles
    size_t wall_triangles = 0;                    ///< Number of side wall triangles
    size_t skipped_nodata = 0;                    ///< Number of cells skipped due to NoData
    double min_elevation = 0.0;                   ///< Minimum elevation in data
    double max_elevation = 0.0;                   ///< Maximum elevation in data
    double geotransform[6] = {0};                 ///< GDAL geotransform parameters
    std::chrono::milliseconds computation_time{0}; ///< Total computation time
};

/**
 * @brief Direct heightmap-to-mesh triangulation
 *
 * Converts GDAL raster elevation data directly into a triangulated mesh suitable
 * for STL export. Creates 2 triangles per grid cell using a sliding window approach
 * for memory efficiency.
 *
 * Key features:
 * - No intermediate polygon conversion
 * - Uses GDAL geotransform for coordinate mapping
 * - Handles NoData values gracefully
 * - Memory-efficient sliding window processing
 * - Optional base platform and side walls for 3D printing
 */
class HeightmapTriangulator {
public:
    /**
     * @brief Constructor
     * @param config Triangulation configuration
     */
    explicit HeightmapTriangulator(const HeightmapTriangulationConfig& config = HeightmapTriangulationConfig{});

    /**
     * @brief Set the logger for diagnostic output
     * @param logger Logger instance
     */
    void set_logger(const Logger& logger);

    /**
     * @brief Triangulate elevation data from GDAL raster band
     *
     * @param dataset GDAL dataset containing elevation data
     * @param band_number Band number to read (1-indexed, default = 1)
     * @return Triangulated mesh ready for export
     */
    TopographicMesh triangulate_from_dataset(GDALDataset* dataset, int band_number = 1);

    /**
     * @brief Triangulate elevation data from raw array
     *
     * @param elevation_data Raw elevation values as 2D array (row-major)
     * @param width Width of elevation grid
     * @param height Height of elevation grid
     * @param geotransform GDAL geotransform array [6 elements]
     * @return Triangulated mesh ready for export
     */
    TopographicMesh triangulate_from_array(
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform,
        std::optional<double> layer_min_elev = std::nullopt,
        std::optional<double> layer_max_elev = std::nullopt
    );

    /**
     * @brief Triangulate elevation data with layer slicing
     *
     * Creates multiple mesh layers at different elevation bands for laser cutting.
     *
     * @param elevation_data Raw elevation values as 2D array (row-major)
     * @param width Width of elevation grid
     * @param height Height of elevation grid
     * @param geotransform GDAL geotransform array [6 elements]
     * @param num_layers Number of elevation layers to create
     * @return Vector of meshes, one per layer
     */
    std::vector<TopographicMesh> triangulate_layers(
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform,
        int num_layers
    );

    /**
     * @brief Get statistics from last triangulation operation
     */
    const HeightmapTriangulationStats& get_stats() const { return stats_; }

    /**
     * @brief Update configuration
     */
    void set_config(const HeightmapTriangulationConfig& config) { config_ = config; }

    /**
     * @brief Get current configuration
     */
    const HeightmapTriangulationConfig& get_config() const { return config_; }

private:
    HeightmapTriangulationConfig config_;
    Logger logger_;
    HeightmapTriangulationStats stats_;

    /**
     * @brief Core triangulation implementation
     *
     * Implements the sliding window algorithm:
     * - Processes 2 rows at a time for memory efficiency
     * - Creates 2 triangles per grid cell from 4 corner points
     * - Uses GDAL geotransform for coordinate conversion
     */
    TopographicMesh triangulate_core(
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform
    );

    /**
     * @brief Add surface triangles for a single grid cell
     *
     * Creates 2 triangles from the 4 corners of a grid cell:
     * - Triangle 1: (i,j) → (i+1,j) → (i,j+1)
     * - Triangle 2: (i+1,j) → (i+1,j+1) → (i,j+1)
     *
     * @param mesh Output mesh
     * @param i Column index
     * @param j Row index
     * @param z00 Elevation at (i, j)
     * @param z10 Elevation at (i+1, j)
     * @param z01 Elevation at (i, j+1)
     * @param z11 Elevation at (i+1, j+1)
     * @param geotransform GDAL geotransform
     */
    void add_cell_triangles(
        TopographicMesh& mesh,
        size_t i, size_t j,
        double z00, double z10, double z01, double z11,
        const double* geotransform
    );

    /**
     * @brief Add base platform triangles at z=0
     *
     * Creates a flat base platform by projecting all grid points to z=0
     * (or config.base_height_mm if set).
     */
    void add_base_platform(
        TopographicMesh& mesh,
        size_t width,
        size_t height,
        const double* geotransform
    );

    /**
     * @brief Add vertical side wall triangles
     *
     * Creates vertical walls around the perimeter connecting surface to base.
     */
    void add_side_walls(
        TopographicMesh& mesh,
        const float* elevation_data,
        size_t width,
        size_t height,
        const double* geotransform
    );

    /**
     * @brief Check if elevation value is NoData
     */
    bool is_nodata(double value) const;

    /**
     * @brief Apply elevation clipping if configured
     */
    double clip_elevation(double elevation) const;

    /**
     * @brief Transform pixel coordinates to real-world coordinates using geotransform
     *
     * Applies GDAL geotransform:
     * X = geotransform[0] + col * geotransform[1] + row * geotransform[2]
     * Y = geotransform[3] + col * geotransform[4] + row * geotransform[5]
     *
     * @param col Column (pixel x)
     * @param row Row (pixel y)
     * @param geotransform GDAL geotransform array
     * @return (X, Y) coordinates in real-world space
     */
    std::pair<double, double> apply_geotransform(
        double col,
        double row,
        const double* geotransform
    ) const;

    /**
     * @brief Calculate elevation statistics for the dataset
     */
    void calculate_elevation_stats(
        const float* elevation_data,
        size_t width,
        size_t height
    );
};

} // namespace topo
