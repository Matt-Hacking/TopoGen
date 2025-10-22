#pragma once

/**
 * @file NominatimClient.hpp
 * @brief OSM Nominatim geocoding client
 *
 * Provides geocoding functionality using OpenStreetMap Nominatim API:
 * - Search by place name ("Mount Denali")
 * - Parse raw coordinates ("63.069,-151.007")
 * - Async requests with Qt Network
 * - Rate limiting (1 req/sec per OSM policy)
 * - Proper User-Agent header
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

// Forward declarations
class QNetworkAccessManager;
class QNetworkReply;

namespace topo {

/**
 * @brief Geocoding result from Nominatim
 */
struct GeocodingResult {
    QString displayName;
    double latitude;
    double longitude;
    double boundingBoxMinLat;
    double boundingBoxMinLon;
    double boundingBoxMaxLat;
    double boundingBoxMaxLon;
    QString type;  // e.g., "peak", "city", "state"
    QString placeClass;  // e.g., "natural", "place"

    bool isValid() const { return !displayName.isEmpty(); }
};

/**
 * @brief Qt client for OSM Nominatim geocoding API
 *
 * Usage:
 *   NominatimClient client;
 *   connect(&client, &NominatimClient::geocodingComplete, ...);
 *   client.geocode("Mount Denali");
 */
class NominatimClient : public QObject {
    Q_OBJECT

public:
    explicit NominatimClient(QObject *parent = nullptr);
    ~NominatimClient() override;

    /**
     * @brief Geocode a location query
     *
     * Query can be:
     * - Place name: "Mount Denali", "San Francisco, CA"
     * - Raw coordinates: "63.069,-151.007" or "63.069°N, 151.007°W"
     *
     * Emits geocodingComplete on success, geocodingFailed on failure
     */
    void geocode(const QString& query);

    /**
     * @brief Cancel any pending geocoding request
     */
    void cancel();

signals:
    /**
     * @brief Emitted when geocoding succeeds
     */
    void geocodingComplete(GeocodingResult result);

    /**
     * @brief Emitted when geocoding fails
     */
    void geocodingFailed(QString error);

private slots:
    void onReplyFinished();
    void onRateLimitTimeout();

private:
    bool tryParseCoordinates(const QString& query, double& lat, double& lon);
    void performGeocodingRequest(const QString& query);

    QNetworkAccessManager* networkManager_;
    QNetworkReply* currentReply_;

    // Rate limiting (1 request per second per OSM policy)
    QTimer rateLimitTimer_;
    QString pendingQuery_;
    bool requestPending_;

    static constexpr const char* NOMINATIM_URL = "https://nominatim.openstreetmap.org/search";
    static constexpr const char* USER_AGENT = "TopographicGenerator/0.22 (Qt GUI)";
    static constexpr int RATE_LIMIT_MS = 1000;  // 1 second between requests
};

} // namespace topo
