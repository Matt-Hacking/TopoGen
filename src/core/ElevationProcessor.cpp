/**
 * @file ElevationProcessor.cpp
 * @brief Complete elevation data processing with SRTM tiles and GDAL
 *
 * Full port of Python elevation processing functionality
 * Handles SRTM tile downloading, mosaicking, and contour generation
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include "ElevationProcessor.hpp"
#include "SRTMDownloader.hpp"
#include "ExecutionPolicies.hpp"
#include "Logger.hpp"
#include <gdal_priv.h>
#include <memory>
#include <gdal_utils.h>
#include <ogr_spatialref.h>
#include <gdalwarper.h>
#include <algorithm>
#include <filesystem>
#include <mutex>
#include <cmath>
#include <numeric>
#include <fstream>
#include <iostream>

// Gzip decompression
#include <zlib.h>

namespace topo {

// RAII wrapper for GDAL dataset
struct GDALDatasetDeleter {
    void operator()(GDALDataset* dataset) {
        if (dataset) {
            GDALClose(dataset);
        }
    }
};

using GDALDatasetPtr = std::unique_ptr<GDALDataset, GDALDatasetDeleter>;

// Compile-time constants for performance (reserved for future use)
[[maybe_unused]] constexpr double MIN_ELEVATION_BOUND = -500.0;   // Below sea level
[[maybe_unused]] constexpr double MAX_ELEVATION_BOUND = 9000.0;   // Above Everest
[[maybe_unused]] constexpr double INTERPOLATION_EPSILON = 1e-12;
[[maybe_unused]] constexpr double GEOMETRY_EPSILON = 1e-12;
[[maybe_unused]] constexpr size_t MAX_NEIGHBORS = 8;

// ============================================================================
// ElevationProcessor::Impl - Complete implementation
// ============================================================================

class ElevationProcessor::Impl {
public:
    Impl() : logger_("ElevationProcessor") {
        GDALAllRegister();
        srtm_downloader_ = std::make_unique<SRTMDownloader>();
    }

    ~Impl() = default;  // Smart pointer handles cleanup automatically
    
    bool load_elevation_data(const BoundingBox& bbox, const std::string& cache_dir = "cache") {
        bbox_ = bbox;

        std::ostringstream msg;
        msg << "Loading elevation data for bounds: ("
            << bbox.min_x << "," << bbox.min_y << ") to ("
            << bbox.max_x << "," << bbox.max_y << ")";
        logger_.info(msg.str());
        
        // Create cache directories
        std::filesystem::create_directories(cache_dir + "/tiles");
        
        // Download SRTM tiles
        auto tile_files = srtm_downloader_->download_tiles(bbox);
        if (tile_files.empty()) {
            logger_.error("No SRTM tiles downloaded for the specified area");
            return false;
        }

        logger_.info("Downloaded " + std::to_string(tile_files.size()) + " SRTM tiles");
        
        // Decompress .hgt.gz files to .hgt files
        std::vector<std::string> hgt_files;
        for (const auto& gz_file : tile_files) {
            std::string hgt_file = gz_file.substr(0, gz_file.length() - 3); // Remove .gz
            if (decompress_gzip_file(gz_file, hgt_file)) {
                hgt_files.push_back(hgt_file);
            } else {
                logger_.error("Failed to decompress: " + gz_file);
            }
        }
        
        if (hgt_files.empty()) {
            logger_.error("No elevation files could be decompressed");
            return false;
        }
        
        // Mosaic tiles into a single VRT (Virtual Dataset)
        std::string mosaic_vrt = cache_dir + "/mosaic.vrt";
        if (!create_vrt_mosaic(hgt_files, mosaic_vrt)) {
            logger_.error("Failed to create mosaic VRT");
            return false;
        }
        
        // Open the mosaicked dataset
        dataset_.reset(static_cast<GDALDataset*>(GDALOpen(mosaic_vrt.c_str(), GA_ReadOnly)));
        if (!dataset_) {
            logger_.error("Failed to open mosaic VRT: " + mosaic_vrt);
            return false;
        }

        logger_.info("Successfully loaded elevation data");
        std::ostringstream size_msg;
        size_msg << "  Dataset size: " << dataset_->GetRasterXSize()
                 << " x " << dataset_->GetRasterYSize();
        logger_.info(size_msg.str());
        
        // Extract elevation data for the specific bounding box
        bool success = extract_elevation_data(bbox);
        elevation_loaded_ = success;
        return success;
    }
    
    std::pair<double, double> get_elevation_range() const {
        if (elevation_data_.empty()) {
            return {0.0, 0.0};
        }
        
        auto [min_it, max_it] = std::minmax_element(elevation_data_.begin(), elevation_data_.end(),
            [this](float a, float b) {
                // Skip nodata values
                if (a == nodata_value_ && b != nodata_value_) return false;
                if (a != nodata_value_ && b == nodata_value_) return true;
                if (a == nodata_value_ && b == nodata_value_) return false;
                return a < b;
            });
        
        double min_elev = (*min_it == nodata_value_) ? 0.0 : static_cast<double>(*min_it);
        double max_elev = (*max_it == nodata_value_) ? 0.0 : static_cast<double>(*max_it);
        
        return {min_elev, max_elev};
    }
    
    double interpolate_elevation(double x, double y) const {
        if (!dataset_ || elevation_data_.empty()) {
            return 0.0;
        }
        
        // Convert geographic coordinates to pixel coordinates
        double geo_x = x;
        double geo_y = y;
        
        // Apply geotransform to get pixel coordinates
        double pixel_x = (geo_x - geotransform_[0]) / geotransform_[1];
        double pixel_y = (geo_y - geotransform_[3]) / geotransform_[5];
        
        // Check bounds
        if (pixel_x < 0 || pixel_x >= width_ - 1 || pixel_y < 0 || pixel_y >= height_ - 1) {
            return nodata_value_;
        }
        
        // Bilinear interpolation
        int x0 = static_cast<int>(std::floor(pixel_x));
        int y0 = static_cast<int>(std::floor(pixel_y));
        int x1 = x0 + 1;
        int y1 = y0 + 1;
        
        double dx = pixel_x - x0;
        double dy = pixel_y - y0;
        
        // Get the four corner values
        double v00 = get_elevation_at_pixel(x0, y0);
        double v10 = get_elevation_at_pixel(x1, y0);
        double v01 = get_elevation_at_pixel(x0, y1);
        double v11 = get_elevation_at_pixel(x1, y1);
        
        // Check for nodata values
        if (v00 == nodata_value_ || v10 == nodata_value_ || 
            v01 == nodata_value_ || v11 == nodata_value_) {
            // Use nearest neighbor if any corner is nodata
            return get_elevation_at_pixel(static_cast<int>(std::round(pixel_x)), 
                                        static_cast<int>(std::round(pixel_y)));
        }
        
        // Bilinear interpolation
        double v0 = v00 * (1 - dx) + v10 * dx;
        double v1 = v01 * (1 - dx) + v11 * dx;
        
        return v0 * (1 - dy) + v1 * dy;
    }
    
    std::vector<Point3D> sample_elevation_points(const Grid& sampling_grid) const {
        std::vector<Point3D> points;
        
        double dx = (sampling_grid.max_x - sampling_grid.min_x) / sampling_grid.width;
        double dy = (sampling_grid.max_y - sampling_grid.min_y) / sampling_grid.height;
        
        points.reserve(sampling_grid.width * sampling_grid.height);
        
        for (size_t j = 0; j < sampling_grid.height; ++j) {
            for (size_t i = 0; i < sampling_grid.width; ++i) {
                double x = sampling_grid.min_x + i * dx;
                double y = sampling_grid.min_y + j * dy;
                double z = interpolate_elevation(x, y);
                
                if (z != nodata_value_) {
                    points.emplace_back(x, y, z);
                }
            }
        }
        
        return points;
    }
    
    template<typename ExecutionPolicy>
    std::vector<Point3D> sample_elevation_points_parallel(
        [[maybe_unused]] ExecutionPolicy&& policy, const Grid& sampling_grid) const {

        // For now, use sequential version
        // TODO: Implement parallel version with TBB/OpenMP
        return sample_elevation_points(sampling_grid);
    }

    bool decompress_gzip_file(const std::string& gz_filename, const std::string& output_filename) {
        // Skip if output already exists
        if (std::filesystem::exists(output_filename)) {
            return true;
        }
        
        gzFile gz_file = gzopen(gz_filename.c_str(), "rb");
        if (!gz_file) {
            logger_.error("Failed to open gzip file: " + gz_filename);
            return false;
        }
        
        std::ofstream output_file(output_filename, std::ios::binary);
        if (!output_file.is_open()) {
            logger_.error("Failed to create output file: " + output_filename);
            gzclose(gz_file);
            return false;
        }
        
        char buffer[8192];
        int bytes_read;
        
        while ((bytes_read = gzread(gz_file, buffer, sizeof(buffer))) > 0) {
            output_file.write(buffer, bytes_read);
        }

        gzclose(gz_file);
        output_file.close();

        if (bytes_read < 0) {
            logger_.error("Error decompressing file: " + gz_filename);
            std::filesystem::remove(output_filename);
            return false;
        }
        
        return true;
    }
    
    bool create_vrt_mosaic(const std::vector<std::string>& hgt_files, const std::string& vrt_filename) {
        // Remove existing VRT file if it exists (equivalent to -overwrite)
        if (std::filesystem::exists(vrt_filename)) {
            std::filesystem::remove(vrt_filename);
        }

        // Create VRT options
        char** vrt_options = nullptr;
        
        // Create input filenames array
        char** input_filenames = nullptr;
        for (const auto& file : hgt_files) {
            input_filenames = CSLAddString(input_filenames, file.c_str());
        }
        
        // Create VRT dataset
        GDALDatasetPtr vrt_dataset;
        GDALBuildVRTOptions* build_options = GDALBuildVRTOptionsNew(vrt_options, nullptr);

        vrt_dataset.reset(static_cast<GDALDataset*>(
            GDALBuildVRT(vrt_filename.c_str(), hgt_files.size(), nullptr,
                         const_cast<const char**>(input_filenames), build_options, nullptr)));

        GDALBuildVRTOptionsFree(build_options);
        CSLDestroy(vrt_options);
        CSLDestroy(input_filenames);

        if (!vrt_dataset) {
            logger_.error("Failed to create VRT mosaic");
            return false;
        }

        return true;
    }
    
    bool extract_elevation_data(const BoundingBox& bbox) {
        if (!dataset_) {
            return false;
        }
        
        // Get dataset geotransform
        geotransform_.resize(6);
        if (dataset_->GetGeoTransform(geotransform_.data()) != CE_None) {
            logger_.error("Failed to get geotransform");
            return false;
        }
        
        // Calculate pixel coordinates for bounding box
        double pixel_min_x = (bbox.min_x - geotransform_[0]) / geotransform_[1];
        double pixel_max_x = (bbox.max_x - geotransform_[0]) / geotransform_[1];
        double pixel_min_y = (bbox.max_y - geotransform_[3]) / geotransform_[5]; // Note: Y is flipped
        double pixel_max_y = (bbox.min_y - geotransform_[3]) / geotransform_[5];
        
        // Ensure proper bounds
        int x_off = std::max(0, static_cast<int>(std::floor(pixel_min_x)));
        int y_off = std::max(0, static_cast<int>(std::floor(pixel_min_y)));
        int x_size = std::min(dataset_->GetRasterXSize() - x_off, 
                             static_cast<int>(std::ceil(pixel_max_x)) - x_off);
        int y_size = std::min(dataset_->GetRasterYSize() - y_off,
                             static_cast<int>(std::ceil(pixel_max_y)) - y_off);
        
        if (x_size <= 0 || y_size <= 0) {
            logger_.error("Invalid extraction window");
            return false;
        }
        
        width_ = x_size;
        height_ = y_size;
        elevation_data_.resize(width_ * height_);
        
        // Read elevation data
        GDALRasterBand* band = dataset_->GetRasterBand(1);
        if (!band) {
            logger_.error("Failed to get raster band");
            return false;
        }
        
        // Get nodata value
        int has_nodata;
        double nodata = band->GetNoDataValue(&has_nodata);
        if (has_nodata) {
            nodata_value_ = nodata;
        }
        
        CPLErr err = band->RasterIO(GF_Read, x_off, y_off, x_size, y_size,
                                   elevation_data_.data(), x_size, y_size, GDT_Float32, 0, 0);
        
        if (err != CE_None) {
            logger_.error("Failed to read elevation data");
            return false;
        }
        
        // Update geotransform for the extracted window
        geotransform_[0] += x_off * geotransform_[1];
        geotransform_[3] += y_off * geotransform_[5];

        std::ostringstream extract_msg;
        extract_msg << "Extracted elevation data: " << width_ << "x" << height_ << " pixels";
        logger_.info(extract_msg.str());

        // Validate elevation data bounds and clean invalid values
        if (!validate_and_clean_elevation_data()) {
            logger_.error("Elevation data validation failed");
            return false;
        }

        auto [min_elev, max_elev] = get_elevation_range();
        std::ostringstream range_msg;
        range_msg << "Elevation range: " << min_elev << "m to " << max_elev << "m";
        logger_.info(range_msg.str());

        return true;
    }
    
    double get_elevation_at_pixel(int x, int y) const {
        if (x < 0 || x >= static_cast<int>(width_) || y < 0 || y >= static_cast<int>(height_)) {
            return nodata_value_;
        }

        return elevation_data_[y * width_ + x];
    }

    const float* get_elevation_data() const {
        return elevation_data_.empty() ? nullptr : elevation_data_.data();
    }

    std::pair<size_t, size_t> get_elevation_dimensions() const {
        return {width_, height_};
    }

    std::vector<double> get_geotransform() const {
        return geotransform_;
    }

    bool validate_and_clean_elevation_data() {
        if (elevation_data_.empty()) {
            logger_.error("No elevation data to validate");
            return false;
        }

        size_t total_pixels = elevation_data_.size();
        size_t invalid_pixels = 0;
        size_t repaired_pixels = 0;
        size_t extreme_values = 0;

        // Define reasonable elevation bounds (in meters)
        const float MIN_ELEVATION = -500.0f;   // Below sea level (Dead Sea, etc.)
        const float MAX_ELEVATION = 9000.0f;   // Above Everest

        logger_.info("Validating elevation data bounds...");

        // First pass: identify and count invalid values
        for (size_t i = 0; i < total_pixels; ++i) {
            float& elev = elevation_data_[i];

            // Check for infinite or NaN values
            if (!std::isfinite(elev)) {
                elev = nodata_value_;
                invalid_pixels++;
            }
            // Check for extreme values that might cause UTM projection issues
            else if (elev < MIN_ELEVATION || elev > MAX_ELEVATION) {
                extreme_values++;
                // Don't mark as nodata yet, just count
            }
        }

        // Second pass: repair extreme values using spatial interpolation
        for (size_t y = 0; y < height_; ++y) {
            for (size_t x = 0; x < width_; ++x) {
                size_t idx = y * width_ + x;
                float& elev = elevation_data_[idx];

                if (elev != nodata_value_ && (elev < MIN_ELEVATION || elev > MAX_ELEVATION)) {
                    // Try to repair using neighboring values
                    float repaired_value = interpolate_from_neighbors(x, y);
                    if (repaired_value != nodata_value_ &&
                        repaired_value >= MIN_ELEVATION && repaired_value <= MAX_ELEVATION) {
                        elev = repaired_value;
                        repaired_pixels++;
                    } else {
                        elev = nodata_value_;
                        invalid_pixels++;
                    }
                }
            }
        }

        // Report validation statistics
        double invalid_percentage = 100.0 * invalid_pixels / total_pixels;
        double extreme_percentage = 100.0 * extreme_values / total_pixels;

        logger_.info("Elevation data validation complete:");
        logger_.info("  Total pixels: " + std::to_string(total_pixels));
        std::ostringstream invalid_msg, extreme_msg, repaired_msg;
        invalid_msg << "  Invalid/infinite pixels: " << invalid_pixels << " (" << invalid_percentage << "%)";
        extreme_msg << "  Extreme value pixels: " << extreme_values << " (" << extreme_percentage << "%)";
        repaired_msg << "  Repaired pixels: " << repaired_pixels;
        logger_.info(invalid_msg.str());
        logger_.info(extreme_msg.str());
        logger_.info(repaired_msg.str());

        // Validation fails if too much data is invalid
        if (invalid_percentage > 50.0) {
            std::ostringstream err_msg;
            err_msg << "Too much invalid elevation data (" << invalid_percentage << "% invalid)";
            logger_.error(err_msg.str());
            return false;
        }

        return true;
    }

    float interpolate_from_neighbors(size_t x, size_t y) {
        std::vector<float> valid_neighbors;

        // Check 8-connected neighbors
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue; // Skip center pixel

                int nx = static_cast<int>(x) + dx;
                int ny = static_cast<int>(y) + dy;

                if (nx >= 0 && nx < static_cast<int>(width_) &&
                    ny >= 0 && ny < static_cast<int>(height_)) {
                    float neighbor_val = elevation_data_[ny * width_ + nx];
                    if (neighbor_val != nodata_value_ && std::isfinite(neighbor_val)) {
                        valid_neighbors.push_back(neighbor_val);
                    }
                }
            }
        }

        if (valid_neighbors.empty()) {
            return nodata_value_;
        }

        // Return median of valid neighbors for robust interpolation
        std::sort(valid_neighbors.begin(), valid_neighbors.end());
        size_t mid = valid_neighbors.size() / 2;
        if (valid_neighbors.size() % 2 == 0) {
            return (valid_neighbors[mid - 1] + valid_neighbors[mid]) / 2.0f;
        } else {
            return valid_neighbors[mid];
        }
    }

private:
    Logger logger_;
    std::unique_ptr<SRTMDownloader> srtm_downloader_;
    GDALDatasetPtr dataset_;
    std::vector<float> elevation_data_;
    std::vector<double> geotransform_;
    size_t width_ = 0;
    size_t height_ = 0;
    double nodata_value_ = -32768.0;
    bool elevation_loaded_ = false;
    BoundingBox bbox_;
};

// ============================================================================
// ElevationProcessor Public Interface
// ============================================================================

ElevationProcessor::ElevationProcessor() : impl_(std::make_unique<Impl>()) {
}

ElevationProcessor::~ElevationProcessor() = default;

bool ElevationProcessor::load_elevation_tiles(const BoundingBox& bbox) {
    return impl_->load_elevation_data(bbox);
}

std::vector<Point3D> ElevationProcessor::sample_elevation_points(const Grid& sampling_grid) const {
    return impl_->sample_elevation_points(sampling_grid);
}

template<typename ExecutionPolicy>
std::vector<Point3D> ElevationProcessor::sample_elevation_points_parallel(
    ExecutionPolicy&& policy, const Grid& sampling_grid) const {
    return impl_->sample_elevation_points_parallel(std::forward<ExecutionPolicy>(policy), sampling_grid);
}

// Explicit template instantiation
template std::vector<Point3D> ElevationProcessor::sample_elevation_points_parallel<ParallelPolicy>(
    ParallelPolicy&& policy, const Grid& sampling_grid) const;

std::pair<double, double> ElevationProcessor::get_elevation_range() const {
    return impl_->get_elevation_range();
}

double ElevationProcessor::interpolate_elevation(double x, double y) const {
    return impl_->interpolate_elevation(x, y);
}

std::vector<double> ElevationProcessor::generate_contour_levels(
    int num_layers, ContourStrategy strategy) const {

    Logger logger("ElevationProcessor");
    auto [min_elev, max_elev] = get_elevation_range();

    if (min_elev == max_elev) {
        return {min_elev};
    }

    // IMPORTANT: Only generate levels within the actual data range
    // Levels outside this range will produce no contours and cause index mismatches
    std::ostringstream layers_msg;
    layers_msg << "Generating " << num_layers << " layers from actual data range: "
               << min_elev << "m to " << max_elev << "m";
    logger.info(layers_msg.str());

    std::vector<double> levels;

    switch (strategy) {
        case ContourStrategy::UNIFORM: {
            levels.resize(num_layers + 1);
            double step = (max_elev - min_elev) / num_layers;
            for (int i = 0; i <= num_layers; ++i) {
                levels[i] = min_elev + i * step;
            }
            break;
        }

        case ContourStrategy::LOGARITHMIC: {
            levels.resize(num_layers + 1);
            double log_min = std::log(std::max(min_elev, 1.0));
            double log_max = std::log(std::max(max_elev, min_elev + 1.0));
            double step = (log_max - log_min) / num_layers;

            for (int i = 0; i <= num_layers; ++i) {
                levels[i] = std::exp(log_min + i * step);
            }
            break;
        }

        case ContourStrategy::EXPONENTIAL: {
            levels.resize(num_layers + 1);
            double range = max_elev - min_elev;

            for (int i = 0; i <= num_layers; ++i) {
                double t = static_cast<double>(i) / num_layers;
                double exp_t = (std::exp(t * 2) - 1) / (std::exp(2) - 1);
                levels[i] = min_elev + exp_t * range;
            }
            break;
        }
    }

    return levels;
}

const float* ElevationProcessor::get_elevation_data() const {
    return impl_->get_elevation_data();
}

std::pair<size_t, size_t> ElevationProcessor::get_elevation_dimensions() const {
    return impl_->get_elevation_dimensions();
}

std::vector<double> ElevationProcessor::get_geotransform() const {
    return impl_->get_geotransform();
}

} // namespace topo