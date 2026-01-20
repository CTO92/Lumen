/// @file portfoliotableview.cpp
/// @brief Portfolio table view widget implementation

#include "portfoliotableview.hpp"
#include "../applicationstate.hpp"
#include "../models/portfoliomodel.hpp"

#include <QVBoxLayout>
#include <QHeaderView>

PortfolioTableView::PortfolioTableView(ApplicationState* state, QWidget* parent)
    : QWidget(parent)
    , state_(state)
{
    setupUI();
    setupConnections();
    onPortfolioChanged();
}

void PortfolioTableView::setupUI() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tableView_ = new QTableView(this);
    tableView_->setAlternatingRowColors(true);
    tableView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView_->setSortingEnabled(true);
    tableView_->setShowGrid(true);
    tableView_->verticalHeader()->setVisible(false);

    // Set up model
    model_ = new PortfolioModel(this);

    proxyModel_ = new QSortFilterProxyModel(this);
    proxyModel_->setSourceModel(model_);
    proxyModel_->setSortCaseSensitivity(Qt::CaseInsensitive);

    tableView_->setModel(proxyModel_);

    // Configure header
    auto* header = tableView_->horizontalHeader();
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);

    // Set initial column widths
    tableView_->setColumnWidth(PortfolioModel::Col_Ticker, 80);
    tableView_->setColumnWidth(PortfolioModel::Col_Shares, 80);
    tableView_->setColumnWidth(PortfolioModel::Col_Price, 90);
    tableView_->setColumnWidth(PortfolioModel::Col_Value, 100);
    tableView_->setColumnWidth(PortfolioModel::Col_CostBasis, 90);
    tableView_->setColumnWidth(PortfolioModel::Col_UnrealizedGain, 110);
    tableView_->setColumnWidth(PortfolioModel::Col_GainPercent, 80);
    tableView_->setColumnWidth(PortfolioModel::Col_Allocation, 90);

    layout->addWidget(tableView_);
}

void PortfolioTableView::setupConnections() {
    connect(state_, &ApplicationState::portfolioChanged,
            this, &PortfolioTableView::onPortfolioChanged);

    connect(tableView_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &PortfolioTableView::onSelectionChanged);

    connect(tableView_, &QTableView::doubleClicked,
            this, &PortfolioTableView::onDoubleClicked);
}

void PortfolioTableView::onPortfolioChanged() {
    model_->setPortfolio(state_->portfolio());

    // Sort by ticker by default
    proxyModel_->sort(PortfolioModel::Col_Ticker, Qt::AscendingOrder);
}

void PortfolioTableView::onSelectionChanged() {
    bool hasSelection = tableView_->selectionModel()->hasSelection();
    emit selectionChanged(hasSelection);
}

void PortfolioTableView::onDoubleClicked(const QModelIndex& index) {
    if (!index.isValid()) return;

    QModelIndex sourceIndex = proxyModel_->mapToSource(index);
    QString ticker = model_->tickerAt(sourceIndex.row());
    if (!ticker.isEmpty()) {
        emit positionDoubleClicked(ticker);
    }
}

QString PortfolioTableView::selectedTicker() const {
    QModelIndexList selected = tableView_->selectionModel()->selectedRows();
    if (selected.isEmpty()) return QString();

    QModelIndex sourceIndex = proxyModel_->mapToSource(selected.first());
    return model_->tickerAt(sourceIndex.row());
}

int PortfolioTableView::selectedRow() const {
    QModelIndexList selected = tableView_->selectionModel()->selectedRows();
    if (selected.isEmpty()) return -1;

    QModelIndex sourceIndex = proxyModel_->mapToSource(selected.first());
    return sourceIndex.row();
}

void PortfolioTableView::selectTicker(const QString& ticker) {
    int row = model_->rowForTicker(ticker);
    if (row >= 0) {
        QModelIndex sourceIndex = model_->index(row, 0);
        QModelIndex proxyIndex = proxyModel_->mapFromSource(sourceIndex);
        tableView_->selectRow(proxyIndex.row());
    }
}
