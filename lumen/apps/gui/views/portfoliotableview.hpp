#pragma once

/// @file portfoliotableview.hpp
/// @brief Portfolio table view widget

#include <QWidget>
#include <QTableView>
#include <QSortFilterProxyModel>

class ApplicationState;
class PortfolioModel;

/// Table view widget for displaying portfolio positions
class PortfolioTableView : public QWidget {
    Q_OBJECT

public:
    explicit PortfolioTableView(ApplicationState* state, QWidget* parent = nullptr);

    /// Get the currently selected ticker
    QString selectedTicker() const;

    /// Get selected row index (-1 if none)
    int selectedRow() const;

    /// Select a row by ticker
    void selectTicker(const QString& ticker);

signals:
    /// Emitted when selection changes
    void selectionChanged(bool hasSelection);

    /// Emitted when a position is double-clicked
    void positionDoubleClicked(const QString& ticker);

private slots:
    void onPortfolioChanged();
    void onSelectionChanged();
    void onDoubleClicked(const QModelIndex& index);

private:
    void setupUI();
    void setupConnections();

    ApplicationState* state_;
    QTableView* tableView_;
    PortfolioModel* model_;
    QSortFilterProxyModel* proxyModel_;
};
