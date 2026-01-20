/// @file targeteditor.cpp
/// @brief Target allocation editor widget implementation

#include "targeteditor.hpp"
#include "../applicationstate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDoubleSpinBox>

TargetEditor::TargetEditor(ApplicationState* state, QWidget* parent)
    : QWidget(parent)
    , state_(state)
{
    setupUI();
    setupConnections();
    rebuildTable();
}

void TargetEditor::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // Table
    tableWidget_ = new QTableWidget(this);
    tableWidget_->setColumnCount(3);
    tableWidget_->setHorizontalHeaderLabels({tr("Ticker"), tr("Current %"), tr("Target %")});
    tableWidget_->horizontalHeader()->setStretchLastSection(true);
    tableWidget_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableWidget_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget_->setAlternatingRowColors(true);
    layout->addWidget(tableWidget_);

    // Total display
    totalLabel_ = new QLabel(tr("Total: 0.00%"), this);
    totalLabel_->setAlignment(Qt::AlignRight);
    layout->addWidget(totalLabel_);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    normalizeBtn_ = new QPushButton(tr("Normalize"), this);
    normalizeBtn_->setToolTip(tr("Adjust targets to sum to 100%"));
    buttonLayout->addWidget(normalizeBtn_);

    equalBtn_ = new QPushButton(tr("Equal"), this);
    equalBtn_->setToolTip(tr("Set equal allocation to all positions"));
    buttonLayout->addWidget(equalBtn_);

    currentBtn_ = new QPushButton(tr("Current"), this);
    currentBtn_->setToolTip(tr("Set targets to current allocation"));
    buttonLayout->addWidget(currentBtn_);

    clearBtn_ = new QPushButton(tr("Clear"), this);
    clearBtn_->setToolTip(tr("Clear all targets"));
    buttonLayout->addWidget(clearBtn_);

    layout->addLayout(buttonLayout);
}

void TargetEditor::setupConnections() {
    connect(state_, &ApplicationState::portfolioChanged,
            this, &TargetEditor::onPortfolioChanged);

    connect(tableWidget_, &QTableWidget::cellChanged,
            this, &TargetEditor::onCellChanged);

    connect(normalizeBtn_, &QPushButton::clicked,
            this, &TargetEditor::normalizeTargets);
    connect(equalBtn_, &QPushButton::clicked,
            this, &TargetEditor::resetToEqual);
    connect(currentBtn_, &QPushButton::clicked,
            this, &TargetEditor::resetToCurrent);
    connect(clearBtn_, &QPushButton::clicked,
            this, &TargetEditor::clearTargets);
}

void TargetEditor::onPortfolioChanged() {
    rebuildTable();
}

void TargetEditor::rebuildTable() {
    updatingTable_ = true;

    tableWidget_->setRowCount(0);

    if (!state_->hasPortfolio()) {
        updatingTable_ = false;
        updateTotal();
        return;
    }

    auto tickers = state_->portfolio().getAllTickers();
    auto allocations = state_->portfolio().getAllocationMap();
    const auto& target = state_->targetAllocation();

    tableWidget_->setRowCount(static_cast<int>(tickers.size()));

    for (int row = 0; row < static_cast<int>(tickers.size()); ++row) {
        const std::string& ticker = tickers[row];
        double currentAlloc = 0.0;
        auto it = allocations.find(ticker);
        if (it != allocations.end()) {
            currentAlloc = it->second;
        }
        double targetAlloc = target.getTarget(ticker);

        // Ticker (read-only)
        auto* tickerItem = new QTableWidgetItem(QString::fromStdString(ticker));
        tickerItem->setFlags(tickerItem->flags() & ~Qt::ItemIsEditable);
        tableWidget_->setItem(row, 0, tickerItem);

        // Current allocation (read-only)
        auto* currentItem = new QTableWidgetItem(
            QString::number(currentAlloc * 100.0, 'f', 2));
        currentItem->setFlags(currentItem->flags() & ~Qt::ItemIsEditable);
        currentItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tableWidget_->setItem(row, 1, currentItem);

        // Target allocation (editable)
        auto* targetItem = new QTableWidgetItem(
            QString::number(targetAlloc * 100.0, 'f', 2));
        targetItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tableWidget_->setItem(row, 2, targetItem);
    }

    updatingTable_ = false;
    updateTotal();
}

