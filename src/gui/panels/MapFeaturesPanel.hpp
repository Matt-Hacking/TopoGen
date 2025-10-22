#pragma once

/**
 * @file MapFeaturesPanel.hpp
 * @brief Left sidebar panel for controlling map feature visibility and altitude filtering
 *
 * Provides checkboxes for toggling:
 * - Contour Lines (via OpenTopoMap tile overlay)
 * - Water Bodies
 * - Roads
 * - Park Boundaries
 * - Peaks
 * - Political Boundaries
 *
 * Also includes a minimum altitude slider that affects both:
 * - Visual display (white space below threshold)
 * - Generation output (filtered by altitude)
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QWidget>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>

namespace topo {

class MapFeaturesPanel : public QWidget {
    Q_OBJECT

public:
    explicit MapFeaturesPanel(QWidget *parent = nullptr);
    ~MapFeaturesPanel() override = default;

    // Feature visibility getters
    bool contourLinesVisible() const { return contourLinesCheckbox_->isChecked(); }
    bool topoMapVisible() const { return topoMapCheckbox_->isChecked(); }
    bool waterVisible() const { return waterCheckbox_->isChecked(); }
    bool roadsVisible() const { return roadsCheckbox_->isChecked(); }
    bool parksVisible() const { return parksCheckbox_->isChecked(); }
    bool peaksVisible() const { return peaksCheckbox_->isChecked(); }
    bool boundariesVisible() const { return boundariesCheckbox_->isChecked(); }

    // Altitude filtering
    int minimumAltitudeMeters() const;
    void setMinimumAltitudeMeters(int meters);

    // Unit system
    void setUseMetric(bool useMetric);

signals:
    void contourLinesVisibilityChanged(bool visible);
    void topoMapVisibilityChanged(bool visible);
    void waterVisibilityChanged(bool visible);
    void roadsVisibilityChanged(bool visible);
    void parksVisibilityChanged(bool visible);
    void peaksVisibilityChanged(bool visible);
    void boundariesVisibilityChanged(bool visible);
    void minimumAltitudeChanged(int meters);

private slots:
    void onContourLinesToggled(bool checked);
    void onTopoMapToggled(bool checked);
    void onPeaksToggled(bool checked);
    void onAltitudeValueChanged(int displayValue);

private:
    void setupUI();
    void updateAltitudeLabel(int meters);

    // Unit conversion helpers
    int metersToDisplayValue(int meters) const;
    int displayValueToMeters(int displayValue) const;
    void updateWidgetRanges();

    // Feature checkboxes
    QCheckBox* contourLinesCheckbox_;
    QCheckBox* topoMapCheckbox_;
    QCheckBox* waterCheckbox_;
    QCheckBox* roadsCheckbox_;
    QCheckBox* parksCheckbox_;
    QCheckBox* peaksCheckbox_;
    QCheckBox* boundariesCheckbox_;

    // Altitude filtering
    QSlider* altitudeSlider_;
    QSpinBox* altitudeSpinBox_;
    QLabel* altitudeLabel_;

    // Layout
    QVBoxLayout* mainLayout_;
    QGroupBox* featuresGroup_;
    QGroupBox* altitudeGroup_;

    // Unit preference
    bool useMetric_;

    // Internal state (always in meters)
    int currentAltitudeMeters_;

    // Constants
    static constexpr int MIN_ALTITUDE_M = -500;  // Below sea level
    static constexpr int MAX_ALTITUDE_M = 9000;  // Above Mt. Everest
    static constexpr int DEFAULT_ALTITUDE_M = 0; // Sea level
};

} // namespace topo
