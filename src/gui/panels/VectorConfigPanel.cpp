/**
 * @file VectorConfigPanel.cpp
 * @brief Implementation of vector configuration panel
 */

#include "VectorConfigPanel.hpp"
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

VectorConfigPanel::VectorConfigPanel(StateManager* stateManager, QWidget *parent)
    : QWidget(parent), stateManager_(stateManager) {
    createUI();
    loadFromSettings();
}

void VectorConfigPanel::createUI() {
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("<h2>Vector Output Configuration</h2>", this));

    // Formats
    auto* formatGroup = new QGroupBox("Output Formats", this);
    auto* formatLayout = new QVBoxLayout(formatGroup);
    svgCheckbox_ = new QCheckBox("SVG (Laser Cutting)", this);
    svgCheckbox_->setChecked(true);
    formatLayout->addWidget(svgCheckbox_);
    geojsonCheckbox_ = new QCheckBox("GeoJSON", this);
    formatLayout->addWidget(geojsonCheckbox_);
    shapefileCheckbox_ = new QCheckBox("Shapefile", this);
    formatLayout->addWidget(shapefileCheckbox_);
    layout->addWidget(formatGroup);

    // Contour Settings
    auto* contourGroup = new QGroupBox("Contour Settings", this);
    auto* contourLayout = new QVBoxLayout(contourGroup);

    auto* intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(new QLabel("Contour Interval (m):", this));
    contourIntervalSpinBox_ = new QDoubleSpinBox(this);
    contourIntervalSpinBox_->setRange(1.0, 1000.0);
    contourIntervalSpinBox_->setValue(DEFAULT_CONTOUR_INTERVAL);
    intervalLayout->addWidget(contourIntervalSpinBox_);
    intervalLayout->addStretch();
    contourLayout->addLayout(intervalLayout);

    auto* simpLayout = new QHBoxLayout();
    simpLayout->addWidget(new QLabel("Simplification (m):", this));
    simplificationSpinBox_ = new QDoubleSpinBox(this);
    simplificationSpinBox_->setRange(0.0, 100.0);
    simplificationSpinBox_->setValue(DEFAULT_SIMPLIFICATION);
    simpLayout->addWidget(simplificationSpinBox_);
    simpLayout->addStretch();
    contourLayout->addLayout(simpLayout);

    auto* strokeLayout = new QHBoxLayout();
    strokeLayout->addWidget(new QLabel("Stroke Width (mm):", this));
    strokeWidthSpinBox_ = new QDoubleSpinBox(this);
    strokeWidthSpinBox_->setRange(0.01, 10.0);
    strokeWidthSpinBox_->setDecimals(2);
    strokeWidthSpinBox_->setValue(DEFAULT_STROKE_WIDTH);
    strokeLayout->addWidget(strokeWidthSpinBox_);
    strokeLayout->addStretch();
    contourLayout->addLayout(strokeLayout);

    layout->addWidget(contourGroup);

    // Options
    auto* optionsGroup = new QGroupBox("Options", this);
    auto* optionsLayout = new QVBoxLayout(optionsGroup);
    removeHolesCheckbox_ = new QCheckBox("Remove Holes (simpler cutting)", this);
    removeHolesCheckbox_->setChecked(true);
    optionsLayout->addWidget(removeHolesCheckbox_);

    insetUpperLayersCheckbox_ = new QCheckBox("Inset Upper Layers (nesting)", this);
    optionsLayout->addWidget(insetUpperLayersCheckbox_);

    auto* insetLayout = new QHBoxLayout();
    insetLayout->addSpacing(20);
    insetLayout->addWidget(new QLabel("Inset Offset (mm):", this));
    insetOffsetSpinBox_ = new QDoubleSpinBox(this);
    insetOffsetSpinBox_->setRange(0.1, 10.0);
    insetOffsetSpinBox_->setValue(1.0);
    insetOffsetSpinBox_->setEnabled(false);
    connect(insetUpperLayersCheckbox_, &QCheckBox::toggled, insetOffsetSpinBox_, &QWidget::setEnabled);
    insetLayout->addWidget(insetOffsetSpinBox_);
    insetLayout->addStretch();
    optionsLayout->addLayout(insetLayout);

    layout->addWidget(optionsGroup);

    // Features
    auto* featuresGroup = new QGroupBox("OSM Features", this);
    auto* featuresLayout = new QVBoxLayout(featuresGroup);
    includeRoadsCheckbox_ = new QCheckBox("Include Roads", this);
    featuresLayout->addWidget(includeRoadsCheckbox_);
    includeBuildingsCheckbox_ = new QCheckBox("Include Buildings", this);
    featuresLayout->addWidget(includeBuildingsCheckbox_);
    includeWaterwaysCheckbox_ = new QCheckBox("Include Waterways", this);
    featuresLayout->addWidget(includeWaterwaysCheckbox_);
    layout->addWidget(featuresGroup);

    // Layers
    auto* layerGroup = new QGroupBox("Layer Configuration", this);
    auto* layerLayout = new QVBoxLayout(layerGroup);

    auto* numLayersLayout = new QHBoxLayout();
    numLayersLayout->addWidget(new QLabel("Number of Layers:", this));
    numLayersSpinBox_ = new QSpinBox(this);
    numLayersSpinBox_->setRange(1, 100);
    numLayersSpinBox_->setValue(10);
    numLayersLayout->addWidget(numLayersSpinBox_);
    numLayersLayout->addStretch();
    layerLayout->addLayout(numLayersLayout);

    auto* heightLayout = new QHBoxLayout();
    heightLayout->addWidget(new QLabel("Height per Layer (m):", this));
    heightPerLayerSpinBox_ = new QDoubleSpinBox(this);
    heightPerLayerSpinBox_->setRange(0.1, 1000.0);
    heightPerLayerSpinBox_->setDecimals(2);
    heightPerLayerSpinBox_->setValue(21.43);
    heightLayout->addWidget(heightPerLayerSpinBox_);
    heightLayout->addStretch();
    layerLayout->addLayout(heightLayout);

    auto* thicknessLayout = new QHBoxLayout();
    thicknessLayout->addWidget(new QLabel("Material Thickness (mm):", this));
    layerThicknessSpinBox_ = new QDoubleSpinBox(this);
    layerThicknessSpinBox_->setRange(0.1, 50.0);
    layerThicknessSpinBox_->setDecimals(1);
    layerThicknessSpinBox_->setValue(3.0);
    thicknessLayout->addWidget(layerThicknessSpinBox_);
    thicknessLayout->addStretch();
    layerLayout->addLayout(thicknessLayout);

    auto* substrateLayout = new QHBoxLayout();
    substrateLayout->addWidget(new QLabel("Substrate Size (mm):", this));
    substrateSizeSpinBox_ = new QDoubleSpinBox(this);
    substrateSizeSpinBox_->setRange(1.0, 1000.0);
    substrateSizeSpinBox_->setDecimals(1);
    substrateSizeSpinBox_->setValue(200.0);
    substrateLayout->addWidget(substrateSizeSpinBox_);
    substrateLayout->addStretch();
    layerLayout->addLayout(substrateLayout);

    layout->addWidget(layerGroup);

    layout->addStretch();

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    auto* saveButton = new QPushButton("Save as Default", this);
    connect(saveButton, &QPushButton::clicked, this, &VectorConfigPanel::onSaveAsDefault);
    buttonLayout->addWidget(saveButton);
    buttonLayout->addStretch();
    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::hide);
    buttonLayout->addWidget(cancelButton);
    auto* generateButton = new QPushButton("Generate", this);
    generateButton->setDefault(true);
    connect(generateButton, &QPushButton::clicked, this, &VectorConfigPanel::onGenerate);
    buttonLayout->addWidget(generateButton);
    layout->addLayout(buttonLayout);
}

