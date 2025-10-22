#pragma once

/**
 * @file VectorConfigPanel.hpp
 * @brief Configuration panel for vector output (SVG, GeoJSON, Shapefile)
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>

namespace topo {

class StateManager;

class VectorConfigPanel : public QWidget {
    Q_OBJECT

public:
    explicit VectorConfigPanel(StateManager* stateManager, QWidget *parent = nullptr);

    void loadFromSettings();
    void saveToSettings();
    QString getConfigJson() const;
    void setConfigFromJson(const QString& json);

signals:
    void generateRequested();

private slots:
    void onSaveAsDefault();
    void onGenerate();

private:
    void createUI();

    StateManager* stateManager_;

    QCheckBox *svgCheckbox_, *geojsonCheckbox_, *shapefileCheckbox_;
    QDoubleSpinBox *contourIntervalSpinBox_, *simplificationSpinBox_, *strokeWidthSpinBox_;
    QCheckBox *removeHolesCheckbox_, *insetUpperLayersCheckbox_;
    QDoubleSpinBox *insetOffsetSpinBox_;
    QCheckBox *includeRoadsCheckbox_, *includeBuildingsCheckbox_, *includeWaterwaysCheckbox_;
    QSpinBox *numLayersSpinBox_;
    QDoubleSpinBox *heightPerLayerSpinBox_, *layerThicknessSpinBox_, *substrateSizeSpinBox_;

    static constexpr double DEFAULT_CONTOUR_INTERVAL = 100.0;
    static constexpr double DEFAULT_SIMPLIFICATION = 5.0;
    static constexpr double DEFAULT_STROKE_WIDTH = 0.2;
};

} // namespace topo
