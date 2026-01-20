/// @file hybrid_orchestrator.cpp
/// @brief Hybrid classical-quantum solver orchestration implementation
///
/// Implementation of the hybrid solver orchestrator that combines
/// classical preprocessing, quantum optimization, and classical polishing.

#include "lumen/solvers/hybrid_orchestrator.hpp"
#include "lumen/solvers/qubo_types.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>

namespace lumen::solvers {

// =============================================================================
// HybridOrchestrator::Impl
// =============================================================================

class HybridOrchestrator::Impl {
public:
    explicit Impl(const HybridOrchestratorConfig& config)
        : config_(config) {
        // Create quantum client based on configuration
        if (config.quantum_config.provider == "mock") {
            quantum_client_ = std::make_unique<DWaveClient>(config.quantum_config);
        } else if (config.quantum_config.provider == "dwave") {
            quantum_client_ = std::make_unique<DWaveClient>(config.quantum_config);
        } else if (config.quantum_config.provider == "ibm") {
            quantum_client_ = std::make_unique<IBMQuantumClient>(config.quantum_config);
        } else {
            // Default to D-Wave
            quantum_client_ = std::make_unique<DWaveClient>(config.quantum_config);
        }
    }

    HybridOrchestratorConfig config_;
    std::unique_ptr<QuantumClient> quantum_client_;

