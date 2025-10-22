/**
 * @file MapFeaturesPanel.cpp
 * @brief Implementation of map features control panel
 */

#include "MapFeaturesPanel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDebug>

namespace topo {

MapFeaturesPanel::MapFeaturesPanel(QWidget *parent)
    : QWidget(parent),
      useMetric_(false),  // Default to imperial, MainWindow will update from settings
      currentAltitudeMeters_(DEFAULT_ALTITUDE_M) {
    setupUI();
}

void MapFeaturesPanel::setupUI() {
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(10, 10, 10, 10);
    mainLayout_->setSpacing(10);

    // Set white background
    setStyleSheet("MapFeaturesPanel { background-color: white; }");
    setMinimumWidth(220);
    setMaximumWidth(300);

    // ===== Features Group =====
    featuresGroup_ = new QGroupBox(tr("Map Features"), this);
    QVBoxLayout* featuresLayout = new QVBoxLayout(featuresGroup_);
    featuresLayout->setSpacing(8);

    // Create checkboxes for each feature
    contourLinesCheckbox_ = new QCheckBox(tr("Contour Lines"), this);
    contourLinesCheckbox_->setToolTip(tr("Display elevation contour lines without hillshading or labels.\n"
        "Note: Labels are pre-rendered by the tile service and may appear small when zoomed in."));
    connect(contourLinesCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::onContourLinesToggled);

    topoMapCheckbox_ = new QCheckBox(tr("Topographic Map"), this);
    topoMapCheckbox_->setToolTip(tr("Display full topographic map (OpenTopoMap: hillshading, contours, and labels)"));
    connect(topoMapCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::onTopoMapToggled);

    waterCheckbox_ = new QCheckBox(tr("Water Bodies"), this);
    waterCheckbox_->setToolTip(tr("Display lakes, rivers, and other water features"));
    waterCheckbox_->setChecked(true);  // Default enabled
    connect(waterCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::waterVisibilityChanged);

    roadsCheckbox_ = new QCheckBox(tr("Roads"), this);
    roadsCheckbox_->setToolTip(tr("Display roads and highways"));
    roadsCheckbox_->setChecked(true);  // Default enabled
    connect(roadsCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::roadsVisibilityChanged);

    parksCheckbox_ = new QCheckBox(tr("Park Boundaries"), this);
    parksCheckbox_->setToolTip(tr("Display national parks, state parks, and protected areas"));
    connect(parksCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::parksVisibilityChanged);

    peaksCheckbox_ = new QCheckBox(tr("Peaks"), this);
    peaksCheckbox_->setToolTip(tr("Display mountain peaks and summits with elevations"));
    connect(peaksCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::onPeaksToggled);

    boundariesCheckbox_ = new QCheckBox(tr("Political Boundaries"), this);
    boundariesCheckbox_->setToolTip(tr("Display state/province and country boundaries"));
    connect(boundariesCheckbox_, &QCheckBox::toggled, this, &MapFeaturesPanel::boundariesVisibilityChanged);

    featuresLayout->addWidget(contourLinesCheckbox_);
    featuresLayout->addWidget(topoMapCheckbox_);
    featuresLayout->addWidget(waterCheckbox_);
    featuresLayout->addWidget(roadsCheckbox_);
    featuresLayout->addWidget(parksCheckbox_);
    featuresLayout->addWidget(peaksCheckbox_);
    featuresLayout->addWidget(boundariesCheckbox_);

    mainLayout_->addWidget(featuresGroup_);

    // Hide map features for now - datasources not good enough yet
    featuresGroup_->hide();

    // ===== Altitude Filter Group =====
    altitudeGroup_ = new QGroupBox(tr("Altitude Filter"), this);
    QVBoxLayout* altitudeLayout = new QVBoxLayout(altitudeGroup_);
    altitudeLayout->setSpacing(8);

    // Altitude value display with spinbox
    QHBoxLayout* valueLayout = new QHBoxLayout();
    QLabel* minLabel = new QLabel(tr("Minimum:"), this);
    altitudeSpinBox_ = new QSpinBox(this);
    altitudeSpinBox_->setRange(MIN_ALTITUDE_M, MAX_ALTITUDE_M);
    altitudeSpinBox_->setValue(DEFAULT_ALTITUDE_M);
    altitudeSpinBox_->setSuffix(" m");
    altitudeSpinBox_->setSingleStep(10);
    altitudeSpinBox_->setMinimumWidth(100);
    valueLayout->addWidget(minLabel);
    valueLayout->addWidget(altitudeSpinBox_);
    valueLayout->addStretch();

    // Slider for altitude selection
    altitudeSlider_ = new QSlider(Qt::Horizontal, this);
    altitudeSlider_->setMinimum(MIN_ALTITUDE_M);
    altitudeSlider_->setMaximum(MAX_ALTITUDE_M);
    altitudeSlider_->setValue(DEFAULT_ALTITUDE_M);
    altitudeSlider_->setTickPosition(QSlider::TicksBelow);
    altitudeSlider_->setTickInterval(1000);  // Tick every 1000m
    altitudeSlider_->setToolTip(tr("Minimum altitude for display and output generation"));

    // Connect spinbox ↔ slider synchronization
    connect(altitudeSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            altitudeSlider_, &QSlider::setValue);
    connect(altitudeSlider_, &QSlider::valueChanged,
            altitudeSpinBox_, &QSpinBox::setValue);

    // Connect slider to handler that converts units and emits signals
    connect(altitudeSlider_, &QSlider::valueChanged,
            this, &MapFeaturesPanel::onAltitudeValueChanged);

    // Keep the label for displaying "Sea Level" or formatted text
    altitudeLabel_ = new QLabel(this);
    updateAltitudeLabel(DEFAULT_ALTITUDE_M);
    altitudeLabel_->setAlignment(Qt::AlignCenter);
    QFont labelFont = altitudeLabel_->font();
    labelFont.setBold(true);
    altitudeLabel_->setFont(labelFont);

    // Min/Max range labels
    QHBoxLayout* rangeLayout = new QHBoxLayout();
    QLabel* minRangeLabel = new QLabel(QString("%1m").arg(MIN_ALTITUDE_M), this);
    QLabel* maxRangeLabel = new QLabel(QString("%1m").arg(MAX_ALTITUDE_M), this);
    minRangeLabel->setStyleSheet("font-size: 9pt; color: gray;");
    maxRangeLabel->setStyleSheet("font-size: 9pt; color: gray;");
    rangeLayout->addWidget(minRangeLabel);
    rangeLayout->addStretch();
    rangeLayout->addWidget(maxRangeLabel);

    // Help text
    QLabel* helpText = new QLabel(
        tr("Areas below this altitude will be shown as white space and excluded from output."),
        this
    );
    helpText->setWordWrap(true);
    helpText->setStyleSheet("font-size: 9pt; color: gray;");

    altitudeLayout->addLayout(valueLayout);
    altitudeLayout->addWidget(altitudeSlider_);
    altitudeLayout->addLayout(rangeLayout);
    altitudeLayout->addWidget(altitudeLabel_);
    altitudeLayout->addWidget(helpText);

    mainLayout_->addWidget(altitudeGroup_);

    // ===== Spacer =====
    mainLayout_->addStretch();

    // Add a subtle separator line at the right edge
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet("QFrame { color: #cccccc; }");

    // Initialize widgets to match current useMetric_ setting
    // This ensures consistency between the default useMetric_ value and widget units
    updateWidgetRanges();
}

void MapFeaturesPanel::updateAltitudeLabel(int meters) {
    QString text;
    if (meters == 0) {
        text = tr("Sea Level");
    } else if (useMetric_) {
        // Metric units
        text = tr("Min: %1 m").arg(meters);
    } else {
        // Imperial units (feet)
        int feet = static_cast<int>(meters * 3.28084);
        text = tr("Min: %1 ft").arg(feet);
    }
    altitudeLabel_->setText(text);
}

int MapFeaturesPanel::minimumAltitudeMeters() const {
    return currentAltitudeMeters_;
}

void MapFeaturesPanel::setMinimumAltitudeMeters(int meters) {
    currentAltitudeMeters_ = meters;
    int displayValue = metersToDisplayValue(meters);
    altitudeSlider_->setValue(displayValue);
    altitudeSpinBox_->setValue(displayValue);
    updateAltitudeLabel(meters);
}

void MapFeaturesPanel::onContourLinesToggled(bool checked) {
    qDebug() << "[MapFeaturesPanel::onContourLinesToggled] Checkbox toggled to:" << checked;
    emit contourLinesVisibilityChanged(checked);
}

void MapFeaturesPanel::onTopoMapToggled(bool checked) {
    qDebug() << "[MapFeaturesPanel::onTopoMapToggled] Checkbox toggled to:" << checked;
    emit topoMapVisibilityChanged(checked);
}

void MapFeaturesPanel::onPeaksToggled(bool checked) {
    qDebug() << "[MapFeaturesPanel::onPeaksToggled] Checkbox toggled to:" << checked;
    emit peaksVisibilityChanged(checked);
}

void MapFeaturesPanel::onAltitudeValueChanged(int displayValue) {
    // Convert display value to meters
    int meters = displayValueToMeters(displayValue);

    // Update internal state
    currentAltitudeMeters_ = meters;

    // Update label using display value to avoid round-trip conversion errors
    // Check if we're at sea level (either 0 in display or 0 in meters)
    QString text;
    if (meters == 0 || displayValue == 0) {
        text = tr("Sea Level");
    } else if (useMetric_) {
        text = tr("Min: %1 m").arg(displayValue);
    } else {
        text = tr("Min: %1 ft").arg(displayValue);
    }
    altitudeLabel_->setText(text);

    // Emit signal with meters (internal representation)
    emit minimumAltitudeChanged(meters);
}

int MapFeaturesPanel::metersToDisplayValue(int meters) const {
    if (useMetric_) {
        return meters;
    } else {
        // Convert meters to feet
        return static_cast<int>(meters * 3.28084);
    }
}

int MapFeaturesPanel::displayValueToMeters(int displayValue) const {
    if (useMetric_) {
        return displayValue;
    } else {
        // Convert feet to meters
        return static_cast<int>(displayValue / 3.28084);
    }
}

void MapFeaturesPanel::updateWidgetRanges() {
    // Block signals to prevent cascading updates
    altitudeSlider_->blockSignals(true);
    altitudeSpinBox_->blockSignals(true);

    // Update ranges based on unit system
    int minDisplay = metersToDisplayValue(MIN_ALTITUDE_M);
    int maxDisplay = metersToDisplayValue(MAX_ALTITUDE_M);

    altitudeSlider_->setRange(minDisplay, maxDisplay);
    altitudeSpinBox_->setRange(minDisplay, maxDisplay);

    // Set tick interval for slider (1000m = ~3281ft)
    int tickInterval = useMetric_ ? 1000 : 3281;
    altitudeSlider_->setTickInterval(tickInterval);

    // Re-enable signals
    altitudeSlider_->blockSignals(false);
    altitudeSpinBox_->blockSignals(false);
}

void MapFeaturesPanel::setUseMetric(bool useMetric) {
    // Store previous unit system to detect changes
    bool wasMetric = useMetric_;
    useMetric_ = useMetric;

    // If unit system hasn't changed, just return
    if (wasMetric == useMetric) {
        return;
    }

    // Block signals to prevent cascading updates during conversion
    altitudeSlider_->blockSignals(true);
    altitudeSpinBox_->blockSignals(true);

    // Update widget ranges to new unit system
    updateWidgetRanges();

    // Convert current altitude from meters to display value in new units
    int displayValue = metersToDisplayValue(currentAltitudeMeters_);

    // Update both widgets with converted value
    altitudeSlider_->setValue(displayValue);
    altitudeSpinBox_->setValue(displayValue);

    // Update spinbox suffix
    altitudeSpinBox_->setSuffix(useMetric ? " m" : " ft");

    // Update label to show correct units
    updateAltitudeLabel(currentAltitudeMeters_);

    // Re-enable signals
    altitudeSlider_->blockSignals(false);
    altitudeSpinBox_->blockSignals(false);
}

} // namespace topo
