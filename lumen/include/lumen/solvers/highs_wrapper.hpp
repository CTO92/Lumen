#pragma once

/// @file highs_wrapper.hpp
/// @brief HiGHS optimization solver wrapper
///
/// This module provides a wrapper around the HiGHS optimization library
/// for solving LP, MILP, and QP problems.

#include <memory>
#include <string>
#include <vector>

#include "lumen/core/constraint.hpp"
#include "lumen/core/portfolio.hpp"
#include "lumen/core/solver_dispatcher.hpp"

// Forward declaration of HiGHS types
class Highs;

namespace lumen::data {
// Forward declarations for tax types
class TaxLotManager;
class TaxOptimizer;
enum class TaxLotMethod;
}  // namespace lumen::data

namespace lumen::solvers {

/// Objective function types
enum class ObjectiveType {
    MINIMIZE_DRIFT,    ///< Minimize deviation from target allocation
    MINIMIZE_COST,     ///< Minimize transaction costs
    MINIMIZE_TAX,      ///< Minimize tax liability
    MULTI_OBJECTIVE    ///< Weighted combination of objectives
};

/// Weights for multi-objective optimization
struct ObjectiveWeights {
    double drift_weight = 1.0;   ///< Weight for allocation drift
    double cost_weight = 0.1;    ///< Weight for transaction costs
    double tax_weight = 0.0;     ///< Weight for tax impact

    /// Normalize weights to sum to 1
    void normalize();
};

/// Tax configuration for optimization
struct TaxConfig {
    data::TaxLotManager* lot_manager = nullptr;   ///< Tax lot manager (optional)
    data::TaxOptimizer* optimizer = nullptr;       ///< Tax optimizer (optional)
    double short_term_rate = 0.35;                 ///< Short-term capital gains tax rate
    double long_term_rate = 0.15;                  ///< Long-term capital gains tax rate
    bool prefer_long_term_lots = true;             ///< Prefer selling long-term lots
    bool block_wash_sales = true;                  ///< Block trades that would trigger wash sales
};

/// Builder for MILP (Mixed-Integer Linear Program) problems
class MILPBuilder {
public:
    MILPBuilder(const core::Portfolio& portfolio, const core::TargetAllocation& target);
    ~MILPBuilder();

    /// Set the objective function type
    void setObjective(ObjectiveType type, const ObjectiveWeights& weights = {});

    /// Set a custom objective with coefficients
    void setCustomObjective(const std::vector<double>& coefficients);

    /// Add constraints from a ConstraintSet
    void addConstraintSet(const core::ConstraintSet& constraints);

    /// Add budget constraint
    void addBudgetConstraint(double budget, double min_cash = 0.0);

    /// Add allocation bounds for positions
    void addAllocationBounds(const std::map<std::string, std::pair<double, double>>& bounds);

    /// Add integer constraints for specific tickers
    void addIntegerConstraints(const std::set<std::string>& tickers);

    /// Add minimum trade size constraint
    void addMinTradeConstraint(double min_amount);

    /// Set tax configuration for tax-aware optimization
    void setTaxConfig(const TaxConfig& config);

    /// Add tax lot constraint (lot selection method enforcement)
    void addTaxLotConstraint(const core::TaxLotConstraint* constraint);

    /// Add wash sale constraint
    void addWashSaleConstraint(const core::WashSaleConstraint* constraint);

    /// Get current prices for tax calculations
    void setCurrentPrices(const std::map<std::string, double>& prices);

    /// Add a variable to the model
    int addVariable(const std::string& name, double lb, double ub, bool is_integer = false);

    /// Get variable index by name
    int getVariableIndex(const std::string& name) const;

    /// Check if the model is ready to solve
    bool isReady() const;

    /// Get number of variables
    int getNumVariables() const;

    /// Get number of constraints
    int getNumConstraints() const;

    /// Extract trades from solution
    std::vector<core::Trade> extractTrades(const std::vector<double>& solution) const;

    /// @name Model data accessors (for solver integration)
    /// @{
    const std::vector<std::string>& getVarNames() const;
    const std::vector<double>& getVarLower() const;
    const std::vector<double>& getVarUpper() const;
    const std::vector<bool>& getVarInteger() const;
    const std::vector<double>& getObjCoeffs() const;
    const std::vector<std::vector<double>>& getConstraintMatrix() const;
    const std::vector<double>& getConstraintLower() const;
    const std::vector<double>& getConstraintUpper() const;
    const std::vector<std::string>& getConstraintNames() const;
    /// @}

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Builder for QP (Quadratic Program) problems
class QPBuilder {
public:
    QPBuilder(const core::Portfolio& portfolio, const core::TargetAllocation& target);
    ~QPBuilder();

    /// Set quadratic objective (Q matrix and c vector for 0.5*x'Qx + c'x)
    void setQuadraticObjective(const std::vector<std::vector<double>>& Q,
                               const std::vector<double>& c);

    /// Build mean-variance objective from covariance matrix
    void buildMeanVarianceObjective(const std::vector<std::vector<double>>& covariance,
                                    const std::vector<double>& expected_returns,
                                    double risk_aversion);

    /// Add constraints from a ConstraintSet
    void addConstraintSet(const core::ConstraintSet& constraints);

    /// Check if the model is ready to solve
    bool isReady() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Abstract base class for solvers
class BaseSolver {
public:
    virtual ~BaseSolver() = default;

    virtual std::string getName() const = 0;
    virtual bool isAvailable() const = 0;

    virtual core::SolverResult solve(const core::Portfolio& portfolio,
                                     const core::TargetAllocation& target,
                                     const core::ConstraintSet& constraints) = 0;

    virtual void setTimeout(long timeout_ms) = 0;
    virtual void setVerbosity(int level) = 0;

    virtual long getLastSolveTime() const = 0;
    virtual int getLastIterations() const = 0;
};

/// HiGHS-based optimizer for LP/MILP/QP problems
class HighsOptimizer : public BaseSolver {
public:
    HighsOptimizer();
    ~HighsOptimizer() override;

    std::string getName() const override { return "HiGHS"; }
    bool isAvailable() const override;

    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints) override;

    void setTimeout(long timeout_ms) override;
    void setVerbosity(int level) override;

    long getLastSolveTime() const override { return last_solve_time_; }
    int getLastIterations() const override { return last_iterations_; }

    // Advanced options
    void setPresolve(bool enable);
    void setParallel(bool enable);
    void setMIPGap(double gap);

    // Solution extraction
    std::vector<double> getPrimalSolution() const;
    std::vector<double> getDualSolution() const;
    double getObjectiveValue() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    long timeout_ms_ = 30000;
    int verbosity_ = 0;
    long last_solve_time_ = 0;
    int last_iterations_ = 0;
};

}  // namespace lumen::solvers
