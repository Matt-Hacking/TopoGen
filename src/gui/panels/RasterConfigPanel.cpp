/**
 * @file RasterConfigPanel.cpp
 * @brief Implementation of raster configuration panel
 */

#include "RasterConfigPanel.hpp"
#include "../utils/StateManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

namespace topo {

RasterConfigPanel::RasterConfigPanel(StateManager* stateManager, QWidget *parent)
    : QWidget(parent),
      stateManager_(stateManager) {

    createUI();
    connectSignals();
    loadFromSettings();
}

void RasterConfigPanel::createUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Title
    auto* titleLabel = new QLabel("<h2>Raster Output Configuration</h2>", this);
    mainLayout->addWidget(titleLabel);

    // Format selection group
    auto* formatGroup = new QGroupBox("Output Formats", this);
    auto* formatLayout = new QVBoxLayout(formatGroup);

    pngCheckbox_ = new QCheckBox("PNG", this);
    pngCheckbox_->setChecked(true);
    formatLayout->addWidget(pngCheckbox_);

    geotiffCheckbox_ = new QCheckBox("GeoTIFF", this);
    formatLayout->addWidget(geotiffCheckbox_);

    mainLayout->addWidget(formatGroup);

    // Resolution group
    auto* resolutionGroup = new QGroupBox("Resolution", this);
    auto* resolutionLayout = new QVBoxLayout(resolutionGroup);

    dpiGroup_ = new QButtonGroup(this);

    dpi300Radio_ = new QRadioButton("300 DPI (Draft)", this);
    dpiGroup_->addButton(dpi300Radio_, 300);
    resolutionLayout->addWidget(dpi300Radio_);

    dpi600Radio_ = new QRadioButton("600 DPI (Standard)", this);
    dpi600Radio_->setChecked(true);
    dpiGroup_->addButton(dpi600Radio_, 600);
    resolutionLayout->addWidget(dpi600Radio_);

    dpi1200Radio_ = new QRadioButton("1200 DPI (High Quality)", this);
    dpiGroup_->addButton(dpi1200Radio_, 1200);
    resolutionLayout->addWidget(dpi1200Radio_);

    mainLayout->addWidget(resolutionGroup);

    // Render mode group
    auto* renderModeGroup = new QGroupBox("Render Mode", this);
    auto* renderModeLayout = new QVBoxLayout(renderModeGroup);

    renderModeGroup_ = new QButtonGroup(this);

    fullColorRadio_ = new QRadioButton("Full Color", this);
    fullColorRadio_->setChecked(true);
    renderModeGroup_->addButton(fullColorRadio_, 0);
    renderModeLayout->addWidget(fullColorRadio_);

    grayscaleRadio_ = new QRadioButton("Grayscale", this);
    renderModeGroup_->addButton(grayscaleRadio_, 1);
    renderModeLayout->addWidget(grayscaleRadio_);

    monochromeRadio_ = new QRadioButton("Monochrome (Outlines Only)", this);
    renderModeGroup_->addButton(monochromeRadio_, 2);
    renderModeLayout->addWidget(monochromeRadio_);

    mainLayout->addWidget(renderModeGroup);

    // Options group
    auto* optionsGroup = new QGroupBox("Options", this);
    auto* optionsLayout = new QVBoxLayout(optionsGroup);

    includeLabelsCheckbox_ = new QCheckBox("Include Labels", this);
    includeLabelsCheckbox_->setChecked(true);
    optionsLayout->addWidget(includeLabelsCheckbox_);

    includeLegendCheckbox_ = new QCheckBox("Include Legend", this);
    optionsLayout->addWidget(includeLegendCheckbox_);

    mainLayout->addWidget(optionsGroup);

    // Layer configuration group
    auto* layerGroup = new QGroupBox("Layer Configuration", this);
    auto* layerLayout = new QVBoxLayout(layerGroup);

    // Number of layers
    auto* numLayersLayout = new QHBoxLayout();
    numLayersLayout->addWidget(new QLabel("Number of Layers:", this));
    numLayersSpinBox_ = new QSpinBox(this);
    numLayersSpinBox_->setRange(1, 100);
    numLayersSpinBox_->setValue(DEFAULT_NUM_LAYERS);
    numLayersLayout->addWidget(numLayersSpinBox_);
    numLayersLayout->addStretch();
    layerLayout->addLayout(numLayersLayout);

    // Height per layer
    auto* heightLayout = new QHBoxLayout();
    heightLayout->addWidget(new QLabel("Height per Layer (m):", this));
    heightPerLayerSpinBox_ = new QDoubleSpinBox(this);
    heightPerLayerSpinBox_->setRange(0.1, 1000.0);
    heightPerLayerSpinBox_->setDecimals(2);
    heightPerLayerSpinBox_->setValue(DEFAULT_HEIGHT_PER_LAYER);
    heightLayout->addWidget(heightPerLayerSpinBox_);
    heightLayout->addStretch();
    layerLayout->addLayout(heightLayout);

    // Layer thickness
    auto* thicknessLayout = new QHBoxLayout();
    thicknessLayout->addWidget(new QLabel("Material Thickness (mm):", this));
    layerThicknessSpinBox_ = new QDoubleSpinBox(this);
    layerThicknessSpinBox_->setRange(0.1, 50.0);
    layerThicknessSpinBox_->setDecimals(1);
    layerThicknessSpinBox_->setValue(DEFAULT_LAYER_THICKNESS);
    thicknessLayout->addWidget(layerThicknessSpinBox_);
    thicknessLayout->addStretch();
    layerLayout->addLayout(thicknessLayout);

