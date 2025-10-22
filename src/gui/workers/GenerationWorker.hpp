#pragma once

/**
 * @file GenerationWorker.hpp
 * @brief Background worker thread for topographic model generation
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QThread>
#include <QString>
#include <QRectF>
#include <QJsonObject>

// Forward declarations
namespace topo {
    struct TopographicConfig;
}

namespace topo {

class GenerationWorker : public QThread {
    Q_OBJECT

public:
    enum class OutputType {
        Raster,
        Vector,
        Mesh
    };

    struct GenerationParams {
        OutputType outputType;
        double minLat;
        double minLon;
        double maxLat;
        double maxLon;
        QString configJson;
        QString outputDir;
        QString basename;
        QString cacheDir;  // Directory for elevation and map tile cache
        bool terrainFollowing;
        int logLevel = 3;  // Log level from preferences (1-6)
    };

    explicit GenerationWorker(const GenerationParams& params, QObject* parent = nullptr);
    ~GenerationWorker() override;

    void run() override;

signals:
    void progressUpdate(int percentage, const QString& message);
    void generationComplete(const QStringList& outputFiles);
    void generationFailed(const QString& errorMessage);

private:
    void configureRasterSettings(TopographicConfig& config, const QJsonObject& json);
    void configureVectorSettings(TopographicConfig& config, const QJsonObject& json);
    void configureMeshSettings(TopographicConfig& config, const QJsonObject& json);

    GenerationParams params_;
};

} // namespace topo
