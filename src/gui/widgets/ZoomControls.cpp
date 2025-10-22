/**
 * @file ZoomControls.cpp
 * @brief Implementation of zoom control overlay
 */

#include "ZoomControls.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <cmath>

namespace topo {

ZoomControls::ZoomControls(QWidget *parent)
    : QWidget(parent),
      zoomInButton_(new QPushButton("+", this)),
      zoomOutButton_(new QPushButton("-", this)),
      zoomLevelLabel_(new QLabel("Zoom: 10.0", this)),
      altitudeLabel_(new QLabel("", this)),
      layout_(new QVBoxLayout(this)),
      currentZoom_(10.0),
      useMetric_(false) {  // Default to imperial, MainWindow will update from settings

    setupUI();
    applyStyle();

    // Connect button signals
    connect(zoomInButton_, &QPushButton::clicked, this, &ZoomControls::zoomInClicked);
    connect(zoomOutButton_, &QPushButton::clicked, this, &ZoomControls::zoomOutClicked);

    // Set initial altitude (will be updated when MainWindow applies settings)
    setAltitude(zoomToAltitudeMeters(10.0), useMetric_);
}

void ZoomControls::setupUI() {
    // Configure buttons
    zoomInButton_->setFixedSize(40, 40);
    zoomOutButton_->setFixedSize(40, 40);

    // Make buttons bold and larger font
    QFont buttonFont;
    buttonFont.setPointSize(18);
    buttonFont.setBold(true);
    zoomInButton_->setFont(buttonFont);
    zoomOutButton_->setFont(buttonFont);

    // Configure zoom level label
    zoomLevelLabel_->setAlignment(Qt::AlignCenter);
    zoomLevelLabel_->setFixedHeight(20);
    QFont labelFont;
    labelFont.setPointSize(10);
    zoomLevelLabel_->setFont(labelFont);

    // Configure altitude label
    altitudeLabel_->setAlignment(Qt::AlignCenter);
    altitudeLabel_->setFixedHeight(20);
    altitudeLabel_->setFont(labelFont);
    altitudeLabel_->setWordWrap(true);

    // Add widgets to layout
    layout_->addWidget(zoomInButton_);
    layout_->addWidget(zoomLevelLabel_);
    layout_->addWidget(altitudeLabel_);
    layout_->addWidget(zoomOutButton_);
    layout_->setSpacing(2);
    layout_->setContentsMargins(4, 4, 4, 4);

    setLayout(layout_);

    // Set fixed size for the control widget (increased height for altitude label)
    setFixedSize(48, 135);

    // Enable mouse tracking to prevent clicks from passing through
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);
}

void ZoomControls::applyStyle() {
    // Modern semi-transparent style
    QString style = R"(
        QWidget {
            background-color: rgba(255, 255, 255, 220);
            border-radius: 4px;
            border: 1px solid rgba(0, 0, 0, 0.2);
        }

        QPushButton {
            background-color: rgba(255, 255, 255, 255);
            border: 1px solid rgba(0, 0, 0, 0.3);
            border-radius: 4px;
            color: #333333;
        }

        QPushButton:hover {
            background-color: rgba(240, 240, 240, 255);
            border: 1px solid rgba(0, 0, 0, 0.5);
        }

        QPushButton:pressed {
            background-color: rgba(220, 220, 220, 255);
        }

        QPushButton:disabled {
            background-color: rgba(240, 240, 240, 180);
            color: rgba(100, 100, 100, 180);
        }

        QLabel {
            background-color: transparent;
            color: #333333;
            border: none;
        }
    )";

    setStyleSheet(style);
}

void ZoomControls::setZoomLevel(double zoom) {
    currentZoom_ = zoom;
    // Show one decimal place if not a whole number, otherwise show as integer
    if (zoom == static_cast<int>(zoom)) {
        zoomLevelLabel_->setText(QString("Zoom: %1").arg(static_cast<int>(zoom)));
    } else {
        zoomLevelLabel_->setText(QString("Zoom: %1").arg(zoom, 0, 'f', 1));
    }

    // Update altitude display with current units preference (fallback calculation)
    setAltitude(zoomToAltitudeMeters(zoom), useMetric_);

    // Disable buttons at min/max zoom
    zoomInButton_->setEnabled(zoom < 18.0);  // OSM max zoom
    zoomOutButton_->setEnabled(zoom > 1.0);   // OSM min zoom
}

