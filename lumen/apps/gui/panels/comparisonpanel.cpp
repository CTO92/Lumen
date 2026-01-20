/// @file comparisonpanel.cpp
/// @brief Before/after comparison panel implementation

#include "comparisonpanel.hpp"
#include "../applicationstate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QtCharts/QPieSeries>
#include <QtCharts/QChart>

using namespace QtCharts;

ComparisonPanel::ComparisonPanel(ApplicationState* state, QWidget* parent)
    : QWidget(parent)
    , state_(state)
{
    setupUI();
    setupConnections();
}

void ComparisonPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // Charts side by side
    auto* chartsLayout = new QHBoxLayout();

    // Before chart
    auto* beforeGroup = new QGroupBox(tr("Before"), this);
    auto* beforeLayout = new QVBoxLayout(beforeGroup);
    beforeChart_ = new QChartView(beforeGroup);
    beforeChart_->setRenderHint(QPainter::Antialiasing);
    beforeChart_->setMinimumHeight(200);
    beforeLayout->addWidget(beforeChart_);
    chartsLayout->addWidget(beforeGroup);

    // After chart
    auto* afterGroup = new QGroupBox(tr("After"), this);
    auto* afterLayout = new QVBoxLayout(afterGroup);
    afterChart_ = new QChartView(afterGroup);
    afterChart_->setRenderHint(QPainter::Antialiasing);
    afterChart_->setMinimumHeight(200);
    afterLayout->addWidget(afterChart_);
    chartsLayout->addWidget(afterGroup);

    layout->addLayout(chartsLayout);

    // Metrics
    auto* metricsGroup = new QGroupBox(tr("Metrics"), this);
    auto* metricsLayout = new QFormLayout(metricsGroup);

    beforeDriftLabel_ = new QLabel("-", metricsGroup);
    metricsLayout->addRow(tr("Before Drift:"), beforeDriftLabel_);

    afterDriftLabel_ = new QLabel("-", metricsGroup);
    metricsLayout->addRow(tr("After Drift:"), afterDriftLabel_);

    improvementLabel_ = new QLabel("-", metricsGroup);
    improvementLabel_->setStyleSheet("font-weight: bold;");
    metricsLayout->addRow(tr("Improvement:"), improvementLabel_);

    layout->addWidget(metricsGroup);

    // Tax impact
    auto* taxGroup = new QGroupBox(tr("Tax Impact"), this);
    auto* taxLayout = new QFormLayout(taxGroup);

    shortTermGainsLabel_ = new QLabel("-", taxGroup);
    taxLayout->addRow(tr("Short-Term Gains:"), shortTermGainsLabel_);

    longTermGainsLabel_ = new QLabel("-", taxGroup);
    taxLayout->addRow(tr("Long-Term Gains:"), longTermGainsLabel_);

    totalGainsLabel_ = new QLabel("-", taxGroup);
    taxLayout->addRow(tr("Total Realized Gains:"), totalGainsLabel_);

    taxLiabilityLabel_ = new QLabel("-", taxGroup);
    taxLiabilityLabel_->setStyleSheet("font-weight: bold;");
    taxLayout->addRow(tr("Est. Tax Liability:"), taxLiabilityLabel_);

    layout->addWidget(taxGroup);

    layout->addStretch();
}

void ComparisonPanel::setupConnections() {
    connect(state_, &ApplicationState::resultAvailable,
            this, &ComparisonPanel::setResult);
}

void ComparisonPanel::setResult(const lumen::core::SolverResult& result) {
    result_ = result;
    updateComparison();
}

void ComparisonPanel::clear() {
    result_ = lumen::core::SolverResult();

    if (beforeChart_->chart()) {
        beforeChart_->chart()->removeAllSeries();
    }
    if (afterChart_->chart()) {
        afterChart_->chart()->removeAllSeries();
    }

    beforeDriftLabel_->setText("-");
    afterDriftLabel_->setText("-");
    improvementLabel_->setText("-");
    shortTermGainsLabel_->setText("-");
    longTermGainsLabel_->setText("-");
    totalGainsLabel_->setText("-");
    taxLiabilityLabel_->setText("-");
}

void ComparisonPanel::updateComparison() {
    updateCharts();
    updateMetrics();
}

