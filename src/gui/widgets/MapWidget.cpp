/**
 * @file MapWidget.cpp
 * @brief Implementation of Leaflet-based map widget
 */

#include "MapWidget.hpp"
#include "ZoomControls.hpp"
#include "../utils/MapBridge.hpp"
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QWebEnginePage>
#include <QFile>
#include <QDebug>
#include <QResizeEvent>
#include <QTimer>

namespace topo {

/**
 * @brief Custom QWebEnginePage that forwards JavaScript console messages to Qt logger
 */
class ConsoleForwardingPage : public QWebEnginePage {
public:
    explicit ConsoleForwardingPage(QObject* parent = nullptr) : QWebEnginePage(parent) {}

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message,
                                   int lineNumber, const QString& sourceID) override {
        QString levelStr;
        switch (level) {
            case InfoMessageLevel:    levelStr = "[JS INFO]"; break;
            case WarningMessageLevel: levelStr = "[JS WARN]"; break;
            case ErrorMessageLevel:   levelStr = "[JS ERROR]"; break;
        }

        if (sourceID.isEmpty()) {
            qDebug() << qPrintable(levelStr) << qPrintable(message);
        } else {
            qDebug() << qPrintable(levelStr) << qPrintable(QString("%1:%2: %3").arg(sourceID).arg(lineNumber).arg(message));
        }
    }
};

MapWidget::MapWidget(QWidget *parent)
    : QWidget(parent),
      centerLat_(63.069),      // Default: Mount Denali
      centerLon_(-151.007),
      zoomLevel_(10.0),
      contourLinesVisible_(false),
      topoMapVisible_(false),
      peaksVisible_(true),
      currentBoundsValid_(false),
      minLat_(0.0),
      minLon_(0.0),
      maxLat_(0.0),
      maxLon_(0.0),
      webView_(nullptr),
      webChannel_(nullptr),
      bridge_(nullptr),
      webViewLoaded_(false),
      zoomControls_(nullptr) {

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create web view for Leaflet map
    webView_ = new QWebEngineView(this);
    layout->addWidget(webView_);

    // Set custom page that forwards console messages
    webView_->setPage(new ConsoleForwardingPage(webView_));

    // Enable JavaScript and other features
    QWebEngineSettings* settings = webView_->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

    // Create Qt ↔ JavaScript bridge
    bridge_ = new MapBridge(this);
    webChannel_ = new QWebChannel(this);
    webChannel_->registerObject("qtBridge", bridge_);
    webView_->page()->setWebChannel(webChannel_);

    // Connect bridge signals
    connect(bridge_, &MapBridge::boundsChanged,
            this, &MapWidget::onBridgeBoundsChanged);
    connect(bridge_, &MapBridge::mapMoved,
            this, &MapWidget::onBridgeMapMoved);

    // Connect web view load signal
    connect(webView_, &QWebEngineView::loadFinished,
            this, &MapWidget::onWebViewLoadFinished);

    // Load Leaflet HTML from resources
    QFile htmlFile(":/leaflet_map.html");
    if (htmlFile.open(QIODevice::ReadOnly)) {
        QString html = QString::fromUtf8(htmlFile.readAll());
        webView_->setHtml(html, QUrl("qrc:/"));
        qDebug() << "[MapWidget] Loading Leaflet HTML from resources";
    } else {
        qWarning() << "[MapWidget] Failed to load leaflet_map.html from resources";
    }

    // Create zoom controls overlay
    zoomControls_ = new ZoomControls(this);
    zoomControls_->setZoomLevel(zoomLevel_, width(), height());
    zoomControls_->raise();  // On top of everything

    // Connect zoom control signals
    connect(zoomControls_, &ZoomControls::zoomInClicked, this, [this]() {
        double newZoom = std::min(zoomLevel_ + 1.0, MAX_ZOOM);
        runJavaScript(QString("window.setZoom(%1);").arg(newZoom));
    });
    connect(zoomControls_, &ZoomControls::zoomOutClicked, this, [this]() {
        double newZoom = std::max(zoomLevel_ - 1.0, MIN_ZOOM);
        runJavaScript(QString("window.setZoom(%1);").arg(newZoom));
    });

    // Position overlays
    updateOverlayPositions();
}

MapWidget::~MapWidget() {
    // Qt parent-child ownership handles cleanup
}

std::pair<QPointF, double> MapWidget::getMapState() const {
    return {QPointF(centerLon_, centerLat_), zoomLevel_};
}

void MapWidget::setMapState(const QPointF& center, double zoom) {
    centerLon_ = center.x();
    centerLat_ = center.y();
    zoomLevel_ = zoom;

    if (webViewLoaded_) {
        runJavaScript(QString("window.setMapView(%1, %2, %3);")
            .arg(centerLat_, 0, 'f', 6)
            .arg(centerLon_, 0, 'f', 6)
            .arg(zoom));
    }
}

void MapWidget::centerOn(double lat, double lon, double zoom) {
    centerLat_ = lat;
    centerLon_ = lon;
    if (zoom >= 0.0) {
        zoomLevel_ = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    }

    if (webViewLoaded_) {
        if (zoom >= 0.0) {
            runJavaScript(QString("window.setMapView(%1, %2, %3);")
                .arg(lat, 0, 'f', 6)
                .arg(lon, 0, 'f', 6)
                .arg(zoom));
        } else {
            runJavaScript(QString("window.panTo(%1, %2);")
                .arg(lat, 0, 'f', 6)
                .arg(lon, 0, 'f', 6));
        }
    }
}

