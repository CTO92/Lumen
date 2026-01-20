/// @file optimizationworker.cpp
/// @brief Background worker for portfolio optimization implementation

#include "optimizationworker.hpp"

#include <QThread>

#include "lumen/utils/config.hpp"

OptimizationWorker::OptimizationWorker(QObject* parent)
    : QObject(parent)
{
}

OptimizationWorker::~OptimizationWorker() = default;

void OptimizationWorker::configure(const lumen::core::Portfolio& portfolio,
                                    const lumen::core::TargetAllocation& target,
                                    const lumen::core::ConstraintSet& constraints) {
    portfolio_ = portfolio;
    target_ = target;
    constraints_ = constraints;
    cancelRequested_ = false;
}

void OptimizationWorker::process() {
    emit started();
    emit progress(0, tr("Initializing..."));

    try {
        // Check for cancellation
        if (cancelRequested_) {
            emit cancelled();
            return;
        }

        emit progress(10, tr("Loading configuration..."));

        // Create configuration
        lumen::utils::Configuration config;

        // Check for cancellation
        if (cancelRequested_) {
            emit cancelled();
            return;
        }

        emit progress(20, tr("Creating solver..."));

        // Create solver dispatcher
        lumen::core::SolverDispatcher dispatcher(config);

        // Check for cancellation
        if (cancelRequested_) {
            emit cancelled();
            return;
        }

        emit progress(30, tr("Analyzing portfolio..."));

        // Classify the problem
        auto characteristics = dispatcher.classifyProblem(portfolio_, constraints_);

        // Check for cancellation
        if (cancelRequested_) {
            emit cancelled();
            return;
        }

        emit progress(40, tr("Running optimization..."));

        // Run the optimization
        auto result = dispatcher.dispatch(portfolio_, target_, constraints_);

        // Check for cancellation
        if (cancelRequested_) {
            emit cancelled();
            return;
        }

        emit progress(90, tr("Finalizing results..."));

        // Done
        emit progress(100, tr("Complete"));
        emit finished(result);

    } catch (const std::exception& e) {
        emit error(tr("Optimization failed: %1").arg(e.what()));
    } catch (...) {
        emit error(tr("Optimization failed with unknown error"));
    }
}

void OptimizationWorker::cancel() {
    cancelRequested_ = true;
}
