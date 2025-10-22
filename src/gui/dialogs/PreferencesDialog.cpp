/**
 * @file PreferencesDialog.cpp
 * @brief Implementation of preferences dialog
 */

#include "PreferencesDialog.hpp"
#include "../utils/StateManager.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QStandardPaths>
#include <QDir>

namespace topo {

PreferencesDialog::PreferencesDialog(StateManager* stateManager, QWidget *parent)
    : QDialog(parent), stateManager_(stateManager) {
    setWindowTitle("Preferences");
    createUI();
    connectSignals();
    loadFromSettings();
}

void PreferencesDialog::createUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Create tab widget for different preference categories
    auto* tabWidget = new QTabWidget(this);

    // Cache settings tab
    auto* cacheTab = new QWidget(this);
    auto* cacheLayout = new QVBoxLayout(cacheTab);

    auto* cacheGroup = new QGroupBox("Cache Settings", this);
    auto* cacheGridLayout = new QGridLayout(cacheGroup);

    cacheGridLayout->addWidget(new QLabel("Tile Cache Size (MB):", this), 0, 0);
    cacheSizeMBSpinBox_ = new QSpinBox(this);
    cacheSizeMBSpinBox_->setRange(50, 10000);
    cacheSizeMBSpinBox_->setValue(DEFAULT_CACHE_SIZE_MB);
    cacheGridLayout->addWidget(cacheSizeMBSpinBox_, 0, 1);

    cacheGridLayout->addWidget(new QLabel("Auto-cleanup after (days):", this), 1, 0);
    cacheCleanupDaysSpinBox_ = new QSpinBox(this);
    cacheCleanupDaysSpinBox_->setRange(1, 365);
    cacheCleanupDaysSpinBox_->setValue(DEFAULT_CLEANUP_DAYS);
    cacheGridLayout->addWidget(cacheCleanupDaysSpinBox_, 1, 1);

    cacheGridLayout->addWidget(new QLabel("Cache Location:", this), 2, 0);
    auto* cacheLocationLayout = new QHBoxLayout();
    cacheLocationEdit_ = new QLineEdit(this);
    cacheLocationEdit_->setReadOnly(true);
    cacheLocationLayout->addWidget(cacheLocationEdit_);
    auto* browseCacheButton = new QPushButton("Browse...", this);
    connect(browseCacheButton, &QPushButton::clicked, this, &PreferencesDialog::onBrowseCacheLocation);
    cacheLocationLayout->addWidget(browseCacheButton);
    cacheGridLayout->addLayout(cacheLocationLayout, 2, 1);

    auto* clearCacheButton = new QPushButton("Clear Cache Now", this);
    connect(clearCacheButton, &QPushButton::clicked, this, &PreferencesDialog::onClearCacheNow);
    cacheGridLayout->addWidget(clearCacheButton, 3, 1);

    cacheLayout->addWidget(cacheGroup);
    cacheLayout->addStretch();

    tabWidget->addTab(cacheTab, "Cache");

    // Logging settings tab
    auto* loggingTab = new QWidget(this);
    auto* loggingLayout = new QVBoxLayout(loggingTab);

    auto* loggingGroup = new QGroupBox("Logging Settings", this);
    auto* loggingGridLayout = new QGridLayout(loggingGroup);

    loggingGridLayout->addWidget(new QLabel("Log Level:", this), 0, 0);
    logLevelCombo_ = new QComboBox(this);
    logLevelCombo_->addItem("1 - Errors Only", 1);
    logLevelCombo_->addItem("2 - Warnings", 2);
    logLevelCombo_->addItem("3 - Information", 3);
    logLevelCombo_->addItem("4 - Detailed Info", 4);
    logLevelCombo_->addItem("5 - Debug", 5);
    logLevelCombo_->addItem("6 - Verbose Debug", 6);
    logLevelCombo_->setCurrentIndex(2); // Default to level 3
    loggingGridLayout->addWidget(logLevelCombo_, 0, 1);

    enableLogFileCheckbox_ = new QCheckBox("Enable Log File", this);
    loggingGridLayout->addWidget(enableLogFileCheckbox_, 1, 0, 1, 2);

