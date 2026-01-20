/// @file allocationchartview.cpp
/// @brief Allocation pie chart widget implementation

#include "allocationchartview.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>

using namespace QtCharts;

const QList<QColor> AllocationChartView::positionColors_ = {
    QColor(0, 120, 212),    // Blue
    QColor(16, 124, 16),    // Green
    QColor(232, 17, 35),    // Red
    QColor(136, 23, 152),   // Purple
    QColor(255, 140, 0),    // Orange
    QColor(0, 183, 195),    // Teal
    QColor(255, 185, 0),    // Yellow
    QColor(107, 105, 214),  // Indigo
    QColor(255, 105, 180),  // Pink
    QColor(128, 128, 128),  // Gray
};

AllocationChartView::AllocationChartView(QWidget* parent)
    : QWidget(parent)
    , chartView_(nullptr)
    , chart_(nullptr)
    , currentSeries_(nullptr)
{
    setupUI();
    setupChart();
}

void AllocationChartView::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);

    // Controls bar
    auto* controlsLayout = new QHBoxLayout();

    auto* viewModeCombo = new QComboBox(this);
    viewModeCombo->addItem(tr("By Position"), static_cast<int>(ByPosition));
    viewModeCombo->addItem(tr("By Asset Class"), static_cast<int>(ByAssetClass));
    connect(viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, viewModeCombo](int index) {
        setViewMode(static_cast<ViewMode>(viewModeCombo->itemData(index).toInt()));
    });

    auto* showTargetCheck = new QCheckBox(tr("Show Target"), this);
    connect(showTargetCheck, &QCheckBox::toggled, this, &AllocationChartView::setShowTarget);

    controlsLayout->addWidget(new QLabel(tr("View:"), this));
    controlsLayout->addWidget(viewModeCombo);
    controlsLayout->addStretch();
    controlsLayout->addWidget(showTargetCheck);

    layout->addLayout(controlsLayout);

    // Chart
    chartView_ = new QChartView(this);
    chartView_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView_, 1);
}

void AllocationChartView::setupChart() {
    chart_ = new QChart();
    chart_->setAnimationOptions(QChart::SeriesAnimations);
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignRight);
    chart_->setTitle(tr("Portfolio Allocation"));

    chartView_->setChart(chart_);
}

void AllocationChartView::setPortfolio(const lumen::core::Portfolio& portfolio) {
    portfolio_ = portfolio;
    updateChart();
}

void AllocationChartView::setTargetAllocation(const lumen::core::TargetAllocation& target) {
    target_ = target;
    if (showTarget_) {
        updateChart();
    }
}

void AllocationChartView::setViewMode(ViewMode mode) {
    if (viewMode_ != mode) {
        viewMode_ = mode;
        updateChart();
    }
}

void AllocationChartView::setShowTarget(bool show) {
    if (showTarget_ != show) {
        showTarget_ = show;
        updateChart();
    }
}

void AllocationChartView::updateChart() {
    // Remove old series
    if (currentSeries_) {
        chart_->removeSeries(currentSeries_);
        currentSeries_ = nullptr;
    }

    if (portfolio_.getPositionCount() == 0) {
        chart_->setTitle(tr("No positions in portfolio"));
        return;
    }

    if (viewMode_ == ByPosition) {
        updateByPosition();
    } else {
        updateByAssetClass();
    }
}

void AllocationChartView::updateByPosition() {
    currentSeries_ = new QPieSeries(this);

    auto allocations = portfolio_.getAllocationMap();
    int colorIndex = 0;

    for (const auto& [ticker, allocation] : allocations) {
        auto* slice = currentSeries_->append(
            QString::fromStdString(ticker),
            allocation * 100.0
        );
        slice->setColor(colorForIndex(colorIndex++));
        slice->setLabelVisible(allocation >= 0.05);  // Show label if >= 5%
        slice->setLabel(QString("%1\n%2%")
                            .arg(QString::fromStdString(ticker))
                            .arg(allocation * 100.0, 0, 'f', 1));
    }

    currentSeries_->setHoleSize(0.3);  // Donut style

    chart_->addSeries(currentSeries_);
    chart_->setTitle(tr("Allocation by Position"));

    // Make slices respond to hover
    connect(currentSeries_, &QPieSeries::hovered,
            this, [](QPieSlice* slice, bool state) {
        slice->setExploded(state);
        slice->setLabelVisible(state || slice->percentage() >= 0.05);
    });
}

void AllocationChartView::updateByAssetClass() {
    currentSeries_ = new QPieSeries(this);

    auto exposures = portfolio_.getAssetClassExposure();

    for (const auto& [assetClass, allocation] : exposures) {
        if (allocation <= 0) continue;

        QString label;
        switch (assetClass) {
            case lumen::core::AssetClass::STOCKS: label = tr("Stocks"); break;
            case lumen::core::AssetClass::BONDS: label = tr("Bonds"); break;
            case lumen::core::AssetClass::CASH: label = tr("Cash"); break;
            case lumen::core::AssetClass::COMMODITIES: label = tr("Commodities"); break;
            case lumen::core::AssetClass::REAL_ESTATE: label = tr("Real Estate"); break;
            case lumen::core::AssetClass::CRYPTO: label = tr("Crypto"); break;
            default: label = tr("Other"); break;
        }

        auto* slice = currentSeries_->append(label, allocation * 100.0);
        slice->setColor(colorForAssetClass(assetClass));
        slice->setLabelVisible(allocation >= 0.05);
        slice->setLabel(QString("%1\n%2%")
                            .arg(label)
                            .arg(allocation * 100.0, 0, 'f', 1));
    }

    currentSeries_->setHoleSize(0.3);

    chart_->addSeries(currentSeries_);
    chart_->setTitle(tr("Allocation by Asset Class"));

    connect(currentSeries_, &QPieSeries::hovered,
            this, [](QPieSlice* slice, bool state) {
        slice->setExploded(state);
        slice->setLabelVisible(state || slice->percentage() >= 0.05);
    });
}

QColor AllocationChartView::colorForIndex(int index) const {
    return positionColors_[index % positionColors_.size()];
}

QColor AllocationChartView::colorForAssetClass(lumen::core::AssetClass ac) const {
    switch (ac) {
        case lumen::core::AssetClass::STOCKS: return QColor(0, 120, 212);
        case lumen::core::AssetClass::BONDS: return QColor(16, 124, 16);
        case lumen::core::AssetClass::CASH: return QColor(128, 128, 128);
        case lumen::core::AssetClass::COMMODITIES: return QColor(255, 140, 0);
        case lumen::core::AssetClass::REAL_ESTATE: return QColor(136, 23, 152);
        case lumen::core::AssetClass::CRYPTO: return QColor(255, 185, 0);
        default: return QColor(100, 100, 100);
    }
}
