#pragma once

/// @file allocationchartview.hpp
/// @brief Allocation pie chart widget

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>

#include "lumen/core/portfolio.hpp"

/// Pie chart widget for displaying portfolio allocation
class AllocationChartView : public QWidget {
    Q_OBJECT

public:
    enum ViewMode {
        ByPosition,     // Individual securities
        ByAssetClass    // Grouped by asset class
    };

    explicit AllocationChartView(QWidget* parent = nullptr);

    void setPortfolio(const lumen::core::Portfolio& portfolio);
    void setTargetAllocation(const lumen::core::TargetAllocation& target);
    void setViewMode(ViewMode mode);
    void setShowTarget(bool show);

public slots:
    void updateChart();

private:
    void setupUI();
    void setupChart();
    void updateByPosition();
    void updateByAssetClass();
    void createLegend();
    QColor colorForIndex(int index) const;
    QColor colorForAssetClass(lumen::core::AssetClass ac) const;

    QChartView* chartView_;
    QChart* chart_;
    QPieSeries* currentSeries_;

    lumen::core::Portfolio portfolio_;
    lumen::core::TargetAllocation target_;
    ViewMode viewMode_ = ByPosition;
    bool showTarget_ = false;

    // Color palette for positions
    static const QList<QColor> positionColors_;
};
