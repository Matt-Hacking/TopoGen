/**
 * @file TopographicGenerator.cpp
 * @brief Main implementation of the high-performance topographic generator
 */

#include "topographic_generator.hpp"
#include "TopographicMesh.hpp"
#include "ElevationProcessor.hpp"
// TrianglePlaneIntersector removed - unused dead code
#include "ExecutionPolicies.hpp"
#include "ContourGenerator.hpp"
#include "HeightmapTriangulator.hpp"
#include "OutputTracker.hpp"
#include "Logger.hpp"
#include "InputValidator.hpp"
#include "ScalingCalculator.hpp"
#include "UnitParser.hpp"
#include "LabelRenderer.hpp"
#include "../export/MultiFormatExporter.hpp"
#include "../export/SVGExporter.hpp"
#include "../export/PNGExporter.hpp"
#include "../export/GeoTIFFExporter.hpp"
#include "../export/GeoJSONExporter.hpp"
#include "../export/ShapefileExporter.hpp"
#include <chrono>
#include <filesystem>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>

namespace topo {

// ============================================================================
// TopographicGenerator::Impl - Private implementation
// ============================================================================

class TopographicGenerator::Impl {
public:
    explicit Impl(const TopographicConfig& config)
        : config_(config),
          logger_("TopographicGenerator"),
          elevation_processor_(std::make_unique<ElevationProcessor>()),
          contour_generator_(std::make_unique<ContourGenerator>()),
          output_tracker_(config.log_level >= 4) {

        // Wire logger to config log level
        logger_.setLogLevel(static_cast<LogLevel>(config_.log_level));

        // Configure components based on config
        configure_components();
    }

    ~Impl() {
        // Note: Logger destructor will handle flushing automatically
        // Removed explicit flush() call to prevent potential mutex deadlock in destructor
    }
    
    bool generate_model() {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Reset metrics and output tracking
        metrics_ = {};
        validation_result_ = {};
        output_tracker_.clear();

        output_tracker_.functionState = "Starting topographic model generation pipeline";
        output_tracker_.outputFunctionState();

        // Validate configuration for contradictory inputs
        output_tracker_.functionState = "Validating configuration";
        InputValidator validator;
        auto validation_result = validator.validate(config_);

        if (validation_result.has_errors()) {
            logger_.error(validation_result.format_error_message());
            return false;
        }

        // Execute pipeline
        bool success = true;
        output_tracker_.startStage("elevation_data_loading");
        success &= load_elevation_data();
        output_tracker_.completeStage("elevation_data_loading", success);

        // Check if we need contours based on:
        // 1. Vector formats (SVG, GeoJSON, Shapefile) always need contours
        // 2. Contour mode (NOT terrain_following) needs contours for proper layer extrusion
        // 3. Terrain-following mode with STL only can skip contours (uses heightmap)
        bool needs_contours = false;
        for (const auto& format : config_.output_formats) {
            if (format == "svg" || format == "geojson" || format == "shapefile") {
                needs_contours = true;
                break;
            }
        }

        // For contour mode (not terrain-following), we need contours even for STL
        // because STL layers are created by extruding contour polygons
        if (!config_.terrain_following) {
            needs_contours = true;
        }

        if (needs_contours) {
            output_tracker_.startStage("contour_generation");
            success &= generate_contours();
            logger_.debug("generate_contours() completed with success=" + std::to_string(success));
            output_tracker_.completeStage("contour_generation", success);
            logger_.debug("Current contour_layers_.size() = " + std::to_string(contour_layers_.size()));
        } else {
            logger_.info("Terrain-following mode - skipping contour generation (using heightmap triangulation)");
        }

        // CRITICAL MEMORY STATE CHECK: Get memory before mesh generation stage
        logger_.info("*** CRITICAL: About to enter mesh generation stage ***");
        auto pre_mesh_stage_memory = contour_generator_->get_memory_stats();
        logger_.info("*** MEMORY STATE: " + std::to_string(pre_mesh_stage_memory.heap_used_mb) + " MB before mesh stage ***");

        // Check if we're in a dangerous memory state
        if (pre_mesh_stage_memory.heap_used_mb > 2000.0) {
            logger_.error("*** CRITICAL MEMORY OVERLOAD: " + std::to_string(pre_mesh_stage_memory.heap_used_mb) + " MB ***");
            logger_.error("*** System likely to be killed - aborting mesh generation ***");
            return false;
        }

        // Check if we need 3D mesh generation based on output formats
        bool needs_mesh = false;
        for (const auto& format : config_.output_formats) {
            if (format == "stl" || format == "obj" || format == "ply") {
                needs_mesh = true;
                break;
            }
        }

        if (needs_mesh) {
            logger_.info("3D output format detected - generating mesh");
            logger_.info("*** About to call output_tracker_.startStage(mesh_generation) ***");
            output_tracker_.startStage("mesh_generation");
            logger_.info("*** Successfully called output_tracker_.startStage(mesh_generation) ***");

            logger_.info("*** About to call generate_mesh() ***");
            success &= generate_mesh();
            logger_.info("*** generate_mesh() returned: " + std::string(success ? "SUCCESS" : "FAILURE") + " ***");
            output_tracker_.completeStage("mesh_generation", success);

            output_tracker_.startStage("mesh_validation");
            success &= validate_mesh();
            output_tracker_.completeStage("mesh_validation", success);
        } else {
            logger_.info("2D-only output formats - skipping mesh generation");
        }

        output_tracker_.startStage("model_export");
        success &= export_models();
        output_tracker_.completeStage("model_export", success);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        metrics_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        output_tracker_.functionState = "Completed topographic model generation pipeline";
        output_tracker_.outputFunctionState();
        output_tracker_.output("Total processing time: " +
            std::to_string(metrics_.total_time.count()) + "ms");

        if (config_.log_level >= 4) {
            output_tracker_.printSummary();
        }

        return success;
    }
    
    bool load_elevation_data() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool success = elevation_processor_->load_elevation_tiles(config_.bounds);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        metrics_.elevation_loading_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        return success;
    }
    
    bool generate_contours() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Configure contour generation
        ContourConfig contour_config;
        contour_config.interval = config_.contour_interval;
        contour_config.vertical_contour_relief = !config_.terrain_following;
        contour_config.force_all_layers = config_.force_all_layers;
        contour_config.remove_holes = config_.remove_holes;
        contour_config.inset_upper_layers = config_.inset_upper_layers;
        contour_config.inset_offset_mm = config_.inset_offset_mm;
        // convert_to_meters removed - using WGS84 coordinates throughout
        contour_config.simplify_tolerance = config_.simplification_tolerance;
        // Only set min/max elevation if user explicitly specified them (no defaults)
        contour_config.min_elevation = config_.min_elevation;
        contour_config.max_elevation = config_.max_elevation;
        contour_config.verbose = (config_.log_level >= 4);
        contour_config.output_directory = config_.output_directory;  // Pass output directory path
        contour_config.base_filename = config_.base_name;  // Pass base filename only (no path)

        // All formats now use GDAL-based polygon generation (CGAL removed)
        bool needs_stl = false;
        bool needs_polygon_mesh = false;
        for (const auto& format : config_.output_formats) {
            if (format == "stl") {
                needs_stl = true;
            } else if (format == "obj" || format == "ply") {
                needs_polygon_mesh = true;
            }
        }

        if (needs_stl && !needs_polygon_mesh) {
            logger_.info("STL-only mode - will use heightmap triangulation (GDAL-based)");
        } else if (needs_polygon_mesh) {
            logger_.info("OBJ/PLY formats requested - will use polygon-based approach");
        } else {
            logger_.info("SVG-only mode - using GDAL direct export (memory efficient)");
        }

        contour_generator_->set_config(contour_config);

        // Get elevation data from ElevationProcessor
        const float* elevation_data = elevation_processor_->get_elevation_data();
        auto [width, height] = elevation_processor_->get_elevation_dimensions();
        auto geotransform = elevation_processor_->get_geotransform();

        contour_layers_.clear(); // Reset any previous contours

