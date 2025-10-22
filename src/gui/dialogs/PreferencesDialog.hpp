#pragma once

/**
 * @file PreferencesDialog.hpp
 * @brief Application preferences dialog for cache, logging, and defaults
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>

namespace topo {

class StateManager;

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(StateManager* stateManager, QWidget *parent = nullptr);

    void loadFromSettings();
    void saveToSettings();

signals:
    void unitsChanged(bool useMetric);
    void outputDirectoryChanged();

private slots:
    void onBrowseCacheLocation();
    void onBrowseLogFile();
    void onBrowseDefaultOutputDir();
    void onBrowseCliConfigDir();
    void onClearCacheNow();
    void onOk();
    void onApply();

private:
    void createUI();
    void connectSignals();

    StateManager* stateManager_;

    // Cache settings
    QSpinBox* cacheSizeMBSpinBox_;
    QSpinBox* cacheCleanupDaysSpinBox_;
    QLineEdit* cacheLocationEdit_;

    // Logging settings
    QComboBox* logLevelCombo_;
    QCheckBox* enableLogFileCheckbox_;
    QLineEdit* logFilePathEdit_;

    // Default settings
    QLineEdit* defaultOutputDirEdit_;
    QLineEdit* defaultBasenameEdit_;

    // Display settings
    QCheckBox* useMetricUnitsCheckbox_;

    // CLI config settings
    QLineEdit* cliConfigDirEdit_;

    // Default values
    static constexpr int DEFAULT_CACHE_SIZE_MB = 500;
    static constexpr int DEFAULT_CLEANUP_DAYS = 30;
    static constexpr int DEFAULT_LOG_LEVEL = 3;
};

} // namespace topo
