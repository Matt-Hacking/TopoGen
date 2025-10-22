#pragma once

/**
 * @file ZoomControls.hpp
 * @brief Visual zoom control overlay for map widget
 *
 * Provides zoom in/out buttons and zoom level display in a semi-transparent
 * overlay positioned in the corner of the map view.
 */

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>

namespace topo {

/**
 * @brief Visual zoom controls overlay
 *
 * Displays zoom in/out buttons and current zoom level in a modern,
 * semi-transparent overlay widget.
 */
class ZoomControls : public QWidget {
    Q_OBJECT

public:
    explicit ZoomControls(QWidget *parent = nullptr);
    ~ZoomControls() override = default;

    /**
     * @brief Update the displayed zoom level and calculate altitude
     * @param zoom Current zoom level (1.0-18.0 for OSM tiles, supports fractional values)
     */
    void setZoomLevel(double zoom);

    /**
     * @brief Update the displayed zoom level with viewport-aware altitude calculation
     * @param zoom Current zoom level (1.0-18.0 for OSM tiles, supports fractional values)
     * @param viewportWidth Width of the map viewport in pixels
     * @param viewportHeight Height of the map viewport in pixels
     */
    void setZoomLevel(double zoom, int viewportWidth, int viewportHeight);

    /**
     * @brief Set altitude display with units
     * @param altitudeMeters Eye altitude in meters
     * @param useMetric True for metric units, false for imperial
     */
    void setAltitude(double altitudeMeters, bool useMetric);

    /**
     * @brief Set unit system preference
     * @param useMetric True for metric units, false for imperial
     */
    void setUseMetric(bool useMetric);

    /**
     * @brief Set position of controls in parent widget
     * @param x X coordinate (typically parent width - control width - margin)
     * @param y Y coordinate (typically a small top margin)
     */
    void updatePosition(int x, int y);

signals:
    /**
     * @brief Emitted when user clicks zoom in button
     */
    void zoomInClicked();

    /**
     * @brief Emitted when user clicks zoom out button
     */
    void zoomOutClicked();

private:
    void setupUI();
    void applyStyle();
    double zoomToAltitudeMeters(double zoom) const;
    double zoomToAltitudeMeters(double zoom, int viewportWidth, int viewportHeight) const;

    QPushButton* zoomInButton_;
    QPushButton* zoomOutButton_;
    QLabel* zoomLevelLabel_;
    QLabel* altitudeLabel_;
    QVBoxLayout* layout_;

    double currentZoom_;
    bool useMetric_;
};

} // namespace topo