    loggingGridLayout->addWidget(new QLabel("Log File Path:", this), 2, 0);
    auto* logFileLayout = new QHBoxLayout();
    logFilePathEdit_ = new QLineEdit(this);
    logFilePathEdit_->setReadOnly(true);
    logFilePathEdit_->setEnabled(false);
    logFileLayout->addWidget(logFilePathEdit_);
    auto* browseLogButton = new QPushButton("Browse...", this);
    browseLogButton->setEnabled(false);
    connect(browseLogButton, &QPushButton::clicked, this, &PreferencesDialog::onBrowseLogFile);
    connect(enableLogFileCheckbox_, &QCheckBox::toggled, logFilePathEdit_, &QWidget::setEnabled);
    connect(enableLogFileCheckbox_, &QCheckBox::toggled, browseLogButton, &QWidget::setEnabled);
    logFileLayout->addWidget(browseLogButton);
    loggingGridLayout->addLayout(logFileLayout, 2, 1);

    loggingLayout->addWidget(loggingGroup);
    loggingLayout->addStretch();

    tabWidget->addTab(loggingTab, "Logging");

    // Default settings tab
    auto* defaultsTab = new QWidget(this);
    auto* defaultsLayout = new QVBoxLayout(defaultsTab);

    auto* defaultsGroup = new QGroupBox("Default Output Settings", this);
    auto* defaultsGridLayout = new QGridLayout(defaultsGroup);

    defaultsGridLayout->addWidget(new QLabel("Default Output Directory:", this), 0, 0);
    auto* outputDirLayout = new QHBoxLayout();
    defaultOutputDirEdit_ = new QLineEdit(this);
    defaultOutputDirEdit_->setReadOnly(true);
    outputDirLayout->addWidget(defaultOutputDirEdit_);
    auto* browseOutputButton = new QPushButton("Browse...", this);
    connect(browseOutputButton, &QPushButton::clicked, this, &PreferencesDialog::onBrowseDefaultOutputDir);
    outputDirLayout->addWidget(browseOutputButton);
    defaultsGridLayout->addLayout(outputDirLayout, 0, 1);

    defaultsGridLayout->addWidget(new QLabel("Default Basename:", this), 1, 0);
    defaultBasenameEdit_ = new QLineEdit(this);
    defaultBasenameEdit_->setPlaceholderText("e.g., mount_denali");
    defaultsGridLayout->addWidget(defaultBasenameEdit_, 1, 1);

    defaultsLayout->addWidget(defaultsGroup);

    // Display settings group
    auto* displayGroup = new QGroupBox("Display Settings", this);
    auto* displayLayout = new QVBoxLayout(displayGroup);

    useMetricUnitsCheckbox_ = new QCheckBox("Use Metric Units (meters, kilometers)", this);
    useMetricUnitsCheckbox_->setToolTip(
        "When unchecked, uses Imperial units (feet, miles).\n"
        "Affects altitude display in zoom controls and minimum altitude slider."
    );
    displayLayout->addWidget(useMetricUnitsCheckbox_);

    defaultsLayout->addWidget(displayGroup);
    defaultsLayout->addStretch();

    tabWidget->addTab(defaultsTab, "Defaults");

    // CLI settings tab
    auto* cliTab = new QWidget(this);
    auto* cliLayout = new QVBoxLayout(cliTab);

    auto* cliGroup = new QGroupBox("CLI Command Generation", this);
    auto* cliGridLayout = new QGridLayout(cliGroup);

    cliGridLayout->addWidget(new QLabel("Config Directory:", this), 0, 0);
    auto* cliConfigDirLayout = new QHBoxLayout();
    cliConfigDirEdit_ = new QLineEdit(this);
    cliConfigDirEdit_->setReadOnly(true);
    cliConfigDirEdit_->setToolTip(
        "Directory where CLI configuration files will be stored when\n"
        "generating commands via the 'Copy CLI Command' menu option."
    );
    cliConfigDirLayout->addWidget(cliConfigDirEdit_);
    auto* browseCliButton = new QPushButton("Browse...", this);
    connect(browseCliButton, &QPushButton::clicked, this, &PreferencesDialog::onBrowseCliConfigDir);
    cliConfigDirLayout->addWidget(browseCliButton);
    cliGridLayout->addLayout(cliConfigDirLayout, 0, 1);

