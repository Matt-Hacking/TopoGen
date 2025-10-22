#pragma once

/**
 * @file StateManager.hpp
 * @brief Manages application state persistence using QSettings
 *
 * Handles saving and restoring:
 * - Window geometry and state
 * - Map position and zoom level
 * - Last selection bounds
 * - Output configuration defaults
 * - User preferences
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QSettings>
#include <QMainWindow>
#include <QString>
#include <QPointF>
#include <optional>

namespace topo {

class StateManager {
public:
    StateManager();
    ~StateManager() = default;

    // Preference structures
    enum class UnitSystem {
        Imperial,  // feet, miles
        Metric     // meters, kilometers
    };

    struct CacheSettings {
        int sizeMB = 500;
        int cleanupDays = 30;
        QString location;
    };

    struct LoggingSettings {
        int level = 3;  // 1-6
        bool enableLogFile = false;
        QString logFilePath;
    };

    struct OutputSettings {
        QString defaultOutputDir;
        QString defaultBasename;
    };

    struct UnitsSettings {
        UnitSystem system = UnitSystem::Imperial;
    };

    struct CliConfigSettings {
        QString configDirectory;  // Where to store CLI config JSON files
    };

    // Window state
    void saveWindow(const QMainWindow& window);
    void restoreWindow(QMainWindow& window);

    // Map state
    void saveMapState(const QPointF& center, int zoom);
    std::pair<QPointF, int> restoreMapState();

    // Selection bounds (min_lat, min_lon, max_lat, max_lon)
    void saveSelectionBounds(double min_lat, double min_lon, double max_lat, double max_lon);
    std::optional<std::tuple<double, double, double, double>> restoreSelectionBounds();

    // Cache settings
    void saveCacheSettings(const CacheSettings& settings);
    CacheSettings restoreCacheSettings();

    // Logging settings
    void saveLoggingSettings(const LoggingSettings& settings);
    LoggingSettings restoreLoggingSettings();

    // Output settings
    void saveOutputSettings(const OutputSettings& settings);
    OutputSettings restoreOutputSettings();

    // Units settings
    void saveUnitsSettings(const UnitsSettings& settings);
    UnitsSettings restoreUnitsSettings();

    // CLI config settings
    void saveCliConfigSettings(const CliConfigSettings& settings);
    CliConfigSettings restoreCliConfigSettings();

    // Generic save/restore for config panels
    void saveConfig(const QString& key, const QString& jsonConfig);
    QString restoreConfig(const QString& key, const QString& defaultValue = "{}") const;

    // Clear all settings
    void clear();

private:
    QSettings settings_;

    // Default values
    static constexpr double DEFAULT_CENTER_LAT = 63.069;
    static constexpr double DEFAULT_CENTER_LON = -151.007;
    static constexpr int DEFAULT_ZOOM = 10;
};

} // namespace topo
