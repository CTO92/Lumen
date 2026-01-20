#pragma once

/// @file portfoliomodel.hpp
/// @brief Qt model for portfolio data

#include <QAbstractTableModel>
#include <QColor>
#include "lumen/core/portfolio.hpp"

/// Qt table model for portfolio positions
class PortfolioModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        Col_Ticker = 0,
        Col_Shares,
        Col_Price,
        Col_Value,
        Col_CostBasis,
        Col_UnrealizedGain,
        Col_GainPercent,
        Col_Allocation,
        Col_AssetClass,
        Col_Count
    };

    explicit PortfolioModel(QObject* parent = nullptr);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole) override;

    // Portfolio management
    void setPortfolio(const lumen::core::Portfolio& portfolio);
    const lumen::core::Portfolio& portfolio() const { return portfolio_; }

    void addPosition(const lumen::core::Position& position);
    void removePosition(int row);
    void updatePosition(int row, const lumen::core::Position& position);
    void updatePrices(const std::map<std::string, double>& prices);

    lumen::core::Position positionAt(int row) const;
    QString tickerAt(int row) const;
    int rowForTicker(const QString& ticker) const;

signals:
    void portfolioChanged();
    void totalValueChanged(double value);

private:
    void rebuildTickerOrder();
    QString formatCurrency(double value) const;
    QString formatPercent(double value) const;
    QColor gainLossColor(double value) const;
    QString assetClassToString(lumen::core::AssetClass ac) const;

    lumen::core::Portfolio portfolio_;
    std::vector<std::string> tickerOrder_;  // For stable row ordering
};
