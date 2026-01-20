#pragma once

/// @file applicationstate.hpp
/// @brief Centralized application state management

#include <QObject>
#include <QString>
#include <memory>

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"

/// Centralized application state for the GUI
///
/// This class holds all application state including the current portfolio,
/// target allocations, constraints, and optimization results. It emits
/// signals when state changes to update the UI.
class ApplicationState : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY unsavedChangesChanged)
    Q_PROPERTY(bool optimizing READ isOptimizing NOTIFY optimizingChanged)
    Q_PROPERTY(bool hasResult READ hasResult NOTIFY resultAvailable)

public:
    explicit ApplicationState(QObject* parent = nullptr);
    ~ApplicationState() override;

    // Portfolio access
    lumen::core::Portfolio& portfolio();
    const lumen::core::Portfolio& portfolio() const;
    void setPortfolio(const lumen::core::Portfolio& portfolio);
    void clearPortfolio();

    // Target allocation
    lumen::core::TargetAllocation& targetAllocation();
    const lumen::core::TargetAllocation& targetAllocation() const;
    void setTargetAllocation(const lumen::core::TargetAllocation& target);

    // Constraints
    lumen::core::ConstraintSet& constraints();
    const lumen::core::ConstraintSet& constraints() const;
    void setConstraints(const lumen::core::ConstraintSet& constraints);

    // Last result
    const lumen::core::SolverResult& lastResult() const;
    void setLastResult(const lumen::core::SolverResult& result);
    bool hasResult() const;
    void clearResult();

    // State queries
    bool isDarkMode() const { return darkMode_; }
    bool hasUnsavedChanges() const { return unsavedChanges_; }
    bool isOptimizing() const { return optimizing_; }
    QString currentFilePath() const { return currentFilePath_; }
    bool hasPortfolio() const;

    // State mutations
    void setDarkMode(bool dark);
    void markUnsaved();
    void markSaved(const QString& filePath);
    void setOptimizing(bool optimizing);

    // Serialization
    bool saveToFile(const QString& filePath);
    bool loadFromFile(const QString& filePath);

signals:
    void darkModeChanged(bool dark);
    void unsavedChangesChanged(bool unsaved);
    void optimizingChanged(bool optimizing);
    void portfolioChanged();
    void targetChanged();
    void constraintsChanged();
    void resultAvailable(const lumen::core::SolverResult& result);
    void errorOccurred(const QString& message);

private:
    lumen::core::Portfolio portfolio_;
    lumen::core::TargetAllocation target_;
    lumen::core::ConstraintSet constraints_;
    lumen::core::SolverResult lastResult_;
    bool hasResult_ = false;

    bool darkMode_ = false;
    bool unsavedChanges_ = false;
    bool optimizing_ = false;
    QString currentFilePath_;
};
