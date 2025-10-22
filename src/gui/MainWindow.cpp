/**
 * @file MainWindow.cpp
 * @brief Implementation of main application window
 */

#include "MainWindow.hpp"
#include "widgets/MapWidget.hpp"
#include "widgets/PreviewDialog.hpp"
#include "panels/MapFeaturesPanel.hpp"
#include "panels/RasterConfigPanel.hpp"
#include "panels/VectorConfigPanel.hpp"
#include "panels/MeshConfigPanel.hpp"
#include "dialogs/PreferencesDialog.hpp"
#include "dialogs/GenerationProgressDialog.hpp"
#include "workers/GenerationWorker.hpp"
#include "workers/TilePreloadWorker.hpp"
#include "utils/StateManager.hpp"
#include "utils/NominatimClient.hpp"
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QApplication>

namespace topo {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      searchEdit_(nullptr),
      rasterButton_(nullptr),
      vectorButton_(nullptr),
      meshButton_(nullptr),
      boundsLabel_(nullptr),
      mapWidget_(nullptr),
      preloadWorker_(nullptr),
      stateManager_(new StateManager()) {

    setWindowTitle("Topographic Generator");
    createUI();
    createConfigPanels();
    connectSignals();

    // Restore map state from last session
    auto [center, zoom] = stateManager_->restoreMapState();
    if (mapWidget_) {
        mapWidget_->setMapState(center, zoom);

        // Note: Selection bounds now automatically use viewport - no need to restore
    }

    // Restore and apply units preference
    auto unitsSettings = stateManager_->restoreUnitsSettings();
    bool useMetric = (unitsSettings.system == StateManager::UnitSystem::Metric);
    applyUnitsPreference(useMetric);
}

MainWindow::~MainWindow() {
    delete stateManager_;
}

void MainWindow::createUI() {
    // Create menu bar
    createMenuBar();

    // Create toolbar
    createToolBar();

    // Create map features panel (left sidebar)
    mapFeaturesPanel_ = new MapFeaturesPanel(this);

    // Create map widget
    mapWidget_ = new MapWidget(this);

    // Configure cache directory from settings
    if (stateManager_) {
        auto cacheSettings = stateManager_->restoreCacheSettings();
        qDebug() << "MainWindow: Cache location from settings:" << cacheSettings.location;
        if (!cacheSettings.location.isEmpty()) {
            QString fullCachePath = cacheSettings.location + "/osm_tiles";
            qDebug() << "MainWindow: Setting cache directory to:" << fullCachePath;
            mapWidget_->setCacheDirectory(fullCachePath);
        } else {
            qDebug() << "MainWindow: Cache location is empty, not setting cache directory";
        }
    }

    // Connect map features panel to map widget
    connect(mapFeaturesPanel_, &MapFeaturesPanel::contourLinesVisibilityChanged,
            mapWidget_, &MapWidget::setContourLinesVisible);
    connect(mapFeaturesPanel_, &MapFeaturesPanel::topoMapVisibilityChanged,
            mapWidget_, &MapWidget::setTopoMapVisible);
    connect(mapFeaturesPanel_, &MapFeaturesPanel::peaksVisibilityChanged,
            mapWidget_, &MapWidget::setPeaksVisible);

    // Create splitter to hold features panel and map
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(mapFeaturesPanel_);
    splitter->addWidget(mapWidget_);

    // Set stretch factors to give map more space
    splitter->setStretchFactor(0, 0);  // Panel doesn't stretch
    splitter->setStretchFactor(1, 1);  // Map stretches to fill

    // Set initial sizes: panel at ~250px, rest for map
    splitter->setSizes({250, 1000});

    // Disable collapsing of the panel
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);

    // Set splitter as central widget
    setCentralWidget(splitter);

    // Create status bar
    createStatusBar();
}

