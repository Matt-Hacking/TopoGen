#pragma once

/**
 * @file MeshConfigPanel.hpp
 * @brief Configuration panel for mesh/3D output (STL, OBJ, PLY)
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

namespace topo {

class StateManager;

class MeshConfigPanel : public QWidget {
    Q_OBJECT

public:
    explicit MeshConfigPanel(StateManager* stateManager, QWidget *parent = nullptr);

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

    QCheckBox *stlCheckbox_, *objCheckbox_, *plyCheckbox_;
    QCheckBox *terrainFollowingCheckbox_;
    QCheckBox *outputStackedCheckbox_;
    QRadioButton *draftRadio_, *mediumRadio_, *highRadio_, *ultraRadio_;
    QButtonGroup* qualityGroup_;
    QSpinBox *numLayersSpinBox_;
    QDoubleSpinBox *heightPerLayerSpinBox_, *layerThicknessSpinBox_, *substrateSizeSpinBox_;
};

} // namespace topo
