#pragma once

/// @file constraintpanel.hpp
/// @brief Constraint configuration panel

#include <QWidget>
#include <QListWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>

#include "lumen/core/constraint.hpp"

class ApplicationState;

/// Panel for configuring portfolio constraints
class ConstraintPanel : public QWidget {
    Q_OBJECT

public:
    explicit ConstraintPanel(ApplicationState* state, QWidget* parent = nullptr);

signals:
    void constraintsChanged();

private slots:
    void onStateConstraintsChanged();
    void onPresetChanged();
    void addCustomConstraint();
    void removeConstraint();
    void applyConstraints();

private:
    void setupUI();
    void setupConnections();
    void updateConstraintList();

    ApplicationState* state_;

    // Preset constraints
    QGroupBox* presetsGroup_;
    QCheckBox* noShortSellingCheck_;
    QCheckBox* respectBandsCheck_;
    QDoubleSpinBox* minPositionSpin_;
    QDoubleSpinBox* maxPositionSpin_;
    QDoubleSpinBox* maxTurnoverSpin_;
    QCheckBox* taxAwareCheck_;

    // Custom constraints
    QListWidget* constraintList_;
    QPushButton* addBtn_;
    QPushButton* removeBtn_;

    // Apply button
    QPushButton* applyBtn_;

    bool updatingUI_ = false;
};
