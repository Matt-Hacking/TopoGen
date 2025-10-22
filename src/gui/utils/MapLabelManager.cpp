/**
 * @file MapLabelManager.cpp
 * @brief Implementation of map label manager
 */

#include "MapLabelManager.hpp"
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace topo {

MapLabelManager::MapLabelManager(QGraphicsScene* scene, QObject* parent)
    : QObject(parent),
      scene_(scene),
      networkManager_(new QNetworkAccessManager(this)),
      currentZoom_(0),
      labelsEnabled_(true) {

    // Initialize all label types as visible by default
    labelTypeVisibility_[LabelType::Peak] = true;
    labelTypeVisibility_[LabelType::City] = true;
    labelTypeVisibility_[LabelType::Village] = true;
    labelTypeVisibility_[LabelType::Water] = true;
    labelTypeVisibility_[LabelType::PointOfInterest] = true;

    connect(networkManager_, &QNetworkAccessManager::finished,
            this, &MapLabelManager::onNetworkReply);
}

MapLabelManager::~MapLabelManager() {
    clearLabels();
}

void MapLabelManager::queryFeatures(double min_lat, double min_lon, double max_lat, double max_lon) {
    // Construct Overpass QL query for peaks and cities
    // Query format: [out:json];(node[natural=peak](bbox);node[place~"city|town"](bbox););out;
    QString query = QString(
        "[out:json][timeout:25];"
        "("
        "  node[\"natural\"=\"peak\"](%1,%2,%3,%4);"
        "  node[\"place\"~\"city|town|village\"](%1,%2,%3,%4);"
        ");"
        "out body;"
    ).arg(min_lat).arg(min_lon).arg(max_lat).arg(max_lon);

    qDebug() << "[MapLabelManager] Querying OSM features in bounds:" << min_lat << min_lon << max_lat << max_lon;

    // Send request to Overpass API
    QUrl url("https://overpass-api.de/api/interpreter");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray postData = "data=" + QUrl::toPercentEncoding(query);
    networkManager_->post(request, postData);
}

void MapLabelManager::updateLabelVisibility(int zoom, double altitudeMeters) {
    currentZoom_ = zoom;

    // Calculate altitude if not provided
    if (altitudeMeters < 0) {
        altitudeMeters = calculateAltitudeFromZoom(zoom);
    }

    if (!labelsEnabled_) {
        for (auto& label : labels_) {
            if (label.item) {
                label.item->setVisible(false);
            }
        }
        return;
    }

    // Update visibility and sizing based on zoom level and altitude
    for (auto& label : labels_) {
        if (!label.item) continue;

        // Check if this label type is enabled
        bool typeVisible = labelTypeVisibility_[label.type];

        bool visible = typeVisible &&
                      (zoom >= label.minZoom) &&
                      (label.maxZoom == -1 || zoom <= label.maxZoom);

        // Calculate intelligent label size based on altitude and type
        double labelSize = calculateLabelSize(label.type, altitudeMeters);

        // Hide labels that are too small to read (< 2pt)
        if (labelSize < 2.0) {
            visible = false;
        }

        label.item->setVisible(visible);

        // Update font size if visible
        if (visible) {
            QFont font = label.item->font();
            font.setPointSizeF(labelSize);
            label.item->setFont(font);
        }
    }
}

void MapLabelManager::updateLabelPositions(std::function<QPointF(double, double)> latLonToSceneFunc) {
    latLonToSceneFunc_ = latLonToSceneFunc;

    // Update all label positions
    for (auto& label : labels_) {
        if (!label.item || !latLonToSceneFunc_) continue;

        QPointF scenePos = latLonToSceneFunc_(label.lat, label.lon);
        label.item->setPos(scenePos);
    }
}

void MapLabelManager::clearLabels() {
    for (auto& label : labels_) {
        if (label.item) {
            scene_->removeItem(label.item);
            delete label.item;
        }
    }
    labels_.clear();
}

void MapLabelManager::setLabelsEnabled(bool enabled) {
    labelsEnabled_ = enabled;
    updateLabelVisibility(currentZoom_);
}

void MapLabelManager::setLabelTypeVisible(LabelType type, bool visible) {
    labelTypeVisibility_[type] = visible;
    updateLabelVisibility(currentZoom_);
}

void MapLabelManager::onNetworkReply() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[MapLabelManager] Network error:" << reply->errorString();
        emit queryError(reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    qDebug() << "[MapLabelManager] Received" << data.size() << "bytes from Overpass API";

    parseOverpassResponse(data);
    reply->deleteLater();
}

void MapLabelManager::parseOverpassResponse(const QByteArray& data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "[MapLabelManager] Invalid JSON response";
        emit queryError("Invalid JSON response");
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray elements = root["elements"].toArray();

    qDebug() << "[MapLabelManager] Parsing" << elements.size() << "elements";

    // Clear existing labels
    clearLabels();

    int labelCount = 0;
    for (const QJsonValue& value : elements) {
        QJsonObject elem = value.toObject();

        // Get node data
        QString name = elem["tags"].toObject()["name"].toString();
        if (name.isEmpty()) {
            name = elem["tags"].toObject()["ref"].toString();  // Fallback to ref
        }
        if (name.isEmpty()) continue;  // Skip unnamed features

        double lat = elem["lat"].toDouble();
        double lon = elem["lon"].toDouble();

        // Determine label type
        LabelType type = getLabelTypeFromTags(elem["tags"].toObject());

        // Create label
        MapLabel label;
        label.name = name;
        label.type = type;
        label.lat = lat;
        label.lon = lon;
        label.minZoom = getMinZoomForType(type);
        label.maxZoom = -1;  // No max zoom

        labels_.push_back(label);
        createLabelItem(labels_.back());
        labelCount++;
    }

    qDebug() << "[MapLabelManager] Created" << labelCount << "labels";
    emit featuresLoaded(labelCount);

    // Update visibility for current zoom
    updateLabelVisibility(currentZoom_);
}

void MapLabelManager::createLabelItem(const MapLabel& label) {
    if (!scene_) return;

    // Create text item
    auto* item = new QGraphicsTextItem(label.name);

    // Style the label based on type
    item->setDefaultTextColor(getLabelColorForType(label.type));
    item->setFont(getLabelFontForType(label.type));

    // Add to scene
    scene_->addItem(item);

    // Update the label's item pointer (const_cast needed since labels_ stores non-const)
    const_cast<MapLabel&>(label).item = item;

    // Position the label if we have a conversion function
    if (latLonToSceneFunc_) {
        QPointF scenePos = latLonToSceneFunc_(label.lat, label.lon);
        item->setPos(scenePos);
    }
}

LabelType MapLabelManager::getLabelTypeFromTags(const QJsonObject& tags) {
    if (tags.contains("natural") && tags["natural"].toString() == "peak") {
        return LabelType::Peak;
    }
    if (tags.contains("place")) {
        QString place = tags["place"].toString();
        if (place == "city") return LabelType::City;
        if (place == "town") return LabelType::City;
        if (place == "village") return LabelType::Village;
    }
    if (tags.contains("natural") && tags["natural"].toString() == "water") {
        return LabelType::Water;
    }
    return LabelType::PointOfInterest;
}

int MapLabelManager::getMinZoomForType(LabelType type) {
    // Minimum zoom levels for different feature types
    switch (type) {
        case LabelType::Peak:
            return 12;  // Show peaks at zoom 12+
        case LabelType::City:
            return 8;   // Show cities at zoom 8+
        case LabelType::Village:
            return 11;  // Show villages at zoom 11+
        case LabelType::Water:
            return 10;  // Show water bodies at zoom 10+
        case LabelType::PointOfInterest:
            return 13;  // Show POIs at zoom 13+
    }
    return 10;  // Default
}

QColor MapLabelManager::getLabelColorForType(LabelType type) {
    switch (type) {
        case LabelType::Peak:
            return QColor(139, 69, 19);  // Brown for peaks
        case LabelType::City:
            return QColor(0, 0, 0);      // Black for cities
        case LabelType::Village:
            return QColor(64, 64, 64);   // Dark gray for villages
        case LabelType::Water:
            return QColor(0, 119, 190);  // Blue for water
        case LabelType::PointOfInterest:
            return QColor(96, 96, 96);   // Gray for POIs
    }
    return QColor(0, 0, 0);  // Default black
}

QFont MapLabelManager::getLabelFontForType(LabelType type) {
    QFont font;

    switch (type) {
        case LabelType::Peak:
            font.setFamily("Helvetica");
            font.setPointSize(9);
            font.setItalic(true);
            break;
        case LabelType::City:
            font.setFamily("Helvetica");
            font.setPointSize(12);
            font.setBold(true);
            break;
        case LabelType::Village:
            font.setFamily("Helvetica");
            font.setPointSize(9);
            break;
        case LabelType::Water:
            font.setFamily("Helvetica");
            font.setPointSize(9);
            font.setItalic(true);
            break;
        case LabelType::PointOfInterest:
            font.setFamily("Helvetica");
            font.setPointSize(8);
            break;
    }

    return font;
}

double MapLabelManager::calculateAltitudeFromZoom(int zoom) const {
    // Web Mercator approximation: altitude (m) ≈ 40075000 / (256 * 2^zoom)
    // Matches the formula used in ZoomControls
    constexpr double EARTH_CIRCUMFERENCE_M = 40075000.0;
    constexpr double TILE_SIZE = 256.0;
    return EARTH_CIRCUMFERENCE_M / (TILE_SIZE * (1 << zoom));
}

double MapLabelManager::getBaseSizeForType(LabelType type) const {
    // Base font sizes for each label type (when zoomed to optimal viewing distance)
    switch (type) {
        case LabelType::City:
            return 12.0;  // Cities are largest
        case LabelType::Peak:
            return 9.0;   // Peaks medium size
        case LabelType::Village:
            return 9.0;   // Villages medium size
        case LabelType::Water:
            return 9.0;   // Water labels medium size
        case LabelType::PointOfInterest:
            return 8.0;   // POIs smallest
    }
    return 10.0;  // Default
}

double MapLabelManager::calculateLabelSize(LabelType type, double altitudeMeters) const {
    // Get base size for this label type
    double baseSize = getBaseSizeForType(type);

    // Reference altitude where labels look good (approximately zoom level 10-12)
    constexpr double REFERENCE_ALTITUDE_M = 150000.0;  // ~zoom 11

    // Calculate scaling factor based on altitude ratio
    // When closer (lower altitude), labels should be larger
    // When farther (higher altitude), labels should be smaller
    double altitudeRatio = REFERENCE_ALTITUDE_M / altitudeMeters;

    // Apply logarithmic scaling for smoother transitions
    // log2(altitudeRatio) provides natural zoom-proportional scaling
    double scaleFactor = std::log2(altitudeRatio) / 2.0 + 1.0;

    // Calculate final size
    double finalSize = baseSize * scaleFactor;

    // Apply constraints:
    // - Minimum 2pt (below this, hide the label)
    // - Maximum 18pt (prevent labels from becoming too large)
    finalSize = std::clamp(finalSize, 2.0, 18.0);

    return finalSize;
}

} // namespace topo
