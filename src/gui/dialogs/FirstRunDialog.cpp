/**
 * @file FirstRunDialog.cpp
 * @brief Implementation of first-run configuration dialog
 */

#include "FirstRunDialog.hpp"
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
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace topo {

FirstRunDialog::FirstRunDialog(StateManager* stateManager, QWidget *parent)
    : QDialog(parent), stateManager_(stateManager) {
    setWindowTitle("First Run Configuration");
    setModal(true);
    createUI();
    connectSignals();
    setDefaultPaths();
}

void FirstRunDialog::createUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Welcome message
    auto* welcomeLabel = new QLabel(
        "<h2>Welcome to Topographic Generator</h2>"
        "<p>Please configure the application directories before continuing.</p>",
        this
    );
    welcomeLabel->setWordWrap(true);
    mainLayout->addWidget(welcomeLabel);

    // Directory settings group
    auto* dirGroup = new QGroupBox("Directory Configuration", this);
    auto* dirLayout = new QGridLayout(dirGroup);

    // Cache directory
    dirLayout->addWidget(new QLabel("Cache Directory:", this), 0, 0);
    auto* cacheLayout = new QHBoxLayout();
    cacheDirectoryEdit_ = new QLineEdit(this);
    cacheLayout->addWidget(cacheDirectoryEdit_);
    auto* browseCacheButton = new QPushButton("Browse...", this);
    cacheLayout->addWidget(browseCacheButton);
    dirLayout->addLayout(cacheLayout, 0, 1);

    auto* cacheHelpLabel = new QLabel(
        "<small>Location for downloaded elevation and map tiles</small>",
        this
    );
    cacheHelpLabel->setWordWrap(true);
    dirLayout->addWidget(cacheHelpLabel, 1, 1);

    // Data directory
    dirLayout->addWidget(new QLabel("Data Directory:", this), 2, 0);
    auto* dataLayout = new QHBoxLayout();
    dataDirectoryEdit_ = new QLineEdit(this);
    dataLayout->addWidget(dataDirectoryEdit_);
    auto* browseDataButton = new QPushButton("Browse...", this);
    dataLayout->addWidget(browseDataButton);
    dirLayout->addLayout(dataLayout, 2, 1);

    auto* dataHelpLabel = new QLabel(
        "<small>Location for application configuration and data files</small>",
        this
    );
    dataHelpLabel->setWordWrap(true);
    dirLayout->addWidget(dataHelpLabel, 3, 1);

    mainLayout->addWidget(dirGroup);

    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("QLabel { color: #666; font-style: italic; }");
    mainLayout->addWidget(statusLabel_);

    mainLayout->addStretch();

    // Button box
    auto* buttonLayout = new QHBoxLayout();

    auto* defaultsButton = new QPushButton("Use Defaults", this);
    buttonLayout->addWidget(defaultsButton);

    buttonLayout->addStretch();

    auto* cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addWidget(cancelButton);

    auto* okButton = new QPushButton("OK", this);
    okButton->setDefault(true);
    buttonLayout->addWidget(okButton);

    mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(browseCacheButton, &QPushButton::clicked, this, &FirstRunDialog::onBrowseCacheDirectory);
    connect(browseDataButton, &QPushButton::clicked, this, &FirstRunDialog::onBrowseDataDirectory);
    connect(defaultsButton, &QPushButton::clicked, this, &FirstRunDialog::onUseDefaults);
    connect(cancelButton, &QPushButton::clicked, this, &FirstRunDialog::onCancel);
    connect(okButton, &QPushButton::clicked, this, &FirstRunDialog::onOk);

    resize(650, 350);
}

void FirstRunDialog::connectSignals() {
    // Signals already connected in createUI()
}

void FirstRunDialog::setDefaultPaths() {
    QString defaultDir = getPlatformDefaultDirectory();
    cacheDirectoryEdit_->setText(defaultDir + "/cache");
    dataDirectoryEdit_->setText(defaultDir);

    statusLabel_->setText("Using platform default directories. You can customize these paths or click OK to continue.");
}