    mainLayout->addWidget(layerGroup);

    // Spacer
    mainLayout->addStretch();

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    auto* saveDefaultButton = new QPushButton("Save as Default", this);
    connect(saveDefaultButton, &QPushButton::clicked, this, &RasterConfigPanel::onSaveAsDefault);
    buttonLayout->addWidget(saveDefaultButton);

    buttonLayout->addStretch();

    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::hide);
    buttonLayout->addWidget(cancelButton);

    auto* generateButton = new QPushButton("Generate", this);
    generateButton->setDefault(true);
    connect(generateButton, &QPushButton::clicked, this, &RasterConfigPanel::onGenerate);
    buttonLayout->addWidget(generateButton);

    mainLayout->addLayout(buttonLayout);
}

void RasterConfigPanel::connectSignals() {
    // Could add signal connections for real-time validation here
}

void RasterConfigPanel::loadFromSettings() {
    if (!stateManager_) return;

    QString json = stateManager_->restoreConfig("raster/config");
    if (!json.isEmpty() && json != "{}") {
        setConfigFromJson(json);
    } else {
        applyDefaults();
    }
}

void RasterConfigPanel::saveToSettings() {
    if (!stateManager_) return;

    QString json = getConfigJson();
    stateManager_->saveConfig("raster/config", json);
}

QString RasterConfigPanel::getConfigJson() const {
    QJsonObject config;

    // Formats
    QJsonObject formats;
    formats["png"] = pngCheckbox_->isChecked();
    formats["geotiff"] = geotiffCheckbox_->isChecked();
    config["formats"] = formats;

    // Resolution
    config["dpi"] = dpiGroup_->checkedId();

    // Render mode
    config["render_mode"] = renderModeGroup_->checkedId();

    // Options
    config["include_labels"] = includeLabelsCheckbox_->isChecked();
    config["include_legend"] = includeLegendCheckbox_->isChecked();

    // Layer configuration
    config["num_layers"] = numLayersSpinBox_->value();
    config["height_per_layer"] = heightPerLayerSpinBox_->value();
    config["layer_thickness"] = layerThicknessSpinBox_->value();

    QJsonDocument doc(config);
    return doc.toJson(QJsonDocument::Compact);
}

void RasterConfigPanel::setConfigFromJson(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject config = doc.object();

    // Formats
    if (config.contains("formats")) {
        QJsonObject formats = config["formats"].toObject();
        pngCheckbox_->setChecked(formats["png"].toBool(true));
        geotiffCheckbox_->setChecked(formats["geotiff"].toBool(false));
    }

    // Resolution
    if (config.contains("dpi")) {
        int dpi = config["dpi"].toInt(DEFAULT_DPI);
        if (dpi == 300) dpi300Radio_->setChecked(true);
        else if (dpi == 1200) dpi1200Radio_->setChecked(true);
        else dpi600Radio_->setChecked(true);
    }

    // Render mode
    if (config.contains("render_mode")) {
        int mode = config["render_mode"].toInt(0);
        if (mode == 1) grayscaleRadio_->setChecked(true);
        else if (mode == 2) monochromeRadio_->setChecked(true);
        else fullColorRadio_->setChecked(true);
    }

    // Options
    includeLabelsCheckbox_->setChecked(config["include_labels"].toBool(true));
    includeLegendCheckbox_->setChecked(config["include_legend"].toBool(false));

    // Layer configuration
    numLayersSpinBox_->setValue(config["num_layers"].toInt(DEFAULT_NUM_LAYERS));
    heightPerLayerSpinBox_->setValue(config["height_per_layer"].toDouble(DEFAULT_HEIGHT_PER_LAYER));
    layerThicknessSpinBox_->setValue(config["layer_thickness"].toDouble(DEFAULT_LAYER_THICKNESS));
}

void RasterConfigPanel::applyDefaults() {
    pngCheckbox_->setChecked(true);
    geotiffCheckbox_->setChecked(false);
    dpi600Radio_->setChecked(true);
    fullColorRadio_->setChecked(true);
    includeLabelsCheckbox_->setChecked(true);
    includeLegendCheckbox_->setChecked(false);
    numLayersSpinBox_->setValue(DEFAULT_NUM_LAYERS);
    heightPerLayerSpinBox_->setValue(DEFAULT_HEIGHT_PER_LAYER);
    layerThicknessSpinBox_->setValue(DEFAULT_LAYER_THICKNESS);
}

void RasterConfigPanel::onSaveAsDefault() {
    saveToSettings();
    // Show confirmation
    parentWidget()->setWindowTitle("Raster Configuration - Saved");
    QTimer::singleShot(2000, [this]() {
        if (parentWidget()) {
            parentWidget()->setWindowTitle("Raster Configuration");
        }
    });
}

void RasterConfigPanel::onGenerate() {
    // Save current settings as defaults
    saveToSettings();

    // Emit signal for MainWindow to handle
    emit generateRequested();

    // Hide panel
    hide();
}

} // namespace topo