void MainWindow::createMenuBar() {
    // File menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newSelectionAction = fileMenu->addAction(tr("&New Selection"));
    connect(newSelectionAction, &QAction::triggered, this, &MainWindow::onNewSelection);

    fileMenu->addSeparator();

    QAction *openConfigAction = fileMenu->addAction(tr("&Open Config..."));
    connect(openConfigAction, &QAction::triggered, this, &MainWindow::onOpenConfig);

    QAction *saveConfigAction = fileMenu->addAction(tr("&Save Config..."));
    connect(saveConfigAction, &QAction::triggered, this, &MainWindow::onSaveConfig);

    fileMenu->addSeparator();

    QAction *preferencesAction = fileMenu->addAction(tr("&Preferences..."));
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::onPreferences);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::createToolBar() {
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);

    // Search bar
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(tr("Search location or enter coordinates..."));
    searchEdit_->setMinimumWidth(300);
    searchEdit_->setClearButtonEnabled(false);  // Disable clear button to avoid visual artifacts

    // Remove all actions to prevent icon rendering artifacts on macOS
    for (QAction* action : searchEdit_->actions()) {
        searchEdit_->removeAction(action);
    }

    // Clean styling with explicit rendering properties to avoid artifacts
    searchEdit_->setStyleSheet(
        "QLineEdit { "
        "  padding: 4px 6px; "
        "  background-color: #ffffff; "
        "  border: 1px solid #cccccc; "
        "  border-style: solid; "
        "  border-radius: 3px; "
        "  selection-background-color: #0078d7; "
        "  selection-color: white; "
        "} "
        "QLineEdit:focus { "
        "  border: 1px solid #0078d7; "
        "  background-color: #ffffff; "
        "} "
        "QLineEdit:hover { "
        "  border: 1px solid #999999; "
        "}"
    );
    toolbar->addWidget(searchEdit_);

    QPushButton *searchButton = new QPushButton(tr("Search"), this);
    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearchTriggered);
    toolbar->addWidget(searchButton);

    toolbar->addSeparator();

    // Action buttons with dropdown menus
    rasterButton_ = new QToolButton(this);
    rasterButton_->setText(tr("Raster"));
    rasterButton_->setPopupMode(QToolButton::MenuButtonPopup);
    connect(rasterButton_, &QToolButton::clicked, this, &MainWindow::onRasterButtonClicked);

    QMenu* rasterMenu = new QMenu(this);
    rasterMenu->addAction(tr("Configure..."), this, &MainWindow::onShowRasterConfig);
    rasterMenu->addAction(tr("Generate with Defaults"), this, &MainWindow::onRasterButtonClicked);
    rasterMenu->addAction(tr("Copy CLI Command"), this, &MainWindow::onCopyRasterCommand);
    rasterButton_->setMenu(rasterMenu);

    toolbar->addWidget(rasterButton_);

    vectorButton_ = new QToolButton(this);
    vectorButton_->setText(tr("Vector"));
    vectorButton_->setPopupMode(QToolButton::MenuButtonPopup);
    connect(vectorButton_, &QToolButton::clicked, this, &MainWindow::onVectorButtonClicked);

    QMenu* vectorMenu = new QMenu(this);
    vectorMenu->addAction(tr("Configure..."), this, &MainWindow::onShowVectorConfig);
    vectorMenu->addAction(tr("Generate with Defaults"), this, &MainWindow::onVectorButtonClicked);
    vectorMenu->addAction(tr("Copy CLI Command"), this, &MainWindow::onCopyVectorCommand);
    vectorButton_->setMenu(vectorMenu);

    toolbar->addWidget(vectorButton_);

    meshButton_ = new QToolButton(this);
    meshButton_->setText(tr("Mesh"));
    meshButton_->setPopupMode(QToolButton::MenuButtonPopup);
    connect(meshButton_, &QToolButton::clicked, this, &MainWindow::onMeshButtonClicked);

    QMenu* meshMenu = new QMenu(this);
    meshMenu->addAction(tr("Configure..."), this, &MainWindow::onShowMeshConfig);
    meshMenu->addAction(tr("Generate with Defaults"), this, &MainWindow::onMeshButtonClicked);
    meshMenu->addAction(tr("Copy CLI Command"), this, &MainWindow::onCopyMeshCommand);
    meshButton_->setMenu(meshMenu);

    toolbar->addWidget(meshButton_);
}