QString FirstRunDialog::getPlatformDefaultDirectory() {
#ifdef Q_OS_MACOS
    // macOS: ~/Library/Application Support/Topographic Generator
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#elif defined(Q_OS_WIN)
    // Windows: ~/Documents/Topographic Generator
    QString docsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return docsPath + "/Topographic Generator";
#else
    // Linux: ~/.topographic-generator
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return homePath + "/.topographic-generator";
#endif
}

bool FirstRunDialog::validatePaths() {
    QString cacheDir = cacheDirectoryEdit_->text();
    QString dataDir = dataDirectoryEdit_->text();

    // Check that paths are not empty
    if (cacheDir.isEmpty() || dataDir.isEmpty()) {
        statusLabel_->setStyleSheet("QLabel { color: red; }");
        statusLabel_->setText("Error: Both directories must be specified.");
        return false;
    }

    // Check if directories exist or can be created
    QDir cacheDirObj(cacheDir);
    QDir dataDirObj(dataDir);

    bool cacheExists = cacheDirObj.exists();
    bool dataExists = dataDirObj.exists();

    // Try to create directories if they don't exist
    if (!cacheExists) {
        if (!cacheDirObj.mkpath(".")) {
            statusLabel_->setStyleSheet("QLabel { color: red; }");
            statusLabel_->setText("Error: Cannot create cache directory. Please check permissions.");
            return false;
        }
    }

    if (!dataExists) {
        if (!dataDirObj.mkpath(".")) {
            statusLabel_->setStyleSheet("QLabel { color: red; }");
            statusLabel_->setText("Error: Cannot create data directory. Please check permissions.");
            return false;
        }
    }

    // Check write permissions by creating a test file
    QString testFile = cacheDir + "/.write_test";
    QFile file(testFile);
    if (!file.open(QIODevice::WriteOnly)) {
        statusLabel_->setStyleSheet("QLabel { color: red; }");
        statusLabel_->setText("Error: Cache directory is not writable. Please select a different location.");
        return false;
    }
    file.close();
    file.remove();

    testFile = dataDir + "/.write_test";
    QFile file2(testFile);
    if (!file2.open(QIODevice::WriteOnly)) {
        statusLabel_->setStyleSheet("QLabel { color: red; }");
        statusLabel_->setText("Error: Data directory is not writable. Please select a different location.");
        return false;
    }
    file2.close();
    file2.remove();

    // All checks passed
    statusLabel_->setStyleSheet("QLabel { color: green; }");
    statusLabel_->setText("✓ Directories are valid and writable.");
    return true;
}

void FirstRunDialog::onBrowseCacheDirectory() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Cache Directory",
        cacheDirectoryEdit_->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        cacheDirectoryEdit_->setText(dir);
        validatePaths();
    }
}

void FirstRunDialog::onBrowseDataDirectory() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Data Directory",
        dataDirectoryEdit_->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        dataDirectoryEdit_->setText(dir);
        validatePaths();
    }
}

void FirstRunDialog::onUseDefaults() {
    setDefaultPaths();
    validatePaths();
}

void FirstRunDialog::onOk() {
    if (!validatePaths()) {
        return;
    }

    // Save directories to settings
    if (stateManager_) {
        StateManager::CacheSettings cacheSettings;
        cacheSettings.location = cacheDirectoryEdit_->text();
        cacheSettings.sizeMB = 500;  // Default size
        cacheSettings.cleanupDays = 30;  // Default cleanup
        stateManager_->saveCacheSettings(cacheSettings);

        // Save data directory
        stateManager_->saveConfig("DataDirectory", dataDirectoryEdit_->text());
    }

    accept();
}

void FirstRunDialog::onCancel() {
    auto result = QMessageBox::question(
        this,
        "Cancel Configuration",
        "The application requires these directories to be configured. "
        "Are you sure you want to cancel? The application will exit.",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (result == QMessageBox::Yes) {
        reject();
    }
}

} // namespace topo