    auto* helpLabel = new QLabel(
        "When you use 'Copy CLI Command' from the toolbar menus, configuration files\n"
        "will be saved to this directory with timestamped filenames.\n\n"
        "The generated command will reference these absolute paths.",
        this
    );
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("QLabel { color: gray; font-size: 10pt; }");
    cliGridLayout->addWidget(helpLabel, 1, 0, 1, 2);

    cliLayout->addWidget(cliGroup);
    cliLayout->addStretch();

    tabWidget->addTab(cliTab, "CLI");

    mainLayout->addWidget(tabWidget);

    // Button box
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                           QDialogButtonBox::Apply |
                                           QDialogButtonBox::Cancel, this);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, &PreferencesDialog::onOk);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &PreferencesDialog::onApply);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    resize(600, 400);
}

void PreferencesDialog::connectSignals() {
    // Signals already connected in createUI()
}

void PreferencesDialog::loadFromSettings() {
    if (!stateManager_) return;

    auto cacheSettings = stateManager_->restoreCacheSettings();
    cacheSizeMBSpinBox_->setValue(cacheSettings.sizeMB);
    cacheCleanupDaysSpinBox_->setValue(cacheSettings.cleanupDays);
    cacheLocationEdit_->setText(cacheSettings.location);

    auto loggingSettings = stateManager_->restoreLoggingSettings();
    logLevelCombo_->setCurrentIndex(loggingSettings.level - 1); // Level 1-6 maps to index 0-5
    enableLogFileCheckbox_->setChecked(loggingSettings.enableLogFile);
    logFilePathEdit_->setText(loggingSettings.logFilePath);

    auto outputSettings = stateManager_->restoreOutputSettings();
    defaultOutputDirEdit_->setText(outputSettings.defaultOutputDir);
    defaultBasenameEdit_->setText(outputSettings.defaultBasename);

    auto unitsSettings = stateManager_->restoreUnitsSettings();
    useMetricUnitsCheckbox_->setChecked(unitsSettings.system == StateManager::UnitSystem::Metric);

    auto cliConfigSettings = stateManager_->restoreCliConfigSettings();
    cliConfigDirEdit_->setText(cliConfigSettings.configDirectory);
}

void PreferencesDialog::saveToSettings() {
    if (!stateManager_) return;

    // Check if output directory changed
    auto oldOutputSettings = stateManager_->restoreOutputSettings();
    QString oldOutputDir = oldOutputSettings.defaultOutputDir;

    StateManager::CacheSettings cacheSettings;
    cacheSettings.sizeMB = cacheSizeMBSpinBox_->value();
    cacheSettings.cleanupDays = cacheCleanupDaysSpinBox_->value();
    cacheSettings.location = cacheLocationEdit_->text();
    stateManager_->saveCacheSettings(cacheSettings);

    StateManager::LoggingSettings loggingSettings;
    loggingSettings.level = logLevelCombo_->currentData().toInt();
    loggingSettings.enableLogFile = enableLogFileCheckbox_->isChecked();
    loggingSettings.logFilePath = logFilePathEdit_->text();
    stateManager_->saveLoggingSettings(loggingSettings);

    StateManager::OutputSettings outputSettings;
    outputSettings.defaultOutputDir = defaultOutputDirEdit_->text();
    outputSettings.defaultBasename = defaultBasenameEdit_->text();
    stateManager_->saveOutputSettings(outputSettings);

    StateManager::UnitsSettings unitsSettings;
    unitsSettings.system = useMetricUnitsCheckbox_->isChecked() ?
        StateManager::UnitSystem::Metric : StateManager::UnitSystem::Imperial;
    stateManager_->saveUnitsSettings(unitsSettings);

    StateManager::CliConfigSettings cliConfigSettings;
    cliConfigSettings.configDirectory = cliConfigDirEdit_->text();
    stateManager_->saveCliConfigSettings(cliConfigSettings);

    // Emit signals to notify changes
    emit unitsChanged(unitsSettings.system == StateManager::UnitSystem::Metric);

    // Emit if output directory changed
    if (oldOutputDir != outputSettings.defaultOutputDir) {
        emit outputDirectoryChanged();
    }
}