void MainWindow::createStatusBar() {
    boundsLabel_ = new QLabel(tr("No selection"), this);
    statusBar()->addPermanentWidget(boundsLabel_);
}

void MainWindow::connectSignals() {
    // Connect search on Enter key
    connect(searchEdit_, &QLineEdit::returnPressed, this, &MainWindow::onSearchTriggered);

    // Connect map widget signals
    if (mapWidget_) {
        connect(mapWidget_, &MapWidget::selectionChanged,
                this, &MainWindow::onSelectionChanged);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Save state before closing
    stateManager_->saveWindow(*this);

    // Save map state and bounds
    if (mapWidget_) {
        auto [center, zoom] = mapWidget_->getMapState();
        stateManager_->saveMapState(center, zoom);

        auto bounds = mapWidget_->getCurrentBounds();
        if (bounds) {
            auto [min_lat, min_lon, max_lat, max_lon] = *bounds;
            stateManager_->saveSelectionBounds(min_lat, min_lon, max_lat, max_lon);
        }
    }

    event->accept();
}

// Slots
void MainWindow::onSearchTriggered() {
    QString query = searchEdit_->text().trimmed();
    if (query.isEmpty()) {
        return;
    }

    statusBar()->showMessage(tr("Searching for: %1...").arg(query));

    // Create geocoding client (one-time use)
    auto* geocoder = new NominatimClient(this);

    // Connect signals
    connect(geocoder, &NominatimClient::geocodingComplete, this,
            [this, geocoder](GeocodingResult result) {
                // Center map on result
                if (mapWidget_) {
                    mapWidget_->centerOn(result.latitude, result.longitude, 12);  // Zoom 12 for good detail
                }

                statusBar()->showMessage(tr("Found: %1").arg(result.displayName), 5000);

                // Clean up
                geocoder->deleteLater();
            });

    connect(geocoder, &NominatimClient::geocodingFailed, this,
            [this, geocoder, query](QString error) {
                statusBar()->showMessage(tr("Search failed: %1").arg(error), 5000);
                QMessageBox::warning(this, tr("Geocoding Error"),
                                    tr("Could not find location \"%1\":\n%2").arg(query, error));

                // Clean up
                geocoder->deleteLater();
            });

    // Start geocoding
    geocoder->geocode(query);
}

void MainWindow::onRasterButtonClicked() {
    startTilePreload(); // Start preloading elevation tiles in background
    // Generate with saved config defaults
    QString configJson = stateManager_->restoreConfig("raster/config", "{}");
    if (configJson.isEmpty() || configJson == "{}") {
        // No saved config, show config panel
        onShowRasterConfig();
    } else {
        startGeneration("raster", configJson);
    }
}

void MainWindow::onVectorButtonClicked() {
    startTilePreload(); // Start preloading elevation tiles in background
    // Generate with saved config defaults
    QString configJson = stateManager_->restoreConfig("vector/config", "{}");
    if (configJson.isEmpty() || configJson == "{}") {
        // No saved config, show config panel
        onShowVectorConfig();
    } else {
        startGeneration("vector", configJson);
    }
}

void MainWindow::onMeshButtonClicked() {
    startTilePreload(); // Start preloading elevation tiles in background
    // Generate with saved config defaults
    QString configJson = stateManager_->restoreConfig("mesh/config", "{}");
    if (configJson.isEmpty() || configJson == "{}") {
        // No saved config, show config panel
        onShowMeshConfig();
    } else {
        startGeneration("mesh", configJson);
    }
}

void MainWindow::onNewSelection() {
    // Selection is now handled via Leaflet JavaScript
    // No Qt-side selection rectangle to clear
}

void MainWindow::onOpenConfig() {
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open Configuration"),
        QString(),
        tr("JSON Files (*.json);;All Files (*)")
    );

    if (!filename.isEmpty()) {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to open file: %1").arg(file.errorString()));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (doc.isNull() || !doc.isObject()) {
            QMessageBox::critical(this, tr("Error"),
                tr("Invalid JSON configuration file"));
            return;
        }

        QJsonObject config = doc.object();

        // Load panel configurations
        if (config.contains("raster") && rasterPanel_) {
            QString rasterConfig = QJsonDocument(config["raster"].toObject()).toJson(QJsonDocument::Compact);
            rasterPanel_->setConfigFromJson(rasterConfig);
        }

        if (config.contains("vector") && vectorPanel_) {
            QString vectorConfig = QJsonDocument(config["vector"].toObject()).toJson(QJsonDocument::Compact);
            vectorPanel_->setConfigFromJson(vectorConfig);
        }

        if (config.contains("mesh") && meshPanel_) {
            QString meshConfig = QJsonDocument(config["mesh"].toObject()).toJson(QJsonDocument::Compact);
            meshPanel_->setConfigFromJson(meshConfig);
        }

        // Optionally load map bounds
        if (config.contains("map_bounds") && mapWidget_) {
            QJsonObject bounds = config["map_bounds"].toObject();
            if (bounds.contains("min_lat") && bounds.contains("min_lon") &&
                bounds.contains("max_lat") && bounds.contains("max_lon")) {
                double minLat = bounds["min_lat"].toDouble();
                double minLon = bounds["min_lon"].toDouble();
                double maxLat = bounds["max_lat"].toDouble();
                double maxLon = bounds["max_lon"].toDouble();
                mapWidget_->fitBounds(minLat, minLon, maxLat, maxLon);
            }
        }

        statusBar()->showMessage(tr("Loaded configuration from: %1").arg(filename), 3000);
    }
}