void MapWidget::fitBounds(double min_lat, double min_lon, double max_lat, double max_lon) {
    if (webViewLoaded_) {
        runJavaScript(QString("window.fitBounds(%1, %2, %3, %4);")
            .arg(min_lat, 0, 'f', 6)
            .arg(min_lon, 0, 'f', 6)
            .arg(max_lat, 0, 'f', 6)
            .arg(max_lon, 0, 'f', 6));
    }
}

void MapWidget::setCacheDirectory(const QString& /*dir*/) {
    // No-op: browser handles caching automatically
    // Kept for API compatibility
}

void MapWidget::setUseMetric(bool useMetric) {
    if (zoomControls_) {
        zoomControls_->setUseMetric(useMetric);
    }
}

void MapWidget::setContourLinesVisible(bool visible) {
    qDebug() << "[MapWidget::setContourLinesVisible] Called with visible =" << visible
             << ", webViewLoaded_ =" << webViewLoaded_;
    contourLinesVisible_ = visible;
    if (webViewLoaded_) {
        // Call with actual JavaScript boolean (no quotes)
        runJavaScript(QString("window.toggleContourLines(%1);").arg(visible ? "true" : "false"));
    }
}

void MapWidget::setTopoMapVisible(bool visible) {
    qDebug() << "[MapWidget::setTopoMapVisible] Called with visible =" << visible
             << ", webViewLoaded_ =" << webViewLoaded_;
    topoMapVisible_ = visible;
    if (webViewLoaded_) {
        // Call with actual JavaScript boolean (no quotes)
        runJavaScript(QString("window.toggleTopoMap(%1);").arg(visible ? "true" : "false"));
    }
}

void MapWidget::setPeaksVisible(bool visible) {
    qDebug() << "[MapWidget::setPeaksVisible] Called with visible =" << visible
             << ", webViewLoaded_ =" << webViewLoaded_;
    peaksVisible_ = visible;
    if (webViewLoaded_) {
        // Call with actual JavaScript boolean (no quotes)
        runJavaScript(QString("window.togglePeaks(%1);").arg(visible ? "true" : "false"));
    }
}

std::optional<std::tuple<double, double, double, double>> MapWidget::getCurrentBounds() const {
    if (currentBoundsValid_) {
        return std::make_tuple(minLat_, minLon_, maxLat_, maxLon_);
    }
    return std::nullopt;
}

void MapWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateOverlayPositions();

    // Tell Leaflet to recalculate map size and emit new bounds
    if (webViewLoaded_) {
        runJavaScript("window.invalidateSize();");
    }
}

void MapWidget::onWebViewLoadFinished(bool success) {
    if (success) {
        qDebug() << "[MapWidget] Leaflet map loaded successfully";

        // Wait 100ms for all JavaScript to initialize before setting webViewLoaded_
        QTimer::singleShot(100, this, [this]() {
            webViewLoaded_ = true;

            // Verify JavaScript functions are available
            runJavaScript("console.log('Functions available:', typeof window.toggleContourLines, typeof window.setMapView);");

            // Set initial map state
            runJavaScript(QString("window.setMapView(%1, %2, %3);")
                .arg(centerLat_, 0, 'f', 6)
                .arg(centerLon_, 0, 'f', 6)
                .arg(zoomLevel_));

            // Apply initial layer visibility
            if (contourLinesVisible_) {
                runJavaScript("window.toggleContourLines(true);");
            }
            if (topoMapVisible_) {
                runJavaScript("window.toggleTopoMap(true);");
            }
            if (!peaksVisible_) {
                runJavaScript("window.togglePeaks(false);");
            }
        });
    } else {
        qWarning() << "[MapWidget] Failed to load Leaflet map";
    }
}

void MapWidget::onBridgeBoundsChanged(double minLat, double minLon, double maxLat, double maxLon) {
    qDebug() << "[MapWidget] Bounds changed from JS:" << minLat << minLon << maxLat << maxLon;

    // Store current bounds
    minLat_ = minLat;
    minLon_ = minLon;
    maxLat_ = maxLat;
    maxLon_ = maxLon;
    currentBoundsValid_ = true;

    // Emit bounds as selection (for status bar display)
    emit selectionChanged(minLat, minLon, maxLat, maxLon);
}

void MapWidget::onBridgeMapMoved(double lat, double lon, double zoom) {
    centerLat_ = lat;
    centerLon_ = lon;
    zoomLevel_ = zoom;

    // Update zoom controls display with viewport-aware altitude calculation
    if (zoomControls_) {
        zoomControls_->setZoomLevel(zoom, width(), height());
    }

    emit mapMoved(lat, lon, zoom);
}

void MapWidget::runJavaScript(const QString& script) {
    qDebug() << "[MapWidget::runJavaScript] Executing:" << script;
    if (webView_ && webView_->page()) {
        // Wrap in try-catch to detect actual JavaScript errors
        // Note: Functions returning undefined are NOT errors
        QString safeScript = QString("try { %1 } catch(e) { console.error('JS Error:', e.message); throw e; }").arg(script);
        webView_->page()->runJavaScript(safeScript, [](const QVariant& /*result*/) {
            // Success - void functions return undefined, which is expected
        });
    } else {
        qDebug() << "[MapWidget::runJavaScript] ERROR: webView or page is null!";
    }
}

void MapWidget::callJsFunction(const QString& function, const QStringList& args) {
    QString argsStr = args.join(", ");
    QString script = QString("%1(%2);").arg(function, argsStr);
    runJavaScript(script);
}

void MapWidget::updateOverlayPositions() {
    int margin = 10;

    // Position zoom controls in top-right corner
    if (zoomControls_) {
        zoomControls_->move(width() - zoomControls_->width() - margin, margin);
        // Update altitude calculation with new viewport size
        zoomControls_->setZoomLevel(zoomLevel_, width(), height());
    }
}

} // namespace topo
