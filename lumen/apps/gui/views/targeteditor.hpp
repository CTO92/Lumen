#pragma once

/// @file targeteditor.hpp
/// @brief Target allocation editor widget

#include <QWidget>
#include <QTableWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>

#include "lumen/core/portfolio.hpp"

class ApplicationState;

/// Widget for editing target allocations
class TargetEditor : public QWidget {
    Q_OBJECT

public:
    explicit TargetEditor(ApplicationState* state, QWidget* parent = nullptr);

signals:
    void targetChanged();

private slots:
    void onPortfolioChanged();
    void onCellChanged(int row, int column);
    void normalizeTargets();
    void resetToEqual();
    void resetToCurrent();
    void clearTargets();

private:
    void setupUI();
    void setupConnections();
    void rebuildTable();
    void updateTotal();
    void applyTargetsToState();

    ApplicationState* state_;

    QTableWidget* tableWidget_;
    QPushButton* normalizeBtn_;
    QPushButton* equalBtn_;
    QPushButton* currentBtn_;
    QPushButton* clearBtn_;
    QLabel* totalLabel_;

    bool updatingTable_ = false;  // Prevent recursive updates
};