void MainWindow::onSaveConfig() {
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save Configuration"),
        QString(),
        tr("JSON Files (*.json);;All Files (*)")
    );

    if (!filename.isEmpty()) {
        // Ensure .json extension
        if (!filename.endsWith(".json", Qt::CaseInsensitive)) {
            filename += ".json";
        }

        QJsonObject config;

        // Save panel configurations
        if (rasterPanel_) {
            QString rasterJson = rasterPanel_->getConfigJson();
            QJsonDocument rasterDoc = QJsonDocument::fromJson(rasterJson.toUtf8());
            if (!rasterDoc.isNull()) {
                config["raster"] = rasterDoc.object();
            }
        }

        if (vectorPanel_) {
            QString vectorJson = vectorPanel_->getConfigJson();
            QJsonDocument vectorDoc = QJsonDocument::fromJson(vectorJson.toUtf8());
            if (!vectorDoc.isNull()) {
                config["vector"] = vectorDoc.object();
            }
        }

        if (meshPanel_) {
            QString meshJson = meshPanel_->getConfigJson();
            QJsonDocument meshDoc = QJsonDocument::fromJson(meshJson.toUtf8());
            if (!meshDoc.isNull()) {
                config["mesh"] = meshDoc.object();
            }
        }

        // Save current map bounds if available
        if (mapWidget_) {
            // Note: We would need to add a method to get current bounds from mapWidget_
            // For now, we'll skip this or implement if needed
        }

        // Write to file
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, tr("Error"),
                tr("Failed to save file: %1").arg(file.errorString()));
            return;
        }

        QJsonDocument doc(config);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        statusBar()->showMessage(tr("Saved configuration to: %1").arg(filename), 3000);
    }
}

void MainWindow::onPreferences() {
    if (!preferencesDialog_) {
        preferencesDialog_ = std::make_unique<PreferencesDialog>(stateManager_, this);
        // Connect units changed signal to update UI immediately
        connect(preferencesDialog_.get(), &PreferencesDialog::unitsChanged,
                this, &MainWindow::applyUnitsPreference);
        // Connect output directory changed signal to clear session memory
        connect(preferencesDialog_.get(), &PreferencesDialog::outputDirectoryChanged,
                this, [this]() { lastUsedOutputDir_.clear(); });
    }
    preferencesDialog_->exec();
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this,
        tr("About Topographic Generator"),
        tr("<h3>Topographic Generator 0.22</h3>"
           "<p>High-performance topographic model generator using professional "
           "geometry libraries and algorithms.</p>"
           "<p>Copyright © 2025 Matthew Block<br>"
           "Licensed under the MIT License.</p>"
           "<p>Core algorithms adapted from Bambu Slicer (libslic3r).</p>")
    );
}

