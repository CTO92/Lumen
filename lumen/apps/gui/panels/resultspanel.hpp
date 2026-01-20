#pragma once

/// @file resultspanel.hpp
/// @brief Optimization results panel

#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>

#include "lumen/core/solver_dispatcher.hpp"

class ApplicationState;

/// Panel for displaying optimization results
class ResultsPanel : public QWidget {
    Q_OBJECT

public:
    explicit ResultsPanel(ApplicationState* state, QWidget* parent = nullptr);

    /// Export results to file
    void exportToFile(const QString& filePath);

signals:
    void tradeSelected(const lumen::core::Trade& trade);

private slots:
    void onResultAvailable(const lumen::core::SolverResult& result);
    void onTradeSelectionChanged();
    void clear();

private:
    void setupUI();
    void setupConnections();
    void updateSummary();
    void updateTradeTable();
    void updateRationale();

    ApplicationState* state_;

    // Summary section
    QLabel* statusLabel_;
    QLabel* objectiveLabel_;
    QLabel* solverLabel_;
    QLabel* timeLabel_;
    QLabel* totalCostLabel_;
    QLabel* tradeCountLabel_;

    // Trade table
    QTableWidget* tradeTable_;

    // Rationale display
    QTextEdit* rationaleText_;

    // Actions
    QPushButton* exportBtn_;
    QPushButton* clearBtn_;

    lumen::core::SolverResult result_;
};
