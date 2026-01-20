/// @file portfoliomodel.cpp
/// @brief Qt model for portfolio data implementation

#include "portfoliomodel.hpp"

#include <QLocale>
#include <algorithm>

PortfolioModel::PortfolioModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int PortfolioModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(tickerOrder_.size());
}

int PortfolioModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return Col_Count;
}

QVariant PortfolioModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(tickerOrder_.size())) {
        return QVariant();
    }

    const std::string& ticker = tickerOrder_[index.row()];

    try {
        const auto& pos = portfolio_.getPosition(ticker);

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (index.column()) {
                case Col_Ticker:
                    return QString::fromStdString(pos.ticker);
                case Col_Shares:
                    return QString::number(pos.shares, 'f', 4);
                case Col_Price:
                    return formatCurrency(pos.current_price);
                case Col_Value:
                    return formatCurrency(pos.getCurrentValue());
                case Col_CostBasis:
                    return formatCurrency(pos.cost_basis);
                case Col_UnrealizedGain:
                    return formatCurrency(pos.getUnrealizedGain());
                case Col_GainPercent:
                    return formatPercent(pos.getUnrealizedGainPercent());
                case Col_Allocation:
                    return formatPercent(portfolio_.getAllocationPercent(ticker));
                case Col_AssetClass:
                    return assetClassToString(pos.asset_class);
            }
        } else if (role == Qt::ForegroundRole) {
            if (index.column() == Col_UnrealizedGain || index.column() == Col_GainPercent) {
                return gainLossColor(pos.getUnrealizedGain());
            }
        } else if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
                case Col_Ticker:
                case Col_AssetClass:
                    return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
                default:
                    return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
            }
        } else if (role == Qt::ToolTipRole) {
            if (index.column() == Col_Ticker) {
                QString tooltip = QString("Ticker: %1\nShares: %2\nValue: %3\nCost Basis: %4")
                    .arg(QString::fromStdString(pos.ticker))
                    .arg(pos.shares, 0, 'f', 4)
                    .arg(formatCurrency(pos.getCurrentValue()))
                    .arg(formatCurrency(pos.getCostBasisTotal()));
                return tooltip;
            }
        }
    } catch (const std::exception&) {
        return QVariant();
    }

    return QVariant();
}

QVariant PortfolioModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
        case Col_Ticker: return tr("Ticker");
        case Col_Shares: return tr("Shares");
        case Col_Price: return tr("Price");
        case Col_Value: return tr("Value");
        case Col_CostBasis: return tr("Cost Basis");
        case Col_UnrealizedGain: return tr("Unrealized G/L");
        case Col_GainPercent: return tr("G/L %");
        case Col_Allocation: return tr("Allocation");
        case Col_AssetClass: return tr("Asset Class");
    }

    return QVariant();
}

Qt::ItemFlags PortfolioModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Allow editing shares and price
    if (index.column() == Col_Shares || index.column() == Col_Price) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

bool PortfolioModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) {
        return false;
    }

    if (index.row() >= static_cast<int>(tickerOrder_.size())) {
        return false;
    }

    const std::string& ticker = tickerOrder_[index.row()];
    bool ok = false;
    double newValue = value.toDouble(&ok);

    if (!ok) return false;

    try {
        if (index.column() == Col_Shares) {
            portfolio_.updatePosition(ticker, newValue);
            emit dataChanged(index, index, {Qt::DisplayRole});
            emit portfolioChanged();
            emit totalValueChanged(portfolio_.getTotalValue());
            return true;
        } else if (index.column() == Col_Price) {
            portfolio_.updatePrice(ticker, newValue);
            emit dataChanged(index, index, {Qt::DisplayRole});
            // Also update value, gain columns
            QModelIndex valueIdx = createIndex(index.row(), Col_Value);
            QModelIndex gainIdx = createIndex(index.row(), Col_GainPercent);
            emit dataChanged(valueIdx, gainIdx, {Qt::DisplayRole});
            emit totalValueChanged(portfolio_.getTotalValue());
            return true;
        }
    } catch (const std::exception&) {
        return false;
    }

    return false;
}

void PortfolioModel::setPortfolio(const lumen::core::Portfolio& portfolio) {
    beginResetModel();
    portfolio_ = portfolio;
    rebuildTickerOrder();
    endResetModel();

    emit portfolioChanged();
    emit totalValueChanged(portfolio_.getTotalValue());
}

