#pragma once

/// @file solver_dispatcher.hpp
/// @brief Solver routing and result structures
///
/// This module defines the solver dispatcher which routes optimization problems
/// to the appropriate solver (classical HiGHS or quantum) based on problem
/// characteristics and configuration.

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/core/constraint.hpp"
#include "lumen/core/portfolio.hpp"

namespace lumen {

// Forward declarations
namespace utils {
class Configuration;
}

namespace solvers {
class HighsOptimizer;
class QuantumClient;
}  // namespace solvers

namespace core {

/// Types of optimization problems
enum class ProblemType {
    LP,    ///< Linear Program (continuous variables)
    MILP,  ///< Mixed-Integer Linear Program
    QP,    ///< Quadratic Program
    QUBO   ///< Quadratic Unconstrained Binary Optimization
};

/// Solver tiers based on problem complexity
enum class SolverTier {
    TIER_1,  ///< Classical only (<20 positions)
    TIER_2,  ///< Hybrid classical/quantum (20-50 positions)
    TIER_3   ///< Quantum required (50+ positions with tax)
};

/// Convert ProblemType to string
std::string problemTypeToString(ProblemType type);

/// Convert SolverTier to string
std::string solverTierToString(SolverTier tier);

/// Characteristics of an optimization problem
struct ProblemCharacteristics {
    int num_positions;           ///< Number of portfolio positions
    int num_constraints;         ///< Number of constraints
    int num_variables;           ///< Number of decision variables
    int num_integer_variables;   ///< Number of integer variables
    ProblemType problem_type;    ///< Type of optimization problem
    bool has_tax_optimization;   ///< Whether tax optimization is enabled
    bool has_integer_constraints;///< Whether integer constraints are present
    double estimated_complexity; ///< Heuristic complexity score
    SolverTier recommended_tier; ///< Recommended solver tier

    nlohmann::json toJSON() const;
};

/// Represents a recommended trade action
struct Trade {
    std::string ticker;  ///< Security symbol

    /// Trade action
    enum class Action {
        BUY,   ///< Purchase shares
        SELL,  ///< Sell shares
        HOLD   ///< No action
    } action;

    double shares;           ///< Number of shares to trade
    double price;            ///< Price per share
    double amount;           ///< Total trade amount (shares * price)
    double transaction_cost; ///< Estimated transaction cost
    std::string rationale;   ///< Explanation for this trade
    std::vector<std::string> lot_ids;  ///< Tax lot IDs (for tax optimization)

    nlohmann::json toJSON() const;
    static Trade fromJSON(const nlohmann::json& j);
};

/// Convert Trade::Action to string
std::string tradeActionToString(Trade::Action action);

/// Result of a solver run
struct SolverResult {
    bool success;            ///< Whether solving succeeded
    std::string status;      ///< Status string: "optimal", "feasible", "infeasible", "timeout"
    double objective_value;  ///< Optimal objective value
    double optimality_gap;   ///< Gap from optimal (for MILP)

    std::vector<Trade> trades;  ///< Recommended trades
    std::map<std::string, double> final_allocation;  ///< Post-trade allocations
    double total_transaction_cost;  ///< Total transaction costs

    // Solver metadata
    std::string solver_used;  ///< Which solver was used
    long solve_time_ms;       ///< Solve time in milliseconds
    int iterations;           ///< Number of solver iterations

    // For explainability
    std::vector<std::string> active_constraints;   ///< Active constraint names
    std::vector<std::string> binding_constraints;  ///< Binding constraint names

    nlohmann::json toJSON() const;
    static SolverResult fromJSON(const nlohmann::json& j);
};

/// Routes optimization problems to appropriate solvers
class SolverDispatcher {
public:
    explicit SolverDispatcher(const utils::Configuration& config);
    ~SolverDispatcher();

    /// Main dispatch method - runs optimization and returns result
    SolverResult dispatch(const Portfolio& portfolio, const TargetAllocation& target,
                          const ConstraintSet& constraints);

    /// Analyze problem to determine characteristics
    ProblemCharacteristics classifyProblem(const Portfolio& portfolio,
                                           const ConstraintSet& constraints) const;

    /// Select solver tier based on characteristics
    SolverTier selectTier(const ProblemCharacteristics& characteristics) const;

    /// Select specific solver for a tier
    std::string selectSolver(SolverTier tier, bool quantum_available) const;

    /// Check if quantum solvers are available
    bool isQuantumAvailable() const;

    /// Check if user is authorized for quantum (premium feature)
    bool isUserAuthorizedForQuantum() const;

    // Configuration
    void setClassicalTimeout(long timeout_ms);
    void setQuantumTimeout(long timeout_ms);
    void enableQuantum(bool enable);
    void setFallbackBehavior(bool fallback_to_classical);

private:
    std::unique_ptr<solvers::HighsOptimizer> classical_solver_;
    std::unique_ptr<solvers::QuantumClient> quantum_solver_;
    bool quantum_enabled_ = false;
    bool fallback_to_classical_ = true;
    long classical_timeout_ms_ = 30000;
    long quantum_timeout_ms_ = 60000;

    // Internal solving methods
    SolverResult solveClassical(const Portfolio& portfolio, const TargetAllocation& target,
                                const ConstraintSet& constraints);

    SolverResult solveQuantum(const Portfolio& portfolio, const TargetAllocation& target,
                              const ConstraintSet& constraints);

    SolverResult solveHybrid(const Portfolio& portfolio, const TargetAllocation& target,
                             const ConstraintSet& constraints);
};

}  // namespace core
}  // namespace lumen
