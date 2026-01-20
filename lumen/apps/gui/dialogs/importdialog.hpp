#pragma once

/// @file importdialog.hpp
/// @brief CSV import dialog

#include <QDialog>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

#include "lumen/core/portfolio.hpp"

/// Dialog for importing portfolio data from CSV files
class ImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportDialog(QWidget* parent = nullptr);

    /// Get the imported portfolio
    lumen::core::Portfolio importedPortfolio() const;

private slots:
    void browseFile();
    void parseCSV();
    void mapColumns();
    void previewData();
    void accept() override;

private:
    void setupUI();
    void detectDelimiter();
    void autoMapColumns();
    void updateColumnMappings();
    void showError(const QString& message);

    QLineEdit* filePathEdit_;
    QComboBox* delimiterCombo_;
    QCheckBox* hasHeaderCheck_;
    QSpinBox* skipRowsSpin_;

    // Column mapping
    QComboBox* tickerColumnCombo_;
    QComboBox* sharesColumnCombo_;
    QComboBox* priceColumnCombo_;
    QComboBox* costBasisColumnCombo_;
    QComboBox* assetClassColumnCombo_;

    QTableWidget* previewTable_;
    QLabel* statusLabel_;
    QLabel* errorLabel_;

    std::vector<std::vector<std::string>> rawData_;
    std::vector<std::string> headers_;
    lumen::core::Portfolio importedPortfolio_;
};