void PortfolioModel::addPosition(const lumen::core::Position& position) {
    // Check if ticker already exists
    auto it = std::find(tickerOrder_.begin(), tickerOrder_.end(), position.ticker);
    if (it != tickerOrder_.end()) {
        // Update existing
        int row = static_cast<int>(std::distance(tickerOrder_.begin(), it));
        portfolio_.removePosition(position.ticker);
        portfolio_.addPosition(position);
        emit dataChanged(index(row, 0), index(row, Col_Count - 1));
    } else {
        // Add new
        int newRow = static_cast<int>(tickerOrder_.size());
        beginInsertRows(QModelIndex(), newRow, newRow);
        portfolio_.addPosition(position);
        tickerOrder_.push_back(position.ticker);
        endInsertRows();
    }

    emit portfolioChanged();
    emit totalValueChanged(portfolio_.getTotalValue());
}

void PortfolioModel::removePosition(int row) {
    if (row < 0 || row >= static_cast<int>(tickerOrder_.size())) {
        return;
    }

    beginRemoveRows(QModelIndex(), row, row);
    std::string ticker = tickerOrder_[row];
    portfolio_.removePosition(ticker);
    tickerOrder_.erase(tickerOrder_.begin() + row);
    endRemoveRows();

    emit portfolioChanged();
    emit totalValueChanged(portfolio_.getTotalValue());
}

void PortfolioModel::updatePosition(int row, const lumen::core::Position& position) {
    if (row < 0 || row >= static_cast<int>(tickerOrder_.size())) {
        return;
    }

    std::string oldTicker = tickerOrder_[row];

    // Remove old, add new
    portfolio_.removePosition(oldTicker);
    portfolio_.addPosition(position);
    tickerOrder_[row] = position.ticker;

    emit dataChanged(index(row, 0), index(row, Col_Count - 1));
    emit portfolioChanged();
    emit totalValueChanged(portfolio_.getTotalValue());
}

void PortfolioModel::updatePrices(const std::map<std::string, double>& prices) {
    portfolio_.updateAllPrices(prices);

    // Update all rows (value and gain columns)
    if (!tickerOrder_.empty()) {
        emit dataChanged(index(0, Col_Value),
                         index(static_cast<int>(tickerOrder_.size()) - 1, Col_GainPercent));
    }
    emit totalValueChanged(portfolio_.getTotalValue());
}

lumen::core::Position PortfolioModel::positionAt(int row) const {
    if (row < 0 || row >= static_cast<int>(tickerOrder_.size())) {
        return lumen::core::Position();
    }
    return portfolio_.getPosition(tickerOrder_[row]);
}

QString PortfolioModel::tickerAt(int row) const {
    if (row < 0 || row >= static_cast<int>(tickerOrder_.size())) {
        return QString();
    }
    return QString::fromStdString(tickerOrder_[row]);
}

int PortfolioModel::rowForTicker(const QString& ticker) const {
    auto it = std::find(tickerOrder_.begin(), tickerOrder_.end(), ticker.toStdString());
    if (it != tickerOrder_.end()) {
        return static_cast<int>(std::distance(tickerOrder_.begin(), it));
    }
    return -1;
}

void PortfolioModel::rebuildTickerOrder() {
    tickerOrder_.clear();
    auto tickers = portfolio_.getAllTickers();
    tickerOrder_.reserve(tickers.size());
    for (const auto& ticker : tickers) {
        tickerOrder_.push_back(ticker);
    }
    // Sort alphabetically
    std::sort(tickerOrder_.begin(), tickerOrder_.end());
}

QString PortfolioModel::formatCurrency(double value) const {
    QLocale locale;
    return locale.toCurrencyString(value, "$", 2);
}

QString PortfolioModel::formatPercent(double value) const {
    return QString::number(value * 100.0, 'f', 2) + "%";
}

QColor PortfolioModel::gainLossColor(double value) const {
    if (value > 0) {
        return QColor(0, 128, 0);  // Green for gains
    } else if (value < 0) {
        return QColor(192, 0, 0);  // Red for losses
    }
    return QColor();  // Default color
}

QString PortfolioModel::assetClassToString(lumen::core::AssetClass ac) const {
    switch (ac) {
        case lumen::core::AssetClass::STOCKS: return tr("Stocks");
        case lumen::core::AssetClass::BONDS: return tr("Bonds");
        case lumen::core::AssetClass::CASH: return tr("Cash");
        case lumen::core::AssetClass::COMMODITIES: return tr("Commodities");
        case lumen::core::AssetClass::REAL_ESTATE: return tr("Real Estate");
        case lumen::core::AssetClass::CRYPTO: return tr("Crypto");
        case lumen::core::AssetClass::OTHER: return tr("Other");
        default: return tr("Unknown");
    }
}
