#pragma once

/**
 * @file GenerationProgressDialog.hpp
 * @brief Modal progress dialog for generation feedback
 *
 * Copyright (c) 2025 Matthew Block
 * Licensed under the MIT License.
 */

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>

namespace topo {

class GenerationProgressDialog : public QDialog {
    Q_OBJECT

public:
    explicit GenerationProgressDialog(QWidget* parent = nullptr);

public slots:
    void updateProgress(int percentage, const QString& message);
    void onGenerationComplete(const QStringList& outputFiles);
    void onGenerationFailed(const QString& errorMessage);

signals:
    void cancelRequested();

private:
    void createUI();

    QProgressBar* progressBar_;
    QLabel* statusLabel_;
    QPushButton* cancelButton_;
    QPushButton* closeButton_;

    bool isComplete_;
};

} // namespace topo