void MainWindow::onSelectionChanged(double min_lat, double min_lon, double max_lat, double max_lon) {
    // Update status bar with viewport bounds
    // Format: "Upper Left: lat, lon → Lower Right: lat, lon"

    auto formatCoord = [](double value, bool isLatitude) -> QString {
        QString dir;
        if (isLatitude) {
            dir = (value >= 0) ? "N" : "S";
        } else {
            dir = (value >= 0) ? "E" : "W";
        }
        return QString("%1°%2").arg(std::abs(value), 0, 'f', 4).arg(dir);
    };

    QString upperLeft = formatCoord(max_lat, true) + ", " + formatCoord(min_lon, false);
    QString lowerRight = formatCoord(min_lat, true) + ", " + formatCoord(max_lon, false);

    boundsLabel_->setText(QString("Upper Left: %1 → Lower Right: %2")
        .arg(upperLeft).arg(lowerRight));
}

void MainWindow::createConfigPanels() {
    // Create config panels (initially hidden)
    rasterPanel_ = std::make_unique<RasterConfigPanel>(stateManager_, this);
    vectorPanel_ = std::make_unique<VectorConfigPanel>(stateManager_, this);
    meshPanel_ = std::make_unique<MeshConfigPanel>(stateManager_, this);

    // Connect generate signals
    connect(rasterPanel_.get(), &RasterConfigPanel::generateRequested,
            this, &MainWindow::onRasterGenerationRequested);
    connect(vectorPanel_.get(), &VectorConfigPanel::generateRequested,
            this, &MainWindow::onVectorGenerationRequested);
    connect(meshPanel_.get(), &MeshConfigPanel::generateRequested,
            this, &MainWindow::onMeshGenerationRequested);

    // Set panels to be separate dialog windows
    rasterPanel_->setWindowFlags(Qt::Window);
    rasterPanel_->setWindowTitle("Raster Configuration");
    rasterPanel_->setWindowModality(Qt::ApplicationModal);

    vectorPanel_->setWindowFlags(Qt::Window);
    vectorPanel_->setWindowTitle("Vector Configuration");
    vectorPanel_->setWindowModality(Qt::ApplicationModal);

    meshPanel_->setWindowFlags(Qt::Window);
    meshPanel_->setWindowTitle("Mesh Configuration");
    meshPanel_->setWindowModality(Qt::ApplicationModal);
}

void MainWindow::onShowRasterConfig() {
    startTilePreload(); // Start preloading elevation tiles in background
    if (rasterPanel_) {
        rasterPanel_->show();
        rasterPanel_->raise();
        rasterPanel_->activateWindow();
    }
}

void MainWindow::onShowVectorConfig() {
    startTilePreload(); // Start preloading elevation tiles in background
    if (vectorPanel_) {
        vectorPanel_->show();
        vectorPanel_->raise();
        vectorPanel_->activateWindow();
    }
}

void MainWindow::onShowMeshConfig() {
    startTilePreload(); // Start preloading elevation tiles in background
    if (meshPanel_) {
        meshPanel_->show();
        meshPanel_->raise();
        meshPanel_->activateWindow();
    }
}

void MainWindow::onRasterGenerationRequested() {
    if (!rasterPanel_) return;

    QString configJson = rasterPanel_->getConfigJson();
    startGeneration("raster", configJson);
}

void MainWindow::onVectorGenerationRequested() {
    if (!vectorPanel_) return;

    QString configJson = vectorPanel_->getConfigJson();
    startGeneration("vector", configJson);
}

void MainWindow::onMeshGenerationRequested() {
    if (!meshPanel_) return;

    QString configJson = meshPanel_->getConfigJson();
    startGeneration("mesh", configJson);
}

