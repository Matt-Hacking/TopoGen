/**
 * @file main.cpp
 * @brief Main entry point for the C++ Topographic Generator
 * 
 * High-performance topographic model generator using professional
 * geometry libraries and algorithms adapted from Bambu Slicer.
 * 
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "topographic_generator.hpp"
#include "core/OutputTracker.hpp"
#include "cli/CommandLineInterface.hpp"
#include "cli/ExportOrchestrator.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>

using namespace topo;

/**
 * @brief Parse command line arguments using built-in parser
 */
bool parse_command_line(int argc, char* argv[], TopographicConfig& config, bool& dry_run) {
    CommandLineInterface cli;

    if (!cli.parse_arguments(argc, argv)) {
        return false;  // Help shown or error occurred
    }

    config = cli.get_config();
    dry_run = cli.is_dry_run();

    return true;
}

/**
 * @brief Print configuration summary
 */
void print_configuration(const TopographicConfig& config) {
    std::cout << "\n=== Topographic Generator Configuration ===\n";
    std::cout << "Input bounds: (" 
              << config.bounds.min_x << ", " << config.bounds.min_y << ") to ("
              << config.bounds.max_x << ", " << config.bounds.max_y << ")\n";
    std::cout << "Layers: " << config.num_layers << "\n";
    std::cout << "Layer thickness: " << config.layer_thickness_mm << "mm\n";
    std::cout << "Substrate size: " << config.substrate_size_mm << "mm\n";
    
    if (config.min_elevation.has_value()) {
        std::cout << "Min elevation: " << *config.min_elevation << "m\n";
    }
    if (config.max_elevation.has_value()) {
        std::cout << "Max elevation: " << *config.max_elevation << "m\n";
    }
    
    std::cout << "Processing mode: " 
              << (config.terrain_following ? "terrain-following" : "contour map") << "\n";
    std::cout << "Parallel processing: " 
              << (config.parallel_processing ? "enabled" : "disabled") << "\n";
    if (config.parallel_processing && config.num_threads > 0) {
        std::cout << "Threads: " << config.num_threads << "\n";
    }
    
    std::cout << "Output formats: ";
    for (size_t i = 0; i < config.output_formats.size(); ++i) {
        std::cout << config.output_formats[i];
        if (i < config.output_formats.size() - 1) std::cout << ", ";
    }
    std::cout << "\n";
    
    std::cout << "Output directory: " << config.output_directory << "\n";
    std::cout << "Base filename: " << config.base_name << "\n";
    std::cout << "============================================\n\n";
}

/**
 * @brief Print performance summary
 */
void print_performance_summary(const PerformanceMetrics& metrics) {
    std::cout << "\n=== Performance Summary ===\n";
    std::cout << "Elevation loading: " << metrics.elevation_loading_time.count() << "ms\n";
    std::cout << "Mesh generation: " << metrics.mesh_generation_time.count() << "ms\n";
    std::cout << "Slicing time: " << metrics.slicing_time.count() << "ms\n";
    std::cout << "Export time: " << metrics.export_time.count() << "ms\n";
    std::cout << "Total time: " << metrics.total_time.count() << "ms\n";
    std::cout << "Triangles generated: " << metrics.triangles_generated << "\n";
    std::cout << "Vertices generated: " << metrics.vertices_generated << "\n";
    std::cout << "Peak memory usage: " << metrics.memory_peak_mb << "MB\n";
    std::cout << "============================\n";
}

/**
 * @brief Print mesh validation results
 */
void print_validation_results(const MeshValidationResult& result) {
    std::cout << "\n=== Mesh Validation Results ===\n";
    std::cout << "Manifold: " << (result.is_manifold ? "YES" : "NO") << "\n";
    std::cout << "Watertight: " << (result.is_watertight ? "YES" : "NO") << "\n";
    
    if (result.num_non_manifold_edges > 0) {
        std::cout << "Non-manifold edges: " << result.num_non_manifold_edges << "\n";
    }
    if (result.num_degenerate_triangles > 0) {
        std::cout << "Degenerate triangles: " << result.num_degenerate_triangles << "\n";
    }
    if (result.num_duplicate_vertices > 0) {
        std::cout << "Duplicate vertices: " << result.num_duplicate_vertices << "\n";
    }
    
    if (result.is_valid()) {
        std::cout << "Overall quality: EXCELLENT ✓\n";
    } else if (result.is_manifold) {
        std::cout << "Overall quality: GOOD (minor issues)\n";
    } else {
        std::cout << "Overall quality: NEEDS REPAIR\n";
    }
    std::cout << "===============================\n";
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // Parse command line arguments
        TopographicConfig config;
        bool dry_run = false;

        if (!parse_command_line(argc, argv, config, dry_run)) {
            return 0;  // Help was shown or parsing failed
        }

        // Print banner only if not silent
        if (config.log_level > 0) {
            std::cout << "Topographic Generator v2.0016\n";
            std::cout << "High-performance topographic modeling using professional algorithms\n";
            std::cout << "Based on Bambu Slicer, GDAL geospatial data, and CGAL geometry libraries\n";
        }

        if (config.log_level >= 4) {  // INFO level or higher
            print_configuration(config);
        }

        if (dry_run) {
            if (config.log_level > 0) {
                std::cout << "Dry run mode - configuration validated successfully\n";
            }
            return 0;
        }

        // Create output directory
        std::filesystem::create_directories(config.output_directory);

        // Create and configure generator
        if (config.log_level > 0) {
            std::cout << "Initializing topographic generator...\n";
        }
        auto generator = std::make_unique<TopographicGenerator>(config);

        // Generate model (data only - no export)
        if (config.log_level > 0) {
            std::cout << "Starting model generation...\n";
        }
        bool success = generator->generate_model();

        if (!success) {
            std::cerr << "Error: Model generation failed\n";
            return 1;
        }

        // Export models using ExportOrchestrator
        if (config.log_level > 0) {
            std::cout << "Starting model export...\n";
        }
        ExportOrchestrator exporter(*generator);
        bool export_success = exporter.export_all_formats();

        if (!export_success) {
            std::cerr << "Error: Model export failed\n";
            return 1;
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);

        // Print results
        const auto& metrics = generator->get_metrics();
        const auto& validation = generator->get_validation_result();

        // Only print detailed summaries at INFO level (4) or higher
        if (config.log_level >= 4) {
            print_performance_summary(metrics);
            // Only print validation if meshes were actually generated (3D formats)
            if (metrics.triangles_generated > 0 || metrics.vertices_generated > 0) {
                print_validation_results(validation);
            }
        }

        if (config.log_level > 0) {
            std::cout << "\nModel generation completed successfully in "
                      << total_duration.count() << "ms\n";
        }

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
}

// Example usage commands:
//
// Basic STL generation:
// ./topo-gen --bounds "47.6062,-122.3321,47.6020,-122.3280" --layers 10 --formats stl
//
// Multi-format with terrain following:
// ./topo-gen -b "47.6062,-122.3321,47.6020,-122.3280" \
//            --terrain-following \
//            --formats stl,obj,nurbs \
//            --quality high \
//            --nurbs-quality high
//
// High-performance parallel generation:
// ./topo-gen -b "47.6062,-122.3321,47.6020,-122.3280" \
//            --layers 20 \
//            --threads 8 \
//            --formats stl,obj \
//            --quality ultra \
//            --obj-colors \
//            --color-scheme terrain// Test comment for incremental build
// Another test comment
