#pragma once

/// @file positiondialog.hpp
/// @brief Dialog for adding/editing portfolio positions

#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QCheckBox>

#include "lumen/core/portfolio.hpp"

/// Dialog for adding or editing a portfolio position
class PositionDialog : public QDialog {
    Q_OBJECT

public:
    explicit PositionDialog(QWidget* parent = nullptr);

    /// Set the position for editing (empty for new position)
    void setPosition(const lumen::core::Position& position);

    /// Get the configured position
    lumen::core::Position position() const;

private slots:
    void validateTicker();
    void updateValue();
    void accept() override;

private:
    void setupUI();
    bool validate();

    QLineEdit* tickerEdit_;
    QDoubleSpinBox* sharesSpin_;
    QDoubleSpinBox* priceSpin_;
    QDoubleSpinBox* costBasisSpin_;
    QComboBox* assetClassCombo_;
    QDateEdit* purchaseDateEdit_;
    QCheckBox* fractionalCheck_;
    QLabel* valueLabel_;
    QLabel* gainLabel_;
    QLabel* errorLabel_;

    bool isEditing_ = false;
    QString originalTicker_;
};