void MainWindow::onCopyRasterCommand() {
    QString configJson = stateManager_->restoreConfig("raster/config", "{}");
    if (configJson.isEmpty() || configJson == "{}") {
        QMessageBox::information(this, tr("No Configuration"),
            tr("Please configure raster settings first using 'Configure...' menu option."));
        return;
    }
    QString command = buildCliCommand("raster", configJson);
    if (!command.isEmpty()) {
        QApplication::clipboard()->setText(command);
        statusBar()->showMessage(tr("CLI command copied to clipboard"), 10000);
    }
}

void MainWindow::onCopyVectorCommand() {
    QString configJson = stateManager_->restoreConfig("vector/config", "{}");
    if (configJson.isEmpty() || configJson == "{}") {
        QMessageBox::information(this, tr("No Configuration"),
            tr("Please configure vector settings first using 'Configure...' menu option."));
        return;
    }
    QString command = buildCliCommand("vector", configJson);
    if (!command.isEmpty()) {
        QApplication::clipboard()->setText(command);
        statusBar()->showMessage(tr("CLI command copied to clipboard"), 10000);
    }
}

void MainWindow::onCopyMeshCommand() {
    QString configJson = stateManager_->restoreConfig("mesh/config", "{}");
    if (configJson.isEmpty() || configJson == "{}") {
        QMessageBox::information(this, tr("No Configuration"),
            tr("Please configure mesh settings first using 'Configure...' menu option."));
        return;
    }
    QString command = buildCliCommand("mesh", configJson);
    if (!command.isEmpty()) {
        QApplication::clipboard()->setText(command);
        statusBar()->showMessage(tr("CLI command copied to clipboard"), 10000);
    }
}

QString MainWindow::buildCliCommand(const QString& outputType, const QString& configJson) {
    // Check if map bounds are set
    auto bounds = mapWidget_->getCurrentBounds();
    if (!bounds) {
        QMessageBox::warning(this, tr("No Map Bounds"),
            tr("Please wait for the map to load before generating CLI command."));
        return QString();
    }

    auto [min_lat, min_lon, max_lat, max_lon] = *bounds;

    // Get CLI config directory
    auto cliConfigSettings = stateManager_->restoreCliConfigSettings();
    QString configDir = cliConfigSettings.configDirectory;

    // Ensure directory exists
    QDir dir(configDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::critical(this, tr("Directory Error"),
                tr("Failed to create CLI config directory:\n%1").arg(configDir));
            return QString();
        }
    }

    // Create timestamped config filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString configFileName = QString("config_%1_%2.json").arg(outputType).arg(timestamp);
    QString configFilePath = QDir(configDir).filePath(configFileName);

    // Write config JSON to file (pretty-printed)
    QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8());
    QFile configFile(configFilePath);
    if (!configFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("File Error"),
            tr("Failed to write config file:\n%1").arg(configFilePath));
        return QString();
    }
    configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();

    // Build CLI command
    QString command = "topo-gen";

    // Add config file path (absolute path with native separators)
    command += QString(" --config \"%1\"").arg(QDir::toNativeSeparators(configFilePath));

    // Add bounds
    command += QString(" --upper-left %1,%2").arg(max_lat, 0, 'f', 6).arg(min_lon, 0, 'f', 6);
    command += QString(" --lower-right %1,%2").arg(min_lat, 0, 'f', 6).arg(max_lon, 0, 'f', 6);

    // Add terrain-following flag if mesh output and enabled
    if (outputType == "mesh") {
        QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8());
        if (doc.isObject() && doc.object()["terrain_following"].toBool(false)) {
            command += " --terrain-following";
        }
    }

    // Display command in status bar
    statusBar()->showMessage(command, 10000);

    return command;
}

