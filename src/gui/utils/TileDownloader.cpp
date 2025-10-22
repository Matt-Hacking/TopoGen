/**
 * @file TileDownloader.cpp
 * @brief Implementation of Qt OSM tile downloader
 */

#include "TileDownloader.hpp"
#include "../../../src/core/OSMTileCache.hpp"
#include <QRunnable>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <cmath>
#include <numbers>

namespace topo {

namespace {
    /**
     * @brief Runnable task for async tile loading
     */
    class TileLoadTask : public QRunnable {
    public:
        TileLoadTask(TileDownloader* downloader, OSMTileCache* cache, int zoom, int x, int y)
            : downloader_(downloader), cache_(cache), zoom_(zoom), x_(x), y_(y) {
            setAutoDelete(true);
        }

        void run() override {
            QString url = downloader_->tileUrl(zoom_, x_, y_);
            QString tempPath = QString("/tmp/osm_tile_%1_%2_%3.png").arg(zoom_).arg(x_).arg(y_);

            qDebug() << "[TileLoadTask] Starting tile load:" << zoom_ << x_ << y_;
            qDebug() << "[TileLoadTask] URL:" << url;
            qDebug() << "[TileLoadTask] Temp path:" << tempPath;

            // Try to get tile (from cache or download)
            bool success = cache_->get_tile(url.toStdString(), tempPath.toStdString());

            qDebug() << "[TileLoadTask] get_tile() returned:" << success;
            qDebug() << "[TileLoadTask] Temp file exists:" << QFile::exists(tempPath);

            if (success && QFile::exists(tempPath)) {
                QPixmap pixmap(tempPath);
                qDebug() << "[TileLoadTask] Pixmap loaded, isNull:" << pixmap.isNull();
                if (!pixmap.isNull()) {
                    qDebug() << "[TileLoadTask] Emitting tileReady for" << zoom_ << x_ << y_;
                    emit downloader_->tileReady(zoom_, x_, y_, pixmap);
                } else {
                    qDebug() << "[TileLoadTask] Emitting tileError: Failed to load pixmap";
                    emit downloader_->tileError(zoom_, x_, y_, "Failed to load pixmap");
                }
                // Clean up temp file
                QFile::remove(tempPath);
            } else {
                qDebug() << "[TileLoadTask] Emitting tileError: Failed to download tile";
                emit downloader_->tileError(zoom_, x_, y_, "Failed to download tile");
            }
        }

    private:
        TileDownloader* downloader_;
        OSMTileCache* cache_;
        int zoom_, x_, y_;
    };
}

TileDownloader::TileDownloader(QObject *parent, const QString& urlTemplate)
    : QObject(parent),
      threadPool_(QThreadPool::globalInstance()),
      urlTemplate_(urlTemplate) {

    // Initialize cache config with generous defaults
    cacheConfig_ = std::make_unique<OSMCacheConfig>();

    // Use absolute path in application data directory for cache persistence
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    cacheConfig_->cache_directory = (appDataPath + "/cache/osm_tiles").toStdString();

    cacheConfig_->max_cache_size_mb = 1000;  // 1 GB default
    cacheConfig_->default_expiry = std::chrono::hours(24 * 30);  // 30 days
    cacheConfig_->enable_cache = true;
    cacheConfig_->user_agent = "TopographicGenerator/0.22";

    qDebug() << "[TileDownloader] Cache directory:" << QString::fromStdString(cacheConfig_->cache_directory);

    // Create cache instance
    cache_ = std::make_unique<OSMTileCache>(*cacheConfig_);
}

TileDownloader::~TileDownloader() {
    // Wait for all pending tasks
    threadPool_->waitForDone();
}

void TileDownloader::requestTile(int zoom, int x, int y) {
    qDebug() << "[TileDownloader] Requesting tile:" << zoom << x << y;
    // Create async task
    auto* task = new TileLoadTask(this, cache_.get(), zoom, x, y);
    threadPool_->start(task);
    qDebug() << "[TileDownloader] Task queued on thread pool";
}

bool TileDownloader::isCached(int zoom, int x, int y) const {
    QString url = this->tileUrl(zoom, x, y);
    return cache_->is_cached(url.toStdString());
}

void TileDownloader::clearCache() {
    cache_->clear_cache();
}

QString TileDownloader::getCacheStats() const {
    auto stats = cache_->get_stats();
    return QString("Cache: %1 hits, %2 misses, %3% hit rate, %4 MB")
        .arg(stats.cache_hits)
        .arg(stats.cache_misses)
        .arg(stats.hit_rate() * 100.0, 0, 'f', 1)
        .arg(stats.total_cache_size_bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

void TileDownloader::setCacheDirectory(const QString& dir) {
    qDebug() << "[TileDownloader] Setting cache directory to:" << dir;
    cacheConfig_->cache_directory = dir.toStdString();
    cache_->update_config(*cacheConfig_);
    qDebug() << "[TileDownloader] Cache directory updated";
}

void TileDownloader::setMaxCacheSizeMB(int size_mb) {
    cacheConfig_->max_cache_size_mb = size_mb;
    cache_->update_config(*cacheConfig_);
}

// Static utility methods

TileCoord TileDownloader::latLonToTile(double lat, double lon, int zoom) {
    // Web Mercator (Slippy Map Tilenames) projection
    // https://wiki.openstreetmap.org/wiki/Slippy_map_tilenames

    int n = 1 << zoom;  // 2^zoom

    // Longitude to tile X
    int x = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));

    // Latitude to tile Y (Web Mercator)
    double lat_rad = lat * std::numbers::pi / 180.0;
    int y = static_cast<int>(std::floor((1.0 - std::asinh(std::tan(lat_rad)) / std::numbers::pi) / 2.0 * n));

    // Clamp to valid tile range
    x = std::clamp(x, 0, n - 1);
    y = std::clamp(y, 0, n - 1);

    return {zoom, x, y};
}

std::pair<double, double> TileDownloader::tileToLatLon(int zoom, int x, int y) {
    // Convert tile coordinates to lat/lon (NW corner of tile)
    int n = 1 << zoom;  // 2^zoom

    // Tile X to longitude
    double lon = x / static_cast<double>(n) * 360.0 - 180.0;

    // Tile Y to latitude (inverse Web Mercator)
    double lat_rad = std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * y / static_cast<double>(n))));
    double lat = lat_rad * 180.0 / std::numbers::pi;

    return {lat, lon};
}

QString TileDownloader::tileUrl(int zoom, int x, int y) const {
    // Replace {z}, {x}, {y} placeholders in URL template
    QString url = urlTemplate_;
    url.replace("{z}", QString::number(zoom));
    url.replace("{x}", QString::number(x));
    url.replace("{y}", QString::number(y));
    return url;
}

QString TileDownloader::osmTileUrl(int zoom, int x, int y) {
    // OpenStreetMap tile server URL pattern
    return QString("https://tile.openstreetmap.org/%1/%2/%3.png")
        .arg(zoom)
        .arg(x)
        .arg(y);
}

QString TileDownloader::openTopoMapUrl(int zoom, int x, int y) {
    // OpenTopoMap tile server URL pattern (contour lines overlay)
    return QString("https://tile.opentopomap.org/%1/%2/%3.png")
        .arg(zoom)
        .arg(x)
        .arg(y);
}

} // namespace topo
