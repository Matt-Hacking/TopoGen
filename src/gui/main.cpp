/**
 * @file main.cpp
 * @brief Qt GUI application entry point for Topographic Generator
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.hpp"
#include "utils/StateManager.hpp"
#include "dialogs/FirstRunDialog.hpp"

int main(int argc, char *argv[]) {
    // Create Qt application
    QApplication app(argc, argv);

    // Set application metadata
    QCoreApplication::setOrganizationName("Matthew Block");
    QCoreApplication::setOrganizationDomain("matthewblock.com");
    QCoreApplication::setApplicationName("Topographic Generator");
    QCoreApplication::setApplicationVersion("0.22.001");

    // Use native platform style
    #ifdef Q_OS_MACOS
    QApplication::setStyle(QStyleFactory::create("macOS"));
    #elif defined(Q_OS_WIN)
    QApplication::setStyle(QStyleFactory::create("Windows"));
    #else
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    #endif

    // Create and restore state manager
    topo::StateManager stateManager;

    // Check if cache directory is configured
    auto cacheSettings = stateManager.restoreCacheSettings();
    qDebug() << "Cache location from settings:" << cacheSettings.location;
    qDebug() << "Is empty:" << cacheSettings.location.isEmpty();

    if (cacheSettings.location.isEmpty()) {
        qDebug() << "Showing FirstRunDialog...";
        // First run - show configuration dialog
        topo::FirstRunDialog firstRunDialog(&stateManager);
        int result = firstRunDialog.exec();
        qDebug() << "FirstRunDialog result:" << result;
        if (result == QDialog::Rejected) {
            qDebug() << "User cancelled, exiting...";
            // User cancelled - exit application
            return 0;
        }
    }

    // Create main window
    topo::MainWindow window;

    // Restore window geometry and state from last session
    stateManager.restoreWindow(window);

    // Show window
    window.show();

    // Run application event loop
    int result = app.exec();

    // Save window state on exit
    stateManager.saveWindow(window);

    return result;
}
