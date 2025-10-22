/**
 * @file GenerationWorker.cpp
 * @brief Implementation of background worker thread for generation
 */

#include "GenerationWorker.hpp"

// Undefine Qt's emit macro before including TBB headers (via topographic_generator.hpp)
// TBB has methods called emit() which conflict with Qt's emit macro
#ifdef emit
#undef emit
#define QT_EMIT_BACKUP
#endif

#include "../../include/topographic_generator.hpp"
#include "../../core/Logger.hpp"
#include "../../core/OutputTracker.hpp"

// Restore Qt's emit macro after TBB headers
#ifdef QT_EMIT_BACKUP
#define emit
#undef QT_EMIT_BACKUP
#endif

#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>
#include <numeric>
#include <algorithm>

namespace topo {

GenerationWorker::GenerationWorker(const GenerationParams& params, QObject* parent)
    : QThread(parent), params_(params) {
}

GenerationWorker::~GenerationWorker() {
    // Wait for thread to finish if still running
    if (isRunning()) {
        quit();
        wait();
    }
}

void GenerationWorker::run() {
    try {
        emit progressUpdate(0, "Starting generation...");

        // Parse configuration JSON
        QJsonDocument doc = QJsonDocument::fromJson(params_.configJson.toUtf8());
        if (!doc.isObject()) {
            emit generationFailed("Invalid configuration JSON");
            return;
        }

        QJsonObject config = doc.object();

        // Debug: Log configuration JSON
        qDebug() << "GenerationWorker: Configuration JSON:" << params_.configJson;

        // Create output directory if needed
        QDir dir(params_.outputDir);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                emit generationFailed("Failed to create output directory: " + params_.outputDir);
                return;
            }
        }

        emit progressUpdate(5, "Configuring topographic generator...");

        // Create TopographicConfig - struct initialization gives us CLI defaults
        TopographicConfig topoConfig;

        // Explicitly set all critical defaults that CLI relies on (matching topographic_generator.hpp)
        // This ensures we have the same baseline as CLI before applying user overrides
        topoConfig.log_level = params_.logLevel;  // Use log level from preferences (1-6)
        topoConfig.output_layers = true;
        topoConfig.output_stacked = false;
        topoConfig.remove_holes = true;
        topoConfig.add_registration_marks = true;
        topoConfig.terrain_following = false;
        topoConfig.force_all_layers = false;
        topoConfig.inset_upper_layers = false;

        // Now override with actual parameters
        // Set geographic bounds
        topoConfig.bounds = BoundingBox(params_.minLon, params_.minLat, params_.maxLon, params_.maxLat);
        topoConfig.upper_left_lat = params_.maxLat;
        topoConfig.upper_left_lon = params_.minLon;
        topoConfig.lower_right_lat = params_.minLat;
        topoConfig.lower_right_lon = params_.maxLon;

        // Set output configuration
        topoConfig.base_name = params_.basename.toStdString();
        topoConfig.output_directory = params_.outputDir.toStdString();
        topoConfig.cache_directory = params_.cacheDir.toStdString();
        topoConfig.terrain_following = params_.terrainFollowing;

        // Configure format-specific settings based on output type
        switch (params_.outputType) {
            case OutputType::Raster:
                configureRasterSettings(topoConfig, config);
                break;
            case OutputType::Vector:
                configureVectorSettings(topoConfig, config);
                qDebug() << "GenerationWorker: Vector settings configured";
                qDebug() << "  Output formats:" << QString::fromStdString(
                    std::accumulate(topoConfig.output_formats.begin(), topoConfig.output_formats.end(), std::string(),
                        [](const std::string& a, const std::string& b) {
                            return a.empty() ? b : a + ", " + b;
                        }));
                qDebug() << "  Output layers:" << topoConfig.output_layers;
                qDebug() << "  Num layers:" << topoConfig.num_layers;
                qDebug() << "  Height per layer:" << topoConfig.height_per_layer;
                qDebug() << "  Substrate size:" << topoConfig.substrate_size_mm;
                qDebug() << "  Layer thickness:" << topoConfig.layer_thickness_mm;
                break;
            case OutputType::Mesh:
                configureMeshSettings(topoConfig, config);
                break;
        }

