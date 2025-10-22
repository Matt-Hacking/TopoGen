/**
 * @file HeightmapTriangulator.cpp
 * @brief Implementation of direct heightmap-to-mesh triangulation
 */

#include "HeightmapTriangulator.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>

namespace topo {

// ============================================================================
// Constructor and Configuration
// ============================================================================

HeightmapTriangulator::HeightmapTriangulator(const HeightmapTriangulationConfig& config)
    : config_(config), logger_("HeightmapTriangulator") {
}

void HeightmapTriangulator::set_logger(const Logger& logger) {
    logger_.setLogLevel(logger.getLogLevel());
}

// ============================================================================
// Public Triangulation Methods
// ============================================================================

TopographicMesh HeightmapTriangulator::triangulate_from_dataset(
    GDALDataset* dataset,
    int band_number) {

    if (!dataset) {
        logger_.error("Invalid GDAL dataset pointer");
        return TopographicMesh();
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Get raster dimensions
    int width = dataset->GetRasterXSize();
    int height = dataset->GetRasterYSize();

    if (width <= 0 || height <= 0) {
        logger_.error("Invalid raster dimensions: " + std::to_string(width) + "x" + std::to_string(height));
        return TopographicMesh();
    }

    // Get geotransform
    double geotransform[6];
    if (dataset->GetGeoTransform(geotransform) != CE_None) {
        logger_.warning("No geotransform found, using identity transform");
        geotransform[0] = 0.0;   // X origin
        geotransform[1] = 1.0;   // X pixel size
        geotransform[2] = 0.0;   // X rotation
        geotransform[3] = 0.0;   // Y origin
        geotransform[4] = 0.0;   // Y rotation
        geotransform[5] = 1.0;   // Y pixel size
    }

    // Get raster band
    GDALRasterBand* band = dataset->GetRasterBand(band_number);
    if (!band) {
        logger_.error("Failed to get raster band " + std::to_string(band_number));
        return TopographicMesh();
    }

    // Get NoData value if set
    int has_nodata = 0;
    double nodata_value = band->GetNoDataValue(&has_nodata);
    if (has_nodata) {
        config_.nodata_value = nodata_value;
        logger_.info("Using NoData value: " + std::to_string(nodata_value));
    }

    // Read elevation data
    std::vector<float> elevation_data(width * height);
    CPLErr err = band->RasterIO(
        GF_Read,
        0, 0,                    // X/Y offset
        width, height,           // Read size
        elevation_data.data(),   // Output buffer
        width, height,           // Buffer size
        GDT_Float32,             // Data type
        0, 0                     // Line spacing (0 = default)
    );

    if (err != CE_None) {
        logger_.error("Failed to read raster data: " + std::string(CPLGetLastErrorMsg()));
        return TopographicMesh();
    }

    logger_.info("Read elevation data: " + std::to_string(width) + "x" + std::to_string(height) + " pixels");

    // Triangulate
    TopographicMesh mesh = triangulate_from_array(
        elevation_data.data(),
        width,
        height,
        geotransform
    );

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.computation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    logger_.info("Triangulation completed in " + std::to_string(stats_.computation_time.count()) + " ms");
    logger_.info("Created " + std::to_string(stats_.surface_triangles) + " surface triangles");

    return mesh;
}

TopographicMesh HeightmapTriangulator::triangulate_from_array(
    const float* elevation_data,
    size_t width,
    size_t height,
    const double* geotransform,
    std::optional<double> layer_min_elev,
    std::optional<double> layer_max_elev) {

    auto start_time = std::chrono::high_resolution_clock::now();

    // Temporarily override config with layer bounds if provided
    // This allows layer-specific filtering without double-applying user's --min-elevation
    auto saved_min = config_.min_elevation;
    auto saved_max = config_.max_elevation;

    if (layer_min_elev.has_value()) {
        config_.min_elevation = layer_min_elev;
    }
    if (layer_max_elev.has_value()) {
        config_.max_elevation = layer_max_elev;
    }

    // Calculate elevation statistics
    calculate_elevation_stats(elevation_data, width, height);

    // Core triangulation
    TopographicMesh mesh = triangulate_core(elevation_data, width, height, geotransform);

    // Add base platform if requested
    if (config_.create_base_platform) {
        add_base_platform(mesh, width, height, geotransform);
        logger_.info("Added " + std::to_string(stats_.base_triangles) + " base platform triangles");
    }

    // Add side walls if requested
    if (config_.create_side_walls) {
        add_side_walls(mesh, elevation_data, width, height, geotransform);
        logger_.info("Added " + std::to_string(stats_.wall_triangles) + " side wall triangles");
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats_.computation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    logger_.info("Heightmap triangulation completed:");
    logger_.info("  Grid: " + std::to_string(width) + "x" + std::to_string(height));
    logger_.info("  Surface triangles: " + std::to_string(stats_.surface_triangles));
    logger_.info("  Base triangles: " + std::to_string(stats_.base_triangles));
    logger_.info("  Wall triangles: " + std::to_string(stats_.wall_triangles));
    logger_.info("  Skipped NoData: " + std::to_string(stats_.skipped_nodata));
    logger_.info("  Elevation range: " + std::to_string(stats_.min_elevation) + " to " + std::to_string(stats_.max_elevation));
    logger_.info("  Time: " + std::to_string(stats_.computation_time.count()) + " ms");

    // Restore original config values
    config_.min_elevation = saved_min;
    config_.max_elevation = saved_max;

    return mesh;
}

std::vector<TopographicMesh> HeightmapTriangulator::triangulate_layers(
    const float* elevation_data,
    size_t width,
    size_t height,
    const double* geotransform,
    int num_layers) {

    // Calculate elevation statistics
    calculate_elevation_stats(elevation_data, width, height);

    // Calculate layer elevations
    double elev_range = stats_.max_elevation - stats_.min_elevation;
    double layer_interval = elev_range / num_layers;

    logger_.info("Creating " + std::to_string(num_layers) + " layers with interval " + std::to_string(layer_interval));

    std::vector<TopographicMesh> layers;
    layers.reserve(num_layers);

    for (int layer = 0; layer < num_layers; ++layer) {
        double layer_min_elev = stats_.min_elevation + layer * layer_interval;
        double layer_max_elev = stats_.min_elevation + (layer + 1) * layer_interval;

        logger_.info("Creating layer " + std::to_string(layer + 1) + "/" + std::to_string(num_layers) +
                    ": elevation range " + std::to_string(layer_min_elev) + "m to " + std::to_string(layer_max_elev) + "m");

        // Triangulate this layer with layer-specific bounds
        // NOTE: Passing layer bounds as parameters prevents double-application of user's --min-elevation
        TopographicMesh layer_mesh = triangulate_from_array(
            elevation_data, width, height, geotransform,
            layer_min_elev, layer_max_elev);

        // Log layer statistics
        logger_.info("  Layer " + std::to_string(layer + 1) + " complete: " +
                    std::to_string(layer_mesh.num_triangles()) + " triangles, " +
                    std::to_string(layer_mesh.num_vertices()) + " vertices");

        layers.push_back(std::move(layer_mesh));
    }

    return layers;
}

// ============================================================================
// Core Triangulation Implementation
// ============================================================================

TopographicMesh HeightmapTriangulator::triangulate_core(
    const float* elevation_data,
    size_t width,
    size_t height,
    const double* geotransform) {

    TopographicMesh mesh;

    // Store geotransform in stats
    std::copy(geotransform, geotransform + 6, stats_.geotransform);
    stats_.grid_width = width;
    stats_.grid_height = height;
    stats_.surface_triangles = 0;
    stats_.skipped_nodata = 0;

    // Reserve space for vertices and triangles
    // Maximum possible: width * height vertices, 2 * (width-1) * (height-1) triangles
    size_t max_triangles = 2 * (width - 1) * (height - 1);
    mesh.reserve_vertices(width * height);
    mesh.reserve_triangles(max_triangles);

    logger_.info("Starting grid triangulation: " + std::to_string(width) + "x" + std::to_string(height) + " cells");
    if (config_.verbose) {
        logger_.debug("Triangulation mode: " + std::string(config_.contour_mode ? "CONTOUR (flat layers)" : "TERRAIN-FOLLOWING (3D surface)"));
    }

    // Iterate over grid cells (not pixels)
    // Each cell (i,j) has corners at (i,j), (i+1,j), (i,j+1), (i+1,j+1)
    for (size_t j = 0; j < height - 1; ++j) {
        for (size_t i = 0; i < width - 1; ++i) {
            // Get 4 corner elevations
            size_t idx00 = j * width + i;
            size_t idx10 = j * width + (i + 1);
            size_t idx01 = (j + 1) * width + i;
            size_t idx11 = (j + 1) * width + (i + 1);

            double z00 = elevation_data[idx00];
            double z10 = elevation_data[idx10];
            double z01 = elevation_data[idx01];
            double z11 = elevation_data[idx11];

            // Skip cell if any corner is NoData
            if (is_nodata(z00) || is_nodata(z10) || is_nodata(z01) || is_nodata(z11)) {
                ++stats_.skipped_nodata;
                continue;
            }

            // Determine Z values based on mode
            double final_z00, final_z10, final_z01, final_z11;

            if (config_.contour_mode) {
                // === CONTOUR MODE (flat layers for laser cutting) ===
                // For contour mode, each layer shows terrain above min_elevation
                // with a flat top surface at max_elevation

                if (config_.min_elevation.has_value()) {
                    // Find max elevation in this cell
                    double cell_max = std::max({z00, z10, z01, z11});

                    // Skip if entire cell is below the minimum elevation for this layer
                    if (cell_max < config_.min_elevation.value()) {
                        continue;  // Cell is entirely below this layer
                    }
                }

                // Use flat elevation for all corners (creates flat top surface)
                double flat_elevation = config_.max_elevation.value_or(
                    std::max({z00, z10, z01, z11})
                ) * config_.vertical_scale;

                final_z00 = flat_elevation;
                final_z10 = flat_elevation;
                final_z01 = flat_elevation;
                final_z11 = flat_elevation;

            } else {
                // === TERRAIN-FOLLOWING MODE (3D terrain surface) ===
                // Use actual elevation values to show terrain topography

                // Skip cell if outside elevation range (for layer filtering)
                if (config_.min_elevation.has_value() || config_.max_elevation.has_value()) {
                    // Find min and max elevation in this cell
                    double cell_min = std::min({z00, z10, z01, z11});
                    double cell_max = std::max({z00, z10, z01, z11});

                    // Skip if cell is entirely outside the elevation range
                    if (config_.max_elevation.has_value() && cell_min > config_.max_elevation.value()) {
                        continue;  // Entire cell is above range
                    }
                    if (config_.min_elevation.has_value() && cell_max < config_.min_elevation.value()) {
                        continue;  // Entire cell is below range
                    }
                }

                // Apply elevation clipping and vertical scaling
                final_z00 = clip_elevation(z00) * config_.vertical_scale;
                final_z10 = clip_elevation(z10) * config_.vertical_scale;
                final_z01 = clip_elevation(z01) * config_.vertical_scale;
                final_z11 = clip_elevation(z11) * config_.vertical_scale;
            }

            // Add triangles for this cell
            add_cell_triangles(mesh, i, j, final_z00, final_z10, final_z01, final_z11, geotransform);
        }

        // Progress logging every 100 rows
        if (config_.verbose && (j % 100 == 0)) {
            logger_.debug("Processed row " + std::to_string(j) + "/" + std::to_string(height));
        }
    }

    logger_.info("Grid triangulation complete: " + std::to_string(stats_.surface_triangles) + " triangles created");

    return mesh;
}

void HeightmapTriangulator::add_cell_triangles(
    TopographicMesh& mesh,
    size_t i, size_t j,
    double z00, double z10, double z01, double z11,
    const double* geotransform) {

    // Transform pixel coordinates to real-world coordinates
    auto [x00, y00] = apply_geotransform(i, j, geotransform);
    auto [x10, y10] = apply_geotransform(i + 1, j, geotransform);
    auto [x01, y01] = apply_geotransform(i, j + 1, geotransform);
    auto [x11, y11] = apply_geotransform(i + 1, j + 1, geotransform);

    // Create vertices
    Vertex3D v00{Point3D(x00, y00, z00)};
    Vertex3D v10{Point3D(x10, y10, z10)};
    Vertex3D v01{Point3D(x01, y01, z01)};
    Vertex3D v11{Point3D(x11, y11, z11)};

    // Add vertices to mesh
    VertexId id00 = mesh.add_vertex(v00);
    VertexId id10 = mesh.add_vertex(v10);
    VertexId id01 = mesh.add_vertex(v01);
    VertexId id11 = mesh.add_vertex(v11);

    // Create 2 triangles per cell
    // Triangle 1: (i,j) → (i+1,j) → (i,j+1)
    // Triangle 2: (i+1,j) → (i+1,j+1) → (i,j+1)

    if (config_.flip_normals) {
        // Reverse winding order
        mesh.add_triangle(id00, id01, id10);
        mesh.add_triangle(id10, id01, id11);
    } else {
        // Standard winding order (counter-clockwise from above)
        mesh.add_triangle(id00, id10, id01);
        mesh.add_triangle(id10, id11, id01);
    }

    stats_.surface_triangles += 2;
}

// ============================================================================
// Base Platform and Side Walls
// ============================================================================

void HeightmapTriangulator::add_base_platform(
    TopographicMesh& mesh,
    size_t width,
    size_t height,
    const double* geotransform) {

    double base_z = config_.base_height_mm;
    size_t base_triangles_start = stats_.base_triangles;

    // Create base platform by projecting all grid points to z=base_z
    for (size_t j = 0; j < height - 1; ++j) {
        for (size_t i = 0; i < width - 1; ++i) {
            // Transform pixel coordinates to real-world coordinates
            auto [x00, y00] = apply_geotransform(i, j, geotransform);
            auto [x10, y10] = apply_geotransform(i + 1, j, geotransform);
            auto [x01, y01] = apply_geotransform(i, j + 1, geotransform);
            auto [x11, y11] = apply_geotransform(i + 1, j + 1, geotransform);

            // Create base vertices at z=base_z
            Vertex3D v00{Point3D(x00, y00, base_z)};
            Vertex3D v10{Point3D(x10, y10, base_z)};
            Vertex3D v01{Point3D(x01, y01, base_z)};
            Vertex3D v11{Point3D(x11, y11, base_z)};

            VertexId id00 = mesh.add_vertex(v00);
            VertexId id10 = mesh.add_vertex(v10);
            VertexId id01 = mesh.add_vertex(v01);
            VertexId id11 = mesh.add_vertex(v11);

            // Add base triangles (opposite winding order from surface)
            if (config_.flip_normals) {
                mesh.add_triangle(id00, id10, id01);
                mesh.add_triangle(id10, id11, id01);
            } else {
                mesh.add_triangle(id00, id01, id10);
                mesh.add_triangle(id10, id01, id11);
            }

            stats_.base_triangles += 2;
        }
    }

    logger_.debug("Base platform: " + std::to_string(stats_.base_triangles - base_triangles_start) + " triangles");
}

void HeightmapTriangulator::add_side_walls(
    TopographicMesh& mesh,
    const float* elevation_data,
    size_t width,
    size_t height,
    const double* geotransform) {

    double base_z = config_.base_height_mm;
    size_t wall_triangles_start = stats_.wall_triangles;

    // For contour mode, calculate constant wall top elevation
    double contour_top_elevation = 0.0;
    if (config_.contour_mode) {
        // Find maximum elevation in the data for flat wall tops
        double max_elev = std::numeric_limits<double>::lowest();
        for (size_t idx = 0; idx < width * height; ++idx) {
            double z = elevation_data[idx] * config_.vertical_scale;
            if (!is_nodata(z)) {
                max_elev = std::max(max_elev, z);
            }
        }
        contour_top_elevation = max_elev;
        logger_.debug("Contour mode: using constant wall top elevation = " +
                     std::to_string(contour_top_elevation) + "mm");
    }

    // Add walls along each edge:
    // - Left edge (i=0)
    // - Right edge (i=width-1)
    // - Top edge (j=0)
    // - Bottom edge (j=height-1)

    // Left edge (i=0)
    for (size_t j = 0; j < height - 1; ++j) {
        size_t idx0 = j * width;
        size_t idx1 = (j + 1) * width;

        double z0_data = elevation_data[idx0] * config_.vertical_scale;
        double z1_data = elevation_data[idx1] * config_.vertical_scale;

        if (is_nodata(z0_data) || is_nodata(z1_data)) continue;

        // Use constant elevation for contour mode, terrain-following for 3D mode
        double z0 = config_.contour_mode ? contour_top_elevation : z0_data;
        double z1 = config_.contour_mode ? contour_top_elevation : z1_data;

        auto [x0, y0] = apply_geotransform(0, j, geotransform);
        auto [x1, y1] = apply_geotransform(0, j + 1, geotransform);

        // Create 4 vertices for wall quad
        Vertex3D top0{Point3D(x0, y0, z0)};
        Vertex3D top1{Point3D(x1, y1, z1)};
        Vertex3D base0{Point3D(x0, y0, base_z)};
        Vertex3D base1{Point3D(x1, y1, base_z)};

        VertexId id_top0 = mesh.add_vertex(top0);
        VertexId id_top1 = mesh.add_vertex(top1);
        VertexId id_base0 = mesh.add_vertex(base0);
        VertexId id_base1 = mesh.add_vertex(base1);

        // Add 2 triangles for wall quad
        mesh.add_triangle(id_top0, id_base0, id_top1);
        mesh.add_triangle(id_base0, id_base1, id_top1);
        stats_.wall_triangles += 2;
    }

    // Right edge (i=width-1)
    for (size_t j = 0; j < height - 1; ++j) {
        size_t idx0 = j * width + (width - 1);
        size_t idx1 = (j + 1) * width + (width - 1);

        double z0_data = elevation_data[idx0] * config_.vertical_scale;
        double z1_data = elevation_data[idx1] * config_.vertical_scale;

        if (is_nodata(z0_data) || is_nodata(z1_data)) continue;

        // Use constant elevation for contour mode, terrain-following for 3D mode
        double z0 = config_.contour_mode ? contour_top_elevation : z0_data;
        double z1 = config_.contour_mode ? contour_top_elevation : z1_data;

        auto [x0, y0] = apply_geotransform(width - 1, j, geotransform);
        auto [x1, y1] = apply_geotransform(width - 1, j + 1, geotransform);

        Vertex3D top0{Point3D(x0, y0, z0)};
        Vertex3D top1{Point3D(x1, y1, z1)};
        Vertex3D base0{Point3D(x0, y0, base_z)};
        Vertex3D base1{Point3D(x1, y1, base_z)};

        VertexId id_top0 = mesh.add_vertex(top0);
        VertexId id_top1 = mesh.add_vertex(top1);
        VertexId id_base0 = mesh.add_vertex(base0);
        VertexId id_base1 = mesh.add_vertex(base1);

        mesh.add_triangle(id_top0, id_top1, id_base0);
        mesh.add_triangle(id_base0, id_top1, id_base1);
        stats_.wall_triangles += 2;
    }

    // Top edge (j=0)
    for (size_t i = 0; i < width - 1; ++i) {
        size_t idx0 = i;
        size_t idx1 = i + 1;

        double z0_data = elevation_data[idx0] * config_.vertical_scale;
        double z1_data = elevation_data[idx1] * config_.vertical_scale;

        if (is_nodata(z0_data) || is_nodata(z1_data)) continue;

        // Use constant elevation for contour mode, terrain-following for 3D mode
        double z0 = config_.contour_mode ? contour_top_elevation : z0_data;
        double z1 = config_.contour_mode ? contour_top_elevation : z1_data;

        auto [x0, y0] = apply_geotransform(i, 0, geotransform);
        auto [x1, y1] = apply_geotransform(i + 1, 0, geotransform);

        Vertex3D top0{Point3D(x0, y0, z0)};
        Vertex3D top1{Point3D(x1, y1, z1)};
        Vertex3D base0{Point3D(x0, y0, base_z)};
        Vertex3D base1{Point3D(x1, y1, base_z)};

        VertexId id_top0 = mesh.add_vertex(top0);
        VertexId id_top1 = mesh.add_vertex(top1);
        VertexId id_base0 = mesh.add_vertex(base0);
        VertexId id_base1 = mesh.add_vertex(base1);

        mesh.add_triangle(id_top0, id_top1, id_base0);
        mesh.add_triangle(id_base0, id_top1, id_base1);
        stats_.wall_triangles += 2;
    }

    // Bottom edge (j=height-1)
    for (size_t i = 0; i < width - 1; ++i) {
        size_t idx0 = (height - 1) * width + i;
        size_t idx1 = (height - 1) * width + (i + 1);

        double z0_data = elevation_data[idx0] * config_.vertical_scale;
        double z1_data = elevation_data[idx1] * config_.vertical_scale;

        if (is_nodata(z0_data) || is_nodata(z1_data)) continue;

        // Use constant elevation for contour mode, terrain-following for 3D mode
        double z0 = config_.contour_mode ? contour_top_elevation : z0_data;
        double z1 = config_.contour_mode ? contour_top_elevation : z1_data;

        auto [x0, y0] = apply_geotransform(i, height - 1, geotransform);
        auto [x1, y1] = apply_geotransform(i + 1, height - 1, geotransform);

        Vertex3D top0{Point3D(x0, y0, z0)};
        Vertex3D top1{Point3D(x1, y1, z1)};
        Vertex3D base0{Point3D(x0, y0, base_z)};
        Vertex3D base1{Point3D(x1, y1, base_z)};

        VertexId id_top0 = mesh.add_vertex(top0);
        VertexId id_top1 = mesh.add_vertex(top1);
        VertexId id_base0 = mesh.add_vertex(base0);
        VertexId id_base1 = mesh.add_vertex(base1);

        mesh.add_triangle(id_top0, id_base0, id_top1);
        mesh.add_triangle(id_base0, id_base1, id_top1);
        stats_.wall_triangles += 2;
    }

    logger_.debug("Side walls: " + std::to_string(stats_.wall_triangles - wall_triangles_start) + " triangles");
}

// ============================================================================
// Helper Methods
// ============================================================================

bool HeightmapTriangulator::is_nodata(double value) const {
    // Check for exact match or NaN
    if (std::isnan(value)) {
        return true;
    }

    // Check against configured NoData value with tolerance
    const double tolerance = 1e-6;
    return std::abs(value - config_.nodata_value) < tolerance;
}

double HeightmapTriangulator::clip_elevation(double elevation) const {
    if (config_.min_elevation.has_value() && elevation < config_.min_elevation.value()) {
        return config_.min_elevation.value();
    }
    if (config_.max_elevation.has_value() && elevation > config_.max_elevation.value()) {
        return config_.max_elevation.value();
    }
    return elevation;
}

std::pair<double, double> HeightmapTriangulator::apply_geotransform(
    double col,
    double row,
    const double* geotransform) const {

    // GDAL geotransform formula:
    // X = geotransform[0] + col * geotransform[1] + row * geotransform[2]
    // Y = geotransform[3] + col * geotransform[4] + row * geotransform[5]

    double x = geotransform[0] + col * geotransform[1] + row * geotransform[2];
    double y = geotransform[3] + col * geotransform[4] + row * geotransform[5];

    // If center coordinates are provided, project from geographic degrees to approximate meters
    if (config_.center_lat.has_value() && config_.center_lon.has_value()) {
        double center_lon = config_.center_lon.value();
        double center_lat = config_.center_lat.value();

        // Approximate meters per degree at this latitude
        // These formulas give reasonable approximations for local projections
        double meters_per_degree_lon = 111320.0 * std::cos(center_lat * M_PI / 180.0);
        double meters_per_degree_lat = 110540.0;

        // Project to meters relative to center point
        double x_meters = (x - center_lon) * meters_per_degree_lon;
        double y_meters = (y - center_lat) * meters_per_degree_lat;

        return {x_meters, y_meters};
    }

    // If no center coordinates, return geographic coordinates as-is
    return {x, y};
}

void HeightmapTriangulator::calculate_elevation_stats(
    const float* elevation_data,
    size_t width,
    size_t height) {

    double min_elev = std::numeric_limits<double>::max();
    double max_elev = std::numeric_limits<double>::lowest();

    size_t valid_count = 0;

    for (size_t i = 0; i < width * height; ++i) {
        double elev = elevation_data[i];

        if (!is_nodata(elev)) {
            min_elev = std::min(min_elev, elev);
            max_elev = std::max(max_elev, elev);
            ++valid_count;
        }
    }

    if (valid_count == 0) {
        logger_.warning("No valid elevation data found");
        stats_.min_elevation = 0.0;
        stats_.max_elevation = 0.0;
    } else {
        stats_.min_elevation = min_elev;
        stats_.max_elevation = max_elev;

        logger_.info("Elevation statistics:");
        logger_.info("  Range: " + std::to_string(min_elev) + " to " + std::to_string(max_elev));
        logger_.info("  Valid pixels: " + std::to_string(valid_count) + " / " + std::to_string(width * height));
    }
}

} // namespace topo
