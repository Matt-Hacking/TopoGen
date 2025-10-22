/**
 * @file MeshConfigPanel.cpp
 * @brief Implementation of mesh configuration panel
 */

#include "MeshConfigPanel.hpp"
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

MeshConfigPanel::MeshConfigPanel(StateManager* stateManager, QWidget *parent)
    : QWidget(parent), stateManager_(stateManager) {
    createUI();
    loadFromSettings();
}

void MeshConfigPanel::createUI() {
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("<h2>Mesh/3D Output Configuration</h2>", this));

    // Formats
    auto* formatGroup = new QGroupBox("Output Formats", this);
    auto* formatLayout = new QVBoxLayout(formatGroup);
    stlCheckbox_ = new QCheckBox("STL (3D Printing)", this);
    stlCheckbox_->setChecked(true);
    formatLayout->addWidget(stlCheckbox_);
    objCheckbox_ = new QCheckBox("OBJ (with materials)", this);
    formatLayout->addWidget(objCheckbox_);
    plyCheckbox_ = new QCheckBox("PLY (with colors)", this);
    formatLayout->addWidget(plyCheckbox_);
    layout->addWidget(formatGroup);

    // Mode
    auto* modeGroup = new QGroupBox("Generation Mode", this);
    auto* modeLayout = new QVBoxLayout(modeGroup);
    terrainFollowingCheckbox_ = new QCheckBox("Terrain-Following Mode", this);
    terrainFollowingCheckbox_->setToolTip(
        "Generate surface that follows terrain topology (3D print).\n"
        "When unchecked, generates vertical contour relief model."
    );
    modeLayout->addWidget(terrainFollowingCheckbox_);

    outputStackedCheckbox_ = new QCheckBox("Generate Stacked Model", this);
    outputStackedCheckbox_->setToolTip(
        "Generate single 3D model with all layers stacked.\n"
        "When unchecked, generates individual layer files only."
    );
    modeLayout->addWidget(outputStackedCheckbox_);
    layout->addWidget(modeGroup);

    // Quality
    auto* qualityGroup = new QGroupBox("Mesh Quality", this);
    auto* qualityLayout = new QVBoxLayout(qualityGroup);
    qualityGroup_ = new QButtonGroup(this);

    draftRadio_ = new QRadioButton("Draft (Fast, lower detail)", this);
    qualityGroup_->addButton(draftRadio_, 0);
    qualityLayout->addWidget(draftRadio_);

    mediumRadio_ = new QRadioButton("Medium (Balanced)", this);
    mediumRadio_->setChecked(true);
    qualityGroup_->addButton(mediumRadio_, 1);
    qualityLayout->addWidget(mediumRadio_);

    highRadio_ = new QRadioButton("High (Slower, more detail)", this);
    qualityGroup_->addButton(highRadio_, 2);
    qualityLayout->addWidget(highRadio_);

    ultraRadio_ = new QRadioButton("Ultra (Maximum detail)", this);
    qualityGroup_->addButton(ultraRadio_, 3);
    qualityLayout->addWidget(ultraRadio_);

    layout->addWidget(qualityGroup);

    // Layer configuration
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
    connect(saveButton, &QPushButton::clicked, this, &MeshConfigPanel::onSaveAsDefault);
    buttonLayout->addWidget(saveButton);
    buttonLayout->addStretch();
    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QWidget::hide);
    buttonLayout->addWidget(cancelButton);
    auto* generateButton = new QPushButton("Generate", this);
    generateButton->setDefault(true);
    connect(generateButton, &QPushButton::clicked, this, &MeshConfigPanel::onGenerate);
    buttonLayout->addWidget(generateButton);
    layout->addLayout(buttonLayout);
}

void MeshConfigPanel::loadFromSettings() {
    if (!stateManager_) return;
    QString json = stateManager_->restoreConfig("mesh/config");
    if (!json.isEmpty() && json != "{}") setConfigFromJson(json);
}

void MeshConfigPanel::saveToSettings() {
    if (!stateManager_) return;
    stateManager_->saveConfig("mesh/config", getConfigJson());
}

QString MeshConfigPanel::getConfigJson() const {
    QJsonObject config;
    QJsonObject formats;
    formats["stl"] = stlCheckbox_->isChecked();
    formats["obj"] = objCheckbox_->isChecked();
    formats["ply"] = plyCheckbox_->isChecked();
    config["formats"] = formats;
    config["terrain_following"] = terrainFollowingCheckbox_->isChecked();
    config["output_stacked"] = outputStackedCheckbox_->isChecked();
    config["quality"] = qualityGroup_->checkedId();
    config["num_layers"] = numLayersSpinBox_->value();
    config["height_per_layer"] = heightPerLayerSpinBox_->value();
    config["layer_thickness"] = layerThicknessSpinBox_->value();
    config["substrate_size"] = substrateSizeSpinBox_->value();
    return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

void MeshConfigPanel::setConfigFromJson(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return;
    QJsonObject config = doc.object();

    if (config.contains("formats")) {
        QJsonObject formats = config["formats"].toObject();
        stlCheckbox_->setChecked(formats["stl"].toBool(true));
        objCheckbox_->setChecked(formats["obj"].toBool(false));
        plyCheckbox_->setChecked(formats["ply"].toBool(false));
    }

    terrainFollowingCheckbox_->setChecked(config["terrain_following"].toBool(false));
    outputStackedCheckbox_->setChecked(config["output_stacked"].toBool(false));

    int quality = config["quality"].toInt(1);
    if (quality == 0) draftRadio_->setChecked(true);
    else if (quality == 2) highRadio_->setChecked(true);
    else if (quality == 3) ultraRadio_->setChecked(true);
    else mediumRadio_->setChecked(true);

    numLayersSpinBox_->setValue(config["num_layers"].toInt(10));
    heightPerLayerSpinBox_->setValue(config["height_per_layer"].toDouble(21.43));
    layerThicknessSpinBox_->setValue(config["layer_thickness"].toDouble(3.0));
    substrateSizeSpinBox_->setValue(config["substrate_size"].toDouble(200.0));
}

void MeshConfigPanel::onSaveAsDefault() {
    saveToSettings();
    if (parentWidget()) {
        parentWidget()->setWindowTitle("Mesh Configuration - Saved");
        QTimer::singleShot(2000, [this]() {
            if (parentWidget()) parentWidget()->setWindowTitle("Mesh Configuration");
        });
    }
}

void MeshConfigPanel::onGenerate() {
    saveToSettings();
    emit generateRequested();
    hide();
}

} // namespace topo