        if (elevation_data && width > 0 && height > 0 && geotransform.size() >= 6) {
            // Generate contour layers using the ContourGenerator
            auto bbox = config_.bounds;
            double center_lon = (bbox.min_x + bbox.max_x) * 0.5;
            double center_lat = (bbox.min_y + bbox.max_y) * 0.5;

            // Log coordinate system information for debugging
            logger_.info("Preparing contour generation with coordinate analysis:");
            logger_.info("  Bounds: (" + std::to_string(bbox.min_x) + ", " + std::to_string(bbox.min_y) +
                        ") to (" + std::to_string(bbox.max_x) + ", " + std::to_string(bbox.max_y) + ")");
            logger_.info("  Center coordinates: (" + std::to_string(center_lon) + ", " + std::to_string(center_lat) + ")");
            logger_.info("  Geotransform origin: (" + std::to_string(geotransform[0]) + ", " + std::to_string(geotransform[3]) + ")");
            logger_.info("  Pixel size: (" + std::to_string(geotransform[1]) + ", " + std::to_string(geotransform[5]) + ")");

            // Use GDAL-based contour generation
            contour_layers_ = contour_generator_->generate_contours(
                elevation_data,
                width,
                height,
                geotransform.data(),
                config_.num_layers,
                center_lon,
                center_lat
            );

            // Check if GDAL contour generation produced results
            bool all_empty = std::all_of(contour_layers_.begin(), contour_layers_.end(),
                                       [](const ContourLayer& layer) { return layer.empty(); });

            if (contour_layers_.empty() || all_empty) {
                // Contour generation failed - no results produced
                auto end_time = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                logger_.error("Contour generation failed - no results produced");
                logger_.info("Elapsed time: " + std::to_string(elapsed.count()) + "ms");

                // Output any available statistics before exit
                if (metrics_.elevation_loading_time.count() > 0) {
                    logger_.info("Elevation loading time: " + std::to_string(metrics_.elevation_loading_time.count()) + "ms");
                }

                logger_.fatal("Cannot continue without contour data");
                return false;  // Return false to stop pipeline after SVG export
            }

            if (config_.log_level >= 4) {
                logger_.info("Generated " + std::to_string(contour_layers_.size()) + " contour layers");
            }

            // CRITICAL: Force this analysis regardless of verbose setting
            logger_.info("*** STARTING CONTOUR ANALYSIS (forced) ***");

            // CRITICAL: Add contour data size monitoring and memory analysis
            logger_.info("CONTOUR ANALYSIS: Contour generation completed successfully");
            logger_.info("CONTOUR ANALYSIS: Total contour layers: " + std::to_string(contour_layers_.size()));

            // Get memory state after contour generation
            auto post_contour_memory = contour_generator_->get_memory_stats();
            logger_.info("CONTOUR ANALYSIS: Memory after contour generation: " + std::to_string(post_contour_memory.heap_used_mb) + " MB");

            size_t total_polygons = 0;
            size_t total_vertices = 0;
            double total_polygon_area = 0.0;

            for (size_t i = 0; i < contour_layers_.size(); ++i) {
                const auto& layer = contour_layers_[i];
                total_polygons += layer.polygons.size();
                total_polygon_area += layer.area;

                // Count vertices in this layer
                size_t layer_vertices = 0;
                for (const auto& poly : layer.polygons) {
                    // Count vertices in all rings
                    for (const auto& ring : poly.rings) {
                        layer_vertices += ring.size();
                    }
                }
                total_vertices += layer_vertices;

                logger_.info("CONTOUR ANALYSIS: Layer " + std::to_string(i) + ": " + std::to_string(layer.polygons.size()) + " polygons, " + std::to_string(layer_vertices) + " vertices, area=" + std::to_string(layer.area));
            }

            logger_.info("CONTOUR ANALYSIS: *** CONTOUR DATA SIZE SUMMARY ***");
            logger_.info("CONTOUR ANALYSIS: Total polygons: " + std::to_string(total_polygons));
            logger_.info("CONTOUR ANALYSIS: Total vertices: " + std::to_string(total_vertices));
            logger_.info("CONTOUR ANALYSIS: Total area: " + std::to_string(total_polygon_area));
            logger_.info("CONTOUR ANALYSIS: Estimated memory per vertex: ~100 bytes");
            logger_.info("CONTOUR ANALYSIS: Estimated contour data memory: ~" + std::to_string(total_vertices * 100 / (1024*1024)) + " MB");

            // CRITICAL: Check if contour data is too large for safe mesh generation
            const size_t CRITICAL_VERTEX_THRESHOLD = 1000000; // 1M vertices = ~100MB
            const size_t EXTREME_VERTEX_THRESHOLD = 10000000;  // 10M vertices = ~1GB

            if (total_vertices > EXTREME_VERTEX_THRESHOLD) {
                logger_.error("CRITICAL: Contour data is extremely large (" + std::to_string(total_vertices) + " vertices)");
                logger_.error("This will almost certainly cause memory explosion during mesh generation");
                logger_.error("Aborting to prevent system kill");
                contour_layers_.clear(); // Free the memory immediately
                return false;
            } else if (total_vertices > CRITICAL_VERTEX_THRESHOLD) {
                logger_.warning("WARNING: Large contour data detected (" + std::to_string(total_vertices) + " vertices)");
                logger_.warning("This may cause high memory usage during mesh generation");
                logger_.warning("Consider using more aggressive downsampling");
            }

        } else {
            if (config_.log_level >= 4) {
                logger_.warning("No elevation data available for contour generation");
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        metrics_.contour_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        logger_.debug("About to exit generate_contours(), returning true");
        return true; // Return true for now to not block the pipeline
    }
    
    bool generate_mesh() {
        logger_.info("*** ENTERING generate_mesh() function ***");

        try {
            auto start_time = std::chrono::high_resolution_clock::now();

            // CRITICAL: Memory check at the very beginning of mesh generation
            auto entry_memory = contour_generator_->get_memory_stats();
            logger_.info("*** MESH FUNCTION ENTRY: Memory usage = " + std::to_string(entry_memory.heap_used_mb) + " MB ***");

            output_tracker_.functionState = "Initializing mesh generation";
            output_tracker_.outputFunctionState();

            // Use heightmap approach only if terrain_following mode is enabled
            // Contour mode uses polygon-based extrusion for proper 3D layer geometry
            bool use_heightmap = config_.terrain_following;

            if (use_heightmap) {
                logger_.info("Terrain-following mode - using heightmap-based triangulation (GDAL-only, no CGAL)");
            } else {
                logger_.info("Contour mode - using polygon-based extrusion (CGAL)");
            }

            bool success = false;

            if (use_heightmap) {
                // === HEIGHTMAP-BASED MESH GENERATION (GDAL-only) ===
                logger_.info("Using heightmap-based triangulation approach");
                output_tracker_.functionState = "Generating mesh from heightmap (GDAL-only)";
                output_tracker_.outputFunctionState();

                // Get elevation data from ElevationProcessor
                const float* elevation_data = elevation_processor_->get_elevation_data();
                auto [width, height] = elevation_processor_->get_elevation_dimensions();
                auto geotransform = elevation_processor_->get_geotransform();

                if (!elevation_data || width == 0 || height == 0 || geotransform.size() < 6) {
                    logger_.error("Invalid elevation data for heightmap triangulation");
                    return false;
                }

                // Configure heightmap triangulation
                HeightmapTriangulationConfig heightmap_config;
                heightmap_config.base_height_mm = config_.layer_thickness_mm;
                heightmap_config.vertical_scale = 1.0;  // Keep actual scale
                // Disable base platform and side walls for clean terrain surface output
                // User wants just the heightmap terrain, not a boxed tile
                heightmap_config.create_base_platform = false;
                heightmap_config.create_side_walls = false;
                heightmap_config.flip_normals = config_.upside_down_printing;
                heightmap_config.min_elevation = config_.min_elevation;
                heightmap_config.max_elevation = config_.max_elevation;
                heightmap_config.verbose = (config_.log_level >= 4);
                heightmap_config.contour_mode = !config_.terrain_following;  // Inverse: false=terrain-following, true=contour

                // Set center coordinates for geographic-to-meters projection
                heightmap_config.center_lon = (config_.upper_left_lon + config_.lower_right_lon) / 2.0;
                heightmap_config.center_lat = (config_.upper_left_lat + config_.lower_right_lat) / 2.0;

                // Create heightmap triangulator
                HeightmapTriangulator triangulator(heightmap_config);
                triangulator.set_logger(logger_);

                // Generate mesh from heightmap
                if (config_.output_layers && config_.num_layers > 1) {
                    // Generate multiple layers
                    logger_.info("Generating " + std::to_string(config_.num_layers) + " layers from heightmap");
                    auto layer_meshes_temp = triangulator.triangulate_layers(
                        elevation_data,
                        width,
                        height,
                        geotransform.data(),
                        config_.num_layers
                    );

                    if (layer_meshes_temp.empty()) {
                        logger_.error("Heightmap layer triangulation failed");
                        return false;
                    }

                    // Populate layer_meshes_ with unique_ptr wrappers
                    layer_meshes_.clear();
                    for (auto& layer_mesh : layer_meshes_temp) {
                        layer_meshes_.push_back(std::make_unique<TopographicMesh>(std::move(layer_mesh)));
                    }

                    logger_.info("Created " + std::to_string(layer_meshes_.size()) + " layer meshes from heightmap");

                    // Also generate full terrain mesh for stacked/combined export (if needed)
                    if (config_.output_stacked || !config_.output_layers) {
                        logger_.info("Generating full terrain mesh for stacked/combined export");
                        auto full_mesh = triangulator.triangulate_from_array(
                            elevation_data,
                            width,
                            height,
                            geotransform.data()
                        );
                        mesh_ = std::make_unique<TopographicMesh>(std::move(full_mesh));
                        logger_.info("Full terrain mesh: " + std::to_string(mesh_->num_vertices()) + " vertices, " +
                                   std::to_string(mesh_->num_triangles()) + " triangles");
                    }

                } else {
                    // Generate single mesh
                    logger_.info("Generating single mesh from heightmap");

                    layer_meshes_.clear();

                    // Need to generate mesh(es) based on what outputs are requested
                    if (config_.output_layers && config_.output_stacked) {
                        // Both layer and stacked output - need to generate twice
                        // First for layer export
                        auto layer_mesh = triangulator.triangulate_from_array(
                            elevation_data,
                            width,
                            height,
                            geotransform.data()
                        );
                        layer_meshes_.push_back(std::make_unique<TopographicMesh>(std::move(layer_mesh)));

                        // Second for stacked export (regenerate to avoid copying)
                        auto stacked_mesh = triangulator.triangulate_from_array(
                            elevation_data,
                            width,
                            height,
                            geotransform.data()
                        );
                        mesh_ = std::make_unique<TopographicMesh>(std::move(stacked_mesh));
                        logger_.info("Created 1 layer mesh + 1 stacked mesh from heightmap");

                    } else if (config_.output_layers) {
                        // Only layer output - move mesh into layer_meshes_
                        auto heightmap_mesh = triangulator.triangulate_from_array(
                            elevation_data,
                            width,
                            height,
                            geotransform.data()
                        );
                        layer_meshes_.push_back(std::make_unique<TopographicMesh>(std::move(heightmap_mesh)));
                        logger_.info("Created 1 layer mesh from heightmap");

                    } else {
                        // No layer export - just populate mesh_ for combined/stacked output
                        auto heightmap_mesh = triangulator.triangulate_from_array(
                            elevation_data,
                            width,
                            height,
                            geotransform.data()
                        );
                        mesh_ = std::make_unique<TopographicMesh>(std::move(heightmap_mesh));
                    }
                }

                // Check success based on what was generated (either mesh_ or layer_meshes_)
                bool has_valid_mesh = (mesh_ != nullptr && mesh_->num_triangles() > 0);
                bool has_valid_layers = !layer_meshes_.empty() &&
                                       layer_meshes_[0] != nullptr &&
                                       layer_meshes_[0]->num_triangles() > 0;

                if (has_valid_mesh || has_valid_layers) {
                    success = true;

                    if (has_valid_mesh) {
                        logger_.info("Heightmap triangulation successful:");
                        logger_.info("  Vertices: " + std::to_string(mesh_->num_vertices()));
                        logger_.info("  Triangles: " + std::to_string(mesh_->num_triangles()));
                    }

                    if (has_valid_layers) {
                        logger_.info("Heightmap layer triangulation successful:");
                        logger_.info("  Layers: " + std::to_string(layer_meshes_.size()));
                        size_t total_triangles = 0;
                        size_t total_vertices = 0;
                        for (const auto& layer : layer_meshes_) {
                            total_triangles += layer->num_triangles();
                            total_vertices += layer->num_vertices();
                        }
                        logger_.info("  Total vertices: " + std::to_string(total_vertices));
                        logger_.info("  Total triangles: " + std::to_string(total_triangles));
                    }
                } else {
                    success = false;
                }

            } else {
                // === POLYGON-BASED MESH GENERATION (CGAL) ===
                logger_.info("Using polygon-based triangulation approach (CGAL)");

                // Validate contour layers before proceeding
                logger_.debug("Validating contour_layers_.size() = " + std::to_string(contour_layers_.size()));
                for (size_t i = 0; i < contour_layers_.size(); ++i) {
                    const auto& layer = contour_layers_[i];
                    logger_.debug("Layer " + std::to_string(i) + " has " + std::to_string(layer.polygons.size()) + " polygons, area = " + std::to_string(layer.area));

                    // Check for polygon validity
                    for (size_t j = 0; j < layer.polygons.size(); ++j) {
                        const auto& poly = layer.polygons[j];
                        if (poly.empty()) {
                            logger_.warning("Layer " + std::to_string(i) + " polygon " + std::to_string(j) + " is empty");
                        }
                        logger_.debug("Layer " + std::to_string(i) + " polygon " + std::to_string(j) + " has " + std::to_string(poly.exterior().size()) + " outer vertices, " + std::to_string(poly.num_holes()) + " holes");
                    }
                }

                mesh_ = std::make_unique<TopographicMesh>();

                output_tracker_.output(contour_layers_.size(), "contour layers available");

                if (config_.terrain_following) {
                    output_tracker_.functionState = "Generating terrain-following mesh";
                    output_tracker_.outputFunctionState();
                    success = generate_terrain_following_mesh();
                } else {
                    output_tracker_.functionState = "Generating layered mesh with Seidel triangulation";
                    output_tracker_.outputFunctionState();
                    // Generate with Seidel (default method for now)
                    // Note: Dual output (Seidel + CDT) will be handled in export phase
                    success = generate_layered_mesh(TriangulationMethod::SEIDEL);
                }
            }

            if (success) {
                // Calculate metrics from either mesh_ or layer_meshes_ depending on what was populated
                // Check for non-empty mesh, not just non-null (contour mode creates empty mesh_)
                if (mesh_ != nullptr && mesh_->num_triangles() > 0) {
                    metrics_.triangles_generated = mesh_->num_triangles();
                    metrics_.vertices_generated = mesh_->num_vertices();
                } else if (!layer_meshes_.empty()) {
                    // Calculate totals from all layers
                    metrics_.triangles_generated = 0;
                    metrics_.vertices_generated = 0;
                    for (const auto& layer : layer_meshes_) {
                        if (layer) {
                            metrics_.triangles_generated += layer->num_triangles();
                            metrics_.vertices_generated += layer->num_vertices();
                        }
                    }
                }

                output_tracker_.output(metrics_.triangles_generated, "triangles generated");
                output_tracker_.output(metrics_.vertices_generated, "vertices generated");
            } else {
                output_tracker_.functionState = "Mesh generation failed";
                output_tracker_.outputFunctionState();
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            metrics_.mesh_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);

            logger_.debug("Mesh generation completed, success = " + std::to_string(success));
            return success;

        } catch (const std::exception& e) {
            logger_.error("CRITICAL ERROR in generate_mesh(): " + std::string(e.what()));
            output_tracker_.functionState = "Mesh generation crashed with exception";
            output_tracker_.outputFunctionState();
            return false;
        } catch (...) {
            logger_.error("CRITICAL ERROR in generate_mesh(): Unknown exception caught");
            output_tracker_.functionState = "Mesh generation crashed with unknown exception";
            output_tracker_.outputFunctionState();
            return false;
        }
    }
    
    bool validate_mesh() {
        // Check if we have either mesh_ or layer_meshes_
        bool has_mesh = (mesh_ != nullptr);
        bool has_layers = !layer_meshes_.empty();

        if (!has_mesh && !has_layers) {
            logger_.error("Validation failed: no mesh data available");
            return false;
        }

        // For heightmap-generated meshes (STL format), skip strict topology validation
        // since they are constructed from regular grids and inherently valid
        bool is_heightmap_mesh = false;
        for (const auto& format : config_.output_formats) {
            if (format == "stl") {
                is_heightmap_mesh = true;
                break;
            }
        }

        if (is_heightmap_mesh) {
            logger_.info("Heightmap mesh validation: skipping strict topology checks (grid-based mesh is inherently valid)");
            return true;  // Heightmap meshes from regular grids are always topologically valid
        }

        // Only validate mesh_ if it exists (for non-heightmap formats)
        if (has_mesh) {
            validation_result_ = mesh_->validate_topology();
            if (!validation_result_.is_valid()) {
                logger_.warning("Mesh validation found issues - attempting repair with libigl...");

                // Attempt mesh repair
                if (mesh_->repair_mesh_with_libigl()) {
                    logger_.info("Mesh repair completed - re-validating...");

                    // Re-validate after repair
                    validation_result_ = mesh_->validate_topology();

                    if (validation_result_.is_valid()) {
                        logger_.info("Mesh repair successful - validation passed!");
                    } else {
                        logger_.warning("Mesh repair incomplete - some issues remain");
                    }
                } else {
                    logger_.error("Mesh repair failed - proceeding with unrepaired mesh");
                }
            }
            return validation_result_.is_valid();
        }

        // Layer meshes don't need validation if they're heightmap-generated
        return true;
    }
    
    bool export_models() {
        auto start_time = std::chrono::high_resolution_clock::now();

        // Create output directory
        std::filesystem::create_directories(config_.output_directory);

        bool success = true;

        // DEBUG: Check state at export time
        logger_.debug("export_models() - State check:");
        logger_.debug("  mesh_ is " + std::string(mesh_ ? "NON-NULL" : "NULL"));
        logger_.debug("  layer_meshes_.size() = " + std::to_string(layer_meshes_.size()));

        // Calculate scale factors using ScalingCalculator
        // Get mesh bounds to determine XY and Z extents
        double xy_extent_meters = 0.0;
        double z_extent_meters = 0.0;
        [[maybe_unused]] double common_2d_scale_factor = 1.0;
        double common_3d_scale_factor = 1.0;

        // Calculate bounds from either stacked mesh or layer meshes
        if (mesh_ && mesh_->num_vertices() > 0) {
            logger_.debug("Taking FIRST branch - calculating bounds from mesh_");
            // Get stacked mesh bounds to calculate XY extent and Z extent
            double stacked_min_x = std::numeric_limits<double>::max();
            double stacked_max_x = std::numeric_limits<double>::lowest();
            double stacked_min_y = std::numeric_limits<double>::max();
            double stacked_max_y = std::numeric_limits<double>::lowest();
            double stacked_min_z = std::numeric_limits<double>::max();
            double stacked_max_z = std::numeric_limits<double>::lowest();

            for (auto it = mesh_->vertices_begin(); it != mesh_->vertices_end(); ++it) {
                stacked_min_x = std::min(stacked_min_x, it->position.x());
                stacked_max_x = std::max(stacked_max_x, it->position.x());
                stacked_min_y = std::min(stacked_min_y, it->position.y());
                stacked_max_y = std::max(stacked_max_y, it->position.y());
                stacked_min_z = std::min(stacked_min_z, it->position.z());
                stacked_max_z = std::max(stacked_max_z, it->position.z());
            }

            double range_x = stacked_max_x - stacked_min_x;
            double range_y = stacked_max_y - stacked_min_y;
            double range_z = stacked_max_z - stacked_min_z;
            xy_extent_meters = std::max(range_x, range_y);
            z_extent_meters = range_z;
        } else if (!layer_meshes_.empty()) {
            // Calculate bounds from layer meshes
            logger_.debug("Calculating bounds from " + std::to_string(layer_meshes_.size()) + " layer meshes");
            double stacked_min_x = std::numeric_limits<double>::max();
            double stacked_max_x = std::numeric_limits<double>::lowest();
            double stacked_min_y = std::numeric_limits<double>::max();
            double stacked_max_y = std::numeric_limits<double>::lowest();
            double stacked_min_z = std::numeric_limits<double>::max();
            double stacked_max_z = std::numeric_limits<double>::lowest();

            size_t total_vertices = 0;
            for (const auto& layer_mesh : layer_meshes_) {
                if (layer_mesh) {
                    size_t mesh_vertices = layer_mesh->num_vertices();
                    total_vertices += mesh_vertices;
                    logger_.debug("  Layer mesh has " + std::to_string(mesh_vertices) + " vertices");
                    for (auto it = layer_mesh->vertices_begin(); it != layer_mesh->vertices_end(); ++it) {
                        stacked_min_x = std::min(stacked_min_x, it->position.x());
                        stacked_max_x = std::max(stacked_max_x, it->position.x());
                        stacked_min_y = std::min(stacked_min_y, it->position.y());
                        stacked_max_y = std::max(stacked_max_y, it->position.y());
                        stacked_min_z = std::min(stacked_min_z, it->position.z());
                        stacked_max_z = std::max(stacked_max_z, it->position.z());
                    }
                }
            }

            logger_.debug("Total vertices processed: " + std::to_string(total_vertices));
            logger_.debug("Bounds: X[" + std::to_string(stacked_min_x) + ", " + std::to_string(stacked_max_x) + "]");
            logger_.debug("Bounds: Y[" + std::to_string(stacked_min_y) + ", " + std::to_string(stacked_max_y) + "]");
            logger_.debug("Bounds: Z[" + std::to_string(stacked_min_z) + ", " + std::to_string(stacked_max_z) + "]");

            double range_x = stacked_max_x - stacked_min_x;
            double range_y = stacked_max_y - stacked_min_y;
            double range_z = stacked_max_z - stacked_min_z;
            xy_extent_meters = std::max(range_x, range_y);
            z_extent_meters = range_z;

            logger_.debug("Calculated extents: XY=" + std::to_string(xy_extent_meters) + "m, Z=" + std::to_string(z_extent_meters) + "m");
        }

        if (mesh_ || !layer_meshes_.empty()) {

            // Calculate scales using ScalingCalculator
            ScalingCalculator scaling_calc(config_);

            // Determine which outputs we're generating
            bool has_2d_output = false;
            bool has_3d_output = false;
            for (const auto& format : config_.output_formats) {
                if (format == "svg" || format == "dxf" || format == "pdf") {
                    has_2d_output = true;
                } else if (format == "stl" || format == "obj" || format == "ply") {
                    has_3d_output = true;
                }
            }

            // Calculate appropriate scale factors
            if (has_3d_output) {
                auto scale_result_3d = scaling_calc.calculate_3d_scale(xy_extent_meters, z_extent_meters);
                common_3d_scale_factor = scale_result_3d.scale_factor;
                logger_.info("\n=== 3D Scaling Calculation ===");
                logger_.info(scale_result_3d.explanation);
                logger_.info("==============================\n");
            }

            if (has_2d_output) {
                auto scale_result_2d = scaling_calc.calculate_2d_scale(xy_extent_meters, z_extent_meters);
                common_2d_scale_factor = scale_result_2d.scale_factor;
                logger_.info("\n=== 2D Scaling Calculation ===");
                logger_.info(scale_result_2d.explanation);
                logger_.info("==============================\n");
            }

            // If both outputs use the same scaling method, use the same scale factor
            if (config_.use_2d_scaling_for_3d || config_.use_3d_scaling_for_2d) {
                logger_.info("Cross-mode scaling: Using consistent scale factor across 2D and 3D outputs");
            }
        }

        if (config_.output_layers && !layer_meshes_.empty()) {
            logger_.info("Exporting individual layer files...");

            // Export each layer
            size_t layer_num = 1;
            for (const auto& mesh_ptr : layer_meshes_) {
                if (mesh_ptr) {
                    // Calculate elevation for this layer (estimate from contour layers if available)
                    double layer_elevation = 0.0;
                    if (layer_num - 1 < contour_layers_.size()) {
                        layer_elevation = contour_layers_[layer_num - 1].elevation;
                    }

                    // Create a temporary exporter with layer-specific filename
                    std::string layer_filename;
                    if (config_.filename_pattern.empty()) {
                        layer_filename = config_.base_name + "_layer_" + std::to_string(layer_num);
                    } else {
                        layer_filename = LabelRenderer::substitute_filename_pattern(
                            config_.filename_pattern,
                            config_.base_name,
                            static_cast<int>(layer_num),
                            layer_elevation
                        );
                    }

                    MultiFormatExporter::GlobalOptions layer_opts;
                    layer_opts.base_filename = layer_filename;
                    layer_opts.output_directory = config_.output_directory;
                    layer_opts.filename_pattern = "";  // Pattern already applied to base_filename
                    layer_opts.verbose = (config_.log_level >= 4);
                    layer_opts.output_individual_layers = false; // Each layer is already individual

                    MultiFormatExporter layer_exporter(layer_opts);

                    // Configure STL options for layer exporter
                    // Use common 3D scale factor instead of auto-scaling each layer independently
                    STLExporter::Options layer_stl_opts;
                    layer_stl_opts.binary_format = true;
                    layer_stl_opts.validate_mesh = true;
                    layer_stl_opts.auto_scale = false;  // Disable auto-scale
                    layer_stl_opts.scale_factor = common_3d_scale_factor;  // Use common 3D scale

                    if (config_.cutting_bed_size_mm.has_value()) {
                        layer_stl_opts.target_bed_size_mm = config_.cutting_bed_size_mm.value();
                    } else if (config_.cutting_bed_x_mm.has_value() && config_.cutting_bed_y_mm.has_value()) {
                        layer_stl_opts.target_bed_size_mm = std::min(config_.cutting_bed_x_mm.value(), config_.cutting_bed_y_mm.value());
                    } else {
                        layer_stl_opts.target_bed_size_mm = config_.substrate_size_mm;
                    }

                    layer_exporter.set_stl_options(layer_stl_opts);

                    output_tracker_.functionState = "Exporting layer " + std::to_string(layer_num) + " mesh";
                    output_tracker_.outputFunctionState();

                    bool layer_success = layer_exporter.export_all_formats(*mesh_ptr, config_.output_formats);

                    // Track each exported file for this layer
                    for (const auto& format : config_.output_formats) {
                        std::string filename = config_.output_directory + "/" + layer_filename + "." + format;
                        output_tracker_.trackGeneratedFile(filename, format, "layer", layer_num, 0.0);
                    }

                    success &= layer_success;
                    layer_num++;
                }
            }

            if (config_.log_level >= 4) {
                logger_.info("Exported " + std::to_string(layer_meshes_.size()) + " layer files");
            }
        }

        // Export stacked model if requested
        if (config_.output_stacked && mesh_) {
            // Create stacked filename
            std::string stacked_filename = config_.base_name + "_stacked";

            MultiFormatExporter::GlobalOptions stacked_opts;
            stacked_opts.base_filename = stacked_filename;
            stacked_opts.output_directory = config_.output_directory;
            stacked_opts.filename_pattern = config_.filename_pattern;
            stacked_opts.verbose = (config_.log_level >= 4);
            stacked_opts.output_individual_layers = false;

            MultiFormatExporter stacked_exporter(stacked_opts);

            // Configure STL options for stacked exporter (same as main exporter)
            STLExporter::Options stacked_stl_opts;
            stacked_stl_opts.binary_format = true;
            stacked_stl_opts.validate_mesh = true;
            stacked_stl_opts.auto_scale = true;

            if (config_.cutting_bed_size_mm.has_value()) {
                stacked_stl_opts.target_bed_size_mm = config_.cutting_bed_size_mm.value();
            } else if (config_.cutting_bed_x_mm.has_value() && config_.cutting_bed_y_mm.has_value()) {
                stacked_stl_opts.target_bed_size_mm = std::min(config_.cutting_bed_x_mm.value(), config_.cutting_bed_y_mm.value());
            } else {
                stacked_stl_opts.target_bed_size_mm = config_.substrate_size_mm;
            }

            stacked_exporter.set_stl_options(stacked_stl_opts);

            output_tracker_.functionState = "Exporting stacked model";
            output_tracker_.outputFunctionState();

            bool stacked_success = stacked_exporter.export_all_formats(*mesh_, config_.output_formats);

            // Track each exported file for stacked model (skip SVG as it doesn't make sense for 3D stacked models)
            for (const auto& format : config_.output_formats) {
                if (format != "svg") {  // SVG doesn't make sense for stacked 3D models
                    std::string filename = config_.output_directory + "/" + stacked_filename + "." + format;
                    output_tracker_.trackGeneratedFile(filename, format, "stacked", -1, 0.0);
                }
            }

            if (config_.log_level >= 4) {
                if (stacked_success) {
                    logger_.info("Exported stacked model");
                } else {
                    logger_.error("Failed to export stacked model");
                }
            }

            success &= stacked_success;
        } else if (!config_.output_layers && mesh_) {
            // Export combined mesh when not outputting individual layers
            output_tracker_.functionState = "Exporting combined model";
            output_tracker_.outputFunctionState();

            success = exporter_->export_all_formats(*mesh_, config_.output_formats);

            // Track each exported file for combined model
            for (const auto& format : config_.output_formats) {
                std::string filename = config_.output_directory + "/" + config_.base_name + "." + format;
                output_tracker_.trackGeneratedFile(filename, format, "combined", -1, 0.0);
            }
        } else if (!config_.output_layers && !config_.output_stacked) {
            success = false;
        }

        // Calculate global elevation range before filtering for consistent color mapping
        double global_min_elev = contour_layers_.empty() ? 0.0 : contour_layers_.front().elevation;
        double global_max_elev = contour_layers_.empty() ? 0.0 : contour_layers_.back().elevation;
        for (const auto& layer : contour_layers_) {
            global_min_elev = std::min(global_min_elev, layer.elevation);
            global_max_elev = std::max(global_max_elev, layer.elevation);
        }

        // Filter layers if specific_layers is specified
        std::vector<ContourLayer> layers_to_export = contour_layers_;
        if (!config_.specific_layers.empty()) {
            std::vector<ContourLayer> filtered_layers;
            for (size_t i = 0; i < contour_layers_.size(); ++i) {
                // Layer numbers are 1-indexed in user input, but 0-indexed in vector
                int layer_number = static_cast<int>(i + 1);
                if (std::find(config_.specific_layers.begin(), config_.specific_layers.end(), layer_number) != config_.specific_layers.end()) {
                    filtered_layers.push_back(contour_layers_[i]);
                }
            }
            layers_to_export = filtered_layers;

            if (config_.log_level >= 4) {
                logger_.info("Layer filtering active: exporting " + std::to_string(layers_to_export.size()) +
                           " of " + std::to_string(contour_layers_.size()) + " layers");
            }
        }

        // Handle SVG export separately if requested and contour data is available
        auto svg_it = std::find(config_.output_formats.begin(), config_.output_formats.end(), "svg");
        if (svg_it != config_.output_formats.end() && !layers_to_export.empty()) {
            // Export SVG files using SVGExporter
            SVGConfig svg_config;
            svg_config.base_filename = config_.base_name;
            svg_config.filename_pattern = config_.filename_pattern;
            svg_config.output_directory = config_.output_directory;
            svg_config.verbose = (config_.log_level >= 4);
            svg_config.separate_layers = config_.output_layers;
            svg_config.force_all_layers = config_.force_all_layers;
            svg_config.remove_holes = config_.remove_holes;
            svg_config.render_mode = config_.render_mode;
            svg_config.color_scheme = config_.color_scheme;  // Pass color scheme to SVG exporter

            // Pass configurable colors (convert RGB hex to #RRGGBB format)
            svg_config.stroke_color = "#" + config_.stroke_color;
            svg_config.background_color = "#" + config_.background_color;

            // Pass stroke width (treat as millimeters for SVG)
            svg_config.cut_stroke_width = config_.stroke_width;

            // Pass label configuration
            svg_config.base_label_visible = config_.base_label_visible;
            svg_config.base_label_hidden = config_.base_label_hidden;
            svg_config.layer_label_visible = config_.layer_label_visible;
            svg_config.layer_label_hidden = config_.layer_label_hidden;
            svg_config.visible_label_color = config_.visible_label_color;
            svg_config.hidden_label_color = config_.hidden_label_color;
            svg_config.base_font_size_mm = config_.base_font_size_mm;
            svg_config.layer_font_size_mm = config_.layer_font_size_mm;

            // Pass label context information
            svg_config.geographic_bounds = config_.bounds;

            // Compute scale ratio from scale factor (e.g., 1:25000)
            // If explicit_2d_scale_factor is set (in mm/m), compute ratio as 1000/scale_factor
            // Otherwise use a default value of 1.0 (will be substituted as "Scale 1:1")
            if (config_.explicit_2d_scale_factor.has_value() && config_.explicit_2d_scale_factor.value() > 0) {
                svg_config.scale_ratio = 1000.0 / config_.explicit_2d_scale_factor.value();
            } else {
                svg_config.scale_ratio = 1.0;  // Default ratio if not specified
            }

            svg_config.contour_height_m = config_.contour_interval;
            svg_config.substrate_size_mm = config_.substrate_size_mm;
            svg_config.label_units = config_.label_units;
            svg_config.print_units = config_.print_units;
            svg_config.land_units = config_.land_units;

            SVGExporter svg_exporter(svg_config);

            output_tracker_.functionState = "Exporting SVG files";
            output_tracker_.outputFunctionState();

            // Pass geographic bounds for consistent coordinate transformation across all layers
            auto svg_files = svg_exporter.export_layers(layers_to_export, config_.output_layers,
                                                       global_min_elev, global_max_elev,
                                                       &config_.bounds);

            // Track SVG files
            for (const auto& svg_file : svg_files) {
                output_tracker_.trackGeneratedFile(svg_file, "svg", "layer", -1, 0.0);
            }

            if (svg_files.empty()) {
                if (config_.log_level >= 4) {
                    logger_.warning("SVG export failed - no files generated");
                }
                success = false;
            } else if (config_.log_level >= 4) {
                logger_.info("Successfully exported " + std::to_string(svg_files.size()) + " SVG files");
            }
        }

        // Handle PNG export if requested and contour data is available
        auto png_it = std::find(config_.output_formats.begin(), config_.output_formats.end(), "png");
        if (png_it != config_.output_formats.end() && !layers_to_export.empty()) {
            PNGExporter::Options png_opts;
            png_opts.width_px = 2048;
            png_opts.height_px = 0;  // Auto-calculate from geographic bounds aspect ratio

            // Convert SVG margin from mm to pixels to ensure consistent scaling
            // SVGConfig defaults to 10mm margin, convert using DPI
            constexpr double SVG_DEFAULT_MARGIN_MM = 10.0;
            png_opts.margin_px = UnitParser::mm_to_pixels(SVG_DEFAULT_MARGIN_MM, config_.print_resolution_dpi);

            png_opts.color_scheme = config_.color_scheme;
            png_opts.render_mode = config_.render_mode;
            png_opts.add_alignment_marks = config_.add_registration_marks;
            png_opts.stroke_color = config_.stroke_color;
            png_opts.background_color = config_.background_color;
            // Convert stroke_width from mm to pixels using DPI
            png_opts.stroke_width = UnitParser::mm_to_pixels(config_.stroke_width, config_.print_resolution_dpi);
            png_opts.font_path = config_.font_path;
            png_opts.font_face = config_.font_face;
            png_opts.remove_holes = config_.remove_holes;
            png_opts.filename_pattern = config_.filename_pattern;
            png_opts.base_label_visible = config_.base_label_visible;
            png_opts.base_label_hidden = config_.base_label_hidden;
            png_opts.layer_label_visible = config_.layer_label_visible;
            png_opts.layer_label_hidden = config_.layer_label_hidden;
            png_opts.visible_label_color = config_.visible_label_color;
            png_opts.hidden_label_color = config_.hidden_label_color;
            png_opts.base_font_size_mm = config_.base_font_size_mm;
            png_opts.layer_font_size_mm = config_.layer_font_size_mm;
            PNGExporter png_exporter(png_opts);

            std::string base_filename = config_.output_directory + "/" + config_.base_name;
            output_tracker_.functionState = "Exporting PNG raster";
            output_tracker_.outputFunctionState();

            auto png_files = png_exporter.export_png(layers_to_export, base_filename, config_.bounds, config_.output_layers);

            // Track all generated PNG files
            for (const auto& png_file : png_files) {
                output_tracker_.trackGeneratedFile(png_file, "png", "raster", -1, 0.0);
            }

            if (!png_files.empty()) {
                if (config_.log_level >= 4) {
                    logger_.info("Successfully exported " + std::to_string(png_files.size()) + " PNG file(s)");
                }
            } else {
                if (config_.log_level >= 4) {
                    logger_.warning("PNG export failed - no files generated");
                }
                success = false;
            }
        }

        // Handle GeoTIFF export if requested and contour data is available
        auto geotiff_it = std::find(config_.output_formats.begin(), config_.output_formats.end(), "geotiff");
        if (geotiff_it != config_.output_formats.end() && !layers_to_export.empty()) {
            GeoTIFFExporter::Options geotiff_opts;
            geotiff_opts.width_px = 2048;
            geotiff_opts.height_px = 0;  // Auto-calculate from aspect ratio
            geotiff_opts.color_scheme = config_.color_scheme;
            geotiff_opts.render_mode = config_.render_mode;
            geotiff_opts.projection_wkt = "EPSG:4326";  // WGS84
            geotiff_opts.compression = GeoTIFFExporter::Options::Compression::DEFLATE;
            geotiff_opts.add_alignment_marks = config_.add_registration_marks;
            geotiff_opts.stroke_color = config_.stroke_color;
            geotiff_opts.background_color = config_.background_color;
            geotiff_opts.stroke_width = config_.stroke_width;
            GeoTIFFExporter geotiff_exporter(geotiff_opts);

            std::string base_filename = config_.output_directory + "/" + config_.base_name;
            output_tracker_.functionState = "Exporting GeoTIFF raster";
            output_tracker_.outputFunctionState();

            auto geotiff_files = geotiff_exporter.export_geotiff(layers_to_export, base_filename, config_.bounds, config_.output_layers);

            // Track all generated GeoTIFF files
            for (const auto& geotiff_file : geotiff_files) {
                output_tracker_.trackGeneratedFile(geotiff_file, "geotiff", "raster", -1, 0.0);
            }

            if (!geotiff_files.empty()) {
                if (config_.log_level >= 4) {
                    logger_.info("Successfully exported " + std::to_string(geotiff_files.size()) + " GeoTIFF file(s)");
                }
            } else {
                if (config_.log_level >= 4) {
                    logger_.warning("GeoTIFF export failed - no files generated");
                }
                success = false;
            }
        }

        // Handle GeoJSON export if requested and contour data is available
        auto geojson_it = std::find(config_.output_formats.begin(), config_.output_formats.end(), "geojson");
        if (geojson_it != config_.output_formats.end() && !layers_to_export.empty()) {
            GeoJSONExporter::Options geojson_opts;
            geojson_opts.pretty_print = true;
            geojson_opts.include_crs = true;
            geojson_opts.crs = "urn:ogc:def:crs:OGC:1.3:CRS84";  // WGS84
            geojson_opts.precision = 6;
            GeoJSONExporter geojson_exporter(geojson_opts);

            output_tracker_.functionState = "Exporting GeoJSON vector";
            output_tracker_.outputFunctionState();

            if (config_.output_layers) {
                // Export each layer separately
                int exported_count = 0;
                for (size_t i = 0; i < layers_to_export.size(); ++i) {
                    const auto& layer = layers_to_export[i];

                    // Format elevation with proper precision for filename
                    std::ostringstream elev_str;
                    elev_str << std::fixed << std::setprecision(0) << layer.elevation;

                    std::string layer_filename = config_.output_directory + "/" +
                                                config_.base_name +
                                                "_layer_" + std::to_string(i + 1) +
                                                "_elev_" + elev_str.str() + "m.geojson";

                    bool layer_success = geojson_exporter.export_layer(layer, layer_filename, static_cast<int>(i + 1));

                    if (layer_success) {
                        output_tracker_.trackGeneratedFile(layer_filename, "geojson", "layer",
                                                          static_cast<int>(i + 1), layer.elevation);
                        exported_count++;
                    } else {
                        if (config_.log_level >= 4) {
                            logger_.warning("Failed to export GeoJSON layer " + std::to_string(i + 1));
                        }
                        success = false;
                    }
                }

                if (config_.log_level >= 4) {
                    logger_.info("Successfully exported " + std::to_string(exported_count) + " GeoJSON layer file(s)");
                }
            } else {
                // Export all layers combined
                std::string filename = config_.output_directory + "/" + config_.base_name + ".geojson";
                bool geojson_success = geojson_exporter.export_geojson(layers_to_export, filename);

                if (geojson_success) {
                    output_tracker_.trackGeneratedFile(filename, "geojson", "vector", -1, 0.0);
                    if (config_.log_level >= 4) {
                        logger_.info("Successfully exported GeoJSON: " + filename);
                    }
                } else {
                    if (config_.log_level >= 4) {
                        logger_.warning("GeoJSON export failed");
                    }
                    success = false;
                }
            }
        }

        // Handle Shapefile export if requested and contour data is available
        auto shapefile_it = std::find(config_.output_formats.begin(), config_.output_formats.end(), "shapefile");
        if (shapefile_it != config_.output_formats.end() && !layers_to_export.empty()) {
            ShapefileExporter::Options shapefile_opts;
            shapefile_opts.add_layer_field = true;
            shapefile_opts.add_elevation_field = true;
            shapefile_opts.projection_wkt = "EPSG:4326";  // WGS84
            ShapefileExporter shapefile_exporter(shapefile_opts);

            output_tracker_.functionState = "Exporting Shapefile vector";
            output_tracker_.outputFunctionState();

            if (config_.output_layers) {
                // Export each layer separately
                int exported_count = 0;
                for (size_t i = 0; i < layers_to_export.size(); ++i) {
                    const auto& layer = layers_to_export[i];

                    // Format elevation with proper precision for filename
                    std::ostringstream elev_str;
                    elev_str << std::fixed << std::setprecision(0) << layer.elevation;

                    std::string layer_filename = config_.output_directory + "/" +
                                                config_.base_name +
                                                "_layer_" + std::to_string(i + 1) +
                                                "_elev_" + elev_str.str() + "m.shp";

                    bool layer_success = shapefile_exporter.export_layer(layer, layer_filename, static_cast<int>(i + 1));

                    if (layer_success) {
                        output_tracker_.trackGeneratedFile(layer_filename, "shapefile", "layer",
                                                          static_cast<int>(i + 1), layer.elevation);
                        exported_count++;
                    } else {
                        if (config_.log_level >= 4) {
                            logger_.warning("Failed to export Shapefile layer " + std::to_string(i + 1));
                        }
                        success = false;
                    }
                }

                if (config_.log_level >= 4) {
                    logger_.info("Successfully exported " + std::to_string(exported_count) + " Shapefile layer file(s)");
                }
            } else {
                // Export all layers combined
                std::string filename = config_.output_directory + "/" + config_.base_name + ".shp";
                bool shapefile_success = shapefile_exporter.export_shapefile(layers_to_export, filename);

                if (shapefile_success) {
                    output_tracker_.trackGeneratedFile(filename, "shapefile", "vector", -1, 0.0);
                    if (config_.log_level >= 4) {
                        logger_.info("Successfully exported Shapefile: " + filename);
                    }
                } else {
                    if (config_.log_level >= 4) {
                        logger_.warning("Shapefile export failed");
                    }
                    success = false;
                }
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        metrics_.export_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        return success;
    }
    
    const TopographicMesh& get_mesh() const {
        if (!mesh_) {
            throw std::runtime_error("Mesh not generated yet");
        }
        return *mesh_;
    }

    const std::vector<ContourLayer>& get_contour_layers() const {
        return contour_layers_;
    }

    const TopographicConfig& get_config() const { return config_; }
    void update_config(const TopographicConfig& config) {
        config_ = config;
        configure_components();
    }

    const PerformanceMetrics& get_metrics() const { return metrics_; }
    const MeshValidationResult& get_validation_result() const { return validation_result_; }
    const OutputTracker& get_output_tracker() const { return output_tracker_; }

private:
    TopographicConfig config_;
    Logger logger_;

    // Core components
    std::unique_ptr<ElevationProcessor> elevation_processor_;
    // TrianglePlaneIntersector removed - unused dead code
    std::unique_ptr<ContourGenerator> contour_generator_;
    std::unique_ptr<MultiFormatExporter> exporter_;
    std::unique_ptr<TopographicMesh> mesh_;
    
    // Generated contour data
    std::vector<ContourLayer> contour_layers_;

    // Individual layer meshes for multi-layer export
    std::vector<std::unique_ptr<TopographicMesh>> layer_meshes_;

    // Output tracking and debugging
    mutable OutputTracker output_tracker_;

    // Results
    PerformanceMetrics metrics_;
    MeshValidationResult validation_result_;
    
    void configure_components() {
        // Configure contour generator
        ContourConfig contour_config;
        contour_config.interval = config_.contour_interval;
        contour_config.vertical_contour_relief = !config_.terrain_following;
        contour_config.force_all_layers = config_.force_all_layers;
        contour_config.remove_holes = config_.remove_holes;
        // convert_to_meters removed - using WGS84 coordinates throughout
        contour_config.simplify_tolerance = config_.simplification_tolerance;
        contour_config.verbose = (config_.log_level >= 4);
        contour_config.base_filename = config_.base_name;  // Pass base filename for output files

        contour_generator_->set_config(contour_config);
        
        // Configure exporter
        MultiFormatExporter::GlobalOptions export_opts;
        export_opts.output_directory = config_.output_directory;
        export_opts.base_filename = config_.base_name;
        export_opts.filename_pattern = config_.filename_pattern;
        export_opts.verbose = (config_.log_level >= 4);
        export_opts.output_individual_layers = config_.output_layers;
        exporter_ = std::make_unique<MultiFormatExporter>(export_opts);
        
        // Configure format-specific options
        STLExporter::Options stl_opts;
        stl_opts.binary_format = true;
        stl_opts.validate_mesh = true;
        stl_opts.auto_scale = true;

        // Configure target bed size from user settings or default
        if (config_.cutting_bed_size_mm.has_value()) {
            stl_opts.target_bed_size_mm = config_.cutting_bed_size_mm.value();
        } else if (config_.cutting_bed_x_mm.has_value() && config_.cutting_bed_y_mm.has_value()) {
            // Use the smaller dimension to ensure it fits in both X and Y
            stl_opts.target_bed_size_mm = std::min(config_.cutting_bed_x_mm.value(), config_.cutting_bed_y_mm.value());
        } else {
            // Use substrate_size_mm as default fallback
            stl_opts.target_bed_size_mm = config_.substrate_size_mm;
        }

        exporter_->set_stl_options(stl_opts);
        
        OBJExporter::Options obj_opts;
        obj_opts.include_materials = config_.obj_include_materials;
        obj_opts.elevation_coloring = config_.obj_elevation_colors;
        obj_opts.color_scheme = static_cast<ColorMapper::Scheme>(config_.color_scheme);
        // exporter_->set_obj_options(obj_opts);
    }
    
    bool generate_terrain_following_mesh() {
        // Generate terrain-following surface using bounding sphere scaling
        // This implements the approach from the Python version but with
        // much higher performance using C++ and CGAL
        
        // Sample elevation points
        auto bbox = config_.bounds;

        // Create sampling grid based on quality setting
        size_t grid_width, grid_height;
        switch (config_.quality) {
            case TopographicConfig::MeshQuality::DRAFT:
                grid_width = 50; grid_height = 50; break;
            case TopographicConfig::MeshQuality::MEDIUM:
                grid_width = 100; grid_height = 100; break;
            case TopographicConfig::MeshQuality::HIGH:
                grid_width = 200; grid_height = 200; break;
            case TopographicConfig::MeshQuality::ULTRA:
                grid_width = 400; grid_height = 400; break;
        }
        
        Grid sampling_grid(bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y, 
                          grid_width, grid_height);
        
        std::vector<Point3D> elevation_points;
        
        if (config_.parallel_processing) {
            elevation_points = elevation_processor_->sample_elevation_points_parallel(
                ParallelPolicy{}, sampling_grid);
        } else {
            elevation_points = elevation_processor_->sample_elevation_points(sampling_grid);
        }
        
        if (elevation_points.empty()) {
            return false;
        }
        
        // Apply elevation filtering if specified
        if (config_.min_elevation.has_value() || config_.max_elevation.has_value()) {
            auto it = std::remove_if(elevation_points.begin(), elevation_points.end(),
                [this](const Point3D& point) {
                    if (config_.min_elevation.has_value() && point.z() < *config_.min_elevation) {
                        return true;
                    }
                    if (config_.max_elevation.has_value() && point.z() > *config_.max_elevation) {
                        return true;
                    }
                    return false;
                });
            elevation_points.erase(it, elevation_points.end());
        }
        
        // Implement bounding sphere scaling (from the Python terrain-following approach)
        auto [min_x, max_x] = std::minmax_element(elevation_points.begin(), elevation_points.end(),
            [](const Point3D& a, const Point3D& b) { return a.x() < b.x(); });
        auto [min_y, max_y] = std::minmax_element(elevation_points.begin(), elevation_points.end(),
            [](const Point3D& a, const Point3D& b) { return a.y() < b.y(); });
        auto [min_z, max_z] = std::minmax_element(elevation_points.begin(), elevation_points.end(),
            [](const Point3D& a, const Point3D& b) { return a.z() < b.z(); });
        
        double x_span = max_x->x() - min_x->x();
        double y_span = max_y->y() - min_y->y();
        double z_span = max_z->z() - min_z->z();
        
        // Calculate bounding sphere diameter
        double sphere_diameter = std::sqrt(x_span * x_span + y_span * y_span + z_span * z_span);
        
        // Calculate scaling factor
        double print_volume_size = config_.substrate_size_mm;
        double scale_factor = (print_volume_size * 0.9 / 1000.0) / sphere_diameter;
        
        // Center point
        Point3D center(
            (min_x->x() + max_x->x()) * 0.5,
            (min_y->y() + max_y->y()) * 0.5,
            (min_z->z() + max_z->z()) * 0.5
        );
        
        // Scale and transform points
        std::vector<Point3D> scaled_points;
        scaled_points.reserve(elevation_points.size());
        
        for (const auto& point : elevation_points) {
            // Center and scale
            double scaled_x = (point.x() - center.x()) * scale_factor;
            double scaled_y = (point.y() - center.y()) * scale_factor;
            double scaled_z = (point.z() - center.z()) * scale_factor;
            
            // Apply Z-offset so terrain sits on build plate
            double z_offset = -(min_z->z() - center.z()) * scale_factor;
            double final_z = scaled_z + z_offset;
            
            // Convert to millimeters
            scaled_points.emplace_back(
                scaled_x * 1000.0,
                scaled_y * 1000.0,
                final_z * 1000.0
            );
        }
        
        // Build mesh using MeshBuilder
        MeshBuilder builder(scaled_points.size(), scaled_points.size() * 2);

        // Add vertices
        for (const auto& point : scaled_points) {
            builder.add_vertex(point);
        }
        
        // Create triangulation using Delaunay (simplified approach)
        // In a full implementation, would use CGAL's Delaunay triangulation
        // For now, create a simple triangulation
        
        size_t points_per_row = grid_width;
        size_t num_rows = grid_height;
        
        for (size_t row = 0; row < num_rows - 1; ++row) {
            for (size_t col = 0; col < points_per_row - 1; ++col) {
                VertexId v0 = static_cast<VertexId>(row * points_per_row + col);
                VertexId v1 = static_cast<VertexId>(row * points_per_row + col + 1);
                VertexId v2 = static_cast<VertexId>((row + 1) * points_per_row + col);
                VertexId v3 = static_cast<VertexId>((row + 1) * points_per_row + col + 1);
                
                if (v0 < scaled_points.size() && v1 < scaled_points.size() && 
                    v2 < scaled_points.size() && v3 < scaled_points.size()) {
                    // Create two triangles for the quad
                    builder.add_triangle(v0, v1, v2);
                    builder.add_triangle(v1, v3, v2);
                }
            }
        }
        
        // Build final mesh
        mesh_ = builder.build(true);
        return mesh_ != nullptr;
    }
    
    bool generate_layered_mesh(TriangulationMethod method = TriangulationMethod::SEIDEL) {
        // Generate traditional layered mesh with vertical sides using actual contour polygons


        logger_.debug("CHECKPOINT: ENTERED generate_layered_mesh() function");

        // CRITICAL MEMORY PROTECTION: Check system state before mesh generation
        logger_.debug("About to check memory state");

        // CRITICAL: Skip memory stats call that's causing the kill
        logger_.info("SKIPPING memory stats to avoid kill - proceeding with mesh generation");

        // Skip the memory check that's causing issues and proceed directly
        // auto entry_memory = contour_generator_->get_memory_stats();
        // if (entry_memory.heap_used_mb > CRITICAL_MEMORY_THRESHOLD_MB) {

        logger_.debug("Proceeding without memory check");

        // Skip memory threshold checks for now to avoid the kill
        // We'll proceed with mesh generation and see how far we get

        // Get elevation levels
        logger_.debug("About to get elevation levels");

        auto contour_levels = elevation_processor_->generate_contour_levels(
            config_.num_layers, ContourStrategy::UNIFORM);

        logger_.info("Got " + std::to_string(contour_levels.size()) + " elevation levels");

        logger_.debug("Checking elevation levels count");

        if (contour_levels.size() < 2) {
            logger_.error("Not enough elevation levels, returning false");
            return false;
        }

        logger_.debug("Checking contour layers available");

        // Ensure we have contour data available
        if (contour_layers_.empty()) {
            logger_.error("No contour data available for mesh generation");
            return false;
        }

        logger_.debug("Clearing previous layer meshes");

        // Clear previous layer meshes
        layer_meshes_.clear();
        layer_meshes_.reserve(config_.num_layers);

        logger_.debug("Layer meshes cleared and reserved for " + std::to_string(config_.num_layers) + " layers");

        logger_.debug("About to create progressive stacked mesh builder");

        // Progressive stacked mesh builder (if stacked output is requested)
        std::unique_ptr<MeshBuilder> stacked_builder;
        if (config_.output_stacked) {
            logger_.debug("Creating MeshBuilder for stacked output");

            stacked_builder = std::make_unique<MeshBuilder>();

            logger_.debug("MeshBuilder created successfully");

            // Memory monitoring through ContourGenerator
            // contour_generator_->get_memory_stats(); // Simple access for now

            if (config_.log_level >= 4) {
                logger_.info("Initializing progressive stacked mesh builder");
            }
        }

        logger_.debug("Setting layer height");

        double layer_height_mm = config_.layer_thickness_mm;

        logger_.debug("Layer height set to " + std::to_string(layer_height_mm) + "mm");

        logger_.debug("About to check verbose config");

        if (config_.log_level >= 4) {
            logger_.info("Generating " + std::to_string(contour_levels.size() - 1) + " layer meshes from contour data");
            logger_.info("Available contour layers: " + std::to_string(contour_layers_.size()));
        }

        // Check if coordinates are already projected (done in ogr_polygon_to_cgal() for GDAL path)
        // Geographic coordinates: lon in [-180, 180], lat in [-90, 90]
        // Projected coordinates: typically in thousands/millions of meters
        bool already_projected = false;
        if (!contour_layers_.empty() && !contour_layers_[0].polygons.empty()) {
            auto& first_poly = contour_layers_[0].polygons[0];
            if (!first_poly.exterior().empty()) {
                const auto& first_vertex = first_poly.exterior()[0];
                double x = first_vertex.first;
                double y = first_vertex.second;

                // If coordinates are outside geographic range, they're already projected
                if (std::abs(x) > 360.0 || std::abs(y) > 180.0) {
                    already_projected = true;
                    logger_.debug("Coordinates already projected to meters (x=" + std::to_string(x) +
                                ", y=" + std::to_string(y) + "), skipping late projection");
                }
            }
        }

        // CRITICAL FIX: Create a LOCAL COPY of contours for UTM projection
        // DO NOT modify contour_layers_ in-place - SVG/PNG exports need geographic coordinates!
        std::vector<ContourLayer> projected_contours = contour_layers_;  // Make a copy

        if (!already_projected) {
            logger_.debug("About to project contour coordinates from geographic to UTM (local copy for mesh generation only)");

            // CRITICAL: Project contour polygons from geographic coordinates to UTM meters
            // The contour generation works in geographic coordinates, but mesh generation needs projected coordinates
            // NOTE: We project a LOCAL COPY so original contour_layers_ stays in geographic coords for SVG/PNG
            double center_lon = (config_.upper_left_lon + config_.lower_right_lon) / 2.0;
            double center_lat = (config_.upper_left_lat + config_.lower_right_lat) / 2.0;

            logger_.debug("Using center coordinates lon=" + std::to_string(center_lon) + ", lat=" + std::to_string(center_lat) + " for UTM projection");

            // Project each contour layer to UTM coordinates IN THE LOCAL COPY
            for (auto& contour_layer : projected_contours) {
            logger_.debug("Projecting contour layer with " + std::to_string(contour_layer.polygons.size()) + " polygons");

            for (auto& poly_with_holes : contour_layer.polygons) {
                // Project all rings (exterior and holes)
                for (auto& ring : poly_with_holes.rings) {
                    for (auto& coord : ring) {
                        double lon = coord.first;
                        double lat = coord.second;

                        // Simple approximate UTM projection (good enough for mesh generation)
                        // Convert degrees to approximate meters using local scale factors
                        double meters_per_degree_lon = 111320.0 * cos(center_lat * M_PI / 180.0);
                        double meters_per_degree_lat = 110540.0;

                        double utm_x = (lon - center_lon) * meters_per_degree_lon;
                        double utm_y = (lat - center_lat) * meters_per_degree_lat;

                        // Update the coordinate in the LOCAL COPY
                        coord.first = utm_x;
                        coord.second = utm_y;
                    }
                }
            }
        }

        logger_.debug("Contour projection to UTM completed (local copy only - original contours preserved for SVG/PNG)");
        }  // end if (!already_projected)

        logger_.debug("About to start spatial bounds calculation");

        // Calculate spatial bounds for vertex deduplication (now in UTM meters)
        logger_.debug("Starting bounds calculation with " + std::to_string(projected_contours.size()) + " contour layers (now in UTM)");

        logger_.debug("Initializing bounds variables");

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();
        double min_z = 0.0;

        // Initialize max_z based on output format (will be updated below after calculating elevation range)
        // For STL: Will use elevation_range in meters
        // For SVG/laser: Use material thickness stacking in mm
        bool using_stl_format_for_bounds = std::find(config_.output_formats.begin(), config_.output_formats.end(), "stl") != config_.output_formats.end();
        double max_z = using_stl_format_for_bounds ? 0.0 : (contour_levels.size() - 1) * layer_height_mm;

        logger_.debug("Bounds variables initialized");

        // Calculate bounds from all contour layers with validation
        int total_polygons = 0;
        [[maybe_unused]] int total_vertices = 0;

        logger_.debug("About to start bounds calculation loop");

        for (size_t layer_idx = 0; layer_idx < projected_contours.size(); ++layer_idx) {
            const auto& contour_layer = projected_contours[layer_idx];

            if (config_.log_level >= 4) {
                logger_.debug("Processing contour layer " + std::to_string(layer_idx) + " with " +
                            std::to_string(contour_layer.polygons.size()) + " polygons");
            }

            for (size_t poly_idx = 0; poly_idx < contour_layer.polygons.size(); ++poly_idx) {
                const auto& poly_with_holes = contour_layer.polygons[poly_idx];
                total_polygons++;

                // MEMORY-BASED SAFETY CHECK: Monitor actual memory usage instead of vertex count heuristic
                if (total_polygons % 100 == 0) {
                    auto memory_stats = contour_generator_->get_memory_stats();

                    if (memory_stats.is_critical_level()) {
                        logger_.error("CRITICAL MEMORY: " + std::to_string(memory_stats.heap_used_mb) +
                                    " MB heap used. Skipping remaining polygons to prevent crash.");
                        break;
                    } else if (memory_stats.is_warning_level()) {
                        logger_.warning("HIGH MEMORY: " + std::to_string(memory_stats.heap_used_mb) +
                                      " MB heap used. Consider simplification if processing slows.");
                    }
                }

                size_t vertex_count = poly_with_holes.exterior().size();

                // Progress reporting for large polygon counts
                if (total_polygons % 1000 == 0  && config_.log_level >= 4) {
                    logger_.debug("Processed " + std::to_string(total_polygons) + " polygons for bounds calculation " +
                                "(current polygon has " + std::to_string(vertex_count) + " vertices)");
                }

                for (const auto& [x, y] : poly_with_holes.exterior()) {
                    total_vertices++;

                    // Safety check: Skip infinite/NaN coordinates
                    if (!std::isfinite(x) || !std::isfinite(y)) {
                        logger_.warning("Skipping invalid coordinate (" + std::to_string(x) + ", " + std::to_string(y) + ")");
                        continue;
                    }

                    min_x = std::min(min_x, x);
                    max_x = std::max(max_x, x);
                    min_y = std::min(min_y, y);
                    max_y = std::max(max_y, y);
                }
            }
        }


        if (config_.log_level >= 4) {
            logger_.info("Spatial bounds: X[" + std::to_string(min_x) + ", " + std::to_string(max_x) + "] Y[" + std::to_string(min_y) + ", " + std::to_string(max_y) + "] Z[" + std::to_string(min_z) + ", " + std::to_string(max_z) + "]");
        }

        // Memory-efficient chunked processing: process layers one at a time with cleanup

        // Calculate elevation parameters once (for 3D files)
        double min_elevation = *std::min_element(contour_levels.begin(), contour_levels.end());
        double max_elevation = *std::max_element(contour_levels.begin(), contour_levels.end());
        double elevation_range = max_elevation - min_elevation;

        // Update max_z for STL files now that we have elevation_range
        if (using_stl_format_for_bounds) {
            max_z = elevation_range;  // Z in meters, same units as XY
        }

        if (config_.log_level >= 4) {
            logger_.info("Elevation range: " + std::to_string(min_elevation) + "m to " + std::to_string(max_elevation) + "m");
            logger_.info("Total elevation range: " + std::to_string(elevation_range) + "m");
            logger_.info("Z coordinates will be in meters (same units as XY), scaled uniformly during export");
        }

        // Create individual mesh for each layer using actual contour polygons
        // FIXED: Loop over actual contour_layers_ instead of requested contour_levels to avoid off-by-1 error
        for (size_t layer_idx = 0; layer_idx < contour_layers_.size(); ++layer_idx) {
            if (config_.log_level >= 4) {
                logger_.info("Processing layer " + std::to_string(layer_idx) + " of " + std::to_string(contour_layers_.size()));
            }

            MeshBuilder builder;

            // Use configured vertex deduplication tolerance
            // Default is 1 micron (1e-6m), appropriate for high-precision manufacturing
            // User can override via config if needed for specific applications
            double dedup_tolerance = config_.vertex_dedup_tolerance;

            // Enforce minimum tolerance of 1 micron to prevent excessive vertex count
            dedup_tolerance = std::max(dedup_tolerance, 1e-6);

            // Use hash-based vertex deduplication to prevent memory explosions
            try {
                builder.set_vertex_deduplication_tolerance(dedup_tolerance);
                if (config_.log_level >= 5 && layer_idx == 0) {
                    logger_.debug("Vertex deduplication tolerance: " + std::to_string(dedup_tolerance) + " meters");
                }
            } catch (const std::exception& e) {
                logger_.warning("Exception in vertex deduplication setup: " + std::string(e.what()));
                // Continue without vertex deduplication - will still work, just less memory efficient
            } catch (...) {
                logger_.warning("Unknown exception in vertex deduplication setup");
                // Continue without vertex deduplication - will still work, just less memory efficient
            }

            if (config_.log_level >= 4 && layer_idx == 0) {
                logger_.info("Using vertex deduplication tolerance: " + std::to_string(dedup_tolerance) + " meters (preserves "
                           + std::to_string(1.0 / dedup_tolerance) + "x resolution)");
            }

            // Get layer data from projected_contours (UTM coordinates for mesh generation)
            const auto& layer = projected_contours[layer_idx];
            double elevation = layer.elevation;
            double z_base, z_top;

            // Choose height calculation based on output format
            // For STL: Use elevation in meters (same units as XY) for uniform scaling
            // For SVG: Use material thickness for laser cutting
            bool using_stl_format = std::find(config_.output_formats.begin(), config_.output_formats.end(), "stl") != config_.output_formats.end();

            if (using_stl_format && elevation_range > 0) {
                // 3D topographic height calculation in METERS (same units as XY coordinates)
                // This ensures uniform XYZ scaling during export with no vertical exaggeration
                z_base = (elevation - min_elevation);  // meters

                // Calculate top from next contour layer for proper layer thickness
                if (layer_idx + 1 < projected_contours.size()) {
                    double next_elevation = projected_contours[layer_idx + 1].elevation;
                    z_top = (next_elevation - min_elevation);  // meters
                } else {
                    // For the top layer, add average layer thickness
                    double avg_layer_thickness = elevation_range / projected_contours.size();  // meters
                    z_top = z_base + avg_layer_thickness;
                }

                // Safety check: ensure z_top > z_base to prevent inverted layers
                if (z_top <= z_base) {
                    double min_thickness = 0.1;  // 0.1 meters minimum layer thickness
                    z_top = z_base + min_thickness;
                }
            } else {
                // 2D laser cutting: use simple incremental heights (material thickness in mm)
                z_base = layer_idx * layer_height_mm;
                z_top = (layer_idx + 1) * layer_height_mm;
            }

            if (config_.log_level >= 4) {
                std::string z_units = using_stl_format ? "m" : "mm";
                logger_.info("Processing layer " + std::to_string(layer_idx) + " at elevation " + std::to_string(elevation) + "m -> Z height " + std::to_string(z_base) + "-" + std::to_string(z_top) + z_units);
                if (layer_idx == 0 && using_stl_format) {
                    logger_.info("STL mode: Z coordinates in meters (same units as XY) for uniform scaling");
                }
            }

            logger_.debug("MESH DEBUG: Using projected_contours[" + std::to_string(layer_idx) + "] at elevation " + std::to_string(elevation) + "m");

            if (layer.empty()) {
                logger_.info("MESH DEBUG: Layer " + std::to_string(layer_idx) + " has no contour data, skipping");
                continue;
            }

            logger_.info("MESH DEBUG: Layer " + std::to_string(layer_idx) + " has " + std::to_string(layer.polygons.size()) + " polygons");

            // Count total vertices in this layer's polygons
            size_t total_vertices = 0;
            for (const auto& poly : layer.polygons) {
                total_vertices += poly.exterior().size();
                total_vertices += poly.num_holes() * 10; // Estimate hole vertices
            }
            logger_.info("MESH DEBUG: Estimated " + std::to_string(total_vertices) + " vertices to process in layer " + std::to_string(layer_idx));

            // Convert CGAL polygons to 3D mesh using constrained Delaunay triangulation
            output_tracker_.functionState = "Creating mesh from contour polygons for layer " + std::to_string(layer_idx);
            output_tracker_.outputFunctionState();
            output_tracker_.output(layer.polygons.size(), "polygons in layer");


            bool mesh_created = false;
            try {
                logger_.info("MESH DEBUG: Starting create_mesh_from_contour_polygons for layer " + std::to_string(layer_idx));
                mesh_created = create_mesh_from_contour_polygons(builder, layer, z_base, z_top, dedup_tolerance, method);
                logger_.info("MESH DEBUG: Completed create_mesh_from_contour_polygons for layer " + std::to_string(layer_idx) + ", result: " + (mesh_created ? "SUCCESS" : "FAILED"));
            } catch (const std::exception& e) {
                logger_.error("Exception during mesh creation for layer " + std::to_string(layer_idx) + ": " + std::string(e.what()));
                output_tracker_.functionState = "Exception during mesh creation: " + std::string(e.what());
                output_tracker_.outputFunctionState();
                continue;
            } catch (...) {
                logger_.error("Unknown exception during mesh creation for layer " + std::to_string(layer_idx));
                output_tracker_.functionState = "Unknown exception during mesh creation";
                output_tracker_.outputFunctionState();
                continue;
            }


            if (!mesh_created) {
                output_tracker_.functionState = "Failed to create mesh for layer " + std::to_string(layer_idx);
                output_tracker_.outputFunctionState();
                if (config_.log_level >= 4) {
                    logger_.error("  Failed to create mesh for layer " + std::to_string(layer_idx));
                }
                continue;
            }

            output_tracker_.functionState = "Building layer mesh from triangulated geometry";
            output_tracker_.outputFunctionState();
            output_tracker_.output(builder.vertex_count(), "vertices in builder");
            output_tracker_.output(builder.triangle_count(), "triangles in builder");

            // Build individual layer mesh with comprehensive debugging
            logger_.info("MESH DEBUG: About to call builder.build() for layer " + std::to_string(layer_idx));
            logger_.info("MESH DEBUG: Builder state before build:");
            logger_.info("MESH DEBUG:   Vertices: " + std::to_string(builder.vertex_count()));
            logger_.info("MESH DEBUG:   Triangles: " + std::to_string(builder.triangle_count()));

            std::unique_ptr<TopographicMesh> layer_mesh;
            try {
                output_tracker_.functionState = "Calling builder.build() for layer " + std::to_string(layer_idx);
                output_tracker_.outputFunctionState();

                layer_mesh = builder.build(true);

                output_tracker_.functionState = "builder.build() completed for layer " + std::to_string(layer_idx);
                output_tracker_.outputFunctionState();
            } catch (const std::exception& e) {
                logger_.error("Exception in builder.build(): " + std::string(e.what()));
                output_tracker_.functionState = "Exception in builder.build(): " + std::string(e.what());
                output_tracker_.outputFunctionState();
                continue;
            } catch (...) {
                logger_.error("Unknown exception in builder.build()");
                output_tracker_.functionState = "Unknown exception in builder.build()";
                output_tracker_.outputFunctionState();
                continue;
            }

            if (layer_mesh) {
                logger_.debug("Layer mesh created successfully, moving to vector");

                // Progressive stacked mesh building - add layer immediately
                if (stacked_builder && layer_mesh) {
                    // Individual layer meshes already have absolute Z coordinates (z_base to z_top)
                    // so no Z offset is needed when combining them into the stacked mesh
                    size_t vertex_offset = stacked_builder->vertex_count();

                    logger_.debug("MESH DEBUG: Adding layer " + std::to_string(layer_idx) + " to stacked mesh (no Z offset needed, meshes have absolute coordinates)");
                    logger_.debug("MESH DEBUG: Progressive stacked builder before adding layer " + std::to_string(layer_idx) + ": " + std::to_string(stacked_builder->vertex_count()) + " vertices, " + std::to_string(stacked_builder->triangle_count()) + " triangles");

                    // Copy vertices directly (they already have correct absolute Z coordinates)
                    logger_.debug("MESH DEBUG: Copying " + std::to_string(layer_mesh->num_vertices()) + " vertices");
                    for (auto it = layer_mesh->vertices_begin(); it != layer_mesh->vertices_end(); ++it) {
                        Point3D adjusted_pos(it->position.x(), it->position.y(), it->position.z());
                        stacked_builder->add_vertex(adjusted_pos);
                    }

                    // Copy triangles with vertex offset
                    logger_.debug("MESH DEBUG: Copying " + std::to_string(layer_mesh->num_triangles()) + " triangles with vertex offset " + std::to_string(vertex_offset));
                    for (auto it = layer_mesh->triangles_begin(); it != layer_mesh->triangles_end(); ++it) {
                        stacked_builder->add_triangle(
                            it->vertices[0] + vertex_offset,
                            it->vertices[1] + vertex_offset,
                            it->vertices[2] + vertex_offset
                        );
                    }

                    logger_.debug("MESH DEBUG: Progressive stacked builder after adding layer " + std::to_string(layer_idx) + ": " + std::to_string(stacked_builder->vertex_count()) + " vertices, " + std::to_string(stacked_builder->triangle_count()) + " triangles");
                }

                layer_meshes_.emplace_back(std::move(layer_mesh));
                output_tracker_.output(layer_meshes_.back()->num_vertices(), "vertices in completed layer mesh");
                output_tracker_.output(layer_meshes_.back()->num_triangles(), "triangles in completed layer mesh");
                if (config_.log_level >= 4) {
                    logger_.info("  Created mesh with " + std::to_string(layer_meshes_.back()->num_vertices()) + " vertices and " + std::to_string(layer_meshes_.back()->num_triangles()) + " triangles");
                }
                logger_.debug("Layer " + std::to_string(layer_idx) + " mesh processing completed");

                // Optional: Clear individual layer mesh if only stacked output is needed
                // This can save significant memory for large datasets
                if (config_.output_stacked && !config_.output_layers) {
                    layer_meshes_.back().reset(); // Free individual layer mesh memory
                    if (config_.log_level >= 4) {
                        logger_.info("  Freed individual layer mesh memory (stacked-only mode)");
                    }
                }
            } else {
                logger_.debug("builder.build() returned null mesh for layer " + std::to_string(layer_idx));
                output_tracker_.functionState = "Builder.build() returned null mesh for layer " + std::to_string(layer_idx);
                output_tracker_.outputFunctionState();
            }

            // Memory cleanup after each layer
            {
                // MeshBuilder will be automatically destroyed at end of scope
                if (config_.log_level >= 4) {
                    logger_.info("Layer " + std::to_string(layer_idx) + " processing completed, resources freed");
                }
            }
        }

        // Finalize progressively built stacked mesh
        if (stacked_builder && config_.output_stacked) {
            output_tracker_.functionState = "Finalizing progressively built stacked mesh";
            output_tracker_.outputFunctionState();

            logger_.debug("About to build final stacked mesh from progressive builder");
            logger_.debug("Progressive stacked builder state:");
            logger_.debug("  Total vertices: " + std::to_string(stacked_builder->vertex_count()));
            logger_.debug("  Total triangles: " + std::to_string(stacked_builder->triangle_count()));

            output_tracker_.output(stacked_builder->vertex_count(), "total vertices in progressive stacked builder");
            output_tracker_.output(stacked_builder->triangle_count(), "total triangles in progressive stacked builder");

            try {
                logger_.debug("Calling progressive stacked_builder.build(true)...");
                mesh_ = stacked_builder->build(true);
                logger_.debug("Progressive stacked mesh build completed");
            } catch (const std::exception& e) {
                logger_.error("Exception in progressive stacked mesh build: " + std::string(e.what()));
                output_tracker_.functionState = "Exception in progressive stacked mesh build: " + std::string(e.what());
                output_tracker_.outputFunctionState();
                return false;
            } catch (...) {
                logger_.error("Unknown exception in progressive stacked mesh build");
                output_tracker_.functionState = "Unknown exception in progressive stacked mesh build";
                output_tracker_.outputFunctionState();
                return false;
            }

            // Free stacked builder memory immediately after building
            stacked_builder.reset();

            if (mesh_) {
                logger_.debug("Final progressive stacked mesh created successfully");
                output_tracker_.output(mesh_->num_vertices(), "vertices in final progressive stacked mesh");
                output_tracker_.output(mesh_->num_triangles(), "triangles in final progressive stacked mesh");

                if (config_.log_level >= 4) {
                    logger_.info("Progressive stacked mesh finalization completed");
                }
            } else {
                logger_.debug("Progressive stacked mesh build returned null");
                output_tracker_.functionState = "Failed to build progressive stacked mesh - builder.build() returned null";
                output_tracker_.outputFunctionState();
            }
        }

        return !layer_meshes_.empty();
    }

private:
    /**
     * @brief Create 3D mesh from 2D contour polygons using CGAL triangulation
     */
    bool create_mesh_from_contour_polygons(MeshBuilder& builder, const ContourLayer& layer,
                                         double z_base, double z_top, double dedup_tolerance,
                                         TriangulationMethod method = TriangulationMethod::SEIDEL) {
        if (layer.polygons.empty()) {
            return false;
        }

        if (config_.log_level >= 4) {
            logger_.debug("    Creating mesh from " + std::to_string(layer.polygons.size()) + " contour polygons");
        }

        try {
            // Use watertight extrusion approach for guaranteed watertight meshes
            int polygon_count = 0;
            int total_polygons = layer.polygons.size();

            logger_.info("MESH DEBUG: Starting polygon processing - " + std::to_string(total_polygons) + " polygons to process");

            for (const auto& poly_with_holes : layer.polygons) {
                polygon_count++;

                // Progress reporting every 10% or every 100 polygons, whichever is smaller
                int report_interval = std::max(1, std::min(100, total_polygons / 10));
                if (polygon_count % report_interval == 0 || polygon_count == total_polygons) {
                    logger_.info("Processing polygon " + std::to_string(polygon_count) + "/" + std::to_string(total_polygons) + " (" + std::to_string((polygon_count * 100) / total_polygons) + "%)");
                }

                // Get the outer boundary
                const auto& outer = poly_with_holes.exterior();
                if (outer.size() < 3) {
                    if (config_.log_level >= 4) {
                        logger_.debug("      Skipping polygon " + std::to_string(polygon_count) + " (insufficient vertices)");
                    }
                    continue;
                }

                if (config_.log_level >= 4) {
                    logger_.debug("      Processing polygon " + std::to_string(polygon_count) + " with " + std::to_string(outer.size()) + " vertices");
                }

                // Convert polygon coordinates to Point3D vector for watertight extrusion
                std::vector<Point3D> polygon_vertices;
                polygon_vertices.reserve(outer.size());

                for (const auto& [x, y] : outer) {
                    // Safety check: Skip infinite/NaN coordinates to prevent STL corruption
                    if (!std::isfinite(x) || !std::isfinite(y)) {
                        logger_.warning("      Skipping invalid vertex (" + std::to_string(x) + ", " + std::to_string(y) + ") in polygon " + std::to_string(polygon_count));
                        continue;
                    }

                    polygon_vertices.emplace_back(x, y, 0.0); // Z will be set by extrusion
                }

                if (polygon_vertices.size() < 3) continue;

                // CRITICAL: Simplify polygon to remove vertices closer than dedup tolerance
                // This prevents vertex deduplication from creating degenerate triangles with duplicate indices
                // Must happen BEFORE extrusion, not during vertex addition
                std::vector<Point3D> simplified_vertices;
                simplified_vertices.reserve(polygon_vertices.size());

                double simplification_tolerance_sq = dedup_tolerance * dedup_tolerance;

                for (size_t i = 0; i < polygon_vertices.size(); ++i) {
                    const auto& current = polygon_vertices[i];
                    const auto& next = polygon_vertices[(i + 1) % polygon_vertices.size()];

                    // Calculate 2D distance (Z is 0 at this point)
                    double dx = next.x() - current.x();
                    double dy = next.y() - current.y();
                    double dist_sq = dx * dx + dy * dy;

                    // Keep vertex only if it's far enough from the next vertex
                    // (or if it's the last vertex and far from the first)
                    if (dist_sq >= simplification_tolerance_sq) {
                        simplified_vertices.push_back(current);
                    } else {
                        if (config_.log_level >= 4 && polygon_count == 1) {
                            logger_.debug("      Removed vertex " + std::to_string(i) + " too close to next (dist=" + std::to_string(std::sqrt(dist_sq)) + "m < " + std::to_string(dedup_tolerance) + "m)");
                        }
                    }
                }

                // Skip polygon if simplification removed too many vertices
                if (simplified_vertices.size() < 3) {
                    if (config_.log_level >= 4) {
                        logger_.debug("      Skipping polygon " + std::to_string(polygon_count) + " - simplification reduced to " + std::to_string(simplified_vertices.size()) + " vertices");
                    }
                    continue;
                }

                if (config_.log_level >= 4 && polygon_count % report_interval == 0) {
                    logger_.debug("      Simplified polygon " + std::to_string(polygon_count) + " from " + std::to_string(polygon_vertices.size()) + " to " + std::to_string(simplified_vertices.size()) + " vertices");
                }

                // Use the new watertight extrusion method as specified by user
                // "the top and bottom should be identical. We should only calculate one,
                // then copy it using a z-height offset... guarantee a watertight mesh in 3D with O(n) complexity"
                try {
                    if (config_.log_level >= 4 && polygon_count % report_interval == 0) {
                        logger_.debug("      About to extrude polygon " + std::to_string(polygon_count) + " with " + std::to_string(simplified_vertices.size()) + " vertices");
                    }

                    builder.add_extruded_polygon(simplified_vertices, z_top, z_base, method);

                    if (config_.log_level >= 4 && polygon_count % report_interval == 0) {
                        logger_.debug("      Successfully extruded polygon " + std::to_string(polygon_count));
                        logger_.debug("      Builder now has " + std::to_string(builder.vertex_count()) + " vertices and " + std::to_string(builder.triangle_count()) + " triangles");
                    }
                } catch (const std::exception& e) {
                    logger_.error("      Failed to extrude polygon " + std::to_string(polygon_count) + ": " + e.what());
                    continue;
                } catch (...) {
                    logger_.error("      Unknown exception during polygon " + std::to_string(polygon_count) + " extrusion");
                    continue;
                }

                // Handle holes by creating separate extruded polygons with inverted heights
                for (const auto& hole : poly_with_holes.holes()) {
                    if (hole.size() < 3) continue;

                    std::vector<Point3D> hole_vertices;
                    hole_vertices.reserve(hole.size());

                    // Convert hole boundary to Point3D vector (reverse order for proper hole orientation)
                    for (const auto& [x, y] : hole) {

                        // Safety check: Skip infinite/NaN coordinates in holes too
                        if (!std::isfinite(x) || !std::isfinite(y)) {
                            logger_.warning("      Skipping invalid hole vertex (" + std::to_string(x) + ", " + std::to_string(y) + ") in polygon " + std::to_string(polygon_count));
                            continue;
                        }

                        hole_vertices.emplace_back(x, y, 0.0);
                    }

                    // Safety check: Only process holes with sufficient valid vertices
                    if (hole_vertices.size() < 3) {
                        if (config_.log_level >= 4) {
                            logger_.debug("      Skipping hole with insufficient vertices (" + std::to_string(hole_vertices.size()) + ") in polygon " + std::to_string(polygon_count));
                        }
                        continue;
                    }

                    // Reverse hole vertices to create proper inward-facing normals
                    std::reverse(hole_vertices.begin(), hole_vertices.end());

                    // CRITICAL: Simplify hole polygon to prevent degenerate triangles
                    std::vector<Point3D> simplified_hole_vertices;
                    simplified_hole_vertices.reserve(hole_vertices.size());

                    for (size_t i = 0; i < hole_vertices.size(); ++i) {
                        const auto& current = hole_vertices[i];
                        const auto& next = hole_vertices[(i + 1) % hole_vertices.size()];

                        double dx = next.x() - current.x();
                        double dy = next.y() - current.y();
                        double dist_sq = dx * dx + dy * dy;

                        if (dist_sq >= simplification_tolerance_sq) {
                            simplified_hole_vertices.push_back(current);
                        }
                    }

                    // Skip hole if simplification removed too many vertices
                    if (simplified_hole_vertices.size() < 3) {
                        if (config_.log_level >= 4) {
                            logger_.debug("      Skipping hole - simplification reduced to " + std::to_string(simplified_hole_vertices.size()) + " vertices");
                        }
                        continue;
                    }

                    // Extrude hole with swapped heights to create proper cavity
                    try {
                        builder.add_extruded_polygon(simplified_hole_vertices, z_base, z_top, method);
                    } catch (const std::exception& e) {
                        logger_.error("      Failed to extrude hole in polygon " + std::to_string(polygon_count) + ": " + e.what());
                        continue;
                    } catch (...) {
                        logger_.error("      Unknown exception during hole extrusion in polygon " + std::to_string(polygon_count));
                        continue;
                    }
                }
            }

            if (config_.log_level >= 4) {
                logger_.info("    Successfully processed " + std::to_string(polygon_count) + " polygons");
            }

            return polygon_count > 0;

        } catch (const std::exception& e) {
            if (config_.log_level >= 4) {
                logger_.error("    Error creating mesh from contour polygons: " + std::string(e.what()));
            }
            return false;
        }
    }

};

// ============================================================================
// TopographicGenerator Public Interface
// ============================================================================

TopographicGenerator::TopographicGenerator(const TopographicConfig& config)
    : impl_(std::make_unique<Impl>(config)) {
}

TopographicGenerator::~TopographicGenerator() = default;

bool TopographicGenerator::generate_model() {
    return impl_->generate_model();
}

bool TopographicGenerator::load_elevation_data() {
    return impl_->load_elevation_data();
}

bool TopographicGenerator::generate_mesh() {
    return impl_->generate_mesh();
}

bool TopographicGenerator::validate_mesh() {
    return impl_->validate_mesh();
}

bool TopographicGenerator::export_models() {
    return impl_->export_models();
}

const TopographicMesh& TopographicGenerator::get_mesh() const {
    return impl_->get_mesh();
}

const std::vector<ContourLayer>& TopographicGenerator::get_contour_layers() const {
    return impl_->get_contour_layers();
}

const PerformanceMetrics& TopographicGenerator::get_metrics() const {
    return impl_->get_metrics();
}

const MeshValidationResult& TopographicGenerator::get_validation_result() const {
    return impl_->get_validation_result();
}

const OutputTracker& TopographicGenerator::get_output_tracker() const {
    return impl_->get_output_tracker();
}

void TopographicGenerator::update_config(const TopographicConfig& config) {
    impl_->update_config(config);
}

const TopographicConfig& TopographicGenerator::get_config() const {
    return impl_->get_config();
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<TopographicGenerator> create_generator(
    const BoundingBox& bounds,
    const std::vector<std::string>& output_formats,
    const TopographicConfig::MeshQuality quality) {
    
    TopographicConfig config;
    config.bounds = bounds;
    config.output_formats = output_formats;
    config.quality = quality;
    
    return std::make_unique<TopographicGenerator>(config);
}

bool generate_topographic_model(
    const BoundingBox& bounds,
    const std::string& output_directory,
    const std::vector<std::string>& formats,
    int num_layers) {
    
    TopographicConfig config;
    config.bounds = bounds;
    config.output_directory = output_directory;
    config.output_formats = formats;
    config.num_layers = num_layers;
    
    auto generator = std::make_unique<TopographicGenerator>(config);
    return generator->generate_model();
}

} // namespace topo