void VectorConfigPanel::loadFromSettings() {
    if (!stateManager_) return;
    QString json = stateManager_->restoreConfig("vector/config");
    if (!json.isEmpty() && json != "{}") setConfigFromJson(json);
}

void VectorConfigPanel::saveToSettings() {
    if (!stateManager_) return;
    stateManager_->saveConfig("vector/config", getConfigJson());
}

QString VectorConfigPanel::getConfigJson() const {
    QJsonObject config;
    QJsonObject formats;
    formats["svg"] = svgCheckbox_->isChecked();
    formats["geojson"] = geojsonCheckbox_->isChecked();
    formats["shapefile"] = shapefileCheckbox_->isChecked();
    config["formats"] = formats;
    config["contour_interval"] = contourIntervalSpinBox_->value();
    config["simplification"] = simplificationSpinBox_->value();
    config["stroke_width"] = strokeWidthSpinBox_->value();
    config["remove_holes"] = removeHolesCheckbox_->isChecked();
    config["inset_upper_layers"] = insetUpperLayersCheckbox_->isChecked();
    config["inset_offset"] = insetOffsetSpinBox_->value();
    config["include_roads"] = includeRoadsCheckbox_->isChecked();
    config["include_buildings"] = includeBuildingsCheckbox_->isChecked();
    config["include_waterways"] = includeWaterwaysCheckbox_->isChecked();
    config["num_layers"] = numLayersSpinBox_->value();
    config["height_per_layer"] = heightPerLayerSpinBox_->value();
    config["layer_thickness"] = layerThicknessSpinBox_->value();
    config["substrate_size"] = substrateSizeSpinBox_->value();
    return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

void VectorConfigPanel::setConfigFromJson(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject config = doc.object();

    if (config.contains("formats")) {
        QJsonObject formats = config["formats"].toObject();
        svgCheckbox_->setChecked(formats["svg"].toBool(true));
        geojsonCheckbox_->setChecked(formats["geojson"].toBool(false));
        shapefileCheckbox_->setChecked(formats["shapefile"].toBool(false));
    }
    contourIntervalSpinBox_->setValue(config["contour_interval"].toDouble(DEFAULT_CONTOUR_INTERVAL));
    simplificationSpinBox_->setValue(config["simplification"].toDouble(DEFAULT_SIMPLIFICATION));
    strokeWidthSpinBox_->setValue(config["stroke_width"].toDouble(DEFAULT_STROKE_WIDTH));
    removeHolesCheckbox_->setChecked(config["remove_holes"].toBool(true));
    insetUpperLayersCheckbox_->setChecked(config["inset_upper_layers"].toBool(false));
    insetOffsetSpinBox_->setValue(config["inset_offset"].toDouble(1.0));
    includeRoadsCheckbox_->setChecked(config["include_roads"].toBool(false));
    includeBuildingsCheckbox_->setChecked(config["include_buildings"].toBool(false));
    includeWaterwaysCheckbox_->setChecked(config["include_waterways"].toBool(false));
    numLayersSpinBox_->setValue(config["num_layers"].toInt(10));
    heightPerLayerSpinBox_->setValue(config["height_per_layer"].toDouble(21.43));
    layerThicknessSpinBox_->setValue(config["layer_thickness"].toDouble(3.0));
    substrateSizeSpinBox_->setValue(config["substrate_size"].toDouble(200.0));
}

void VectorConfigPanel::onSaveAsDefault() {
    saveToSettings();
    if (parentWidget()) {
        parentWidget()->setWindowTitle("Vector Configuration - Saved");
        QTimer::singleShot(2000, [this]() {
            if (parentWidget()) parentWidget()->setWindowTitle("Vector Configuration");
        });
    }
}

void VectorConfigPanel::onGenerate() {
    saveToSettings();
    emit generateRequested();
    hide();
}

} // namespace topo