void MainWindow::startTilePreload() {
    // Skip if already preloading
    if (preloadWorker_ != nullptr && preloadWorker_->isRunning()) {
        return;
    }

    // Get current map bounds
    auto bounds = mapWidget_->getCurrentBounds();
    if (!bounds) {
        return; // No bounds yet, skip silently
    }

    auto [min_lat, min_lon, max_lat, max_lon] = *bounds;

    // Get cache directory
    auto cacheSettings = stateManager_->restoreCacheSettings();
    QString cacheDir = cacheSettings.location;
    if (cacheDir.isEmpty()) {
        return; // No cache directory configured, skip
    }

    // Clean up old worker if it exists but isn't running
    if (preloadWorker_ != nullptr) {
        preloadWorker_->wait(); // Ensure it's fully stopped
        delete preloadWorker_;
        preloadWorker_ = nullptr;
    }

    // Create preload parameters
    TilePreloadWorker::PreloadParams params;
    params.minLat = min_lat;
    params.minLon = min_lon;
    params.maxLat = max_lat;
    params.maxLon = max_lon;
    params.cacheDir = cacheDir + "/tiles"; // Add tiles subdirectory for SRTM tiles

    // Create and configure worker
    preloadWorker_ = new TilePreloadWorker(params, this);

    // Connect signals (optional status updates)
    connect(preloadWorker_, &TilePreloadWorker::tilesFound, this,
            [this](int total, int /*cached*/, int needDownload) {
                if (needDownload > 0) {
                    statusBar()->showMessage(
                        tr("Preloading elevation tiles: %1 of %2 tiles need download...")
                            .arg(needDownload).arg(total), 5000);
                }
            });

    connect(preloadWorker_, &TilePreloadWorker::preloadComplete, this,
            [this](int downloaded) {
                if (downloaded > 0) {
                    statusBar()->showMessage(
                        tr("Preloaded %1 elevation tile(s)").arg(downloaded), 3000);
                }
            });

    connect(preloadWorker_, &TilePreloadWorker::preloadFailed, this,
            [](const QString& error) {
                // Silent failure - don't interrupt user workflow
                qDebug() << "Tile preload failed (non-critical):" << error;
            });

    // Clean up when finished
    connect(preloadWorker_, &TilePreloadWorker::finished, preloadWorker_, &QObject::deleteLater);
    connect(preloadWorker_, &TilePreloadWorker::finished, this,
            [this]() {
                preloadWorker_ = nullptr; // Clear pointer after deletion
            });

    // Start with low priority
    preloadWorker_->start(QThread::LowPriority);
}

