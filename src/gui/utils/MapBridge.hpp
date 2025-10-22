#pragma once

/**
 * @file MapBridge.hpp
 * @brief Qt↔JavaScript bridge for Leaflet map communication
 *
 * Provides QWebChannel interface for bidirectional communication
 * between Qt (C++) and Leaflet (JavaScript) map.
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QObject>

namespace topo {

/**
 * @brief Bridge object for Qt ↔ JavaScript communication via QWebChannel
 *
 * This object is exposed to JavaScript via QWebChannel and receives
 * callbacks from the Leaflet map when bounds or zoom changes.
 */
class MapBridge : public QObject {
    Q_OBJECT

public:
    explicit MapBridge(QObject* parent = nullptr) : QObject(parent) {}

public slots:
    /**
     * @brief Called from JavaScript when map bounds change
     * @param minLat Minimum latitude (south)
     * @param minLon Minimum longitude (west)
     * @param maxLat Maximum latitude (north)
     * @param maxLon Maximum longitude (east)
     */
    void onBoundsChanged(double minLat, double minLon, double maxLat, double maxLon) {
        emit boundsChanged(minLat, minLon, maxLat, maxLon);
    }

    /**
     * @brief Called from JavaScript when map is moved/zoomed
     * @param lat Center latitude
     * @param lon Center longitude
     * @param zoom Current zoom level (supports fractional zoom)
     */
    void onMapMoved(double lat, double lon, double zoom) {
        emit mapMoved(lat, lon, zoom);
    }

signals:
    /**
     * @brief Emitted when map bounds change (from JavaScript)
     */
    void boundsChanged(double minLat, double minLon, double maxLat, double maxLon);

    /**
     * @brief Emitted when map is moved or zoomed (from JavaScript)
     */
    void mapMoved(double lat, double lon, double zoom);
};

} // namespace topo
