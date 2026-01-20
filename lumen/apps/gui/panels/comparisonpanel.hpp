#pragma once

/// @file comparisonpanel.hpp
/// @brief Before/after comparison panel

#include <QWidget>
#include <QLabel>
#include <QtCharts/QChartView>

#include "lumen/core/portfolio.hpp"
#include "lumen/core/solver_dispatcher.hpp"

class ApplicationState;

/// Panel for showing before/after comparison of optimization
class ComparisonPanel : public QWidget {
    Q_OBJECT

public:
    explicit ComparisonPanel(ApplicationState* state, QWidget* parent = nullptr);

    void setResult(const lumen::core::SolverResult& result);
    void clear();

private:
    void setupUI();
    void setupConnections();
    void updateComparison();
    void updateCharts();
    void updateMetrics();

    ApplicationState* state_;

    // Side-by-side charts
    QtCharts::QChartView* beforeChart_;
    QtCharts::QChartView* afterChart_;

    // Metrics
    QLabel* beforeDriftLabel_;
    QLabel* afterDriftLabel_;
    QLabel* improvementLabel_;

    // Tax impact
    QLabel* shortTermGainsLabel_;
    QLabel* longTermGainsLabel_;
    QLabel* totalGainsLabel_;
    QLabel* taxLiabilityLabel_;

    lumen::core::SolverResult result_;
};