void TargetEditor::onCellChanged(int row, int column) {
    if (updatingTable_ || column != 2) return;  // Only respond to target column changes

    QTableWidgetItem* item = tableWidget_->item(row, column);
    if (!item) return;

    bool ok;
    double value = item->text().toDouble(&ok);

    if (!ok || value < 0 || value > 100) {
        // Reset to previous value
        QTableWidgetItem* tickerItem = tableWidget_->item(row, 0);
        if (tickerItem) {
            double target = state_->targetAllocation().getTarget(tickerItem->text().toStdString());
            updatingTable_ = true;
            item->setText(QString::number(target * 100.0, 'f', 2));
            updatingTable_ = false;
        }
        return;
    }

    applyTargetsToState();
    updateTotal();
}

void TargetEditor::updateTotal() {
    double total = 0.0;

    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget_->item(row, 2);
        if (item) {
            total += item->text().toDouble();
        }
    }

    totalLabel_->setText(tr("Total: %1%").arg(total, 0, 'f', 2));

    // Color based on whether it sums to 100%
    if (std::abs(total - 100.0) < 0.01) {
        totalLabel_->setStyleSheet("color: green;");
    } else {
        totalLabel_->setStyleSheet("color: red;");
    }
}

void TargetEditor::applyTargetsToState() {
    lumen::core::TargetAllocation newTarget(lumen::core::AllocationMode::PERCENTAGE);

    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        QTableWidgetItem* tickerItem = tableWidget_->item(row, 0);
        QTableWidgetItem* targetItem = tableWidget_->item(row, 2);

        if (tickerItem && targetItem) {
            std::string ticker = tickerItem->text().toStdString();
            double value = targetItem->text().toDouble() / 100.0;
            if (value > 0) {
                newTarget.setTarget(ticker, value);
            }
        }
    }

    state_->setTargetAllocation(newTarget);
    emit targetChanged();
}

void TargetEditor::normalizeTargets() {
    double total = 0.0;

    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget_->item(row, 2);
        if (item) {
            total += item->text().toDouble();
        }
    }

    if (total <= 0) return;

    updatingTable_ = true;

    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget_->item(row, 2);
        if (item) {
            double value = item->text().toDouble();
            double normalized = (value / total) * 100.0;
            item->setText(QString::number(normalized, 'f', 2));
        }
    }

    updatingTable_ = false;
    applyTargetsToState();
    updateTotal();
}

void TargetEditor::resetToEqual() {
    int count = tableWidget_->rowCount();
    if (count == 0) return;

    double equalValue = 100.0 / count;

    updatingTable_ = true;

    for (int row = 0; row < count; ++row) {
        QTableWidgetItem* item = tableWidget_->item(row, 2);
        if (item) {
            item->setText(QString::number(equalValue, 'f', 2));
        }
    }

    updatingTable_ = false;
    applyTargetsToState();
    updateTotal();
}

void TargetEditor::resetToCurrent() {
    updatingTable_ = true;

    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        QTableWidgetItem* currentItem = tableWidget_->item(row, 1);
        QTableWidgetItem* targetItem = tableWidget_->item(row, 2);

        if (currentItem && targetItem) {
            targetItem->setText(currentItem->text());
        }
    }

    updatingTable_ = false;
    applyTargetsToState();
    updateTotal();
}

void TargetEditor::clearTargets() {
    updatingTable_ = true;

    for (int row = 0; row < tableWidget_->rowCount(); ++row) {
        QTableWidgetItem* item = tableWidget_->item(row, 2);
        if (item) {
            item->setText("0.00");
        }
    }

    updatingTable_ = false;
    applyTargetsToState();
    updateTotal();
}
