/**
 * @file StateManager.cpp
 * @brief Implementation of application state persistence
 */

#include "StateManager.hpp"
#include <QPoint>
#include <QSize>
#include <QStandardPaths>
#include <QDir>

namespace topo {

StateManager::StateManager()
    : settings_(QSettings::IniFormat, QSettings::UserScope,
                "Matthew Block", "Topographic Generator") {
    // QSettings will use platform-native storage:
    // - macOS: ~/Library/Preferences/com.matthewblock.topographic-generator.plist
    // - Linux: ~/.config/Matthew Block/Topographic Generator.conf
    // - Windows: Registry HKEY_CURRENT_USER\Software\Matthew Block\Topographic Generator
}

void StateManager::saveWindow(const QMainWindow& window) {
    settings_.beginGroup("Window");
    settings_.setValue("geometry", window.saveGeometry());
    settings_.setValue("state", window.saveState());
    settings_.endGroup();
}

void StateManager::restoreWindow(QMainWindow& window) {
    settings_.beginGroup("Window");

    if (settings_.contains("geometry")) {
        window.restoreGeometry(settings_.value("geometry").toByteArray());
    } else {
        // Default window size if no saved geometry
        window.resize(1200, 800);
    }

    if (settings_.contains("state")) {
        window.restoreState(settings_.value("state").toByteArray());
    }

    settings_.endGroup();
}

void StateManager::saveMapState(const QPointF& center, int zoom) {
    settings_.beginGroup("Map");
    settings_.setValue("center_lat", center.y());
    settings_.setValue("center_lon", center.x());
    settings_.setValue("zoom_level", zoom);
    settings_.endGroup();
}

std::pair<QPointF, int> StateManager::restoreMapState() {
    settings_.beginGroup("Map");

    double lat = settings_.value("center_lat", DEFAULT_CENTER_LAT).toDouble();
    double lon = settings_.value("center_lon", DEFAULT_CENTER_LON).toDouble();
    int zoom = settings_.value("zoom_level", DEFAULT_ZOOM).toInt();

    settings_.endGroup();

    return {QPointF(lon, lat), zoom};
}

void StateManager::saveSelectionBounds(double min_lat, double min_lon, double max_lat, double max_lon) {
    settings_.beginGroup("Selection");
    settings_.setValue("min_lat", min_lat);
    settings_.setValue("min_lon", min_lon);
    settings_.setValue("max_lat", max_lat);
    settings_.setValue("max_lon", max_lon);
    settings_.endGroup();
}

std::optional<std::tuple<double, double, double, double>> StateManager::restoreSelectionBounds() {
    settings_.beginGroup("Selection");

    if (!settings_.contains("min_lat")) {
        settings_.endGroup();
        return std::nullopt;
    }

    double min_lat = settings_.value("min_lat").toDouble();
    double min_lon = settings_.value("min_lon").toDouble();
    double max_lat = settings_.value("max_lat").toDouble();
    double max_lon = settings_.value("max_lon").toDouble();

    settings_.endGroup();

    return std::make_tuple(min_lat, min_lon, max_lat, max_lon);
}

void StateManager::saveCacheSettings(const CacheSettings& settings) {
    settings_.beginGroup("Cache");
    settings_.setValue("size_mb", settings.sizeMB);
    settings_.setValue("cleanup_days", settings.cleanupDays);
    settings_.setValue("location", settings.location);
    settings_.endGroup();
}

StateManager::CacheSettings StateManager::restoreCacheSettings() {
    settings_.beginGroup("Cache");

    CacheSettings settings;
    settings.sizeMB = settings_.value("size_mb", 500).toInt();
    settings.cleanupDays = settings_.value("cleanup_days", 30).toInt();

    // Platform-specific default cache directory
    QString defaultCacheDir;
#ifdef Q_OS_MACOS
    // macOS: ~/Library/Application Support/Matthew Block/Topographic Generator/cache/
    defaultCacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache";
#elif defined(Q_OS_WIN)
    // Windows: %Documents%\Topo-Gen\cache\
    defaultCacheDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/cache";
#else
    // Linux: ~/.topo-gen/cache/
    defaultCacheDir = QDir::homePath() + "/.topo-gen/cache";
#endif

    settings.location = settings_.value("location", defaultCacheDir).toString();

    settings_.endGroup();

    return settings;
}

void StateManager::saveLoggingSettings(const LoggingSettings& settings) {
    settings_.beginGroup("Logging");
    settings_.setValue("level", settings.level);
    settings_.setValue("enable_log_file", settings.enableLogFile);
    settings_.setValue("log_file_path", settings.logFilePath);
    settings_.endGroup();
}

StateManager::LoggingSettings StateManager::restoreLoggingSettings() {
    settings_.beginGroup("Logging");

    LoggingSettings settings;
    settings.level = settings_.value("level", 3).toInt();
    settings.enableLogFile = settings_.value("enable_log_file", false).toBool();

    // Platform-specific default log file path
    QString defaultLogPath;
#ifdef Q_OS_MACOS
    // macOS: ~/Library/Application Support/Matthew Block/Topographic Generator/logs/topographic_generator.log
    defaultLogPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs/topographic_generator.log";
#elif defined(Q_OS_WIN)
    // Windows: %Documents%\Topo-Gen\logs\topographic_generator.log
    defaultLogPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/logs/topographic_generator.log";
#else
    // Linux: ~/.topo-gen/logs/topographic_generator.log
    defaultLogPath = QDir::homePath() + "/.topo-gen/logs/topographic_generator.log";
#endif

    settings.logFilePath = settings_.value("log_file_path", defaultLogPath).toString();

    settings_.endGroup();

    return settings;
}

void StateManager::saveOutputSettings(const OutputSettings& settings) {
    settings_.beginGroup("Output");
    settings_.setValue("default_output_dir", settings.defaultOutputDir);
    settings_.setValue("default_basename", settings.defaultBasename);
    settings_.endGroup();
}

StateManager::OutputSettings StateManager::restoreOutputSettings() {
    settings_.beginGroup("Output");

    OutputSettings settings;

    // Platform-specific default output directory
    QString defaultOutputDir;
#ifdef Q_OS_MACOS
    // macOS: ~/Documents/Topo-Gen/
    defaultOutputDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen";
#elif defined(Q_OS_WIN)
    // Windows: %Documents%\Topo-Gen\Output\
    defaultOutputDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/Output";
#else
    // Linux: ~/Topo-Gen/
    defaultOutputDir = QDir::homePath() + "/Topo-Gen";
#endif

    settings.defaultOutputDir = settings_.value("default_output_dir", defaultOutputDir).toString();
    settings.defaultBasename = settings_.value("default_basename", "").toString();

    settings_.endGroup();

    return settings;
}

void StateManager::saveUnitsSettings(const UnitsSettings& settings) {
    settings_.beginGroup("Units");
    settings_.setValue("system", static_cast<int>(settings.system));
    settings_.endGroup();
}

StateManager::UnitsSettings StateManager::restoreUnitsSettings() {
    settings_.beginGroup("Units");

    UnitsSettings settings;
    settings.system = static_cast<UnitSystem>(
        settings_.value("system", static_cast<int>(UnitSystem::Imperial)).toInt()
    );

    settings_.endGroup();

    return settings;
}

void StateManager::saveCliConfigSettings(const CliConfigSettings& settings) {
    settings_.beginGroup("CliConfig");
    settings_.setValue("config_directory", settings.configDirectory);
    settings_.endGroup();
}

StateManager::CliConfigSettings StateManager::restoreCliConfigSettings() {
    settings_.beginGroup("CliConfig");

    CliConfigSettings settings;

    // Platform-specific default directory
    QString defaultDir;
#ifdef Q_OS_MACOS
    // macOS: ~/Library/Application Support/Matthew Block/Topographic Generator/cli-configs/
    defaultDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cli-configs";
#elif defined(Q_OS_WIN)
    // Windows: %Documents%\Topo-Gen\cli-configs\
    defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Topo-Gen/cli-configs";
#else
    // Linux: ~/.topo-gen/cli-configs/
    defaultDir = QDir::homePath() + "/.topo-gen/cli-configs";
#endif

    settings.configDirectory = settings_.value("config_directory", defaultDir).toString();

    settings_.endGroup();

    return settings;
}

void StateManager::saveConfig(const QString& key, const QString& jsonConfig) {
    settings_.setValue(key, jsonConfig);
}

QString StateManager::restoreConfig(const QString& key, const QString& defaultValue) const {
    return settings_.value(key, defaultValue).toString();
}

void StateManager::clear() {
    settings_.clear();
}

} // namespace topo
