/// @file resultspanel.cpp
/// @brief Optimization results panel implementation

#include "resultspanel.hpp"
#include "../applicationstate.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

ResultsPanel::ResultsPanel(ApplicationState* state, QWidget* parent)
    : QWidget(parent)
    , state_(state)
{
    setupUI();
    setupConnections();
}

void ResultsPanel::setupUI() {
    auto* layout = new QVBoxLayout(this);

    // Summary section
    auto* summaryGroup = new QGroupBox(tr("Summary"), this);
    auto* summaryLayout = new QFormLayout(summaryGroup);

    statusLabel_ = new QLabel(tr("-"), summaryGroup);
    statusLabel_->setStyleSheet("font-weight: bold;");
    summaryLayout->addRow(tr("Status:"), statusLabel_);

    solverLabel_ = new QLabel(tr("-"), summaryGroup);
    summaryLayout->addRow(tr("Solver:"), solverLabel_);

    timeLabel_ = new QLabel(tr("-"), summaryGroup);
    summaryLayout->addRow(tr("Time:"), timeLabel_);

    objectiveLabel_ = new QLabel(tr("-"), summaryGroup);
    summaryLayout->addRow(tr("Objective:"), objectiveLabel_);

    tradeCountLabel_ = new QLabel(tr("-"), summaryGroup);
    summaryLayout->addRow(tr("Trades:"), tradeCountLabel_);

    totalCostLabel_ = new QLabel(tr("-"), summaryGroup);
    summaryLayout->addRow(tr("Transaction Cost:"), totalCostLabel_);

    layout->addWidget(summaryGroup);

    // Splitter for trade table and rationale
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // Trade table
    tradeTable_ = new QTableWidget(splitter);
    tradeTable_->setColumnCount(5);
    tradeTable_->setHorizontalHeaderLabels({
        tr("Ticker"), tr("Action"), tr("Shares"), tr("Amount"), tr("Tax Lots")
    });
    tradeTable_->horizontalHeader()->setStretchLastSection(true);
    tradeTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tradeTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    tradeTable_->setAlternatingRowColors(true);
    tradeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splitter->addWidget(tradeTable_);

    // Rationale
    auto* rationaleGroup = new QGroupBox(tr("Trade Rationale"), splitter);
    auto* rationaleLayout = new QVBoxLayout(rationaleGroup);
    rationaleText_ = new QTextEdit(rationaleGroup);
    rationaleText_->setReadOnly(true);
    rationaleText_->setPlaceholderText(tr("Select a trade to see its rationale..."));
    rationaleLayout->addWidget(rationaleText_);
    splitter->addWidget(rationaleGroup);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter, 1);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    exportBtn_ = new QPushButton(tr("Export..."), this);
    exportBtn_->setEnabled(false);
    buttonLayout->addWidget(exportBtn_);

    clearBtn_ = new QPushButton(tr("Clear"), this);
    clearBtn_->setEnabled(false);
    buttonLayout->addWidget(clearBtn_);

    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
}

void ResultsPanel::setupConnections() {
    connect(state_, &ApplicationState::resultAvailable,
            this, &ResultsPanel::onResultAvailable);

    connect(tradeTable_, &QTableWidget::itemSelectionChanged,
            this, &ResultsPanel::onTradeSelectionChanged);

    connect(clearBtn_, &QPushButton::clicked, this, &ResultsPanel::clear);
}

void ResultsPanel::onResultAvailable(const lumen::core::SolverResult& result) {
    result_ = result;
    updateSummary();
    updateTradeTable();
    rationaleText_->clear();

    exportBtn_->setEnabled(true);
    clearBtn_->setEnabled(true);
}

void ResultsPanel::updateSummary() {
    if (result_.success) {
        statusLabel_->setText(tr("Optimal"));
        statusLabel_->setStyleSheet("font-weight: bold; color: green;");
    } else {
        statusLabel_->setText(QString::fromStdString(result_.status));
        statusLabel_->setStyleSheet("font-weight: bold; color: red;");
    }

    solverLabel_->setText(QString::fromStdString(result_.solver_used));
    timeLabel_->setText(tr("%1 ms").arg(result_.solve_time_ms));
    objectiveLabel_->setText(QString::number(result_.objective_value, 'f', 4));
    tradeCountLabel_->setText(QString::number(result_.trades.size()));
    totalCostLabel_->setText(QString("$%1").arg(result_.total_transaction_cost, 0, 'f', 2));
}