void PreferencesDialog::onBrowseCacheLocation() {
    // Use platform-specific default
    QString defaultPath;
#ifdef Q_OS_MACOS
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache";
#elif defined(Q_OS_WIN)
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/cache";
#else
    defaultPath = QDir::homePath() + "/.topo-gen/cache";
#endif

    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Cache Directory",
        cacheLocationEdit_->text().isEmpty() ? defaultPath : cacheLocationEdit_->text()
    );

    if (!dir.isEmpty()) {
        cacheLocationEdit_->setText(dir);
    }
}

void PreferencesDialog::onBrowseLogFile() {
    // Use platform-specific default
    QString defaultPath;
#ifdef Q_OS_MACOS
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs/topographic_generator.log";
#elif defined(Q_OS_WIN)
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/logs/topographic_generator.log";
#else
    defaultPath = QDir::homePath() + "/.topo-gen/logs/topographic_generator.log";
#endif

    QString file = QFileDialog::getSaveFileName(
        this,
        "Select Log File",
        logFilePathEdit_->text().isEmpty() ? defaultPath : logFilePathEdit_->text(),
        "Log Files (*.log);;All Files (*)"
    );

    if (!file.isEmpty()) {
        logFilePathEdit_->setText(file);
    }
}

void PreferencesDialog::onBrowseDefaultOutputDir() {
    // Use platform-specific default
    QString defaultPath;
#ifdef Q_OS_MACOS
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen";
#elif defined(Q_OS_WIN)
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/Output";
#else
    defaultPath = QDir::homePath() + "/Topo-Gen";
#endif

    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Default Output Directory",
        defaultOutputDirEdit_->text().isEmpty() ? defaultPath : defaultOutputDirEdit_->text()
    );

    if (!dir.isEmpty()) {
        defaultOutputDirEdit_->setText(dir);
    }
}

void PreferencesDialog::onBrowseCliConfigDir() {
    // Use platform-specific default
    QString defaultPath;
#ifdef Q_OS_MACOS
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cli-configs";
#elif defined(Q_OS_WIN)
    defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/cli-configs";
#else
    defaultPath = QDir::homePath() + "/.topo-gen/cli-configs";
#endif

    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select CLI Config Directory",
        cliConfigDirEdit_->text().isEmpty() ? defaultPath : cliConfigDirEdit_->text()
    );

    if (!dir.isEmpty()) {
        cliConfigDirEdit_->setText(dir);
    }
}

void PreferencesDialog::onClearCacheNow() {
    auto result = QMessageBox::question(
        this,
        "Clear Cache",
        "Are you sure you want to clear all cached tiles? This cannot be undone.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result == QMessageBox::Yes) {
        QString cacheLocation = cacheLocationEdit_->text();
        if (cacheLocation.isEmpty()) {
            // Use platform-specific default
#ifdef Q_OS_MACOS
            cacheLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache";
#elif defined(Q_OS_WIN)
            cacheLocation = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/cache";
#else
            cacheLocation = QDir::homePath() + "/.topo-gen/cache";
#endif
        }

        QDir cacheDir(cacheLocation);
        if (cacheDir.exists()) {
            // Remove all files in cache directory
            QStringList files = cacheDir.entryList(QDir::Files);
            int removedCount = 0;
            for (const QString& file : files) {
                if (cacheDir.remove(file)) {
                    removedCount++;
                }
            }

            // Also remove subdirectories (zoom levels)
            QStringList subdirs = cacheDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString& subdir : subdirs) {
                QDir subDir(cacheDir.filePath(subdir));
                subDir.removeRecursively();
            }

            QMessageBox::information(
                this,
                "Cache Cleared",
                QString("Successfully cleared %1 cached files.").arg(removedCount)
            );
        } else {
            QMessageBox::warning(
                this,
                "Cache Not Found",
                "Cache directory does not exist or is already empty."
            );
        }
    }
}

void PreferencesDialog::onOk() {
    saveToSettings();
    accept();
}

void PreferencesDialog::onApply() {
    saveToSettings();
}

} // namespace topo
