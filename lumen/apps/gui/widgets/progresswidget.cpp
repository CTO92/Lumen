/// @file progresswidget.cpp
/// @brief Progress indicator widget implementation

#include "progresswidget.hpp"

#include <QHBoxLayout>

ProgressWidget::ProgressWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void ProgressWidget::setupUI() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    statusLabel_ = new QLabel(tr("Ready"), this);
    layout->addWidget(statusLabel_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    progressBar_->setFixedWidth(150);
    layout->addWidget(progressBar_);

    elapsedLabel_ = new QLabel("0:00", this);
    elapsedLabel_->setFixedWidth(40);
    layout->addWidget(elapsedLabel_);

    cancelBtn_ = new QPushButton(tr("Cancel"), this);
    cancelBtn_->setFlat(true);
    connect(cancelBtn_, &QPushButton::clicked, this, &ProgressWidget::cancelRequested);
    layout->addWidget(cancelBtn_);

    elapsedTimer_ = new QTimer(this);
    connect(elapsedTimer_, &QTimer::timeout, this, &ProgressWidget::updateElapsedTime);
}

void ProgressWidget::start() {
    elapsedSeconds_ = 0;
    elapsedLabel_->setText("0:00");
    progressBar_->setValue(0);
    statusLabel_->setText(tr("Starting..."));
    elapsedTimer_->start(1000);
    cancelBtn_->setEnabled(true);
}

void ProgressWidget::stop() {
    elapsedTimer_->stop();
    cancelBtn_->setEnabled(false);
}

void ProgressWidget::setProgress(int percent, const QString& status) {
    progressBar_->setValue(percent);
    statusLabel_->setText(status);
}

void ProgressWidget::setIndeterminate(bool indeterminate) {
    if (indeterminate) {
        progressBar_->setRange(0, 0);  // Indeterminate mode
    } else {
        progressBar_->setRange(0, 100);
    }
}

void ProgressWidget::updateElapsedTime() {
    elapsedSeconds_++;
    int minutes = elapsedSeconds_ / 60;
    int seconds = elapsedSeconds_ % 60;
    elapsedLabel_->setText(QString("%1:%2")
                               .arg(minutes)
                               .arg(seconds, 2, 10, QChar('0')));
}
