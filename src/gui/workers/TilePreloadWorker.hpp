#pragma once

/**
 * @file TilePreloadWorker.hpp
 * @brief Background worker thread for preloading SRTM elevation tiles
 *
 * Checks which tiles are needed for current map bounds and downloads
 * any missing tiles in the background before generation starts.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QThread>
#include <QString>

namespace topo {

/**
 * @brief Background worker for preloading elevation tiles
 *
 * Runs at low priority to avoid impacting UI responsiveness.
 * Downloads missing SRTM tiles for the current map bounds so they're
 * cached by the time the user clicks Generate.
 */
class TilePreloadWorker : public QThread {
    Q_OBJECT

public:
    /**
     * @brief Parameters for tile preloading
     */
    struct PreloadParams {
        double minLat;
        double minLon;
        double maxLat;
        double maxLon;
        QString cacheDir;
    };

    /**
     * @brief Construct preload worker
     * @param params Bounds and cache directory
     * @param parent Parent QObject
     */
    explicit TilePreloadWorker(const PreloadParams& params, QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~TilePreloadWorker() override = default;

    /**
     * @brief Run the preload operation
     *
     * Checks which tiles are needed, which are cached, and downloads
     * any missing tiles. Emits signals for progress tracking.
     */
    void run() override;

signals:
    /**
     * @brief Emitted after checking cache status
     * @param total Total number of tiles needed
     * @param cached Number already in cache
     * @param needDownload Number that need to be downloaded
     */
    void tilesFound(int total, int cached, int needDownload);

    /**
     * @brief Emitted during download progress
     * @param current Current tile being downloaded (1-based)
     * @param total Total tiles to download
     * @param tileName Name of current tile
     */
    void downloadProgress(int current, int total, const QString& tileName);

    /**
     * @brief Emitted when preloading completes successfully
     * @param downloaded Number of tiles that were downloaded
     */
    void preloadComplete(int downloaded);

    /**
     * @brief Emitted if preloading fails
     * @param error Error message
     */
    void preloadFailed(const QString& error);

private:
    PreloadParams params_;
};

} // namespace topo