        emit progressUpdate(10, "Initializing topographic generator...");

        // Create and run generator
        Logger logger("GenerationWorker");
        logger.setLogLevel(LogLevel::DEBUG);  // Enable debug logging to see what's happening

        qDebug() << "GenerationWorker: Creating TopographicGenerator with config:";
        qDebug() << "  Bounds:" << topoConfig.bounds.min_x << topoConfig.bounds.min_y
                 << "to" << topoConfig.bounds.max_x << topoConfig.bounds.max_y;
        qDebug() << "  Output directory:" << QString::fromStdString(topoConfig.output_directory);
        qDebug() << "  Base name:" << QString::fromStdString(topoConfig.base_name);
        qDebug() << "  Log level:" << topoConfig.log_level;

        TopographicGenerator generator(topoConfig);

        emit progressUpdate(20, "Loading elevation data...");

        // Run generation pipeline with comprehensive logging and exception handling
        qDebug() << "GenerationWorker: Calling generate_model()...";
        qDebug() << "GenerationWorker: Output formats:" << QString::fromStdString(
            std::accumulate(topoConfig.output_formats.begin(), topoConfig.output_formats.end(), std::string(),
                [](const std::string& a, const std::string& b) {
                    return a.empty() ? b : a + ", " + b;
                }));

        bool generationSuccess = false;
        try {
            generationSuccess = generator.generate_model();
            qDebug() << "GenerationWorker: generate_model() returned:" << (generationSuccess ? "SUCCESS" : "FAILURE");
        } catch (const std::exception& e) {
            qDebug() << "GenerationWorker: EXCEPTION caught in generate_model():" << e.what();
            emit generationFailed(QString("Exception during generation: %1").arg(e.what()));
            return;
        } catch (...) {
            qDebug() << "GenerationWorker: UNKNOWN EXCEPTION caught in generate_model()";
            emit generationFailed("Unknown exception during generation");
            return;
        }

        if (!generationSuccess) {
            qDebug() << "GenerationWorker: Generation returned false - model creation failed";
            emit generationFailed("Generation failed during model creation");
            return;
        }

        emit progressUpdate(90, "Finalizing output files...");

        // Get list of generated files from OutputTracker
        const auto& tracker = generator.get_output_tracker();
        auto outputFiles = tracker.getOutputFiles();

        qDebug() << "GenerationWorker: OutputTracker reports" << outputFiles.size() << "files generated";

        if (outputFiles.empty()) {
            qDebug() << "GenerationWorker: ERROR - No output files were generated!";
            emit generationFailed("No output files were generated");
            return;
        }

        // Convert to QStringList and verify files actually exist
        QStringList qOutputFiles;
        for (const auto& file : outputFiles) {
            QString qFile = QString::fromStdString(file);
            qOutputFiles << qFile;

            // Check if file exists and log its size
            QFileInfo fileInfo(qFile);
            if (fileInfo.exists()) {
                qint64 fileSize = fileInfo.size();
                qDebug() << "GenerationWorker: File exists:" << qFile << "(" << fileSize << "bytes)";
                if (fileSize == 0) {
                    qDebug() << "GenerationWorker: WARNING - File is EMPTY!";
                } else if (fileSize < 100) {
                    qDebug() << "GenerationWorker: WARNING - File is very small, possibly blank";
                }
            } else {
                qDebug() << "GenerationWorker: ERROR - File does NOT exist:" << qFile;
            }
        }

        emit progressUpdate(100, "Generation complete");
        emit generationComplete(qOutputFiles);

    } catch (const std::exception& e) {
        emit generationFailed(QString("Generation failed: %1").arg(e.what()));
    } catch (...) {
        emit generationFailed("Unknown error during generation");
    }
}

