/**
 * @file ShapefileExporter.cpp
 * @brief Implementation of Shapefile export
 */

#include "ShapefileExporter.hpp"
#include "../core/Logger.hpp"
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_geometry.h>
#include <ogr_spatialref.h>
#include <fstream>
#include <cmath>

namespace topo {

ShapefileExporter::ShapefileExporter()
    : options_() {
    GDALAllRegister();
}

ShapefileExporter::ShapefileExporter(const Options& options)
    : options_(options) {
    GDALAllRegister();
}

bool ShapefileExporter::export_shapefile(const std::vector<ContourLayer>& layers,
                                        const std::string& filename) {
    Logger logger("ShapefileExporter");

    if (layers.empty()) {
        logger.error("No layers to export");
        return false;
    }

    // Get Shapefile driver
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (!driver) {
        logger.error("Shapefile driver not available");
        return false;
    }

    // Create datasource
    std::string shp_filename = filename;
    if (shp_filename.substr(shp_filename.length() - 4) != ".shp") {
        shp_filename += ".shp";
    }

    GDALDataset* dataset = driver->Create(shp_filename.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        logger.error("Failed to create Shapefile: " + shp_filename);
        return false;
    }

    // Create spatial reference
    OGRSpatialReference srs;
    std::string wkt = options_.projection_wkt.empty() ? get_wgs84_wkt() : options_.projection_wkt;
    srs.importFromWkt(wkt.c_str());

    // Determine geometry type
    OGRwkbGeometryType geom_type = (options_.geometry_type == Options::GeometryType::POLYGON25D) ?
                                   wkbPolygon25D : wkbPolygon;

    // Create layer
    OGRLayer* layer = dataset->CreateLayer("contours", &srs, geom_type, nullptr);
    if (!layer) {
        logger.error("Failed to create layer");
        GDALClose(dataset);
        return false;
    }

    // Create attribute fields
    create_attribute_fields(layer);

    // Add features
    for (size_t layer_idx = 0; layer_idx < layers.size(); ++layer_idx) {
        const auto& contour_layer = layers[layer_idx];

        for (const auto& poly : contour_layer.polygons) {
            // Create OGR geometry
            OGRGeometry* geom = polygon_to_ogr(poly);
            if (!geom) {
                logger.warning("Skipping invalid polygon");
                continue;
            }

            // Set Z value for 2.5D geometries
            if (options_.geometry_type == Options::GeometryType::POLYGON25D) {
                geom->setCoordinateDimension(3);
                // Note: Would need to set Z values for each coordinate
            }

            // Create feature
            OGRFeature* feature = OGRFeature::CreateFeature(layer->GetLayerDefn());
            feature->SetGeometry(geom);

            // Set attributes
            set_feature_attributes(feature, contour_layer.elevation,
                                  static_cast<int>(layer_idx + 1), poly);

            // Add feature to layer
            if (layer->CreateFeature(feature) != OGRERR_NONE) {
                logger.warning("Failed to create feature");
            }

            // Clean up
            OGRFeature::DestroyFeature(feature);
            OGRGeometryFactory::destroyGeometry(geom);
        }
    }

    // Close dataset
    GDALClose(dataset);

    // Create .prj file if requested
    if (options_.create_prj_file) {
        std::string base = shp_filename.substr(0, shp_filename.length() - 4);
        write_prj_file(base, wkt);
    }

    logger.info("Exported Shapefile: " + shp_filename);
    return true;
}

bool ShapefileExporter::export_layer(const ContourLayer& layer,
                                     const std::string& filename,
                                     int layer_number) {
    Logger logger("ShapefileExporter");

    // Get Shapefile driver
    GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("ESRI Shapefile");
    if (!driver) {
        logger.error("Shapefile driver not available");
        return false;
    }

    // Create datasource
    std::string shp_filename = filename;
    if (shp_filename.substr(shp_filename.length() - 4) != ".shp") {
        shp_filename += ".shp";
    }

    GDALDataset* dataset = driver->Create(shp_filename.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if (!dataset) {
        logger.error("Failed to create Shapefile: " + shp_filename);
        return false;
    }

    // Create spatial reference
    OGRSpatialReference srs;
    std::string wkt = options_.projection_wkt.empty() ? get_wgs84_wkt() : options_.projection_wkt;
    srs.importFromWkt(wkt.c_str());

    // Determine geometry type
    OGRwkbGeometryType geom_type = (options_.geometry_type == Options::GeometryType::POLYGON25D) ?
                                   wkbPolygon25D : wkbPolygon;

    // Create layer
    OGRLayer* ogr_layer = dataset->CreateLayer("contours", &srs, geom_type, nullptr);
    if (!ogr_layer) {
        logger.error("Failed to create layer");
        GDALClose(dataset);
        return false;
    }

    // Create attribute fields
    create_attribute_fields(ogr_layer);

    // Add features from the contour layer using the provided layer_number
    for (const auto& poly : layer.polygons) {
        // Create OGR geometry
        OGRGeometry* geom = polygon_to_ogr(poly);
        if (!geom) {
            logger.warning("Skipping invalid polygon");
            continue;
        }

        // Set Z value for 2.5D geometries
        if (options_.geometry_type == Options::GeometryType::POLYGON25D) {
            geom->setCoordinateDimension(3);
        }

        // Create feature
        OGRFeature* feature = OGRFeature::CreateFeature(ogr_layer->GetLayerDefn());
        feature->SetGeometry(geom);

        // Set attributes with the provided layer_number
        set_feature_attributes(feature, layer.elevation, layer_number, poly);

        // Add feature to layer
        if (ogr_layer->CreateFeature(feature) != OGRERR_NONE) {
            logger.warning("Failed to create feature");
        }

        // Clean up
        OGRFeature::DestroyFeature(feature);
        OGRGeometryFactory::destroyGeometry(geom);
    }

    // Close dataset
    GDALClose(dataset);

    // Create .prj file if requested
    if (options_.create_prj_file) {
        std::string base = shp_filename.substr(0, shp_filename.length() - 4);
        write_prj_file(base, wkt);
    }

    logger.info("Exported Shapefile: " + shp_filename);
    return true;
}

OGRGeometry* ShapefileExporter::polygon_to_ogr(const ContourLayer::PolygonData& poly) const {
    if (poly.empty()) {
        return nullptr;
    }

    // Create OGR polygon
    OGRPolygon* ogr_poly = new OGRPolygon();

    // Add exterior ring (first ring)
    OGRLinearRing* outer_ring = new OGRLinearRing();
    const auto& exterior = poly.exterior();
    for (const auto& [x, y] : exterior) {
        outer_ring->addPoint(x, y);
    }
    outer_ring->closeRings();
    ogr_poly->addRingDirectly(outer_ring);

    // Add holes (subsequent rings)
    for (const auto& hole : poly.holes()) {
        OGRLinearRing* hole_ring = new OGRLinearRing();
        for (const auto& [x, y] : hole) {
            hole_ring->addPoint(x, y);
        }
        hole_ring->closeRings();
        ogr_poly->addRingDirectly(hole_ring);
    }

    return ogr_poly;
}

void ShapefileExporter::create_attribute_fields(OGRLayer* layer) {
    if (options_.add_elevation_field) {
        OGRFieldDefn elevation_field(options_.elevation_field_name.c_str(), OFTReal);
        elevation_field.SetWidth(12);
        elevation_field.SetPrecision(2);
        layer->CreateField(&elevation_field);
    }

    if (options_.add_layer_field) {
        OGRFieldDefn layer_field(options_.layer_field_name.c_str(), OFTInteger);
        layer_field.SetWidth(6);
        layer->CreateField(&layer_field);
    }

    if (options_.add_area_field) {
        OGRFieldDefn area_field(options_.area_field_name.c_str(), OFTReal);
        area_field.SetWidth(16);
        area_field.SetPrecision(4);
        layer->CreateField(&area_field);
    }

    if (options_.add_perimeter_field) {
        OGRFieldDefn perimeter_field(options_.perimeter_field_name.c_str(), OFTReal);
        perimeter_field.SetWidth(16);
        perimeter_field.SetPrecision(4);
        layer->CreateField(&perimeter_field);
    }
}

void ShapefileExporter::set_feature_attributes(OGRFeature* feature,
                                               double elevation,
                                               int layer_number,
                                               const ContourLayer::PolygonData& poly) {
    if (options_.add_elevation_field) {
        feature->SetField(options_.elevation_field_name.c_str(), elevation);
    }

    if (options_.add_layer_field) {
        feature->SetField(options_.layer_field_name.c_str(), layer_number);
    }

    if (options_.add_area_field) {
        // Calculate area using shoelace formula
        double area = 0.0;
        const auto& exterior = poly.exterior();
        for (size_t i = 0; i < exterior.size(); ++i) {
            size_t j = (i + 1) % exterior.size();
            area += exterior[i].first * exterior[j].second;
            area -= exterior[j].first * exterior[i].second;
        }
        area = std::abs(area) / 2.0;
        feature->SetField(options_.area_field_name.c_str(), area);
    }

    if (options_.add_perimeter_field) {
        // Calculate perimeter
        double perimeter = 0.0;
        const auto& exterior = poly.exterior();
        for (size_t i = 0; i < exterior.size(); ++i) {
            size_t j = (i + 1) % exterior.size();
            double dx = exterior[j].first - exterior[i].first;
            double dy = exterior[j].second - exterior[i].second;
            perimeter += std::sqrt(dx * dx + dy * dy);
        }
        feature->SetField(options_.perimeter_field_name.c_str(), perimeter);
    }
}

std::string ShapefileExporter::get_wgs84_wkt() const {
    return "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]],"
           "PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]";
}

bool ShapefileExporter::write_prj_file(const std::string& base_filename, const std::string& wkt) {
    std::string prj_filename = base_filename + ".prj";
    std::ofstream prj_file(prj_filename);

    if (!prj_file.is_open()) {
        Logger logger("ShapefileExporter");
        logger.warning("Failed to create .prj file: " + prj_filename);
        return false;
    }

    prj_file << wkt;
    prj_file.close();

    return true;
}

} // namespace topo
