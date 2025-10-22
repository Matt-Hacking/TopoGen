#pragma once

/**
 * @file TileDownloader.hpp
 * @brief Qt wrapper for OSM tile downloading with async signals
 *
 * Provides Qt-friendly interface to OSMTileCache for map display:
 * - Converts tile coordinates (z, x, y) to OSM URLs
 * - Async tile loading with signals
 * - Thread-safe tile caching
 * - QPixmap conversion for display
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QThreadPool>
#include <memory>

// Forward declare to avoid including the header
namespace topo {
    class OSMTileCache;
    struct OSMCacheConfig;
}

namespace topo {

/**
 * @brief Tile coordinates in OSM tile system
 */
struct TileCoord {
    int zoom;
    int x;
    int y;

    bool operator==(const TileCoord& other) const {
        return zoom == other.zoom && x == other.x && y == other.y;
    }
};

/**
 * @brief Hash function for TileCoord (required by QHash)
 */
inline size_t qHash(const TileCoord& key, size_t seed = 0) {
    // Use Qt's hash combine approach compatible with Qt6
    size_t h1 = ::qHash(static_cast<uint>(key.zoom), seed);
    size_t h2 = ::qHash(static_cast<uint>(key.x), seed);
    size_t h3 = ::qHash(static_cast<uint>(key.y), seed);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
}

/**
 * @brief Qt-friendly tile downloader wrapping OSMTileCache
 *
 * Downloads OSM tiles asynchronously and emits signals when complete.
 * Uses existing OSMTileCache for caching, HTTP headers, LRU cleanup.
 */
class TileDownloader : public QObject {
    Q_OBJECT

public:
    explicit TileDownloader(QObject *parent = nullptr,
                          const QString& urlTemplate = "https://tile.openstreetmap.org/{z}/{x}/{y}.png");
    ~TileDownloader() override;

    /**
     * @brief Request a tile (async)
     * Emits tileReady when downloaded/cached
     */
    void requestTile(int zoom, int x, int y);

    /**
     * @brief Check if tile is already cached
     */
    bool isCached(int zoom, int x, int y) const;

    /**
     * @brief Clear all cached tiles
     */
    void clearCache();

    /**
     * @brief Get cache statistics
     */
    QString getCacheStats() const;

    /**
     * @brief Configure cache settings
     */
    void setCacheDirectory(const QString& dir);
    void setMaxCacheSizeMB(int size_mb);

    /**
     * @brief Convert lat/lon to tile coordinates at zoom level
     */
    static TileCoord latLonToTile(double lat, double lon, int zoom);

    /**
     * @brief Convert tile coordinates to lat/lon (NW corner)
     */
    static std::pair<double, double> tileToLatLon(int zoom, int x, int y);

    /**
     * @brief Generate tile URL using the configured template
     */
    QString tileUrl(int zoom, int x, int y) const;

    /**
     * @brief Generate OSM tile URL (static convenience method)
     */
    static QString osmTileUrl(int zoom, int x, int y);

    /**
     * @brief Generate OpenTopoMap tile URL (static convenience method)
     */
    static QString openTopoMapUrl(int zoom, int x, int y);

signals:
    /**
     * @brief Emitted when a requested tile is ready
     */
    void tileReady(int zoom, int x, int y, QPixmap pixmap);

    /**
     * @brief Emitted when a tile download fails
     */
    void tileError(int zoom, int x, int y, QString error);

private:
    std::unique_ptr<OSMTileCache> cache_;
    QThreadPool* threadPool_;

    // Helper to convert cache config
    std::unique_ptr<OSMCacheConfig> cacheConfig_;

    // URL template (e.g., "https://tile.openstreetmap.org/{z}/{x}/{y}.png")
    QString urlTemplate_;
};

} // namespace topo

// Hash function for TileCoord to use in QHash
inline uint qHash(const topo::TileCoord& coord, uint seed = 0) {
    return qHash(coord.zoom, seed) ^ qHash(coord.x, seed) ^ qHash(coord.y, seed);
}
