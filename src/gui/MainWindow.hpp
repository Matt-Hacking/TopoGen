#pragma once

/**
 * @file MainWindow.hpp
 * @brief Main application window for Topographic Generator GUI
 *
 * Layout:
 * - Top toolbar: Search bar, terrain-following checkbox, action buttons
 * - Center: Map widget with OSM tiles and selection rectangle
 * - Bottom: Status bar with current bounds display
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QMainWindow>
#include <QLineEdit>
#include <QCheckBox>
#include <QToolButton>
#include <QLabel>
#include <QMenu>
#include <memory>

// Forward declarations
namespace topo {
    class MapWidget;
    class MapFeaturesPanel;
    class StateManager;
    class RasterConfigPanel;
    class VectorConfigPanel;
    class MeshConfigPanel;
    class PreferencesDialog;
    class GenerationWorker;
    class TilePreloadWorker;
    class GenerationProgressDialog;
    class PreviewDialog;
}

namespace topo {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Search functionality
    void onSearchTriggered();

    // Action buttons
    void onRasterButtonClicked();
    void onVectorButtonClicked();
    void onMeshButtonClicked();

    // Config panel actions
    void onShowRasterConfig();
    void onShowVectorConfig();
    void onShowMeshConfig();

    // Generation requests from config panels
    void onRasterGenerationRequested();
    void onVectorGenerationRequested();
    void onMeshGenerationRequested();

    // Copy CLI command actions
    void onCopyRasterCommand();
    void onCopyVectorCommand();
    void onCopyMeshCommand();

    // Menu actions
    void onNewSelection();
    void onOpenConfig();
    void onSaveConfig();
    void onPreferences();
    void onAbout();

    // Map interactions
    void onSelectionChanged(double min_lat, double min_lon, double max_lat, double max_lon);

private:
    void createUI();
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void connectSignals();
    void createConfigPanels();

    // Helper method for starting generation
    void startGeneration(const QString& outputType, const QString& configJson);

    // Helper method for building CLI commands
    QString buildCliCommand(const QString& outputType, const QString& configJson);

    // Helper method for preloading elevation tiles
    void startTilePreload();

    // Apply units preference to all widgets
    void applyUnitsPreference(bool useMetric);

    // UI Components
    QLineEdit *searchEdit_;
    QToolButton *rasterButton_;
    QToolButton *vectorButton_;
    QToolButton *meshButton_;
    QLabel *boundsLabel_;

    // Central widgets
    MapWidget *mapWidget_;
    MapFeaturesPanel *mapFeaturesPanel_;

    // Config panels (created on demand)
    std::unique_ptr<RasterConfigPanel> rasterPanel_;
    std::unique_ptr<VectorConfigPanel> vectorPanel_;
    std::unique_ptr<MeshConfigPanel> meshPanel_;

    // Dialogs
    std::unique_ptr<PreferencesDialog> preferencesDialog_;

    // Background workers
    TilePreloadWorker* preloadWorker_;

    // State
    StateManager *stateManager_;
    QString lastUsedOutputDir_;  // Session-specific output directory memory
};

} // namespace topo
