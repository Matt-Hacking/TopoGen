/**
 * @file ConfigurationManager.cpp  
 * @brief Configuration management for the topographic generator
 */

#include "ConfigurationManager.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

namespace topo {

bool ConfigurationManager::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    // Simple key=value parser
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        config_values_[key] = value;
    }
    
    return true;
}

bool ConfigurationManager::save_to_file(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# Topographic Generator Configuration" << std::endl;
    file << "# Generated automatically" << std::endl;
    file << std::endl;
    
    for (const auto& [key, value] : config_values_) {
        file << key << "=" << value << std::endl;
    }
    
    return true;
}

TopographicConfig ConfigurationManager::to_topographic_config() const {
    TopographicConfig config;
    
    // Parse bounds
    if (has_value("bounds")) {
        auto bounds_str = get_string("bounds");
        // Parse as lat1,lon1,lat2,lon2
        std::istringstream iss(bounds_str);
        std::string token;
        std::vector<double> coords;
        
        while (std::getline(iss, token, ',')) {
            try {
                coords.push_back(std::stod(token));
            } catch (const std::exception&) {
                // Skip invalid values
            }
        }
        
        if (coords.size() == 4) {
            config.bounds = BoundingBox(coords[1], coords[0], coords[3], coords[2]);
        }
    }
    
    // Parse other configuration values
    config.output_directory = get_string("output_directory", ".");
    config.base_name = get_string("base_name", "topographic_model");
    config.num_layers = get_int("num_layers", 10);
    config.layer_thickness_mm = get_double("layer_thickness_mm", 2.0);
    config.substrate_size_mm = get_double("substrate_size_mm", 200.0);
    config.terrain_following = get_bool("terrain_following", false);
    config.parallel_processing = get_bool("parallel_processing", true);
    
    // Parse quality
    auto quality_str = get_string("quality", "medium");
    if (quality_str == "draft") config.quality = TopographicConfig::MeshQuality::DRAFT;
    else if (quality_str == "high") config.quality = TopographicConfig::MeshQuality::HIGH;
    else if (quality_str == "ultra") config.quality = TopographicConfig::MeshQuality::ULTRA;
    else config.quality = TopographicConfig::MeshQuality::MEDIUM;
    
    // Parse output formats
    if (has_value("output_formats")) {
        auto formats_str = get_string("output_formats");
        std::istringstream iss(formats_str);
        std::string format;
        config.output_formats.clear();
        
        while (std::getline(iss, format, ',')) {
            format.erase(0, format.find_first_not_of(" \t"));
            format.erase(format.find_last_not_of(" \t") + 1);
            if (!format.empty()) {
                config.output_formats.push_back(format);
            }
        }
    } else {
        config.output_formats = {"stl"};
    }
    
    return config;
}

void ConfigurationManager::from_topographic_config(const TopographicConfig& config) {
    // Store bounds as lat1,lon1,lat2,lon2
    std::ostringstream bounds_ss;
    bounds_ss << config.bounds.min_y << "," << config.bounds.min_x << ","
              << config.bounds.max_y << "," << config.bounds.max_x;
    set_value("bounds", bounds_ss.str());
    
    set_value("output_directory", config.output_directory);
    set_value("base_name", config.base_name);
    set_value("num_layers", std::to_string(config.num_layers));
    set_value("layer_thickness_mm", std::to_string(config.layer_thickness_mm));
    set_value("substrate_size_mm", std::to_string(config.substrate_size_mm));
    set_value("terrain_following", config.terrain_following ? "true" : "false");
    set_value("parallel_processing", config.parallel_processing ? "true" : "false");
    
    // Store quality
    std::string quality_str;
    switch (config.quality) {
        case TopographicConfig::MeshQuality::DRAFT: quality_str = "draft"; break;
        case TopographicConfig::MeshQuality::HIGH: quality_str = "high"; break;
        case TopographicConfig::MeshQuality::ULTRA: quality_str = "ultra"; break;
        default: quality_str = "medium"; break;
    }
    set_value("quality", quality_str);
    
    // Store output formats
    std::ostringstream formats_ss;
    for (size_t i = 0; i < config.output_formats.size(); ++i) {
        if (i > 0) formats_ss << ",";
        formats_ss << config.output_formats[i];
    }
    set_value("output_formats", formats_ss.str());
}

} // namespace topo