void GenerationWorker::configureRasterSettings(TopographicConfig& config, const QJsonObject& json) {
    // Extract formats
    QJsonObject formats = json["formats"].toObject();
    bool generatePNG = formats["png"].toBool(true);
    bool generateGeoTIFF = formats["geotiff"].toBool(false);

    std::vector<std::string> outputFormats;
    if (generatePNG) outputFormats.push_back("png");
    if (generateGeoTIFF) outputFormats.push_back("geotiff");
    config.output_formats = outputFormats;

    // Extract render mode
    int renderMode = json["render_mode"].toInt(0);
    if (renderMode == 0) config.render_mode = TopographicConfig::RenderMode::FULL_COLOR;
    else if (renderMode == 1) config.render_mode = TopographicConfig::RenderMode::GRAYSCALE;
    else if (renderMode == 2) config.render_mode = TopographicConfig::RenderMode::MONOCHROME;

    // Extract settings
    config.print_resolution_dpi = json["dpi"].toInt(600);
    config.num_layers = json["num_layers"].toInt(10);
    config.height_per_layer = json["height_per_layer"].toDouble(21.43);
    config.layer_thickness_mm = json["layer_thickness"].toDouble(3.0);
    config.output_layers = true;
}

void GenerationWorker::configureVectorSettings(TopographicConfig& config, const QJsonObject& json) {
    // Extract formats
    QJsonObject formats = json["formats"].toObject();
    bool generateSVG = formats["svg"].toBool(true);
    bool generateGeoJSON = formats["geojson"].toBool(false);
    bool generateShapefile = formats["shapefile"].toBool(false);

    std::vector<std::string> outputFormats;
    if (generateSVG) outputFormats.push_back("svg");
    if (generateGeoJSON) outputFormats.push_back("geojson");
    if (generateShapefile) outputFormats.push_back("shapefile");
    config.output_formats = outputFormats;

    // Extract settings
    config.contour_interval = json["contour_interval"].toDouble(100.0);
    config.simplification_tolerance = json["simplification"].toDouble(5.0);
    config.stroke_width = json["stroke_width"].toDouble(0.2);
    config.remove_holes = json["remove_holes"].toBool(true);
    config.inset_upper_layers = json["inset_upper_layers"].toBool(false);
    config.inset_offset_mm = json["inset_offset"].toDouble(1.0);
    config.include_roads = json["include_roads"].toBool(false);
    config.include_buildings = json["include_buildings"].toBool(false);
    config.include_waterways = json["include_waterways"].toBool(false);
    config.num_layers = json["num_layers"].toInt(10);
    config.height_per_layer = json["height_per_layer"].toDouble(21.43);

    // SVG needs physical dimensions for scaling
    config.substrate_size_mm = json["substrate_size"].toDouble(200.0);
    config.layer_thickness_mm = json["layer_thickness"].toDouble(3.0);

    config.output_layers = true;
}

void GenerationWorker::configureMeshSettings(TopographicConfig& config, const QJsonObject& json) {
    // Extract formats
    QJsonObject formats = json["formats"].toObject();
    bool generateSTL = formats["stl"].toBool(true);
    bool generateOBJ = formats["obj"].toBool(false);
    bool generatePLY = formats["ply"].toBool(false);

    std::vector<std::string> outputFormats;
    if (generateSTL) outputFormats.push_back("stl");
    if (generateOBJ) outputFormats.push_back("obj");
    if (generatePLY) outputFormats.push_back("ply");
    config.output_formats = outputFormats;

    // Extract quality
    int quality = json["quality"].toInt(1);
    if (quality == 0) config.quality = TopographicConfig::MeshQuality::DRAFT;
    else if (quality == 2) config.quality = TopographicConfig::MeshQuality::HIGH;
    else if (quality == 3) config.quality = TopographicConfig::MeshQuality::ULTRA;
    else config.quality = TopographicConfig::MeshQuality::MEDIUM;

    // Extract settings
    config.num_layers = json["num_layers"].toInt(10);
    config.height_per_layer = json["height_per_layer"].toDouble(21.43);
    config.layer_thickness_mm = json["layer_thickness"].toDouble(3.0);
    config.substrate_size_mm = json["substrate_size"].toDouble(200.0);
    config.output_layers = true;
    config.output_stacked = json["output_stacked"].toBool(false);
}

} // namespace topo
