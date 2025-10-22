#pragma once

/**
 * @file PreviewDialog.hpp
 * @brief Preview dialog for generated output files
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QSvgWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QString>
#include <QStringList>

namespace topo {

class PreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreviewDialog(QWidget *parent = nullptr);

    void setFiles(const QStringList& files);
    void addFile(const QString& filePath);
    void clear();

private:
    void createUI();
    void loadFile(const QString& filePath);

    enum class FileType {
        PNG,
        TIFF,
        SVG,
        STL,
        OBJ,
        PLY,
        GeoJSON,
        Shapefile,
        Unknown
    };

    FileType detectFileType(const QString& filePath) const;
    QWidget* createPreviewWidget(const QString& filePath, FileType type);

    QWidget* createImagePreview(const QString& filePath);
    QWidget* createSvgPreview(const QString& filePath);
    QWidget* create3DPreview(const QString& filePath);
    QWidget* createTextPreview(const QString& filePath);

    QTabWidget* tabWidget_;
};

} // namespace topo