void ZoomControls::setZoomLevel(double zoom, int viewportWidth, int viewportHeight) {
    currentZoom_ = zoom;
    // Show one decimal place if not a whole number, otherwise show as integer
    if (zoom == static_cast<int>(zoom)) {
        zoomLevelLabel_->setText(QString("Zoom: %1").arg(static_cast<int>(zoom)));
    } else {
        zoomLevelLabel_->setText(QString("Zoom: %1").arg(zoom, 0, 'f', 1));
    }

    // Update altitude display with viewport-aware calculation
    setAltitude(zoomToAltitudeMeters(zoom, viewportWidth, viewportHeight), useMetric_);

    // Disable buttons at min/max zoom
    zoomInButton_->setEnabled(zoom < 18.0);  // OSM max zoom
    zoomOutButton_->setEnabled(zoom > 1.0);   // OSM min zoom
}

void ZoomControls::setAltitude(double altitudeMeters, bool useMetric) {
    QString text;
    if (useMetric) {
        if (altitudeMeters >= 1000) {
            text = QString("%1 km").arg(altitudeMeters / 1000.0, 0, 'f', 1);
        } else {
            text = QString("%1 m").arg(static_cast<int>(altitudeMeters));
        }
    } else {
        // Convert to feet
        double altitudeFeet = altitudeMeters * 3.28084;
        if (altitudeFeet >= 5280) {
            text = QString("%1 mi").arg(altitudeFeet / 5280.0, 0, 'f', 1);
        } else {
            text = QString("%1 ft").arg(static_cast<int>(altitudeFeet));
        }
    }
    altitudeLabel_->setText(text);
}

void ZoomControls::setUseMetric(bool useMetric) {
    useMetric_ = useMetric;
    // Update altitude display with new units
    setAltitude(zoomToAltitudeMeters(currentZoom_), useMetric_);
}

double ZoomControls::zoomToAltitudeMeters(double zoom) const {
    // Web Mercator approximation: altitude (m) ≈ 40075000 / (256 * 2^zoom)
    // This gives approximate "eye altitude" for a given zoom level
    // at mid-latitudes (roughly equivalent to Google Maps eye altitude)
    constexpr double EARTH_CIRCUMFERENCE_M = 40075000.0;
    constexpr double TILE_SIZE = 256.0;
    return EARTH_CIRCUMFERENCE_M / (TILE_SIZE * std::pow(2.0, zoom));
}

double ZoomControls::zoomToAltitudeMeters(double zoom, int viewportWidth, int viewportHeight) const {
    // Calculate eye altitude based on viewport size and field of view
    //
    // At zoom level z, the world is represented as (256 * 2^z) pixels at the equator
    // Each pixel represents: EARTH_CIRCUMFERENCE / (256 * 2^z) meters
    //
    // The viewport shows: viewportWidth * meters_per_pixel = ground distance
    //
    // With a field of view (FOV) of 120 degrees (60° from center to edge):
    // altitude = ground_distance / (2 * tan(60°))
    //          = ground_distance / (2 * √3)
    //          = ground_distance / 3.464

    constexpr double EARTH_CIRCUMFERENCE_M = 40075000.0;
    constexpr double TILE_SIZE = 256.0;
    constexpr double FOV_DEGREES = 120.0;
    constexpr double HALF_FOV_RAD = (FOV_DEGREES / 2.0) * M_PI / 180.0; // 60° in radians

    // Calculate meters per pixel at this zoom level
    double metersPerPixel = EARTH_CIRCUMFERENCE_M / (TILE_SIZE * std::pow(2.0, zoom));

    // Use the larger dimension (width or height) for conservative altitude estimate
    int largerDimension = (viewportWidth > viewportHeight) ? viewportWidth : viewportHeight;

    // Calculate ground distance visible in viewport
    double groundDistanceMeters = largerDimension * metersPerPixel;

    // Calculate altitude from ground distance and field of view
    // h = d / (2 * tan(θ)) where θ is half the FOV
    double altitude = groundDistanceMeters / (2.0 * std::tan(HALF_FOV_RAD));

    return altitude;
}

void ZoomControls::updatePosition(int x, int y) {
    move(x, y);
}

} // namespace topo