    // Results from last solve
    PreprocessResult last_preprocess_result_;
    PostprocessResult last_postprocess_result_;
    QuantumResult last_quantum_result_;
    int last_iteration_count_ = 0;
    int verbosity_ = 0;
};

// =============================================================================
// HybridOrchestrator Public Interface
// =============================================================================

HybridOrchestrator::HybridOrchestrator(const HybridOrchestratorConfig& config)
    : impl_(std::make_unique<Impl>(config)) {}

HybridOrchestrator::~HybridOrchestrator() = default;

core::SolverResult HybridOrchestrator::solve(
    const core::Portfolio& portfolio,
    const core::TargetAllocation& target,
    const core::ConstraintSet& constraints) {

    auto start_time = std::chrono::steady_clock::now();

    core::SolverResult result;
    result.solver_used = "Hybrid Quantum-Classical";

    // Check if we should use hybrid workflow
    if (!shouldUseHybrid(portfolio, constraints) && !impl_->config_.force_quantum) {
        // Fall back to pure quantum for small problems
        if (impl_->verbosity_ > 0) {
            // Log: Using pure quantum solver
        }

        auto quantum_result = quantumOptimize(portfolio, target, constraints);
        impl_->last_quantum_result_ = quantum_result;

        if (!quantum_result.success) {
            result.success = false;
            result.status = quantum_result.status;
            return result;
        }

        // Create formulator to interpret solution
        QuboFormulator formulator(portfolio, target);
        formulator.formulate(constraints);
        result.trades = formulator.interpretSolution(quantum_result.best_sample);
        result.objective_value = quantum_result.best_energy;
        result.success = true;
        result.status = "optimal";

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        result.solve_time_ms = duration.count();

        return result;
    }

    // === Stage 1: Preprocessing ===
    auto preprocess_result = preprocess(portfolio, target, constraints);
    impl_->last_preprocess_result_ = preprocess_result;

    if (impl_->verbosity_ > 0) {
        // Log preprocessing stats
    }

    // === Iterative Refinement Loop ===
    core::Portfolio working_portfolio = preprocess_result.reduced_portfolio;
    std::vector<core::Trade> best_trades;
    double best_objective = std::numeric_limits<double>::max();

    for (int iter = 0; iter < impl_->config_.max_iterations; ++iter) {
        impl_->last_iteration_count_ = iter + 1;

        // === Stage 2: Quantum Optimization ===
        auto quantum_result = quantumOptimize(working_portfolio, target, constraints);
        impl_->last_quantum_result_ = quantum_result;

        if (!quantum_result.success) {
            if (iter == 0) {
                result.success = false;
                result.status = quantum_result.status;
                return result;
            }
            // Use best solution from previous iterations
            break;
        }

        // === Stage 3: Postprocessing ===
        auto postprocess_result = postprocess(
            quantum_result, preprocess_result, portfolio, target, constraints);
        impl_->last_postprocess_result_ = postprocess_result;

        // Check if this iteration improved the solution
        if (postprocess_result.polished_objective < best_objective) {
            double improvement = (best_objective - postprocess_result.polished_objective) /
                                 std::abs(best_objective);

            best_objective = postprocess_result.polished_objective;
            best_trades = postprocess_result.trades;

            // Check convergence
            if (improvement < impl_->config_.improvement_threshold && iter > 0) {
                break;
            }
        } else {
            // No improvement, stop iterating
            break;
        }
    }

    // Build final result
    result.trades = best_trades;
    result.objective_value = best_objective;
    result.success = true;
    result.status = impl_->last_postprocess_result_.is_feasible ? "optimal" : "feasible";
    result.iterations = impl_->last_iteration_count_;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    result.solve_time_ms = duration.count();

    return result;
}

const PreprocessResult& HybridOrchestrator::getLastPreprocessResult() const {
    return impl_->last_preprocess_result_;
}

const PostprocessResult& HybridOrchestrator::getLastPostprocessResult() const {
    return impl_->last_postprocess_result_;
}

int HybridOrchestrator::getLastIterationCount() const {
    return impl_->last_iteration_count_;
}

const QuantumResult& HybridOrchestrator::getLastQuantumResult() const {
    return impl_->last_quantum_result_;
}

double HybridOrchestrator::estimateCost(const core::Portfolio& portfolio,
                                         const core::ConstraintSet& constraints) const {
    if (!impl_->quantum_client_) {
        return 0.0;
    }

    // Estimate number of qubits needed
    int num_positions = static_cast<int>(portfolio.getPositionCount());
    int precision_bits = 8;  // Default encoding
    int num_qubits = num_positions * precision_bits;

    return impl_->quantum_client_->estimateCost(num_qubits);
}

bool HybridOrchestrator::isQuantumAvailable() const {
    if (!impl_->quantum_client_) {
        return false;
    }
    return impl_->quantum_client_->isAvailable();
}

void HybridOrchestrator::configure(const HybridOrchestratorConfig& config) {
    impl_->config_ = config;
}

void HybridOrchestrator::setQuantumClient(std::unique_ptr<QuantumClient> client) {
    impl_->quantum_client_ = std::move(client);
}

void HybridOrchestrator::setVerbosity(int level) {
    impl_->verbosity_ = level;
}

// =============================================================================
// Private Methods
// =============================================================================

PreprocessResult HybridOrchestrator::preprocess(
    const core::Portfolio& portfolio,
    const core::TargetAllocation& target,
    const core::ConstraintSet& constraints) {

    PreprocessResult result;
    result.original_size = static_cast<int>(portfolio.getPositionCount());

    if (!impl_->config_.preprocess_enabled) {
        result.reduced_portfolio = portfolio;
        result.reduced_size = result.original_size;
        result.preprocessing_log.push_back("Preprocessing disabled");
        return result;
    }

    // Create reduced portfolio
    result.reduced_portfolio = core::Portfolio(portfolio.getName() + "_reduced");
    result.reduced_portfolio.setCurrency(portfolio.getCurrency());

    // Analyze each position
    auto positions = portfolio.getAllPositions();
    double total_value = portfolio.getTotalValue();

    for (const auto& pos : positions) {
        double current_weight = (total_value > 0) ?
            pos.getCurrentValue() / total_value : 0.0;
        double target_weight = target.getTarget(pos.ticker);
        double drift = std::abs(current_weight - target_weight);

        // Decision 1: Position already at target
        if (drift < impl_->config_.near_target_tolerance) {
            result.fixed_positions.push_back({pos.ticker, current_weight});
            std::stringstream ss;
            ss << "Fixed " << pos.ticker << " at " << (current_weight * 100)
               << "% (at target)";
            result.preprocessing_log.push_back(ss.str());
            continue;
        }

        // Decision 2: Target is zero and position not held
        if (target_weight < 0.001 && current_weight < 0.001) {
            result.fixed_positions.push_back({pos.ticker, 0.0});
            result.preprocessing_log.push_back(
                "Fixed " + pos.ticker + " at 0% (target is zero, not held)");
            continue;
        }

        // Position needs optimization - add to reduced portfolio
        result.reduced_portfolio.addPosition(pos);
    }

    // Also check for positions in target but not in portfolio
    for (const auto& alloc_target : target.getAllTargets()) {
        if (!alloc_target.is_asset_class &&
            !portfolio.hasPosition(alloc_target.identifier)) {
            // New position to buy - include in reduced portfolio
            core::Position new_pos;
            new_pos.ticker = alloc_target.identifier;
            new_pos.shares = 0.0;
            new_pos.current_price = 1.0;  // Placeholder
            new_pos.cost_basis = 0.0;
            new_pos.asset_class = core::AssetClass::STOCKS;

            result.reduced_portfolio.addPosition(new_pos);
            result.preprocessing_log.push_back(
                "Added " + alloc_target.identifier + " (new position to buy)");
        }
    }

    result.reduced_size = static_cast<int>(result.reduced_portfolio.getPositionCount());

    std::stringstream summary;
    summary << "Reduced problem from " << result.original_size
            << " to " << result.reduced_size << " positions";
    result.preprocessing_log.push_back(summary.str());

    return result;
}

QuantumResult HybridOrchestrator::quantumOptimize(
    const core::Portfolio& reduced_portfolio,
    const core::TargetAllocation& target,
    const core::ConstraintSet& constraints) {

    if (!impl_->quantum_client_) {
        QuantumResult result;
        result.success = false;
        result.status = "No quantum client configured";
        return result;
    }

    // Authenticate if needed
    if (!impl_->quantum_client_->isAvailable()) {
        impl_->quantum_client_->authenticate();
    }

    // Create QUBO formulation
    QuboFormulator formulator(reduced_portfolio, target);
    auto Q = formulator.formulate(constraints);

    if (Q.empty()) {
        QuantumResult result;
        result.success = false;
        result.status = "Empty QUBO formulation";
        return result;
    }

    // Submit to quantum solver
    return impl_->quantum_client_->submitQUBO(
        Q, impl_->config_.quantum_config.num_reads);
}

PostprocessResult HybridOrchestrator::postprocess(
    const QuantumResult& quantum_result,
    const PreprocessResult& preprocess_result,
    const core::Portfolio& original_portfolio,
    const core::TargetAllocation& target,
    const core::ConstraintSet& constraints) {

    PostprocessResult result;

    if (!impl_->config_.postprocess_enabled) {
        // Just interpret the quantum solution directly
        QuboFormulator formulator(preprocess_result.reduced_portfolio, target);
        formulator.formulate(constraints);
        result.trades = formulator.interpretSolution(quantum_result.best_sample);
        result.is_feasible = true;
        result.polished_objective = quantum_result.best_energy;
        result.postprocessing_log.push_back("Postprocessing disabled");
        return result;
    }

    // Step 1: Decode quantum solution
    QuboFormulator formulator(preprocess_result.reduced_portfolio, target);
    formulator.formulate(constraints);
    auto quantum_trades = formulator.interpretSolution(quantum_result.best_sample);

    // Step 2: Combine with fixed positions
    // Fixed positions don't generate trades (they're already at target)
    result.trades = quantum_trades;

    result.postprocessing_log.push_back(
        "Decoded " + std::to_string(result.trades.size()) + " trades from quantum solution");

    // Step 3: Validate constraints
    result.violations = validateConstraints(result.trades, original_portfolio, constraints);

    if (!result.violations.empty()) {
        result.postprocessing_log.push_back(
            "Found " + std::to_string(result.violations.size()) + " constraint violations");

        // Step 4: Repair violations using classical solver
        result.trades = repairSolution(
            result.trades, result.violations, original_portfolio, target, constraints);

        // Re-validate
        result.violations = validateConstraints(result.trades, original_portfolio, constraints);

        if (result.violations.empty()) {
            result.postprocessing_log.push_back("All violations repaired");
        } else {
            result.postprocessing_log.push_back(
                "Warning: " + std::to_string(result.violations.size()) +
                " violations could not be repaired");
        }
    }

    result.is_feasible = result.violations.empty();

    // Step 5: Calculate polished objective
    result.polished_objective = calculateObjective(result.trades, original_portfolio, target);
    result.improvement = quantum_result.best_energy - result.polished_objective;

    std::stringstream ss;
    ss << "Polished objective: " << result.polished_objective
       << " (improvement: " << result.improvement << ")";
    result.postprocessing_log.push_back(ss.str());

    return result;
}

std::vector<std::string> HybridOrchestrator::validateConstraints(
    const std::vector<core::Trade>& trades,
    const core::Portfolio& portfolio,
    const core::ConstraintSet& constraints) const {

    std::vector<std::string> violations;

    // Use the constraint set's built-in validation
    if (!constraints.areAllSatisfied(portfolio, trades)) {
        auto violated = constraints.getViolatedConstraints();
        for (const auto* c : violated) {
            violations.push_back(c->getName() + ": " + c->getDescription());
        }
    }

    return violations;
}

std::vector<core::Trade> HybridOrchestrator::repairSolution(
    const std::vector<core::Trade>& trades,
    const std::vector<std::string>& violations,
    const core::Portfolio& portfolio,
    const core::TargetAllocation& target,
    const core::ConstraintSet& constraints) {

    // For now, use a simple heuristic repair
    // TODO: Use HiGHS solver for proper constraint repair

    std::vector<core::Trade> repaired = trades;

    // Simple heuristic: scale trades to satisfy budget constraint
    double total_buy = 0.0;
    double total_sell = 0.0;

    for (const auto& trade : repaired) {
        if (trade.action == core::Trade::Action::BUY) {
            total_buy += trade.amount;
        } else if (trade.action == core::Trade::Action::SELL) {
            total_sell += trade.amount;
        }
    }

    // If buying more than selling, scale down buys
    if (total_buy > total_sell + portfolio.getCashBalance()) {
        double scale = (total_sell + portfolio.getCashBalance()) / total_buy;
        for (auto& trade : repaired) {
            if (trade.action == core::Trade::Action::BUY) {
                trade.amount *= scale;
                trade.shares *= scale;
            }
        }
    }

    return repaired;
}

double HybridOrchestrator::calculateObjective(
    const std::vector<core::Trade>& trades,
    const core::Portfolio& portfolio,
    const core::TargetAllocation& target) const {

    double total_value = portfolio.getTotalValue();
    if (total_value <= 0) return 0.0;

    // Calculate post-trade allocations
    std::map<std::string, double> post_trade_values;

    for (const auto& pos : portfolio.getAllPositions()) {
        post_trade_values[pos.ticker] = pos.getCurrentValue();
    }

    for (const auto& trade : trades) {
        if (trade.action == core::Trade::Action::BUY) {
            post_trade_values[trade.ticker] += trade.amount;
            total_value += trade.amount;
        } else if (trade.action == core::Trade::Action::SELL) {
            post_trade_values[trade.ticker] -= trade.amount;
            total_value -= trade.amount;
        }
    }

    // Calculate squared drift from target
    double drift_sum = 0.0;
    for (const auto& [ticker, value] : post_trade_values) {
        double actual_weight = value / total_value;
        double target_weight = target.getTarget(ticker);
        double drift = actual_weight - target_weight;
        drift_sum += drift * drift;
    }

    // Calculate total transaction cost
    double cost_sum = 0.0;
    for (const auto& trade : trades) {
        cost_sum += trade.transaction_cost;
    }

    // Combined objective (matching QUBO formulation weights)
    return drift_sum + 0.1 * cost_sum;
}

int HybridOrchestrator::countTaxLots(const core::Portfolio& portfolio) const {
    // For now, estimate based on position count
    // In full implementation, would query TaxLotManager
    return static_cast<int>(portfolio.getPositionCount()) * 3;  // Assume ~3 lots per position
}

bool HybridOrchestrator::shouldUseHybrid(
    const core::Portfolio& portfolio,
    const core::ConstraintSet& constraints) const {

    int estimated_lots = countTaxLots(portfolio);
    return estimated_lots > impl_->config_.lot_threshold;
}

}  // namespace lumen::solvers
