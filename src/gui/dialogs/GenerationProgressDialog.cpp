/**
 * @file GenerationProgressDialog.cpp
 * @brief Implementation of generation progress dialog
 */

#include "GenerationProgressDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFileInfo>

namespace topo {

GenerationProgressDialog::GenerationProgressDialog(QWidget* parent)
    : QDialog(parent), isComplete_(false) {
    setWindowTitle("Generating...");
    setModal(true);
    setMinimumWidth(400);

    createUI();
}

void GenerationProgressDialog::createUI() {
    auto* layout = new QVBoxLayout(this);

    // Status label
    statusLabel_ = new QLabel("Initializing...", this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    layout->addWidget(progressBar_);

    layout->addSpacing(10);

    // Button layout
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    cancelButton_ = new QPushButton("Cancel", this);
    connect(cancelButton_, &QPushButton::clicked, this, [this]() {
        auto result = QMessageBox::question(
            this,
            "Cancel Generation",
            "Are you sure you want to cancel the generation?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (result == QMessageBox::Yes) {
            emit cancelRequested();
            reject();
        }
    });
    buttonLayout->addWidget(cancelButton_);

    closeButton_ = new QPushButton("Close", this);
    closeButton_->setEnabled(false);
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton_);

    layout->addLayout(buttonLayout);
}

void GenerationProgressDialog::updateProgress(int percentage, const QString& message) {
    progressBar_->setValue(percentage);
    statusLabel_->setText(message);

    // Allow UI to update
    QCoreApplication::processEvents();
}

void GenerationProgressDialog::onGenerationComplete(const QStringList& outputFiles) {
    isComplete_ = true;
    progressBar_->setValue(100);

    QString message = QString("Generation complete!\n\nGenerated %1 file(s):\n").arg(outputFiles.size());

    // Show first few files
    int count = qMin(5, outputFiles.size());
    for (int i = 0; i < count; ++i) {
        QFileInfo info(outputFiles[i]);
        message += QString("  • %1\n").arg(info.fileName());
    }

    if (outputFiles.size() > 5) {
        message += QString("  ... and %1 more").arg(outputFiles.size() - 5);
    }

    statusLabel_->setText(message);

    // Enable close button, disable cancel
    cancelButton_->setEnabled(false);
    closeButton_->setEnabled(true);
    closeButton_->setDefault(true);
    closeButton_->setFocus();

    setWindowTitle("Generation Complete");
}

void GenerationProgressDialog::onGenerationFailed(const QString& errorMessage) {
    isComplete_ = true;
    progressBar_->setValue(0);
    progressBar_->setStyleSheet("QProgressBar::chunk { background-color: #d32f2f; }");

    statusLabel_->setText(QString("Generation failed:\n\n%1").arg(errorMessage));

    // Enable close button, disable cancel
    cancelButton_->setEnabled(false);
    closeButton_->setEnabled(true);
    closeButton_->setDefault(true);
    closeButton_->setFocus();

    setWindowTitle("Generation Failed");

    // Also show error in message box
    QMessageBox::critical(this, "Generation Error", errorMessage);
}

} // namespace topo
