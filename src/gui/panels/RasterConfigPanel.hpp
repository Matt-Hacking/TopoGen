#pragma once

/**
 * @file RasterConfigPanel.hpp
 * @brief Configuration panel for raster output (PNG, GeoTIFF)
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QButtonGroup>
#include <memory>

namespace topo {

class StateManager;

/**
 * @brief Configuration panel for raster output options
 */
class RasterConfigPanel : public QWidget {
    Q_OBJECT

public:
    explicit RasterConfigPanel(StateManager* stateManager, QWidget *parent = nullptr);
    ~RasterConfigPanel() override = default;

    /**
     * @brief Load configuration from QSettings
     */
    void loadFromSettings();

    /**
     * @brief Save current configuration to QSettings
     */
    void saveToSettings();

    /**
     * @brief Get configuration as JSON string for storage
     */
    QString getConfigJson() const;

    /**
     * @brief Set configuration from JSON string
     */
    void setConfigFromJson(const QString& json);

signals:
    /**
     * @brief Emitted when user clicks Generate button
     */
    void generateRequested();

private slots:
    void onSaveAsDefault();
    void onGenerate();

private:
    void createUI();
    void connectSignals();
    void applyDefaults();

    StateManager* stateManager_;

    // Format options
    QCheckBox* pngCheckbox_;
    QCheckBox* geotiffCheckbox_;

    // Resolution
    QRadioButton* dpi300Radio_;
    QRadioButton* dpi600Radio_;
    QRadioButton* dpi1200Radio_;
    QButtonGroup* dpiGroup_;

    // Render mode
    QRadioButton* fullColorRadio_;
    QRadioButton* grayscaleRadio_;
    QRadioButton* monochromeRadio_;
    QButtonGroup* renderModeGroup_;

    // Options
    QCheckBox* includeLabelsCheckbox_;
    QCheckBox* includeLegendCheckbox_;

    // Layer configuration
    QSpinBox* numLayersSpinBox_;
    QDoubleSpinBox* heightPerLayerSpinBox_;
    QDoubleSpinBox* layerThicknessSpinBox_;

    // Default values
    static constexpr int DEFAULT_NUM_LAYERS = 10;
    static constexpr double DEFAULT_HEIGHT_PER_LAYER = 21.43;
    static constexpr double DEFAULT_LAYER_THICKNESS = 3.0;
    static constexpr int DEFAULT_DPI = 600;
};

} // namespace topo