void ComparisonPanel::updateCharts() {
    // Before chart - current allocation
    auto* beforeChartObj = new QChart();
    beforeChartObj->setAnimationOptions(QChart::SeriesAnimations);
    beforeChartObj->legend()->hide();

    auto* beforeSeries = new QPieSeries(beforeChartObj);
    auto currentAlloc = state_->portfolio().getAllocationMap();

    static const QList<QColor> colors = {
        QColor(0, 120, 212),
        QColor(16, 124, 16),
        QColor(232, 17, 35),
        QColor(136, 23, 152),
        QColor(255, 140, 0),
        QColor(0, 183, 195),
    };

    int colorIndex = 0;
    for (const auto& [ticker, alloc] : currentAlloc) {
        auto* slice = beforeSeries->append(QString::fromStdString(ticker), alloc * 100);
        slice->setColor(colors[colorIndex % colors.size()]);
        slice->setLabelVisible(alloc >= 0.05);
        colorIndex++;
    }
    beforeSeries->setHoleSize(0.3);
    beforeChartObj->addSeries(beforeSeries);
    beforeChart_->setChart(beforeChartObj);

    // After chart - final allocation
    auto* afterChartObj = new QChart();
    afterChartObj->setAnimationOptions(QChart::SeriesAnimations);
    afterChartObj->legend()->hide();

    auto* afterSeries = new QPieSeries(afterChartObj);

    colorIndex = 0;
    for (const auto& [ticker, alloc] : result_.final_allocation) {
        auto* slice = afterSeries->append(QString::fromStdString(ticker), alloc * 100);
        slice->setColor(colors[colorIndex % colors.size()]);
        slice->setLabelVisible(alloc >= 0.05);
        colorIndex++;
    }
    afterSeries->setHoleSize(0.3);
    afterChartObj->addSeries(afterSeries);
    afterChart_->setChart(afterChartObj);
}

void ComparisonPanel::updateMetrics() {
    const auto& portfolio = state_->portfolio();
    const auto& target = state_->targetAllocation();

    // Calculate before drift
    double beforeDrift = portfolio.calculateDrift(target);
    beforeDriftLabel_->setText(QString("%1%").arg(beforeDrift * 100, 0, 'f', 2));

    // Calculate after drift (from final allocation)
    double afterDrift = 0.0;
    auto targets = target.getAllTargets();
    for (const auto& t : targets) {
        auto it = result_.final_allocation.find(t.identifier);
        double actual = (it != result_.final_allocation.end()) ? it->second : 0.0;
        afterDrift += std::abs(actual - t.target_value);
    }
    afterDrift /= 2.0;  // Average

    afterDriftLabel_->setText(QString("%1%").arg(afterDrift * 100, 0, 'f', 2));

    // Improvement
    double improvement = beforeDrift - afterDrift;
    if (improvement > 0) {
        improvementLabel_->setText(QString("-%1% (better)").arg(improvement * 100, 0, 'f', 2));
        improvementLabel_->setStyleSheet("font-weight: bold; color: green;");
    } else if (improvement < 0) {
        improvementLabel_->setText(QString("+%1% (worse)").arg(-improvement * 100, 0, 'f', 2));
        improvementLabel_->setStyleSheet("font-weight: bold; color: red;");
    } else {
        improvementLabel_->setText(tr("No change"));
        improvementLabel_->setStyleSheet("font-weight: bold;");
    }

    // Tax impact estimation
    double shortTermGains = 0.0;
    double longTermGains = 0.0;

    for (const auto& trade : result_.trades) {
        if (trade.action == lumen::core::Trade::Action::SELL) {
            // Estimate gains based on the trade
            // This is a simplified calculation
            try {
                auto pos = portfolio.getPosition(trade.ticker);
                double gain = (trade.price - pos.cost_basis) * trade.shares;
                if (pos.isLongTerm()) {
                    longTermGains += gain;
                } else {
                    shortTermGains += gain;
                }
            } catch (...) {
                // Position not found
            }
        }
    }

    shortTermGainsLabel_->setText(QString("$%1").arg(shortTermGains, 0, 'f', 2));
    if (shortTermGains > 0) {
        shortTermGainsLabel_->setStyleSheet("color: red;");
    } else if (shortTermGains < 0) {
        shortTermGainsLabel_->setStyleSheet("color: green;");
    } else {
        shortTermGainsLabel_->setStyleSheet("");
    }

    longTermGainsLabel_->setText(QString("$%1").arg(longTermGains, 0, 'f', 2));
    if (longTermGains > 0) {
        longTermGainsLabel_->setStyleSheet("color: red;");
    } else if (longTermGains < 0) {
        longTermGainsLabel_->setStyleSheet("color: green;");
    } else {
        longTermGainsLabel_->setStyleSheet("");
    }

    double totalGains = shortTermGains + longTermGains;
    totalGainsLabel_->setText(QString("$%1").arg(totalGains, 0, 'f', 2));

    // Estimate tax liability (simplified: 37% short-term, 20% long-term)
    double taxLiability = 0.0;
    if (shortTermGains > 0) taxLiability += shortTermGains * 0.37;
    if (longTermGains > 0) taxLiability += longTermGains * 0.20;

    taxLiabilityLabel_->setText(QString("$%1").arg(taxLiability, 0, 'f', 2));
}
