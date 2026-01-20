#pragma once

/// @file optimizationworker.hpp
/// @brief Background worker for portfolio optimization

#include <QObject>
#include <atomic>

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"

/// Background worker for running portfolio optimization
class OptimizationWorker : public QObject {
    Q_OBJECT

public:
    explicit OptimizationWorker(QObject* parent = nullptr);
    ~OptimizationWorker() override;

    /// Configure the optimization problem
    void configure(const lumen::core::Portfolio& portfolio,
                   const lumen::core::TargetAllocation& target,
                   const lumen::core::ConstraintSet& constraints);

public slots:
    /// Run the optimization (call from worker thread)
    void process();

    /// Request cancellation
    void cancel();

signals:
    /// Emitted when optimization starts
    void started();

    /// Emitted to report progress (0-100)
    void progress(int percent, const QString& status);

    /// Emitted when optimization completes successfully
    void finished(const lumen::core::SolverResult& result);

    /// Emitted when an error occurs
    void error(const QString& message);

    /// Emitted when optimization is cancelled
    void cancelled();

private:
    lumen::core::Portfolio portfolio_;
    lumen::core::TargetAllocation target_;
    lumen::core::ConstraintSet constraints_;
    std::atomic<bool> cancelRequested_{false};
};
