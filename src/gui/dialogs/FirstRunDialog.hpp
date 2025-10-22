#pragma once

/**
 * @file FirstRunDialog.hpp
 * @brief First-run configuration dialog for cache and data directories
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QDialog>
#include <QLineEdit>
#include <QLabel>

namespace topo {

class StateManager;

/**
 * @brief Dialog for configuring application directories on first run
 *
 * This modal dialog appears when the application is launched for the first time
 * or when directory settings are invalid. It allows users to configure:
 * - Cache directory (for downloaded elevation and OSM tiles)
 * - Configuration directory (for application settings)
 *
 * Platform-specific defaults:
 * - macOS: ~/Library/Application Support/Topographic Generator/
 * - Windows: ~/Documents/Topographic Generator/
 * - Linux: ~/.topographic-generator/
 */
class FirstRunDialog : public QDialog {
    Q_OBJECT

public:
    explicit FirstRunDialog(StateManager* stateManager, QWidget *parent = nullptr);

    /**
     * @brief Get the configured cache directory path
     */
    QString getCacheDirectory() const { return cacheDirectoryEdit_->text(); }

    /**
     * @brief Get the configured data directory path
     */
    QString getDataDirectory() const { return dataDirectoryEdit_->text(); }

private slots:
    void onBrowseCacheDirectory();
    void onBrowseDataDirectory();
    void onUseDefaults();
    void onOk();
    void onCancel();

private:
    void createUI();
    void connectSignals();
    void setDefaultPaths();
    bool validatePaths();

    /**
     * @brief Get platform-specific default directory path
     */
    static QString getPlatformDefaultDirectory();

    StateManager* stateManager_;

    QLineEdit* cacheDirectoryEdit_;
    QLineEdit* dataDirectoryEdit_;
    QLabel* statusLabel_;
};

} // namespace topo
