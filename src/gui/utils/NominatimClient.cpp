/**
 * @file NominatimClient.cpp
 * @brief Implementation of OSM Nominatim geocoding client
 */

#include "NominatimClient.hpp"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>

namespace topo {

NominatimClient::NominatimClient(QObject *parent)
    : QObject(parent),
      networkManager_(new QNetworkAccessManager(this)),
      currentReply_(nullptr),
      requestPending_(false) {

    // Configure rate limit timer
    rateLimitTimer_.setSingleShot(true);
    connect(&rateLimitTimer_, &QTimer::timeout, this, &NominatimClient::onRateLimitTimeout);
}

NominatimClient::~NominatimClient() {
    cancel();
}

void NominatimClient::geocode(const QString& query) {
    if (query.trimmed().isEmpty()) {
        emit geocodingFailed("Empty query");
        return;
    }

    // Try parsing as raw coordinates first
    double lat, lon;
    if (tryParseCoordinates(query, lat, lon)) {
        // Immediate result for raw coordinates
        GeocodingResult result;
        result.displayName = QString("%1, %2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6);
        result.latitude = lat;
        result.longitude = lon;
        result.boundingBoxMinLat = lat - 0.01;
        result.boundingBoxMinLon = lon - 0.01;
        result.boundingBoxMaxLat = lat + 0.01;
        result.boundingBoxMaxLon = lon + 0.01;
        result.type = "coordinate";
        result.placeClass = "coordinate";

        emit geocodingComplete(result);
        return;
    }

    // Queue request (rate limiting)
    pendingQuery_ = query.trimmed();
    requestPending_ = true;

    if (!rateLimitTimer_.isActive()) {
        // Can send immediately
        performGeocodingRequest(pendingQuery_);
        requestPending_ = false;

        // Start rate limit timer for next request
        rateLimitTimer_.start(RATE_LIMIT_MS);
    }
    // Otherwise, will be sent when timer fires
}

void NominatimClient::cancel() {
    if (currentReply_) {
        currentReply_->abort();
        currentReply_->deleteLater();
        currentReply_ = nullptr;
    }
    requestPending_ = false;
    pendingQuery_.clear();
}

bool NominatimClient::tryParseCoordinates(const QString& query, double& lat, double& lon) {
    // Try parsing various coordinate formats:
    // "63.069,-151.007"
    // "63.069, -151.007"
    // "63.069°N, 151.007°W"
    // "N63.069, W151.007"

    QString cleanQuery = query.trimmed();

    // Remove degree symbols and direction letters for parsing
    cleanQuery.replace("°", "");
    cleanQuery.replace("N", "");
    cleanQuery.replace("S", "-");
    cleanQuery.replace("E", "");
    cleanQuery.replace("W", "-");

    // Try splitting by comma
    QStringList parts = cleanQuery.split(",");
    if (parts.size() != 2) {
        return false;
    }

    bool latOk, lonOk;
    lat = parts[0].trimmed().toDouble(&latOk);
    lon = parts[1].trimmed().toDouble(&lonOk);

    if (!latOk || !lonOk) {
        return false;
    }

    // Validate ranges
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        return false;
    }

    return true;
}

void NominatimClient::performGeocodingRequest(const QString& query) {
    // Build Nominatim URL
    QUrl url(NOMINATIM_URL);
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("q", query);
    urlQuery.addQueryItem("format", "json");
    urlQuery.addQueryItem("limit", "1");  // Only need first result
    urlQuery.addQueryItem("addressdetails", "0");  // Don't need full address breakdown
    url.setQuery(urlQuery);

    // Create request with proper User-Agent header (required by OSM)
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", USER_AGENT);

    // Send request
    currentReply_ = networkManager_->get(request);
    connect(currentReply_, &QNetworkReply::finished, this, &NominatimClient::onReplyFinished);
}

void NominatimClient::onReplyFinished() {
    if (!currentReply_) return;

    // Handle errors
    if (currentReply_->error() != QNetworkReply::NoError) {
        QString error = currentReply_->errorString();
        currentReply_->deleteLater();
        currentReply_ = nullptr;
        emit geocodingFailed(error);
        return;
    }

    // Parse JSON response
    QByteArray responseData = currentReply_->readAll();
    currentReply_->deleteLater();
    currentReply_ = nullptr;

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isArray()) {
        emit geocodingFailed("Invalid JSON response");
        return;
    }

    QJsonArray results = doc.array();
    if (results.isEmpty()) {
        emit geocodingFailed("Location not found");
        return;
    }

    // Parse first result
    QJsonObject result = results[0].toObject();

    GeocodingResult geocodingResult;
    geocodingResult.displayName = result["display_name"].toString();
    geocodingResult.latitude = result["lat"].toString().toDouble();
    geocodingResult.longitude = result["lon"].toString().toDouble();
    geocodingResult.type = result["type"].toString();
    geocodingResult.placeClass = result["class"].toString();

    // Parse bounding box if available
    if (result.contains("boundingbox")) {
        QJsonArray bbox = result["boundingbox"].toArray();
        if (bbox.size() == 4) {
            geocodingResult.boundingBoxMinLat = bbox[0].toString().toDouble();
            geocodingResult.boundingBoxMaxLat = bbox[1].toString().toDouble();
            geocodingResult.boundingBoxMinLon = bbox[2].toString().toDouble();
            geocodingResult.boundingBoxMaxLon = bbox[3].toString().toDouble();
        }
    } else {
        // Default bounding box (small area around point)
        geocodingResult.boundingBoxMinLat = geocodingResult.latitude - 0.01;
        geocodingResult.boundingBoxMaxLat = geocodingResult.latitude + 0.01;
        geocodingResult.boundingBoxMinLon = geocodingResult.longitude - 0.01;
        geocodingResult.boundingBoxMaxLon = geocodingResult.longitude + 0.01;
    }

    emit geocodingComplete(geocodingResult);
}

void NominatimClient::onRateLimitTimeout() {
    // Rate limit period expired, send pending request if any
    if (requestPending_) {
        performGeocodingRequest(pendingQuery_);
        requestPending_ = false;

        // Restart rate limit timer
        rateLimitTimer_.start(RATE_LIMIT_MS);
    }
}

} // namespace topo
