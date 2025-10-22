#pragma once

/**
 * @file MapWidget.hpp
 * @brief Interactive map widget using embedded Leaflet.js
 *
 * Features:
 * - Fast tile rendering via browser engine (Leaflet.js)
 * - Qt zoom controls overlay
 * - JavaScript ↔ Qt communication via QWebChannel
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QWidget>
#include <QWebEngineView>
#include <QWebChannel>
#include <QPointF>
#include <optional>
#include <tuple>

namespace topo {

class ZoomControls;
class MapBridge;

class MapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MapWidget(QWidget *parent = nullptr);
    ~MapWidget() override;

    // Map state
    std::pair<QPointF, double> getMapState() const;
    void setMapState(const QPointF& center, double zoom);

    // Map navigation
    void centerOn(double lat, double lon, double zoom = -1.0);
    void fitBounds(double min_lat, double min_lon, double max_lat, double max_lon);

    // Cache configuration (no-op for browser-based implementation)
    void setCacheDirectory(const QString& dir);

    // Unit system
    void setUseMetric(bool useMetric);

    // Layer controls (calls JavaScript)
    void setContourLinesVisible(bool visible);
    bool isContourLinesVisible() const { return contourLinesVisible_; }

    void setTopoMapVisible(bool visible);
    bool isTopoMapVisible() const { return topoMapVisible_; }

    // Label visibility controls
    void setPeaksVisible(bool visible);

    // Map bounds
    std::optional<std::tuple<double, double, double, double>> getCurrentBounds() const;

signals:
    void selectionChanged(double min_lat, double min_lon, double max_lat, double max_lon);
    void mapMoved(double center_lat, double center_lon, double zoom);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onWebViewLoadFinished(bool success);
    void onBridgeBoundsChanged(double minLat, double minLon, double maxLat, double maxLon);
    void onBridgeMapMoved(double lat, double lon, double zoom);

private:
    // Call JavaScript functions in Leaflet map
    void runJavaScript(const QString& script);
    void callJsFunction(const QString& function, const QStringList& args = QStringList());

    // Update overlay positions
    void updateOverlayPositions();

    // Map state
    double centerLat_;
    double centerLon_;
    double zoomLevel_;

    // Layer visibility state
    bool contourLinesVisible_;
    bool topoMapVisible_;
    bool peaksVisible_;

    // Current map bounds
    bool currentBoundsValid_;
    double minLat_;
    double minLon_;
    double maxLat_;
    double maxLon_;

    // Web view for Leaflet map
    QWebEngineView* webView_;
    QWebChannel* webChannel_;
    MapBridge* bridge_;
    bool webViewLoaded_;

    // Qt widget overlays
    ZoomControls* zoomControls_;

    // Constants
    static constexpr double MIN_ZOOM = 1.0;
    static constexpr double MAX_ZOOM = 18.0;
};

} // namespace topo