void MainWindow::startGeneration(const QString& outputType, const QString& configJson) {
    // Check if map bounds are set
    auto bounds = mapWidget_->getCurrentBounds();
    if (!bounds) {
        QMessageBox::warning(this, tr("No Map Bounds"),
                           tr("Please wait for the map to load before generating output."));
        return;
    }

    auto [min_lat, min_lon, max_lat, max_lon] = *bounds;

    // Save timestamped generation config for record-keeping
    QString genConfigDir;
#ifdef Q_OS_MACOS
    genConfigDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/generation-configs";
#elif defined(Q_OS_WIN)
    genConfigDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/generation-configs";
#else
    genConfigDir = QDir::homePath() + "/.topo-gen/generation-configs";
#endif

    QDir genDir(genConfigDir);
    if (!genDir.exists()) {
        genDir.mkpath(".");
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString genConfigFileName = QString("generation_%1_%2.json").arg(outputType).arg(timestamp);
    QString genConfigPath = genDir.filePath(genConfigFileName);

    QFile genConfigFile(genConfigPath);
    if (genConfigFile.open(QIODevice::WriteOnly)) {
        // Create a complete generation config including bounds
        QJsonDocument configDoc = QJsonDocument::fromJson(configJson.toUtf8());
        QJsonObject fullConfig = configDoc.object();

        // Add bounds to the config
        fullConfig["bounds"] = QJsonObject{
            {"min_lat", min_lat},
            {"min_lon", min_lon},
            {"max_lat", max_lat},
            {"max_lon", max_lon}
        };
        fullConfig["timestamp"] = timestamp;
        fullConfig["output_type"] = outputType;

        QJsonDocument fullConfigDoc(fullConfig);
        genConfigFile.write(fullConfigDoc.toJson(QJsonDocument::Indented));
        genConfigFile.close();
    }

    // Get output settings from preferences
    auto outputSettings = stateManager_->restoreOutputSettings();
    QString basename = outputSettings.defaultBasename;

    // Determine output directory: use session memory if available, otherwise preference default
    QString outputDir;
    if (!lastUsedOutputDir_.isEmpty()) {
        // Use the directory from this session
        outputDir = lastUsedOutputDir_;
    } else if (!outputSettings.defaultOutputDir.isEmpty()) {
        // Use preference default for first generation
        outputDir = outputSettings.defaultOutputDir;
        lastUsedOutputDir_ = outputDir;  // Remember for this session
    } else {
        // No default set, ask user
        outputDir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Output Directory"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        );

        if (outputDir.isEmpty()) {
            return; // User cancelled
        }
        lastUsedOutputDir_ = outputDir;  // Remember for this session
    }

    // If no default basename, use coordinates-based name
    if (basename.isEmpty()) {
        basename = QString("topo_%1_%2")
            .arg(static_cast<int>(min_lat))
            .arg(static_cast<int>(min_lon));
    }

    // Create worker parameters
    GenerationWorker::GenerationParams params;
    params.minLat = min_lat;
    params.minLon = min_lon;
    params.maxLat = max_lat;
    params.maxLon = max_lon;
    params.configJson = configJson;
    params.outputDir = outputDir;
    params.basename = basename;

    // Parse terrain-following from config (mesh only)
    bool terrainFollowing = false;
    if (outputType == "mesh") {
        QJsonDocument doc = QJsonDocument::fromJson(configJson.toUtf8());
        if (doc.isObject()) {
            terrainFollowing = doc.object()["terrain_following"].toBool(false);
        }
    }
    params.terrainFollowing = terrainFollowing;

    // Get cache directory and log level from settings
    auto cacheSettings = stateManager_->restoreCacheSettings();
    params.cacheDir = cacheSettings.location;

    auto loggingSettings = stateManager_->restoreLoggingSettings();
    params.logLevel = loggingSettings.level;

    if (outputType == "raster") {
        params.outputType = GenerationWorker::OutputType::Raster;
    } else if (outputType == "vector") {
        params.outputType = GenerationWorker::OutputType::Vector;
    } else if (outputType == "mesh") {
        params.outputType = GenerationWorker::OutputType::Mesh;
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Unknown output type: %1").arg(outputType));
        return;
    }

    // Create and configure worker
    auto* worker = new GenerationWorker(params, this);

    // Create progress dialog
    auto* progressDialog = new GenerationProgressDialog(this);

    // Connect worker signals to progress dialog
    connect(worker, &GenerationWorker::progressUpdate,
            progressDialog, &GenerationProgressDialog::updateProgress);
    connect(worker, &GenerationWorker::generationComplete,
            progressDialog, &GenerationProgressDialog::onGenerationComplete);
    connect(worker, &GenerationWorker::generationFailed,
            progressDialog, &GenerationProgressDialog::onGenerationFailed);

    // Connect cancel button to worker termination
    connect(progressDialog, &GenerationProgressDialog::cancelRequested,
            worker, &GenerationWorker::terminate);

    // Show preview on success
    connect(worker, &GenerationWorker::generationComplete,
            this, [this, progressDialog](const QStringList& outputFiles) {
                // Show preview dialog
                auto* previewDialog = new PreviewDialog(this);
                previewDialog->setFiles(outputFiles);
                previewDialog->setAttribute(Qt::WA_DeleteOnClose);

                // Close progress dialog and show preview
                progressDialog->accept();
                previewDialog->show();
            });

    // Clean up worker when done
    connect(worker, &GenerationWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &GenerationWorker::finished, progressDialog, &QObject::deleteLater);

    // Start generation
    worker->start();
    progressDialog->exec();
}

void MainWindow::applyUnitsPreference(bool useMetric) {
    // Apply to MapFeaturesPanel altitude display
    if (mapFeaturesPanel_) {
        mapFeaturesPanel_->setUseMetric(useMetric);
    }

    // Apply to MapWidget's ZoomControls
    if (mapWidget_) {
        mapWidget_->setUseMetric(useMetric);
    }
}

} // namespace topo
