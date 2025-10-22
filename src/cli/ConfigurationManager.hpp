/**
 * @file ConfigurationManager.hpp
 * @brief Configuration file management for the topographic generator
 */

#pragma once

#include "topographic_generator.hpp"
#include <string>
#include <map>
#include <optional>

namespace topo {

/**
 * @brief Configuration file manager for loading and saving settings
 */
class ConfigurationManager {
public:
    ConfigurationManager() = default;
    
    /**
     * @brief Load configuration from file
     * @param filename Path to configuration file
     * @return true if successful, false otherwise
     */
    bool load_from_file(const std::string& filename);
    
    /**
     * @brief Save configuration to file
     * @param filename Path to configuration file
     * @return true if successful, false otherwise
     */
    bool save_to_file(const std::string& filename) const;
    
    /**
     * @brief Convert to TopographicConfig object
     * @return TopographicConfig with values from this manager
     */
    TopographicConfig to_topographic_config() const;
    
    /**
     * @brief Load from TopographicConfig object
     * @param config TopographicConfig to load values from
     */
    void from_topographic_config(const TopographicConfig& config);
    
    // Value setters and getters
    void set_value(const std::string& key, const std::string& value) {
        config_values_[key] = value;
    }
    
    bool has_value(const std::string& key) const {
        return config_values_.find(key) != config_values_.end();
    }
    
    std::string get_string(const std::string& key, const std::string& default_value = "") const {
        auto it = config_values_.find(key);
        return (it != config_values_.end()) ? it->second : default_value;
    }
    
    int get_int(const std::string& key, int default_value = 0) const {
        auto it = config_values_.find(key);
        if (it != config_values_.end()) {
            try {
                return std::stoi(it->second);
            } catch (const std::exception&) {
                // Fall through to default
            }
        }
        return default_value;
    }
    
    double get_double(const std::string& key, double default_value = 0.0) const {
        auto it = config_values_.find(key);
        if (it != config_values_.end()) {
            try {
                return std::stod(it->second);
            } catch (const std::exception&) {
                // Fall through to default
            }
        }
        return default_value;
    }
    
    bool get_bool(const std::string& key, bool default_value = false) const {
        auto it = config_values_.find(key);
        if (it != config_values_.end()) {
            const std::string& value = it->second;
            return value == "true" || value == "1" || value == "yes";
        }
        return default_value;
    }

private:
    std::map<std::string, std::string> config_values_;
};

} // namespace topo