void ResultsPanel::updateTradeTable() {
    tradeTable_->setRowCount(0);
    tradeTable_->setRowCount(static_cast<int>(result_.trades.size()));

    for (size_t i = 0; i < result_.trades.size(); ++i) {
        const auto& trade = result_.trades[i];
        int row = static_cast<int>(i);

        // Ticker
        tradeTable_->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(trade.ticker)));

        // Action
        QString action;
        QColor actionColor;
        switch (trade.action) {
            case lumen::core::Trade::Action::BUY:
                action = tr("BUY");
                actionColor = QColor(0, 128, 0);
                break;
            case lumen::core::Trade::Action::SELL:
                action = tr("SELL");
                actionColor = QColor(192, 0, 0);
                break;
            case lumen::core::Trade::Action::HOLD:
                action = tr("HOLD");
                actionColor = QColor(128, 128, 128);
                break;
        }
        auto* actionItem = new QTableWidgetItem(action);
        actionItem->setForeground(actionColor);
        tradeTable_->setItem(row, 1, actionItem);

        // Shares
        auto* sharesItem = new QTableWidgetItem(
            QString::number(trade.shares, 'f', 4));
        sharesItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tradeTable_->setItem(row, 2, sharesItem);

        // Amount
        auto* amountItem = new QTableWidgetItem(
            QString("$%1").arg(trade.amount, 0, 'f', 2));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        tradeTable_->setItem(row, 3, amountItem);

        // Tax lots
        QString lots = trade.lot_ids.empty() ? "-" :
            QString::number(trade.lot_ids.size()) + " lots";
        tradeTable_->setItem(row, 4, new QTableWidgetItem(lots));
    }

    tradeTable_->resizeColumnsToContents();
}

void ResultsPanel::onTradeSelectionChanged() {
    int row = tradeTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(result_.trades.size())) {
        rationaleText_->clear();
        return;
    }

    const auto& trade = result_.trades[row];
    updateRationale();
}

void ResultsPanel::updateRationale() {
    int row = tradeTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(result_.trades.size())) {
        return;
    }

    const auto& trade = result_.trades[row];

    QString text;
    text += QString("<b>%1 - %2</b><br><br>")
        .arg(QString::fromStdString(trade.ticker))
        .arg(lumen::core::tradeActionToString(trade.action).c_str());

    text += QString("<b>Rationale:</b><br>%1<br><br>")
        .arg(QString::fromStdString(trade.rationale));

    text += QString("<b>Details:</b><br>");
    text += QString("Shares: %1<br>").arg(trade.shares, 0, 'f', 4);
    text += QString("Price: $%1<br>").arg(trade.price, 0, 'f', 2);
    text += QString("Amount: $%1<br>").arg(trade.amount, 0, 'f', 2);
    text += QString("Transaction Cost: $%1<br>").arg(trade.transaction_cost, 0, 'f', 2);

    if (!trade.lot_ids.empty()) {
        text += QString("<br><b>Tax Lots:</b><br>");
        for (const auto& lotId : trade.lot_ids) {
            text += QString("- %1<br>").arg(QString::fromStdString(lotId));
        }
    }

    rationaleText_->setHtml(text);

    emit tradeSelected(trade);
}

void ResultsPanel::clear() {
    result_ = lumen::core::SolverResult();

    statusLabel_->setText(tr("-"));
    statusLabel_->setStyleSheet("font-weight: bold;");
    solverLabel_->setText(tr("-"));
    timeLabel_->setText(tr("-"));
    objectiveLabel_->setText(tr("-"));
    tradeCountLabel_->setText(tr("-"));
    totalCostLabel_->setText(tr("-"));

    tradeTable_->setRowCount(0);
    rationaleText_->clear();

    exportBtn_->setEnabled(false);
    clearBtn_->setEnabled(false);

    state_->clearResult();
}

void ResultsPanel::exportToFile(const QString& filePath) {
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        // Export as CSV
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Export Error"),
                                 tr("Failed to open file for writing."));
            return;
        }

        QTextStream out(&file);
        out << "Ticker,Action,Shares,Price,Amount,Transaction Cost,Rationale\n";

        for (const auto& trade : result_.trades) {
            out << QString::fromStdString(trade.ticker) << ","
                << lumen::core::tradeActionToString(trade.action).c_str() << ","
                << trade.shares << ","
                << trade.price << ","
                << trade.amount << ","
                << trade.transaction_cost << ","
                << "\"" << QString::fromStdString(trade.rationale).replace("\"", "\"\"") << "\"\n";
        }

        file.close();

    } else {
        // Export as JSON
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, tr("Export Error"),
                                 tr("Failed to open file for writing."));
            return;
        }

        auto json = result_.toJSON();
        QJsonDocument doc = QJsonDocument::fromJson(
            QString::fromStdString(json.dump(2)).toUtf8());
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}
