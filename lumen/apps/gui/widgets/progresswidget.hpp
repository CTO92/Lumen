#pragma once

/// @file progresswidget.hpp
/// @brief Progress indicator widget

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

/// Widget for showing optimization progress
class ProgressWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProgressWidget(QWidget* parent = nullptr);

    /// Start the progress indicator
    void start();

    /// Stop the progress indicator
    void stop();

    /// Set the current progress
    void setProgress(int percent, const QString& status);

    /// Set indeterminate mode (busy indicator)
    void setIndeterminate(bool indeterminate);

signals:
    /// Emitted when cancel is clicked
    void cancelRequested();

private:
    void setupUI();
    void updateElapsedTime();

    QProgressBar* progressBar_;
    QLabel* statusLabel_;
    QLabel* elapsedLabel_;
    QPushButton* cancelBtn_;
    QTimer* elapsedTimer_;
    int elapsedSeconds_ = 0;
};
