/**
 * @file TilePreloadWorker.cpp
 * @brief Implementation of background tile preloading worker
 */

#include "TilePreloadWorker.hpp"

// Temporarily undefine Qt's emit macro to avoid conflict with TBB headers
#ifdef emit
#define EMIT_DEFINED
#undef emit
#endif

#include "../../core/SRTMDownloader.hpp"

// Restore Qt's emit macro
#ifdef EMIT_DEFINED
#define emit
#undef EMIT_DEFINED
#endif

#include <cmath>
#include <vector>
#include <string>

namespace topo {

TilePreloadWorker::TilePreloadWorker(const PreloadParams& params, QObject* parent)
    : QThread(parent), params_(params) {
}

void TilePreloadWorker::run() {
    try {
        // Create SRTM downloader with cache directory
        SRTMDownloader::Config config;
        config.cache_directory = params_.cacheDir.toStdString();
        SRTMDownloader downloader(config);

        // Calculate which tiles are needed for the bounding box
        // SRTM tiles are 1° x 1° and named by their southwest corner
        int minLat = static_cast<int>(std::floor(params_.minLat));
        int maxLat = static_cast<int>(std::floor(params_.maxLat));
        int minLon = static_cast<int>(std::floor(params_.minLon));
        int maxLon = static_cast<int>(std::floor(params_.maxLon));

        // Generate list of all tiles needed
        std::vector<std::string> tilesNeeded;
        for (int lat = minLat; lat <= maxLat; ++lat) {
            for (int lon = minLon; lon <= maxLon; ++lon) {
                std::string filename = SRTMDownloader::get_tile_filename(lat, lon);
                tilesNeeded.push_back(filename);
            }
        }

        // Check which tiles are already cached
        int cachedCount = 0;
        std::vector<std::string> tilesToDownload;

        for (const auto& tile : tilesNeeded) {
            if (downloader.tile_exists_in_cache(tile)) {
                cachedCount++;
            } else {
                tilesToDownload.push_back(tile);
            }
        }

        // Emit cache status
        emit tilesFound(tilesNeeded.size(), cachedCount, tilesToDownload.size());

        // If all tiles are cached, we're done
        if (tilesToDownload.empty()) {
            emit preloadComplete(0);
            return;
        }

        // Download missing tiles
        // Note: SRTMDownloader::download_tiles() already handles the actual downloading
        // We just need to call it with the bounding box
        BoundingBox bounds(params_.minLon, params_.minLat,
                          params_.maxLon, params_.maxLat);

        downloader.download_tiles(bounds);

        // Emit completion signal
        emit preloadComplete(tilesToDownload.size());

    } catch (const std::exception& e) {
        // Emit failure but don't crash - this is a background operation
        emit preloadFailed(QString::fromStdString(e.what()));
    } catch (...) {
        emit preloadFailed(QString("Unknown error during tile preload"));
    }
}

} // namespace topo
