#pragma once

/**
 * @file MapLabelManager.hpp
 * @brief Manages map labels from OSM data (peaks, cities, POIs)
 *
 * Queries Overpass API for geographic features and creates label overlays
 * that appear at appropriate zoom levels.
 */

#include <QObject>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHash>
#include <QPointF>
#include <vector>
#include <map>

namespace topo {

/**
 * @brief Label types for different feature categories
 */
enum class LabelType {
    Peak,           // Mountain peaks
    City,           // Cities and towns
    Village,        // Small settlements
    Water,          // Lakes, rivers
    PointOfInterest // Other notable features
};

/**
 * @brief Geographic feature with label information
 */
struct MapLabel {
    QString name;
    LabelType type;
    double lat;
    double lon;
    int minZoom;    // Minimum zoom level to display
    int maxZoom;    // Maximum zoom level to display (or -1 for no limit)
    QGraphicsTextItem* item;  // Graphics item in scene

    MapLabel() : type(LabelType::PointOfInterest), lat(0), lon(0),
                 minZoom(0), maxZoom(-1), item(nullptr) {}
};

/**
 * @brief Manages map labels from OSM data
 */
class MapLabelManager : public QObject {
    Q_OBJECT

public:
    explicit MapLabelManager(QGraphicsScene* scene, QObject* parent = nullptr);
    ~MapLabelManager() override;

    /**
     * @brief Query OSM for features in the given bounds
     * @param min_lat Minimum latitude
     * @param min_lon Minimum longitude
     * @param max_lat Maximum latitude
     * @param max_lon Maximum longitude
     */
    void queryFeatures(double min_lat, double min_lon, double max_lat, double max_lon);

    /**
     * @brief Update label visibility and sizing based on current zoom level and altitude
     * @param zoom Current zoom level
     * @param altitudeMeters Eye altitude in meters (optional, calculated from zoom if not provided)
     */
    void updateLabelVisibility(int zoom, double altitudeMeters = -1.0);

    /**
     * @brief Update label positions in scene coordinates
     * @param latLonToSceneFunc Function to convert lat/lon to scene coordinates
     */
    void updateLabelPositions(std::function<QPointF(double, double)> latLonToSceneFunc);

    /**
     * @brief Clear all labels
     */
    void clearLabels();

    /**
     * @brief Enable/disable label display
     */
    void setLabelsEnabled(bool enabled);

    /**
     * @brief Set visibility for a specific label type
     * @param type The label type to control
     * @param visible Whether labels of this type should be visible
     */
    void setLabelTypeVisible(LabelType type, bool visible);

signals:
    void featuresLoaded(int count);
    void queryError(QString error);

private slots:
    void onNetworkReply();

private:
    void parseOverpassResponse(const QByteArray& data);
    void createLabelItem(const MapLabel& label);
    LabelType getLabelTypeFromTags(const QJsonObject& tags);
    int getMinZoomForType(LabelType type);
    QColor getLabelColorForType(LabelType type);
    QFont getLabelFontForType(LabelType type);

    // Intelligent scaling helpers
    double calculateAltitudeFromZoom(int zoom) const;
    double calculateLabelSize(LabelType type, double altitudeMeters) const;
    double getBaseSizeForType(LabelType type) const;

    QGraphicsScene* scene_;
    QNetworkAccessManager* networkManager_;
    std::vector<MapLabel> labels_;
    int currentZoom_;
    bool labelsEnabled_;
    std::map<LabelType, bool> labelTypeVisibility_;
    std::function<QPointF(double, double)> latLonToSceneFunc_;
};

} // namespace topo
