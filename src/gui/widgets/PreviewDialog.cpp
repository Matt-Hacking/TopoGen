/**
 * @file PreviewDialog.cpp
 * @brief Implementation of preview dialog
 */

#include "PreviewDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileInfo>
#include <QPixmap>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QSvgRenderer>

namespace topo {

PreviewDialog::PreviewDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Preview");
    setMinimumSize(800, 600);
    createUI();
}

void PreviewDialog::createUI() {
    auto* layout = new QVBoxLayout(this);

    // Tab widget for multiple files
    tabWidget_ = new QTabWidget(this);
    layout->addWidget(tabWidget_);

    // Close button
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

void PreviewDialog::setFiles(const QStringList& files) {
    clear();
    for (const QString& file : files) {
        addFile(file);
    }
}

void PreviewDialog::addFile(const QString& filePath) {
    loadFile(filePath);
}

void PreviewDialog::clear() {
    while (tabWidget_->count() > 0) {
        QWidget* widget = tabWidget_->widget(0);
        tabWidget_->removeTab(0);
        delete widget;
    }
}

void PreviewDialog::loadFile(const QString& filePath) {
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "File Not Found",
                           QString("Cannot preview: %1\nFile does not exist.").arg(filePath));
        return;
    }

    FileType type = detectFileType(filePath);
    QWidget* previewWidget = createPreviewWidget(filePath, type);

    if (previewWidget) {
        tabWidget_->addTab(previewWidget, fileInfo.fileName());
    }
}

PreviewDialog::FileType PreviewDialog::detectFileType(const QString& filePath) const {
    QString suffix = QFileInfo(filePath).suffix().toLower();

    if (suffix == "png") return FileType::PNG;
    if (suffix == "tif" || suffix == "tiff") return FileType::TIFF;
    if (suffix == "svg") return FileType::SVG;
    if (suffix == "stl") return FileType::STL;
    if (suffix == "obj") return FileType::OBJ;
    if (suffix == "ply") return FileType::PLY;
    if (suffix == "geojson" || suffix == "json") return FileType::GeoJSON;
    if (suffix == "shp") return FileType::Shapefile;

    return FileType::Unknown;
}

QWidget* PreviewDialog::createPreviewWidget(const QString& filePath, FileType type) {
    switch (type) {
        case FileType::PNG:
        case FileType::TIFF:
            return createImagePreview(filePath);

        case FileType::SVG:
            return createSvgPreview(filePath);

        case FileType::STL:
        case FileType::OBJ:
        case FileType::PLY:
            return create3DPreview(filePath);

        case FileType::GeoJSON:
            return createTextPreview(filePath);

        case FileType::Shapefile:
        case FileType::Unknown:
        default: {
            auto* label = new QLabel(QString("Preview not available for this file type.\n\nFile: %1")
                                    .arg(QFileInfo(filePath).fileName()), this);
            label->setAlignment(Qt::AlignCenter);
            label->setMargin(20);
            return label;
        }
    }
}

QWidget* PreviewDialog::createImagePreview(const QString& filePath) {
    QPixmap pixmap(filePath);

    if (pixmap.isNull()) {
        auto* label = new QLabel(QString("Failed to load image:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        return label;
    }

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(false);
    scrollArea->setAlignment(Qt::AlignCenter);

    auto* label = new QLabel(scrollArea);
    label->setPixmap(pixmap);
    scrollArea->setWidget(label);

    return scrollArea;
}

QWidget* PreviewDialog::createSvgPreview(const QString& filePath) {
    auto* svgWidget = new QSvgWidget(filePath, this);

    if (!svgWidget->renderer()->isValid()) {
        delete svgWidget;
        auto* label = new QLabel(QString("Failed to load SVG:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        return label;
    }

    // SVG in scroll area for large files
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidget(svgWidget);
    scrollArea->setWidgetResizable(true);

    return scrollArea;
}

QWidget* PreviewDialog::create3DPreview(const QString& filePath) {
    // 3D preview not implemented yet - would require Qt3D or similar
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    auto* label = new QLabel(
        QString("3D Preview not yet implemented.\n\n"
                "File: %1\n\n"
                "You can open this file in a 3D viewer application:\n"
                "• Blender (free, open-source)\n"
                "• MeshLab (free, open-source)\n"
                "• Fusion 360 (commercial)\n"
                "• Or your slicer software for 3D printing")
            .arg(QFileInfo(filePath).fileName()),
        container
    );
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto* openButton = new QPushButton("Open in System Viewer", container);
    connect(openButton, &QPushButton::clicked, [filePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    });
    layout->addWidget(openButton, 0, Qt::AlignCenter);

    return container;
}

QWidget* PreviewDialog::createTextPreview(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        auto* label = new QLabel(QString("Failed to open file:\n%1").arg(filePath), this);
        label->setAlignment(Qt::AlignCenter);
        return label;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Limit preview to first 10000 characters for large files
    if (content.length() > 10000) {
        content = content.left(10000) + "\n\n... (file truncated for preview)";
    }

    auto* textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(content);
    textEdit->setFontFamily("Courier");

    return textEdit;
}

} // namespace topo
