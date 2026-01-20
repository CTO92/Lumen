#pragma once

/// @file hybrid_orchestrator.hpp
/// @brief Hybrid classical-quantum solver orchestration
///
/// This module provides the hybrid solver orchestrator that combines
/// classical preprocessing, quantum optimization, and classical
/// postprocessing/polishing for optimal portfolio rebalancing.

#include <memory>
#include <string>
#include <vector>

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/solvers/quantum_client.hpp"

namespace lumen::solvers {

/// Result of the preprocessing stage
struct PreprocessResult {
    /// Positions fixed to specific weights (not sent to quantum)
    std::vector<std::pair<std::string, double>> fixed_positions;

    /// Reduced portfolio after fixing obvious decisions
    core::Portfolio reduced_portfolio;

    /// Problem size after reduction
    int reduced_size = 0;

    /// Original problem size
    int original_size = 0;

    /// Reduction ratio (reduced_size / original_size)
    double reduction_ratio() const {
        return (original_size > 0) ? static_cast<double>(reduced_size) / original_size : 1.0;
    }

    /// Log of preprocessing decisions
    std::vector<std::string> preprocessing_log;
};

/// Result of postprocessing/polishing stage
struct PostprocessResult {
    /// Final trades after polishing
    std::vector<core::Trade> trades;

    /// Constraint violations found (empty if feasible)
    std::vector<std::string> violations;

    /// Whether all constraints are satisfied
    bool is_feasible = false;

    /// Objective value after polishing
    double polished_objective = 0.0;

    /// Improvement from quantum solution (negative means worse)
    double improvement = 0.0;

    /// Log of postprocessing decisions
    std::vector<std::string> postprocessing_log;
};

/// Configuration for the hybrid orchestrator
struct HybridOrchestratorConfig {
    /// Threshold for using hybrid vs pure quantum (in tax lots)
    int lot_threshold = 50;

    /// Enable classical preprocessing
    bool preprocess_enabled = true;

    /// Enable classical postprocessing/polishing
    bool postprocess_enabled = true;

    /// Maximum classical-quantum iterations
    int max_iterations = 3;

    /// Stop if improvement below this threshold
    double improvement_threshold = 0.001;

    /// Tolerance for "near-target" positions in preprocessing
    double near_target_tolerance = 0.001;

    /// Use quantum for any problem size (ignore tier selection)
    bool force_quantum = false;

    /// Quantum provider configuration
    QuantumSolverConfig quantum_config;
};

/// Hybrid solver orchestrator
class HybridOrchestrator {
public:
    explicit HybridOrchestrator(const HybridOrchestratorConfig& config);
    ~HybridOrchestrator();

    /// Main solve method - runs the full hybrid workflow
    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints);

    /// Get the preprocessing result from the last solve
    const PreprocessResult& getLastPreprocessResult() const;

    /// Get the postprocessing result from the last solve
    const PostprocessResult& getLastPostprocessResult() const;

    /// Get number of iterations from the last solve
    int getLastIterationCount() const;

    /// Get the quantum result from the last solve
    const QuantumResult& getLastQuantumResult() const;

    /// Estimate cost for a given problem
    double estimateCost(const core::Portfolio& portfolio,
                        const core::ConstraintSet& constraints) const;

    /// Check if quantum solver is available
    bool isQuantumAvailable() const;

    /// Configure the orchestrator
    void configure(const HybridOrchestratorConfig& config);

    /// Set the quantum client to use
    void setQuantumClient(std::unique_ptr<QuantumClient> client);

    /// Set verbosity level
    void setVerbosity(int level);

private:
    /// Stage 1: Classical preprocessing
    PreprocessResult preprocess(const core::Portfolio& portfolio,
                                const core::TargetAllocation& target,
                                const core::ConstraintSet& constraints);

    /// Stage 2: Quantum optimization
    QuantumResult quantumOptimize(const core::Portfolio& reduced_portfolio,
                                  const core::TargetAllocation& target,
                                  const core::ConstraintSet& constraints);

    /// Stage 3: Classical postprocessing/polishing
    PostprocessResult postprocess(const QuantumResult& quantum_result,
                                  const PreprocessResult& preprocess_result,
                                  const core::Portfolio& original_portfolio,
                                  const core::TargetAllocation& target,
                                  const core::ConstraintSet& constraints);

    /// Validate a solution against constraints
    std::vector<std::string> validateConstraints(
        const std::vector<core::Trade>& trades,
        const core::Portfolio& portfolio,
        const core::ConstraintSet& constraints) const;

    /// Repair constraint violations using classical solver
    std::vector<core::Trade> repairSolution(
        const std::vector<core::Trade>& trades,
        const std::vector<std::string>& violations,
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints);

    /// Calculate objective value for a set of trades
    double calculateObjective(const std::vector<core::Trade>& trades,
                              const core::Portfolio& portfolio,
                              const core::TargetAllocation& target) const;

    /// Count tax lots in portfolio
    int countTaxLots(const core::Portfolio& portfolio) const;

    /// Should use hybrid workflow for this problem?
    bool shouldUseHybrid(const core::Portfolio& portfolio,
                         const core::ConstraintSet& constraints) const;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lumen::solvers
