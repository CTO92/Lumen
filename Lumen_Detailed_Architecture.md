# Lumen Detailed Architecture

**Version:** 1.0
**Date:** January 19, 2026
**Organization:** OA Quantum Labs
**Status:** Detailed Design Specification

---

## Table of Contents

1. [Overview](#overview)
2. [Module Architecture](#module-architecture)
3. [Core Module](#1-core-module)
4. [Solvers Module](#2-solvers-module)
5. [Data Module](#3-data-module)
6. [Explain Module](#4-explain-module)
7. [Utils Module](#5-utils-module)
8. [Applications Layer](#6-applications-layer)
9. [Data Flow Diagrams](#data-flow-diagrams)
10. [Database Schema](#database-schema)
11. [Build Configuration](#build-configuration)
12. [Design Patterns](#design-patterns)
13. [Security Architecture](#security-architecture)
14. [Performance Specifications](#performance-specifications)

---

## Overview

This document provides fine-grained architectural details for Lumen, expanding on the top-level architecture with complete class definitions, method signatures, data structures, and implementation specifications.

**Architecture Principles:**
- **Separation of Concerns:** Each module handles a single responsibility
- **Dependency Injection:** Solvers, data providers, and loggers are injected
- **Interface-Based Design:** Abstract base classes enable extensibility
- **Fail-Safe Defaults:** Classical solvers always available as fallback

---

## Module Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        APPLICATIONS LAYER                           │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────────┐ │
│  │ CLI App     │  │ REST Server │  │ Desktop GUI (Future)        │ │
│  └──────┬──────┘  └──────┬──────┘  └──────────────┬──────────────┘ │
└─────────┼────────────────┼─────────────────────────┼────────────────┘
          └────────────────┼─────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│                         CORE MODULE                                  │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐ │
│  │ Portfolio      │  │ Constraint     │  │ SolverDispatcher       │ │
│  │ Position       │  │ ConstraintSet  │  │ ProblemClassifier      │ │
│  │ TargetAlloc    │  │ BudgetConstr   │  │ SolverResult           │ │
│  └────────┬───────┘  └────────┬───────┘  └────────────┬───────────┘ │
└───────────┼───────────────────┼────────────────────────┼────────────┘
            │                   │                        │
┌───────────▼───────────────────▼────────────────────────▼────────────┐
│                        SOLVERS MODULE                                │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐ │
│  │ BaseSolver     │  │ HighsOptimizer │  │ QuantumClient          │ │
│  │ QPBuilder      │  │ MILPBuilder    │  │ QuboFormulator         │ │
│  └────────────────┘  └────────────────┘  └────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
            │                   │                        │
┌───────────▼───────────────────▼────────────────────────▼────────────┐
│                         DATA MODULE                                  │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐ │
│  │ MarketData     │  │ TaxLot         │  │ DataCache              │ │
│  │ PriceProvider  │  │ TaxOptimizer   │  │ CovarianceCalc         │ │
│  └────────────────┘  └────────────────┘  └────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
            │                   │                        │
┌───────────▼───────────────────▼────────────────────────▼────────────┐
│                        EXPLAIN MODULE                                │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐ │
│  │ Provenance     │  │ Explainer      │  │ SensitivityAnalysis    │ │
│  │ DataSource     │  │ TradeRationale │  │ ExplanationDocument    │ │
│  └────────────────┘  └────────────────┘  └────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
            │                   │                        │
┌───────────▼───────────────────▼────────────────────────▼────────────┐
│                         UTILS MODULE                                 │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────────────┐ │
│  │ Logger         │  │ Configuration  │  │ PyFlareExporter        │ │
│  │ LogLevel       │  │ ConfigSection  │  │ TelemetryEvent         │ │
│  └────────────────┘  └────────────────┘  └────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 1. Core Module

**Location:** `include/lumen/core/` and `src/core/`

### 1.1 Portfolio Classes

#### Position

```cpp
// include/lumen/core/portfolio.hpp

namespace lumen::core {

enum class AssetClass {
    STOCKS,
    BONDS,
    CASH,
    COMMODITIES,
    REAL_ESTATE,
    CRYPTO,
    OTHER
};

struct Position {
    std::string ticker;              // Security symbol (e.g., "AAPL")
    double shares;                   // Number of shares held
    double current_price;            // Current market price per share
    double cost_basis;               // Average cost basis per share
    std::chrono::system_clock::time_point purchase_date;
    AssetClass asset_class;
    std::string exchange;            // Exchange (e.g., "NASDAQ")
    bool supports_fractional;        // Whether fractional shares allowed

    // Computed properties
    double getCurrentValue() const { return shares * current_price; }
    double getCostBasisTotal() const { return shares * cost_basis; }
    double getUnrealizedGain() const { return getCurrentValue() - getCostBasisTotal(); }
    double getUnrealizedGainPercent() const {
        return (getCostBasisTotal() > 0) ? (getUnrealizedGain() / getCostBasisTotal()) * 100.0 : 0.0;
    }
    bool isLongTerm() const {
        auto now = std::chrono::system_clock::now();
        auto days_held = std::chrono::duration_cast<std::chrono::hours>(now - purchase_date).count() / 24;
        return days_held > 365;
    }

    // Serialization
    nlohmann::json toJSON() const;
    static Position fromJSON(const nlohmann::json& j);
    static Position fromCSVRow(const std::vector<std::string>& row, const CSVSchema& schema);
};
```

#### Portfolio

```cpp
class Portfolio {
public:
    // Constructors
    Portfolio() = default;
    explicit Portfolio(const std::string& name);

    // Position management
    void addPosition(const Position& pos);
    void addPosition(const std::string& ticker, double shares, double cost_basis);
    void removePosition(const std::string& ticker);
    void updatePosition(const std::string& ticker, double new_shares);
    void updatePrice(const std::string& ticker, double new_price);
    void updateAllPrices(const std::map<std::string, double>& prices);

    // Accessors
    const Position& getPosition(const std::string& ticker) const;
    std::vector<Position> getAllPositions() const;
    std::vector<std::string> getAllTickers() const;
    size_t getPositionCount() const { return positions_.size(); }
    bool hasPosition(const std::string& ticker) const;

    // Portfolio metrics
    double getTotalValue() const;
    double getTotalCostBasis() const;
    double getTotalUnrealizedGain() const;
    double getCashBalance() const { return cash_balance_; }
    void setCashBalance(double cash) { cash_balance_ = cash; }

    // Allocation analysis
    double getAllocationPercent(const std::string& ticker) const;
    std::map<std::string, double> getAllocationMap() const;
    std::map<AssetClass, double> getAssetClassExposure() const;
    double getAssetClassPercent(AssetClass ac) const;

    // Deviation from target
    double calculateDrift(const TargetAllocation& target) const;
    std::map<std::string, double> getDeviationMap(const TargetAllocation& target) const;

    // I/O
    nlohmann::json toJSON() const;
    static Portfolio fromJSON(const nlohmann::json& j);
    static Portfolio fromCSV(const std::string& filepath);
    static Portfolio fromBrokerExport(const std::string& filepath, BrokerFormat format);
    void saveToFile(const std::string& filepath) const;

private:
    std::string name_;
    std::string currency_ = "USD";
    std::map<std::string, Position> positions_;
    double cash_balance_ = 0.0;
    std::chrono::system_clock::time_point last_updated_;
};
```

#### TargetAllocation

```cpp
enum class AllocationMode {
    PERCENTAGE,      // Target as percentage of portfolio
    DOLLAR_AMOUNT    // Target as fixed dollar amount
};

struct AllocationTarget {
    std::string identifier;     // Ticker or asset class name
    double target_value;        // Target % or $ amount
    double lower_bound;         // Min acceptable (for bands)
    double upper_bound;         // Max acceptable (for bands)
    bool is_asset_class;        // true = asset class, false = specific ticker
};

class TargetAllocation {
public:
    TargetAllocation() = default;
    explicit TargetAllocation(AllocationMode mode);

    // Target management
    void setTarget(const std::string& identifier, double target,
                   double tolerance = 0.05, bool is_asset_class = false);
    void setTargetWithBounds(const std::string& identifier,
                             double target, double lower, double upper,
                             bool is_asset_class = false);
    void removeTarget(const std::string& identifier);

    // Accessors
    double getTarget(const std::string& identifier) const;
    AllocationTarget getFullTarget(const std::string& identifier) const;
    std::vector<AllocationTarget> getAllTargets() const;
    AllocationMode getMode() const { return mode_; }

    // Validation
    bool validate() const;  // Check targets sum to 100% (or reasonable total)
    std::vector<std::string> getValidationErrors() const;

    // Rebalancing triggers
    bool needsRebalancing(const Portfolio& portfolio) const;
    std::map<std::string, double> getRebalancingNeeds(const Portfolio& portfolio) const;

    // I/O
    nlohmann::json toJSON() const;
    static TargetAllocation fromJSON(const nlohmann::json& j);

private:
    AllocationMode mode_ = AllocationMode::PERCENTAGE;
    std::map<std::string, AllocationTarget> targets_;
    double global_tolerance_ = 0.05;  // 5% default tolerance band
};
```

### 1.2 Constraint Classes

#### Base Constraint

```cpp
// include/lumen/core/constraint.hpp

namespace lumen::core {

enum class ConstraintType {
    BUDGET,
    ALLOCATION,
    MIN_TRADE,
    INTEGER_SHARES,
    TAX_LOT,
    WASH_SALE,
    POSITION_LIMIT,
    SECTOR_LIMIT,
    CUSTOM
};

enum class ConstraintStatus {
    ACTIVE,      // Constraint is enforced
    BINDING,     // Constraint is at its limit in solution
    SLACK,       // Constraint has room
    VIOLATED,    // Constraint could not be satisfied
    DISABLED     // Constraint turned off
};

class Constraint {
public:
    virtual ~Constraint() = default;

    // Core interface
    virtual ConstraintType getType() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;

    // Validation
    virtual bool isValid() const = 0;
    virtual bool isSatisfied(const Portfolio& portfolio,
                             const std::vector<Trade>& trades) const = 0;

    // Symbolic representation for solvers
    virtual SymEngine::Expression getSymbolicExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const = 0;

    // For LP/MILP formulation
    virtual void addToModel(HighsModel& model,
                           const std::vector<int>& var_indices) const = 0;

    // Serialization
    virtual nlohmann::json toJSON() const = 0;

    // Status tracking
    ConstraintStatus getStatus() const { return status_; }
    void setStatus(ConstraintStatus s) { status_ = s; }
    double getSlackValue() const { return slack_value_; }
    void setSlackValue(double s) { slack_value_ = s; }

protected:
    ConstraintStatus status_ = ConstraintStatus::ACTIVE;
    double slack_value_ = 0.0;
};
```

#### Concrete Constraints

```cpp
class BudgetConstraint : public Constraint {
public:
    BudgetConstraint(double total_budget, double min_cash_reserve = 0.0);

    ConstraintType getType() const override { return ConstraintType::BUDGET; }
    std::string getName() const override { return "Budget"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    SymEngine::Expression getSymbolicExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const override;
    void addToModel(HighsModel& model,
                   const std::vector<int>& var_indices) const override;

    nlohmann::json toJSON() const override;

    // Accessors
    double getTotalBudget() const { return total_budget_; }
    double getMinCashReserve() const { return min_cash_reserve_; }

private:
    double total_budget_;
    double min_cash_reserve_;
};

class AllocationConstraint : public Constraint {
public:
    AllocationConstraint(const std::string& identifier,
                         double lower_bound, double upper_bound,
                         bool is_asset_class = false);

    ConstraintType getType() const override { return ConstraintType::ALLOCATION; }
    std::string getName() const override;
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    SymEngine::Expression getSymbolicExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const override;
    void addToModel(HighsModel& model,
                   const std::vector<int>& var_indices) const override;

    nlohmann::json toJSON() const override;

private:
    std::string identifier_;
    double lower_bound_;
    double upper_bound_;
    bool is_asset_class_;
};

class MinTradeSizeConstraint : public Constraint {
public:
    MinTradeSizeConstraint(double min_amount = 0.0, double min_shares = 0.0);

    ConstraintType getType() const override { return ConstraintType::MIN_TRADE; }
    std::string getName() const override { return "Minimum Trade Size"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    SymEngine::Expression getSymbolicExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const override;
    void addToModel(HighsModel& model,
                   const std::vector<int>& var_indices) const override;

    nlohmann::json toJSON() const override;

private:
    double min_trade_amount_;
    double min_trade_shares_;
};

class IntegerShareConstraint : public Constraint {
public:
    IntegerShareConstraint(bool enforce = true,
                           std::set<std::string> exempt_tickers = {});

    ConstraintType getType() const override { return ConstraintType::INTEGER_SHARES; }
    std::string getName() const override { return "Integer Shares"; }
    std::string getDescription() const override;

    bool isValid() const override { return true; }
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    SymEngine::Expression getSymbolicExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const override;
    void addToModel(HighsModel& model,
                   const std::vector<int>& var_indices) const override;

    nlohmann::json toJSON() const override;

    bool isExempt(const std::string& ticker) const;

private:
    bool enforce_;
    std::set<std::string> exempt_tickers_;
};

class PositionLimitConstraint : public Constraint {
public:
    PositionLimitConstraint(double max_position_percent = 0.25);  // Default 25%

    ConstraintType getType() const override { return ConstraintType::POSITION_LIMIT; }
    std::string getName() const override { return "Max Position Size"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    SymEngine::Expression getSymbolicExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const override;
    void addToModel(HighsModel& model,
                   const std::vector<int>& var_indices) const override;

    nlohmann::json toJSON() const override;

private:
    double max_position_percent_;
};
```

#### ConstraintSet

```cpp
class ConstraintSet {
public:
    ConstraintSet() = default;

    // Constraint management
    void addConstraint(std::unique_ptr<Constraint> constraint);
    void removeConstraint(const std::string& name);
    void clearAll();

    // Accessors
    const Constraint* getConstraint(const std::string& name) const;
    std::vector<const Constraint*> getAllConstraints() const;
    std::vector<const Constraint*> getConstraintsByType(ConstraintType type) const;
    size_t getConstraintCount() const { return constraints_.size(); }

    // Validation
    bool validateConsistency() const;
    std::vector<std::string> getInconsistencies() const;
    bool areAllSatisfied(const Portfolio& portfolio,
                         const std::vector<Trade>& trades) const;

    // After solving
    std::vector<const Constraint*> getActiveConstraints() const;
    std::vector<const Constraint*> getBindingConstraints() const;
    std::vector<const Constraint*> getViolatedConstraints() const;

    // For solver formulation
    void addAllToModel(HighsModel& model,
                      const std::vector<int>& var_indices) const;
    SymEngine::Expression getCombinedExpression(
        const std::map<std::string, SymEngine::Symbol>& variables) const;

    // I/O
    nlohmann::json toJSON() const;
    static ConstraintSet fromJSON(const nlohmann::json& j);

private:
    std::vector<std::unique_ptr<Constraint>> constraints_;
};
```

### 1.3 Solver Dispatcher

```cpp
// include/lumen/core/solver_dispatcher.hpp

namespace lumen::core {

enum class ProblemType {
    LP,      // Linear Program (continuous variables)
    MILP,    // Mixed-Integer Linear Program
    QP,      // Quadratic Program
    QUBO     // Quadratic Unconstrained Binary Optimization
};

enum class SolverTier {
    TIER_1,  // Classical only (<20 positions)
    TIER_2,  // Hybrid classical/quantum (20-50 positions)
    TIER_3   // Quantum required (50+ positions with tax)
};

struct ProblemCharacteristics {
    int num_positions;
    int num_constraints;
    int num_variables;
    int num_integer_variables;
    ProblemType problem_type;
    bool has_tax_optimization;
    bool has_integer_constraints;
    double estimated_complexity;  // Heuristic score
    SolverTier recommended_tier;
};

struct Trade {
    std::string ticker;
    enum class Action { BUY, SELL, HOLD } action;
    double shares;
    double price;
    double amount;  // shares * price
    double transaction_cost;
    std::string rationale;
    std::vector<std::string> lot_ids;  // For tax lot tracking
};

struct SolverResult {
    bool success;
    std::string status;  // "optimal", "feasible", "infeasible", "timeout"
    double objective_value;
    double optimality_gap;  // For MILP, how close to optimal

    std::vector<Trade> trades;
    std::map<std::string, double> final_allocation;
    double total_transaction_cost;

    // Solver metadata
    std::string solver_used;
    long solve_time_ms;
    int iterations;

    // For explainability
    std::vector<std::string> active_constraints;
    std::vector<std::string> binding_constraints;

    nlohmann::json toJSON() const;
};

class SolverDispatcher {
public:
    SolverDispatcher(const Configuration& config);

    // Main dispatch method
    SolverResult dispatch(const Portfolio& portfolio,
                          const TargetAllocation& target,
                          const ConstraintSet& constraints);

    // Problem analysis
    ProblemCharacteristics classifyProblem(const Portfolio& portfolio,
                                           const ConstraintSet& constraints) const;

    // Solver selection
    SolverTier selectTier(const ProblemCharacteristics& characteristics) const;
    std::string selectSolver(SolverTier tier, bool quantum_available) const;

    // Quantum availability check
    bool isQuantumAvailable() const;
    bool isUserAuthorizedForQuantum() const;

    // Configuration
    void setClassicalTimeout(long timeout_ms);
    void setQuantumTimeout(long timeout_ms);
    void enableQuantum(bool enable);
    void setFallbackBehavior(bool fallback_to_classical);

private:
    std::unique_ptr<solvers::HighsOptimizer> classical_solver_;
    std::unique_ptr<solvers::QuantumClient> quantum_solver_;
    Configuration config_;
    bool quantum_enabled_ = false;
    bool fallback_to_classical_ = true;

    SolverResult solveClassical(const Portfolio& portfolio,
                                const TargetAllocation& target,
                                const ConstraintSet& constraints);

    SolverResult solveQuantum(const Portfolio& portfolio,
                              const TargetAllocation& target,
                              const ConstraintSet& constraints);

    SolverResult solveHybrid(const Portfolio& portfolio,
                             const TargetAllocation& target,
                             const ConstraintSet& constraints);
};

}  // namespace lumen::core
```

---

## 2. Solvers Module

**Location:** `include/lumen/solvers/` and `src/solvers/`

### 2.1 Base Solver Interface

```cpp
// include/lumen/solvers/base_solver.hpp

namespace lumen::solvers {

class BaseSolver {
public:
    virtual ~BaseSolver() = default;

    virtual std::string getName() const = 0;
    virtual bool isAvailable() const = 0;

    virtual core::SolverResult solve(
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints) = 0;

    virtual void setTimeout(long timeout_ms) = 0;
    virtual void setVerbosity(int level) = 0;

    // Statistics
    virtual long getLastSolveTime() const = 0;
    virtual int getLastIterations() const = 0;
};

}  // namespace lumen::solvers
```

### 2.2 HiGHS Optimizer

```cpp
// include/lumen/solvers/highs_wrapper.hpp

namespace lumen::solvers {

enum class ObjectiveType {
    MINIMIZE_DRIFT,           // Minimize deviation from target
    MINIMIZE_COST,            // Minimize transaction costs
    MINIMIZE_TAX,             // Minimize tax liability
    MULTI_OBJECTIVE           // Weighted combination
};

struct ObjectiveWeights {
    double drift_weight = 1.0;
    double cost_weight = 0.1;
    double tax_weight = 0.0;

    void normalize() {
        double total = drift_weight + cost_weight + tax_weight;
        if (total > 0) {
            drift_weight /= total;
            cost_weight /= total;
            tax_weight /= total;
        }
    }
};

class MILPBuilder {
public:
    MILPBuilder(const core::Portfolio& portfolio,
                const core::TargetAllocation& target);

    // Build objective function
    void setObjective(ObjectiveType type, const ObjectiveWeights& weights = {});
    void setCustomObjective(const std::vector<double>& coefficients);

    // Add constraints
    void addConstraintSet(const core::ConstraintSet& constraints);
    void addBudgetConstraint(double budget, double min_cash = 0.0);
    void addAllocationBounds(const std::map<std::string, std::pair<double, double>>& bounds);
    void addIntegerConstraints(const std::set<std::string>& tickers);
    void addMinTradeConstraint(double min_amount);

    // Variable management
    int addVariable(const std::string& name, double lb, double ub, bool is_integer = false);
    int getVariableIndex(const std::string& name) const;

    // Build final model
    HighsModel build() const;

    // Extract solution
    std::vector<core::Trade> extractTrades(const std::vector<double>& solution) const;

private:
    core::Portfolio portfolio_;
    core::TargetAllocation target_;
    std::vector<std::string> variable_names_;
    std::map<std::string, int> var_index_map_;
    HighsModel model_;
};

class QPBuilder {
public:
    QPBuilder(const core::Portfolio& portfolio,
              const core::TargetAllocation& target);

    // Quadratic objective (for variance minimization)
    void setQuadraticObjective(const Eigen::MatrixXd& Q, const Eigen::VectorXd& c);
    void buildMeanVarianceObjective(const Eigen::MatrixXd& covariance,
                                    const Eigen::VectorXd& expected_returns,
                                    double risk_aversion);

    // Constraints
    void addConstraintSet(const core::ConstraintSet& constraints);
    void addLinearConstraints(const Eigen::MatrixXd& A,
                              const Eigen::VectorXd& lb,
                              const Eigen::VectorXd& ub);

    // Build
    HighsModel build() const;

private:
    core::Portfolio portfolio_;
    core::TargetAllocation target_;
    Eigen::MatrixXd Q_;  // Quadratic term
    Eigen::VectorXd c_;  // Linear term
};

class HighsOptimizer : public BaseSolver {
public:
    HighsOptimizer();
    ~HighsOptimizer() override;

    std::string getName() const override { return "HiGHS"; }
    bool isAvailable() const override { return true; }  // Always available

    core::SolverResult solve(
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints) override;

    // Direct model solving
    core::SolverResult solveModel(const HighsModel& model);

    void setTimeout(long timeout_ms) override { timeout_ms_ = timeout_ms; }
    void setVerbosity(int level) override { verbosity_ = level; }

    long getLastSolveTime() const override { return last_solve_time_; }
    int getLastIterations() const override { return last_iterations_; }

    // Advanced options
    void setPresolve(bool enable);
    void setParallel(bool enable);
    void setMIPGap(double gap);  // Optimality tolerance for MILP

    // Solution extraction
    std::vector<double> getPrimalSolution() const;
    std::vector<double> getDualSolution() const;  // For sensitivity analysis
    double getObjectiveValue() const;

private:
    std::unique_ptr<Highs> highs_;
    long timeout_ms_ = 30000;
    int verbosity_ = 0;
    long last_solve_time_ = 0;
    int last_iterations_ = 0;
    std::vector<double> primal_solution_;
    std::vector<double> dual_solution_;
    double objective_value_ = 0.0;
};

}  // namespace lumen::solvers
```

### 2.3 Quantum Client

```cpp
// include/lumen/solvers/quantum_client.hpp

namespace lumen::solvers {

using QuboMatrix = Eigen::MatrixXd;  // Symmetric matrix for QUBO

struct QuantumSolverConfig {
    std::string provider;       // "dwave", "ibm", "ionq"
    std::string api_key_env;    // Environment variable name
    long timeout_ms = 60000;
    int num_reads = 1000;       // Number of samples for annealing
    bool use_hybrid = true;     // Use hybrid solver (recommended)
};

struct QuantumResult {
    bool success;
    std::string status;
    std::vector<int> best_sample;      // Binary solution
    double best_energy;                 // Objective value
    std::vector<std::vector<int>> all_samples;
    std::vector<double> all_energies;

    // Quantum-specific metrics
    double qpu_access_time_us;
    double total_time_ms;
    int num_occurrences;  // How many times best solution found
    double chain_break_fraction;  // For D-Wave

    nlohmann::json toJSON() const;
};

class QuboFormulator {
public:
    QuboFormulator(const core::Portfolio& portfolio,
                   const core::TargetAllocation& target);

    // Formulate QUBO from portfolio optimization problem
    QuboMatrix formulate(const core::ConstraintSet& constraints);

    // Set objective weights
    void setWeights(double drift_weight, double cost_weight, double tax_weight);

    // For tax-lot selection
    void enableTaxLotOptimization(const std::vector<data::TaxLot>& lots);

    // Penalty parameters for constraint enforcement
    void setConstraintPenalty(double penalty);

    // Solution interpretation
    std::vector<core::Trade> interpretSolution(const std::vector<int>& binary_solution) const;

    // Validation
    bool validateFormulation() const;
    int getNumQubits() const;

private:
    core::Portfolio portfolio_;
    core::TargetAllocation target_;
    double drift_weight_ = 1.0;
    double cost_weight_ = 0.1;
    double tax_weight_ = 0.5;
    double constraint_penalty_ = 1000.0;
    std::vector<data::TaxLot> tax_lots_;
    std::map<int, std::string> qubit_to_variable_;
};

class QuantumClient : public BaseSolver {
public:
    virtual ~QuantumClient() = default;

    virtual bool authenticate() = 0;
    virtual bool checkConnection() = 0;
    virtual double estimateCost(int num_qubits) const = 0;

    virtual QuantumResult submitQUBO(const QuboMatrix& Q, int num_reads = 1000) = 0;
};

class DWaveClient : public QuantumClient {
public:
    explicit DWaveClient(const QuantumSolverConfig& config);
    ~DWaveClient() override;

    std::string getName() const override { return "D-Wave"; }
    bool isAvailable() const override;

    bool authenticate() override;
    bool checkConnection() override;
    double estimateCost(int num_qubits) const override;

    core::SolverResult solve(
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints) override;

    QuantumResult submitQUBO(const QuboMatrix& Q, int num_reads = 1000) override;

    void setTimeout(long timeout_ms) override { config_.timeout_ms = timeout_ms; }
    void setVerbosity(int level) override { verbosity_ = level; }

    long getLastSolveTime() const override { return last_solve_time_; }
    int getLastIterations() const override { return last_iterations_; }

    // D-Wave specific
    void setNumReads(int reads) { config_.num_reads = reads; }
    void useHybridSolver(bool use) { config_.use_hybrid = use; }
    std::vector<std::string> getAvailableSolvers() const;

private:
    QuantumSolverConfig config_;
    int verbosity_ = 0;
    long last_solve_time_ = 0;
    int last_iterations_ = 0;
    std::string api_key_;
    std::string selected_solver_;

    // HTTP client for API calls
    std::unique_ptr<httplib::Client> http_client_;

    std::string buildRequestBody(const QuboMatrix& Q, int num_reads) const;
    QuantumResult parseResponse(const std::string& response) const;
};

class IBMQuantumClient : public QuantumClient {
public:
    explicit IBMQuantumClient(const QuantumSolverConfig& config);
    ~IBMQuantumClient() override;

    std::string getName() const override { return "IBM Quantum"; }
    bool isAvailable() const override;

    bool authenticate() override;
    bool checkConnection() override;
    double estimateCost(int num_qubits) const override;

    core::SolverResult solve(
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints) override;

    QuantumResult submitQUBO(const QuboMatrix& Q, int num_reads = 1000) override;

    void setTimeout(long timeout_ms) override { config_.timeout_ms = timeout_ms; }
    void setVerbosity(int level) override { verbosity_ = level; }

    long getLastSolveTime() const override { return last_solve_time_; }
    int getLastIterations() const override { return last_iterations_; }

    // IBM specific - QAOA parameters
    void setQAOADepth(int p) { qaoa_depth_ = p; }
    void setOptimizer(const std::string& optimizer) { optimizer_ = optimizer; }
    std::vector<std::string> getAvailableBackends() const;

private:
    QuantumSolverConfig config_;
    int verbosity_ = 0;
    long last_solve_time_ = 0;
    int last_iterations_ = 0;
    int qaoa_depth_ = 1;
    std::string optimizer_ = "COBYLA";
    std::string api_key_;
    std::unique_ptr<httplib::Client> http_client_;

    std::string buildQAOACircuit(const QuboMatrix& Q) const;
};

}  // namespace lumen::solvers
```

---

## 3. Data Module

**Location:** `include/lumen/data/` and `src/data/`

### 3.1 Market Data

```cpp
// include/lumen/data/market_data.hpp

namespace lumen::data {

struct PriceQuote {
    std::string ticker;
    double price;
    double bid;
    double ask;
    double volume;
    std::chrono::system_clock::time_point timestamp;
    std::string source;  // Provider name

    nlohmann::json toJSON() const;
};

struct OHLCV {
    std::chrono::system_clock::time_point date;
    double open;
    double high;
    double low;
    double close;
    long volume;
};

struct TimeSeries {
    std::string ticker;
    std::vector<OHLCV> data;
    std::string interval;  // "daily", "weekly", "monthly"

    // Analysis helpers
    Eigen::VectorXd getClosePrices() const;
    Eigen::VectorXd getReturns() const;  // Daily returns
    double getMean() const;
    double getStdDev() const;
    double getAnnualizedVolatility() const;
};

class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;

    virtual std::string getName() const = 0;
    virtual bool isAuthenticated() const = 0;
    virtual bool authenticate() = 0;

    virtual PriceQuote getCurrentPrice(const std::string& ticker) = 0;
    virtual std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers) = 0;
    virtual TimeSeries getHistoricalData(
        const std::string& ticker,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) = 0;

    // Rate limiting
    virtual int getRateLimitRemaining() const = 0;
    virtual void waitForRateLimit() = 0;
};

class AlphaVantageProvider : public MarketDataProvider {
public:
    AlphaVantageProvider();
    explicit AlphaVantageProvider(const std::string& api_key);

    std::string getName() const override { return "Alpha Vantage"; }
    bool isAuthenticated() const override { return !api_key_.empty(); }
    bool authenticate() override;

    PriceQuote getCurrentPrice(const std::string& ticker) override;
    std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers) override;
    TimeSeries getHistoricalData(
        const std::string& ticker,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) override;

    int getRateLimitRemaining() const override { return rate_limit_remaining_; }
    void waitForRateLimit() override;

    // Alpha Vantage specific
    TimeSeries getDailyAdjusted(const std::string& ticker);
    TimeSeries getIntraday(const std::string& ticker, const std::string& interval);

private:
    std::string api_key_;
    std::unique_ptr<httplib::Client> http_client_;
    int rate_limit_remaining_ = 5;  // Free tier: 5 calls/minute
    std::chrono::system_clock::time_point last_call_;

    nlohmann::json makeRequest(const std::string& function,
                               const std::map<std::string, std::string>& params);
    PriceQuote parseQuote(const nlohmann::json& j) const;
    TimeSeries parseTimeSeries(const nlohmann::json& j) const;
};

class YahooFinanceProvider : public MarketDataProvider {
public:
    YahooFinanceProvider();

    std::string getName() const override { return "Yahoo Finance"; }
    bool isAuthenticated() const override { return true; }  // No auth needed
    bool authenticate() override { return true; }

    PriceQuote getCurrentPrice(const std::string& ticker) override;
    std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers) override;
    TimeSeries getHistoricalData(
        const std::string& ticker,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) override;

    int getRateLimitRemaining() const override { return 100; }  // No strict limit
    void waitForRateLimit() override { /* No-op */ }

private:
    std::unique_ptr<httplib::Client> http_client_;

    std::string buildURL(const std::string& ticker, long period1, long period2) const;
    TimeSeries parseCSV(const std::string& csv_data) const;
};

class CovarianceCalculator {
public:
    // Calculate covariance matrix from historical returns
    static Eigen::MatrixXd calculate(const std::vector<TimeSeries>& series);

    // Calculate correlation matrix
    static Eigen::MatrixXd calculateCorrelation(const std::vector<TimeSeries>& series);

    // Expected returns (mean historical return)
    static Eigen::VectorXd calculateExpectedReturns(const std::vector<TimeSeries>& series);

    // Annualized metrics
    static Eigen::MatrixXd annualizeCovariance(const Eigen::MatrixXd& daily_cov);
    static Eigen::VectorXd annualizeReturns(const Eigen::VectorXd& daily_returns);

    // Risk metrics
    static double calculatePortfolioVariance(
        const Eigen::VectorXd& weights, const Eigen::MatrixXd& cov);
    static double calculatePortfolioVolatility(
        const Eigen::VectorXd& weights, const Eigen::MatrixXd& cov);
    static double calculateSharpeRatio(
        const Eigen::VectorXd& weights,
        const Eigen::VectorXd& returns,
        const Eigen::MatrixXd& cov,
        double risk_free_rate = 0.02);
};

class DataCache {
public:
    explicit DataCache(const std::string& db_path);
    ~DataCache();

    // Price caching
    std::optional<PriceQuote> getPrice(const std::string& ticker) const;
    void putPrice(const std::string& ticker, const PriceQuote& quote, int ttl_minutes = 60);
    bool isPriceStale(const std::string& ticker) const;

    // Time series caching
    std::optional<TimeSeries> getTimeSeries(const std::string& ticker,
                                            const std::string& interval) const;
    void putTimeSeries(const std::string& ticker, const TimeSeries& series,
                       int ttl_minutes = 1440);  // 24 hours

    // Cache management
    void purgeExpired();
    void clearAll();
    size_t getCacheSize() const;
    double getHitRate() const;

private:
    std::unique_ptr<SQLite::Database> db_;
    mutable int hits_ = 0;
    mutable int misses_ = 0;

    void initializeSchema();
};

class MarketDataClient {
public:
    explicit MarketDataClient(const utils::Configuration& config);

    // High-level API (uses cache + providers)
    PriceQuote getCurrentPrice(const std::string& ticker);
    std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers);
    TimeSeries getHistoricalData(
        const std::string& ticker, int days_back = 252);

    // Derived data
    Eigen::MatrixXd getCovarianceMatrix(const std::vector<std::string>& tickers);
    Eigen::VectorXd getExpectedReturns(const std::vector<std::string>& tickers);

    // Provider management
    void addProvider(std::unique_ptr<MarketDataProvider> provider);
    void setPrimaryProvider(const std::string& name);

    // Cache control
    void enableCache(bool enable);
    void setCacheTTL(int minutes);
    DataCache& getCache() { return cache_; }

private:
    std::vector<std::unique_ptr<MarketDataProvider>> providers_;
    std::string primary_provider_;
    DataCache cache_;
    bool cache_enabled_ = true;

    MarketDataProvider* getProvider(const std::string& name = "") const;
};

}  // namespace lumen::data
```

### 3.2 Tax Lot Management

```cpp
// include/lumen/data/tax_lot.hpp

namespace lumen::data {

enum class TaxLotMethod {
    FIFO,     // First In, First Out
    LIFO,     // Last In, First Out
    HIFO,     // Highest In, First Out (tax loss harvesting)
    LOFO,     // Lowest In, First Out
    SPEC_ID   // Specific Identification
};

enum class GainType {
    SHORT_TERM,  // Held < 1 year
    LONG_TERM    // Held >= 1 year
};

struct TaxLot {
    std::string id;              // Unique identifier
    std::string ticker;
    double shares;
    double cost_basis_per_share;
    std::chrono::system_clock::time_point purchase_date;
    std::optional<std::chrono::system_clock::time_point> sale_date;
    std::optional<double> sale_price;

    // Computed properties
    double getTotalCostBasis() const { return shares * cost_basis_per_share; }
    double getCurrentValue(double current_price) const { return shares * current_price; }
    double getUnrealizedGain(double current_price) const {
        return getCurrentValue(current_price) - getTotalCostBasis();
    }
    double getRealizedGain() const {
        if (sale_price.has_value()) {
            return (sale_price.value() - cost_basis_per_share) * shares;
        }
        return 0.0;
    }

    GainType getGainType() const {
        auto ref_date = sale_date.value_or(std::chrono::system_clock::now());
        auto days = std::chrono::duration_cast<std::chrono::hours>(
            ref_date - purchase_date).count() / 24;
        return (days > 365) ? GainType::LONG_TERM : GainType::SHORT_TERM;
    }

    bool isSold() const { return sale_date.has_value(); }

    nlohmann::json toJSON() const;
    static TaxLot fromJSON(const nlohmann::json& j);
};

struct WashSaleViolation {
    std::string lot_id;
    std::string replacement_ticker;
    std::chrono::system_clock::time_point violation_date;
    double disallowed_loss;
    std::string description;
};

class TaxLotManager {
public:
    TaxLotManager() = default;

    // Lot management
    void addLot(const TaxLot& lot);
    void removeLot(const std::string& lot_id);
    void updateLot(const std::string& lot_id, const TaxLot& updated);

    // Query
    const TaxLot& getLot(const std::string& lot_id) const;
    std::vector<TaxLot> getLotsForTicker(const std::string& ticker) const;
    std::vector<TaxLot> getUnsoldLots() const;
    std::vector<TaxLot> getSoldLots() const;
    std::vector<TaxLot> getAllLots() const;

    // Lot selection for selling
    std::vector<TaxLot> selectLotsToSell(const std::string& ticker,
                                          double shares_to_sell,
                                          TaxLotMethod method) const;

    // Tax calculations
    double getTotalCostBasis(const std::string& ticker) const;
    double getAverageCostBasis(const std::string& ticker) const;
    double getTotalUnrealizedGain(double current_price,
                                   const std::string& ticker) const;

    // Persistence
    void saveToDatabase(SQLite::Database& db);
    void loadFromDatabase(SQLite::Database& db);
    nlohmann::json toJSON() const;
    static TaxLotManager fromJSON(const nlohmann::json& j);

private:
    std::map<std::string, TaxLot> lots_;  // lot_id -> TaxLot
    std::map<std::string, std::vector<std::string>> ticker_lots_;  // ticker -> [lot_ids]
};

struct TaxHarvestingOpportunity {
    TaxLot lot;
    double current_price;
    double unrealized_loss;
    GainType gain_type;
    std::vector<std::string> replacement_candidates;  // Similar ETFs
    bool would_trigger_wash_sale;
};

struct CapitalGainsReport {
    double short_term_gains;
    double long_term_gains;
    double short_term_losses;
    double long_term_losses;
    double net_short_term;
    double net_long_term;
    double harvested_losses;
    double estimated_tax_savings;  // Based on assumed tax rates
    std::vector<core::Trade> trades;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

class TaxOptimizer {
public:
    TaxOptimizer(TaxLotManager& lot_manager, double tax_rate_short = 0.35,
                 double tax_rate_long = 0.15);

    // Tax-loss harvesting
    std::vector<TaxHarvestingOpportunity> findHarvestingOpportunities(
        const std::map<std::string, double>& current_prices,
        double min_loss_threshold = 100.0) const;

    // Wash sale detection
    bool wouldTriggerWashSale(const TaxLot& lot_to_sell,
                              const std::string& replacement_ticker,
                              const std::chrono::system_clock::time_point& trade_date) const;
    std::vector<WashSaleViolation> checkWashSaleViolations(
        const std::vector<core::Trade>& proposed_trades) const;

    // Optimal lot selection (for tax efficiency)
    std::vector<TaxLot> selectOptimalLots(const std::string& ticker,
                                           double shares_to_sell,
                                           double current_price,
                                           bool prefer_losses = true) const;

    // Capital gains calculation
    CapitalGainsReport calculateCapitalGains(
        const std::vector<core::Trade>& trades,
        const std::map<std::string, double>& prices) const;

    // Tax impact estimation
    double estimateTaxImpact(const std::vector<core::Trade>& trades,
                             const std::map<std::string, double>& prices) const;

    // Configuration
    void setTaxRates(double short_term, double long_term);
    void setReplacementCandidates(const std::string& ticker,
                                  const std::vector<std::string>& candidates);

private:
    TaxLotManager& lot_manager_;
    double tax_rate_short_;
    double tax_rate_long_;
    std::map<std::string, std::vector<std::string>> replacement_map_;  // Similar ETFs

    bool isWithinWashSalePeriod(
        const std::chrono::system_clock::time_point& sale_date,
        const std::chrono::system_clock::time_point& purchase_date) const;
};

}  // namespace lumen::data
```

---

## 4. Explain Module

**Location:** `include/lumen/explain/` and `src/explain/`

### 4.1 Provenance Tracking

```cpp
// include/lumen/explain/provenance.hpp

namespace lumen::explain {

struct DataSourceRecord {
    std::string provider_name;
    std::string endpoint;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> parameters;  // No API keys
    bool from_cache;
    int http_status;
    long latency_ms;

    nlohmann::json toJSON() const;
};

struct ConstraintRecord {
    std::string constraint_name;
    core::ConstraintType type;
    core::ConstraintStatus status;
    double slack_value;
    std::string description;

    nlohmann::json toJSON() const;
};

struct SolverRecord {
    std::string solver_name;
    std::string solver_version;
    long solve_time_ms;
    int iterations;
    std::string termination_status;
    double objective_value;
    double optimality_gap;
    std::map<std::string, std::string> solver_options;

    nlohmann::json toJSON() const;
};

class Provenance {
public:
    Provenance();

    // Recording
    void recordDataSource(const DataSourceRecord& record);
    void recordConstraint(const ConstraintRecord& record);
    void recordSolver(const SolverRecord& record);
    void recordAssumption(const std::string& assumption);
    void recordWarning(const std::string& warning);
    void setRunTimestamp(const std::chrono::system_clock::time_point& ts);
    void setSessionId(const std::string& id);

    // Query
    std::vector<DataSourceRecord> getDataSources() const { return data_sources_; }
    std::vector<ConstraintRecord> getConstraints() const { return constraints_; }
    SolverRecord getSolverInfo() const { return solver_; }
    std::vector<std::string> getAssumptions() const { return assumptions_; }
    std::vector<std::string> getWarnings() const { return warnings_; }
    std::string getSessionId() const { return session_id_; }
    std::chrono::system_clock::time_point getRunTimestamp() const { return run_timestamp_; }

    // Constraint status helpers
    std::vector<ConstraintRecord> getActiveConstraints() const;
    std::vector<ConstraintRecord> getBindingConstraints() const;
    std::vector<ConstraintRecord> getViolatedConstraints() const;

    // Serialization
    nlohmann::json toJSON() const;
    static Provenance fromJSON(const nlohmann::json& j);

private:
    std::string session_id_;
    std::chrono::system_clock::time_point run_timestamp_;
    std::vector<DataSourceRecord> data_sources_;
    std::vector<ConstraintRecord> constraints_;
    SolverRecord solver_;
    std::vector<std::string> assumptions_;
    std::vector<std::string> warnings_;
};

}  // namespace lumen::explain
```

### 4.2 Explainer

```cpp
// include/lumen/explain/explainer.hpp

namespace lumen::explain {

struct TradeRationale {
    core::Trade trade;
    std::vector<std::string> reasons;
    double impact_on_objective;
    std::vector<std::string> constraints_addressed;
    double before_allocation_pct;
    double after_allocation_pct;
    double target_allocation_pct;
    std::optional<data::CapitalGainsReport> tax_impact;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

struct AllocationComparison {
    std::map<std::string, double> before;
    std::map<std::string, double> after;
    std::map<std::string, double> target;
    std::map<std::string, double> deviation_before;
    std::map<std::string, double> deviation_after;
    double total_drift_before;
    double total_drift_after;
    double drift_reduction_pct;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

struct CostBreakdown {
    double total_transaction_cost;
    double commission_cost;
    double spread_cost;
    double market_impact_estimate;
    double tax_cost_estimate;
    std::map<std::string, double> per_trade_costs;

    nlohmann::json toJSON() const;
};

struct SensitivityResult {
    std::string parameter_name;
    double original_value;
    double changed_value;
    double change_percent;
    double new_objective_value;
    double objective_change_pct;
    std::string impact_description;

    nlohmann::json toJSON() const;
};

class SensitivityAnalysis {
public:
    SensitivityAnalysis(const core::SolverResult& result,
                        const core::Portfolio& portfolio,
                        const core::TargetAllocation& target);

    // Price sensitivity
    SensitivityResult priceShock(const std::string& ticker, double pct_change);
    std::vector<SensitivityResult> allPriceShocks(double pct_change);

    // Constraint sensitivity
    SensitivityResult relaxConstraint(const std::string& constraint_name, double delta);
    std::vector<SensitivityResult> allConstraintSensitivities();

    // Weight sensitivity
    SensitivityResult changeTargetWeight(const std::string& ticker, double new_weight);

    // Generate full report
    std::vector<SensitivityResult> runFullAnalysis();

private:
    core::SolverResult original_result_;
    core::Portfolio portfolio_;
    core::TargetAllocation target_;
};

enum class OutputFormat {
    JSON,
    PLAIN_TEXT,
    HTML,
    MARKDOWN
};

class ExplanationDocument {
public:
    ExplanationDocument();

    // Building the document
    void setSummary(const std::string& summary);
    void setAllocationComparison(const AllocationComparison& comparison);
    void addTradeRationale(const TradeRationale& rationale);
    void setCostBreakdown(const CostBreakdown& costs);
    void setProvenance(const Provenance& provenance);
    void addSensitivityResult(const SensitivityResult& result);
    void setTaxReport(const data::CapitalGainsReport& report);

    // Output
    std::string render(OutputFormat format) const;
    nlohmann::json toJSON() const;
    std::string toPlainText() const;
    std::string toHTML() const;
    std::string toMarkdown() const;

    void saveToFile(const std::string& filepath, OutputFormat format) const;

private:
    std::string summary_;
    AllocationComparison allocation_;
    std::vector<TradeRationale> trade_rationales_;
    CostBreakdown costs_;
    Provenance provenance_;
    std::vector<SensitivityResult> sensitivity_results_;
    std::optional<data::CapitalGainsReport> tax_report_;

    std::string renderHeader(OutputFormat format) const;
    std::string renderTrades(OutputFormat format) const;
    std::string renderConstraints(OutputFormat format) const;
};

class Explainer {
public:
    Explainer();

    // Generate explanations
    TradeRationale explainTrade(const core::Trade& trade,
                                 const core::Portfolio& portfolio,
                                 const core::TargetAllocation& target,
                                 const core::ConstraintSet& constraints,
                                 const core::SolverResult& result);

    AllocationComparison compareAllocations(
        const core::Portfolio& before,
        const std::vector<core::Trade>& trades,
        const core::TargetAllocation& target);

    CostBreakdown calculateCosts(const std::vector<core::Trade>& trades,
                                  double commission_per_trade = 0.0,
                                  double spread_estimate_bps = 5.0);

    // Full document generation
    ExplanationDocument generateFullExplanation(
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints,
        const core::SolverResult& result,
        const Provenance& provenance);

    // Configuration
    void setVerbosity(int level) { verbosity_ = level; }
    void enableSensitivityAnalysis(bool enable) { do_sensitivity_ = enable; }
    void setCommissionRate(double rate) { commission_rate_ = rate; }

private:
    int verbosity_ = 1;
    bool do_sensitivity_ = true;
    double commission_rate_ = 0.0;

    std::string generateTradeReason(const core::Trade& trade,
                                    const core::Portfolio& portfolio,
                                    const core::TargetAllocation& target);
    std::vector<std::string> identifyAddressedConstraints(
        const core::Trade& trade,
        const core::ConstraintSet& constraints);
};

}  // namespace lumen::explain
```

---

## 5. Utils Module

**Location:** `include/lumen/utils/` and `src/utils/`

### 5.1 Logging

```cpp
// include/lumen/utils/logging.hpp

namespace lumen::utils {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

struct LogContext {
    std::string component;
    std::string operation;
    std::map<std::string, std::string> tags;

    LogContext& set(const std::string& key, const std::string& value) {
        tags[key] = value;
        return *this;
    }
};

class Logger {
public:
    static Logger& getInstance();

    // Logging methods
    void trace(const std::string& message, const LogContext& ctx = {});
    void debug(const std::string& message, const LogContext& ctx = {});
    void info(const std::string& message, const LogContext& ctx = {});
    void warn(const std::string& message, const LogContext& ctx = {});
    void error(const std::string& message, const LogContext& ctx = {});
    void fatal(const std::string& message, const LogContext& ctx = {});

    // Metrics
    void recordMetric(const std::string& name, double value,
                      const std::string& unit = "");
    void recordTiming(const std::string& name, long milliseconds);
    void recordCounter(const std::string& name, int delta = 1);

    // Configuration
    void setLevel(LogLevel level);
    void setOutputFile(const std::string& filepath);
    void enableConsoleOutput(bool enable);
    void setRotationSize(size_t bytes);

private:
    Logger();
    ~Logger();

    LogLevel level_ = LogLevel::INFO;
    std::string log_file_;
    bool console_enabled_ = true;
    size_t rotation_size_ = 100 * 1024 * 1024;  // 100MB
    std::mutex mutex_;
    std::ofstream file_stream_;

    void write(LogLevel level, const std::string& message, const LogContext& ctx);
    std::string formatMessage(LogLevel level, const std::string& message,
                              const LogContext& ctx) const;
    void rotateIfNeeded();
};

// Convenience macros
#define LOG_TRACE(msg) lumen::utils::Logger::getInstance().trace(msg)
#define LOG_DEBUG(msg) lumen::utils::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) lumen::utils::Logger::getInstance().info(msg)
#define LOG_WARN(msg) lumen::utils::Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) lumen::utils::Logger::getInstance().error(msg)

}  // namespace lumen::utils
```

### 5.2 PyFlare Exporter

```cpp
// include/lumen/utils/pyflare.hpp

namespace lumen::utils {

enum class TelemetryEventType {
    SOLVER_RUN,
    QUANTUM_API_CALL,
    MARKET_DATA_FETCH,
    CONSTRAINT_CHECK,
    ERROR
};

struct TelemetryEvent {
    TelemetryEventType type;
    std::string session_id;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> string_fields;
    std::map<std::string, double> numeric_fields;
    std::map<std::string, bool> bool_fields;

    nlohmann::json toJSON() const;
};

class PyFlareExporter {
public:
    explicit PyFlareExporter(const std::string& log_directory);
    ~PyFlareExporter();

    // Event recording
    void recordEvent(const TelemetryEvent& event);

    // Convenience methods for common events
    void recordSolverRun(const std::string& session_id,
                         const std::string& solver_name,
                         int num_positions,
                         int num_constraints,
                         const std::string& problem_type,
                         long solve_time_ms,
                         const std::string& status,
                         double objective_value);

    void recordQuantumAPICall(const std::string& session_id,
                              const std::string& provider,
                              int num_qubits,
                              long latency_ms,
                              double qpu_time_us,
                              double estimated_cost,
                              const std::string& status);

    void recordMarketDataFetch(const std::string& session_id,
                               const std::string& provider,
                               int num_tickers,
                               long latency_ms,
                               bool cache_hit);

    void recordConstraintViolation(const std::string& session_id,
                                   const std::string& constraint_name,
                                   double violation_amount);

    void recordError(const std::string& session_id,
                     const std::string& error_type,
                     const std::string& message);

    // File management
    void flush();
    void rotate();
    std::string getCurrentLogFile() const { return current_file_; }

    // Configuration
    void setMaxFileSize(size_t bytes);
    void setBufferSize(int events);

private:
    std::string log_directory_;
    std::string current_file_;
    std::vector<TelemetryEvent> buffer_;
    int buffer_size_ = 100;
    size_t max_file_size_ = 50 * 1024 * 1024;  // 50MB
    std::mutex mutex_;

    void writeBuffer();
    std::string generateFilename() const;
};

}  // namespace lumen::utils
```

### 5.3 Configuration

```cpp
// include/lumen/utils/config.hpp

namespace lumen::utils {

struct SolverConfig {
    std::string default_classical_solver = "highs";
    long classical_timeout_ms = 30000;
    bool quantum_enabled = false;
    std::string quantum_provider = "dwave";
    long quantum_timeout_ms = 60000;
    int quantum_num_reads = 1000;
    bool quantum_fallback_to_classical = true;
};

struct MarketDataConfig {
    std::string primary_provider = "alpha_vantage";
    std::vector<std::string> fallback_providers = {"yahoo_finance"};
    int cache_ttl_minutes = 60;
    int history_days_default = 252;  // 1 year of trading days
};

struct ObservabilityConfig {
    bool pyflare_enabled = true;
    std::string pyflare_log_directory = "~/.lumen/logs/pyflare/";
    bool local_logging_enabled = true;
    std::string local_log_file = "~/.lumen/logs/lumen.log";
    size_t log_rotation_size_mb = 100;
    LogLevel log_level = LogLevel::INFO;
};

struct PersistenceConfig {
    std::string database_path = "~/.lumen/data/lumen.db";
    bool encryption_enabled = true;
};

class Configuration {
public:
    Configuration();
    explicit Configuration(const std::string& config_path);

    // Load/Save
    void loadFromFile(const std::string& filepath);
    void saveToFile(const std::string& filepath) const;
    void loadFromEnvironment();

    // Accessors
    const SolverConfig& getSolverConfig() const { return solver_; }
    const MarketDataConfig& getMarketDataConfig() const { return market_data_; }
    const ObservabilityConfig& getObservabilityConfig() const { return observability_; }
    const PersistenceConfig& getPersistenceConfig() const { return persistence_; }

    // Mutators
    SolverConfig& getSolverConfig() { return solver_; }
    MarketDataConfig& getMarketDataConfig() { return market_data_; }
    ObservabilityConfig& getObservabilityConfig() { return observability_; }
    PersistenceConfig& getPersistenceConfig() { return persistence_; }

    // API key retrieval (from environment)
    std::string getAPIKey(const std::string& env_var) const;

    // Path resolution (expand ~)
    static std::string resolvePath(const std::string& path);

    // Validation
    bool validate() const;
    std::vector<std::string> getValidationErrors() const;

private:
    SolverConfig solver_;
    MarketDataConfig market_data_;
    ObservabilityConfig observability_;
    PersistenceConfig persistence_;
    std::string config_path_;

    void setDefaults();
    void parseYAML(const YAML::Node& root);
};

}  // namespace lumen::utils
```

---

## 6. Applications Layer

### 6.1 CLI Application

```cpp
// apps/cli/main.cpp

#include <CLI/CLI.hpp>
#include "lumen/core/portfolio.hpp"
#include "lumen/core/solver_dispatcher.hpp"
// ... other includes

int main(int argc, char** argv) {
    CLI::App app{"Lumen - Quantum-Enhanced Portfolio Optimizer"};

    // Global options
    std::string config_path = "~/.lumen/config.yaml";
    app.add_option("-c,--config", config_path, "Configuration file path");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Enable verbose output");

    // Subcommand: optimize
    auto* optimize_cmd = app.add_subcommand("optimize", "Optimize portfolio allocation");
    std::string portfolio_file;
    std::string target_spec;
    std::string output_file = "result.json";

    optimize_cmd->add_option("-p,--portfolio", portfolio_file, "Portfolio CSV/JSON file")
        ->required();
    optimize_cmd->add_option("-t,--target", target_spec, "Target allocation (e.g., '60-40-stocks-bonds')");
    optimize_cmd->add_option("-o,--output", output_file, "Output file for results");

    // Subcommand: tax-harvest
    auto* tax_cmd = app.add_subcommand("tax-harvest", "Find tax-loss harvesting opportunities");
    std::string tax_portfolio;
    double tax_rate = 0.22;
    double min_loss = 100.0;

    tax_cmd->add_option("-p,--portfolio", tax_portfolio, "Portfolio file")->required();
    tax_cmd->add_option("--tax-rate", tax_rate, "Marginal tax rate");
    tax_cmd->add_option("--min-loss", min_loss, "Minimum loss threshold");

    // Subcommand: explain
    auto* explain_cmd = app.add_subcommand("explain", "Explain optimization results");
    std::string result_file;
    std::string explain_format = "text";

    explain_cmd->add_option("-r,--result", result_file, "Result JSON file")->required();
    explain_cmd->add_option("-f,--format", explain_format, "Output format (text, json, html)");

    // Subcommand: config
    auto* config_cmd = app.add_subcommand("config", "Manage configuration");
    bool show_config = false;
    config_cmd->add_flag("--show", show_config, "Display current configuration");

    CLI11_PARSE(app, argc, argv);

    // Load configuration
    utils::Configuration config(utils::Configuration::resolvePath(config_path));

    // Initialize logging
    auto& logger = utils::Logger::getInstance();
    if (verbose) {
        logger.setLevel(utils::LogLevel::DEBUG);
    }

    // Handle subcommands
    if (optimize_cmd->parsed()) {
        return runOptimize(config, portfolio_file, target_spec, output_file, verbose);
    } else if (tax_cmd->parsed()) {
        return runTaxHarvest(config, tax_portfolio, tax_rate, min_loss, verbose);
    } else if (explain_cmd->parsed()) {
        return runExplain(config, result_file, explain_format, verbose);
    } else if (config_cmd->parsed() && show_config) {
        return runShowConfig(config);
    }

    std::cout << app.help() << std::endl;
    return 0;
}
```

### 6.2 REST API Server

```cpp
// apps/server/main.cpp

#include <httplib.h>
#include "lumen/core/portfolio.hpp"
// ... other includes

class LumenServer {
public:
    explicit LumenServer(const utils::Configuration& config)
        : config_(config), dispatcher_(config) {}

    void run(int port = 8080) {
        httplib::Server svr;

        // POST /api/v1/optimize
        svr.Post("/api/v1/optimize", [this](const httplib::Request& req,
                                            httplib::Response& res) {
            handleOptimize(req, res);
        });

        // GET /api/v1/market/:ticker
        svr.Get(R"(/api/v1/market/(\w+))", [this](const httplib::Request& req,
                                                   httplib::Response& res) {
            std::string ticker = req.matches[1];
            handleMarketData(ticker, res);
        });

        // GET /api/v1/health
        svr.Get("/api/v1/health", [](const httplib::Request&,
                                     httplib::Response& res) {
            res.set_content(R"({"status": "ok"})", "application/json");
        });

        LOG_INFO("Starting server on port " + std::to_string(port));
        svr.listen("127.0.0.1", port);
    }

private:
    utils::Configuration config_;
    core::SolverDispatcher dispatcher_;
    data::MarketDataClient market_client_{config_};

    void handleOptimize(const httplib::Request& req, httplib::Response& res);
    void handleMarketData(const std::string& ticker, httplib::Response& res);
};
```

---

## Data Flow Diagrams

### Complete Optimization Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            USER REQUEST                                      │
│  "Rebalance $100k portfolio to 60/40 stocks/bonds, max 20% per position"    │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 1: INPUT PROCESSING (core/portfolio.hpp)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│  Portfolio::fromCSV(file)                                                   │
│  ├─ Parse ticker, shares, cost_basis for each position                     │
│  ├─ Validate data types and ranges                                         │
│  └─ Create Position objects                                                │
│                                                                             │
│  TargetAllocation::setTarget("stocks", 0.60)                               │
│  TargetAllocation::setTarget("bonds", 0.40)                                │
│                                                                             │
│  ConstraintSet::addConstraint(PositionLimitConstraint(0.20))               │
│  ConstraintSet::addConstraint(BudgetConstraint(100000))                    │
│  ConstraintSet::validateConsistency() → true                               │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 2: MARKET DATA RETRIEVAL (data/market_data.hpp)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  MarketDataClient::getBatchPrices(tickers)                                 │
│  ├─ Check DataCache for each ticker                                        │
│  │   ├─ Cache HIT → Return cached PriceQuote                              │
│  │   └─ Cache MISS → Continue to provider                                 │
│  ├─ AlphaVantageProvider::getBatchPrices(missing_tickers)                  │
│  │   ├─ Build API request (no API key in logs)                            │
│  │   ├─ HTTP GET https://alphavantage.co/query?function=...               │
│  │   └─ Parse JSON response → PriceQuote objects                          │
│  └─ DataCache::putPrice(ticker, quote, ttl=60)                            │
│                                                                             │
│  Record: DataSourceRecord{provider="Alpha Vantage", latency=234ms}         │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 3: PORTFOLIO VALUATION                                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  Portfolio::updateAllPrices(prices)                                        │
│  Portfolio::getTotalValue() → $100,000                                     │
│  Portfolio::getAssetClassExposure()                                        │
│    → {stocks: 65%, bonds: 35%}  // Currently off-target                    │
│  Portfolio::calculateDrift(target) → 0.10  // 10% total drift              │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 4: PROBLEM CLASSIFICATION (core/solver_dispatcher.hpp)                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  SolverDispatcher::classifyProblem(portfolio, constraints)                 │
│  ├─ num_positions = 15                                                     │
│  ├─ num_constraints = 5                                                    │
│  ├─ has_integer_constraints = true (whole shares)                          │
│  ├─ has_tax_optimization = false                                           │
│  └─ problem_type = MILP                                                    │
│                                                                             │
│  SolverDispatcher::selectTier(characteristics)                             │
│  └─ 15 positions < 20 threshold → TIER_1 (Classical only)                 │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 5: CONSTRAINT FORMULATION (solvers/highs_wrapper.hpp)                   │
├─────────────────────────────────────────────────────────────────────────────┤
│  MILPBuilder builder(portfolio, target)                                    │
│                                                                             │
│  Variables (trade quantities for each position):                           │
│    x_VTI ∈ [-100, 100] (integer)  // shares to buy(+) or sell(-)          │
│    x_BND ∈ [-100, 100] (integer)                                          │
│    ...                                                                     │
│                                                                             │
│  Objective: minimize Σ(deviation_i²) + 0.1·(transaction_cost)              │
│    deviation_i = |current_alloc_i + trade_i - target_alloc_i|             │
│                                                                             │
│  Constraints:                                                               │
│    Budget: Σ(position_value_i) = $100,000                                  │
│    Position limits: position_i / total ≤ 0.20 for all i                   │
│    Asset class: Σ(stocks) / total = 0.60                                   │
│    Asset class: Σ(bonds) / total = 0.40                                    │
│    Integer: x_i ∈ ℤ for all i                                              │
│                                                                             │
│  builder.setObjective(MULTI_OBJECTIVE, {drift: 1.0, cost: 0.1})           │
│  builder.addConstraintSet(constraints)                                     │
│  HighsModel model = builder.build()                                        │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 6: SOLVER EXECUTION (solvers/highs_wrapper.hpp)                         │
├─────────────────────────────────────────────────────────────────────────────┤
│  HighsOptimizer optimizer                                                  │
│  optimizer.setTimeout(30000)                                               │
│  optimizer.setMIPGap(0.01)  // 1% optimality tolerance                    │
│                                                                             │
│  SolverResult result = optimizer.solveModel(model)                         │
│  ├─ HiGHS presolve reduces problem size                                   │
│  ├─ Branch-and-bound search for integer solution                          │
│  ├─ 47 iterations                                                          │
│  └─ Solve time: 234ms                                                      │
│                                                                             │
│  Result:                                                                    │
│    status = "optimal"                                                       │
│    objective_value = 0.0023                                                │
│    x_VTI = +15 (buy 15 shares)                                            │
│    x_BND = -8 (sell 8 shares)                                             │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 7: SOLUTION POST-PROCESSING                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  MILPBuilder::extractTrades(solution) → vector<Trade>                      │
│    Trade{ticker: "VTI", action: BUY, shares: 15, price: $250}             │
│    Trade{ticker: "BND", action: SELL, shares: 8, price: $100}             │
│                                                                             │
│  Validate constraints satisfied:                                           │
│    ConstraintSet::areAllSatisfied(portfolio, trades) → true               │
│                                                                             │
│  Calculate transaction costs:                                               │
│    15 * $250 * 0.001 + 8 * $100 * 0.001 = $4.55                          │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 8: EXPLANATION GENERATION (explain/explainer.hpp)                       │
├─────────────────────────────────────────────────────────────────────────────┤
│  Explainer::generateFullExplanation(portfolio, target, constraints, result)│
│                                                                             │
│  TradeRationale for VTI:                                                   │
│    reasons: ["Increases stock allocation from 63% to 65%",                │
│              "Moves toward 60% target",                                    │
│              "Position remains under 20% limit (18%)"]                    │
│    impact_on_objective: -0.0015                                            │
│                                                                             │
│  AllocationComparison:                                                      │
│    before: {stocks: 65%, bonds: 35%}                                       │
│    after: {stocks: 60%, bonds: 40%}                                        │
│    target: {stocks: 60%, bonds: 40%}                                       │
│    drift_reduction: 100%                                                    │
│                                                                             │
│  CostBreakdown:                                                             │
│    total_transaction_cost: $4.55                                           │
│    commission: $0 (commission-free)                                        │
│    spread_estimate: $4.55                                                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 9: PROVENANCE & TELEMETRY (explain/provenance.hpp, utils/pyflare.hpp)   │
├─────────────────────────────────────────────────────────────────────────────┤
│  Provenance provenance                                                     │
│  provenance.recordDataSource({provider: "Alpha Vantage", latency: 234ms}) │
│  provenance.recordSolver({solver: "HiGHS", time: 234ms, iterations: 47})  │
│  provenance.recordConstraint({name: "Budget", status: BINDING})           │
│  provenance.recordConstraint({name: "Position Limit", status: SLACK})     │
│                                                                             │
│  PyFlareExporter::recordSolverRun(session_id, "HiGHS", 15, 5,             │
│                                    "MILP", 234, "optimal", 0.0023)        │
│  → Writes to ~/.lumen/logs/pyflare/2026-01-19-session-abc123.json      │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ STEP 10: OUTPUT                                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│  ExplanationDocument doc = explainer.generateFullExplanation(...)          │
│  doc.render(OutputFormat::JSON) →                                          │
│                                                                             │
│  {                                                                          │
│    "status": "optimal",                                                     │
│    "trades": [                                                              │
│      {"ticker": "VTI", "action": "BUY", "shares": 15,                      │
│       "rationale": "Increases stock allocation toward 60% target"},        │
│      {"ticker": "BND", "action": "SELL", "shares": 8,                      │
│       "rationale": "Reduces bond allocation toward 40% target"}           │
│    ],                                                                       │
│    "allocation": {                                                          │
│      "before": {"stocks": "65%", "bonds": "35%"},                         │
│      "after": {"stocks": "60%", "bonds": "40%"}                           │
│    },                                                                       │
│    "transaction_cost": "$4.55",                                            │
│    "constraints_satisfied": true,                                          │
│    "solver": {"name": "HiGHS", "time_ms": 234}                            │
│  }                                                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Database Schema

```sql
-- ~/.lumen/data/lumen.db

-- Portfolio snapshots
CREATE TABLE portfolio_snapshots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id TEXT UNIQUE NOT NULL,
    name TEXT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    total_value REAL,
    currency TEXT DEFAULT 'USD',
    data_json TEXT NOT NULL  -- Full portfolio as JSON
);

CREATE INDEX idx_snapshots_timestamp ON portfolio_snapshots(timestamp);

-- Individual positions (for querying)
CREATE TABLE positions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    snapshot_id TEXT NOT NULL,
    ticker TEXT NOT NULL,
    shares REAL NOT NULL,
    current_price REAL,
    cost_basis REAL,
    asset_class TEXT,
    FOREIGN KEY (snapshot_id) REFERENCES portfolio_snapshots(snapshot_id)
);

CREATE INDEX idx_positions_snapshot ON positions(snapshot_id);
CREATE INDEX idx_positions_ticker ON positions(ticker);

-- Tax lots
CREATE TABLE tax_lots (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    lot_id TEXT UNIQUE NOT NULL,
    ticker TEXT NOT NULL,
    shares REAL NOT NULL,
    cost_basis_per_share REAL NOT NULL,
    purchase_date DATE NOT NULL,
    sale_date DATE,
    sale_price REAL,
    capital_gain REAL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_lots_ticker ON tax_lots(ticker);
CREATE INDEX idx_lots_unsold ON tax_lots(sale_date) WHERE sale_date IS NULL;

-- Market data cache
CREATE TABLE market_cache (
    ticker TEXT PRIMARY KEY,
    price REAL NOT NULL,
    bid REAL,
    ask REAL,
    volume REAL,
    source TEXT,
    fetched_at DATETIME NOT NULL,
    expires_at DATETIME NOT NULL
);

CREATE INDEX idx_cache_expires ON market_cache(expires_at);

-- Historical price data cache
CREATE TABLE historical_cache (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ticker TEXT NOT NULL,
    interval TEXT NOT NULL,  -- 'daily', 'weekly', 'monthly'
    date DATE NOT NULL,
    open REAL,
    high REAL,
    low REAL,
    close REAL NOT NULL,
    volume INTEGER,
    fetched_at DATETIME NOT NULL,
    UNIQUE(ticker, interval, date)
);

CREATE INDEX idx_historical_ticker ON historical_cache(ticker, interval);

-- Optimization runs
CREATE TABLE optimization_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id TEXT UNIQUE NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    portfolio_snapshot_id TEXT,
    solver_used TEXT NOT NULL,
    solve_time_ms INTEGER,
    status TEXT NOT NULL,
    objective_value REAL,
    num_trades INTEGER,
    result_json TEXT,  -- Full SolverResult
    explanation_json TEXT,  -- Full ExplanationDocument
    FOREIGN KEY (portfolio_snapshot_id) REFERENCES portfolio_snapshots(snapshot_id)
);

CREATE INDEX idx_runs_timestamp ON optimization_runs(timestamp);

-- User preferences
CREATE TABLE preferences (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

---

## Build Configuration

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(lumen
    VERSION 1.0.0
    DESCRIPTION "Quantum-Enhanced Portfolio Optimizer"
    LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_QUANTUM "Enable quantum solver support" ON)
option(ENABLE_PYFLARE "Enable PyFlare telemetry" ON)

# Find dependencies
find_package(Eigen3 3.4 REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(SQLite3 REQUIRED)

# HiGHS (may need custom find module)
find_package(HIGHS REQUIRED)

# SymEngine
find_package(SymEngine REQUIRED)

# cpp-httplib (header-only)
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.0
)
FetchContent_MakeAvailable(httplib)

# CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.3.2
)
FetchContent_MakeAvailable(cli11)

# yaml-cpp
find_package(yaml-cpp REQUIRED)

# Library target
add_library(lumen-core STATIC
    src/core/portfolio.cpp
    src/core/constraint.cpp
    src/core/solver_dispatcher.cpp
    src/solvers/highs_wrapper.cpp
    src/solvers/quantum_client.cpp
    src/data/market_data.cpp
    src/data/tax_lot.cpp
    src/explain/provenance.cpp
    src/explain/explainer.cpp
    src/utils/logging.cpp
    src/utils/config.cpp
    src/utils/pyflare.cpp
)

target_include_directories(lumen-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(lumen-core PUBLIC
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
    highs::highs
    symengine
    httplib::httplib
    yaml-cpp
)

# CLI executable
add_executable(lumen-cli apps/cli/main.cpp)
target_link_libraries(lumen-cli PRIVATE lumen-core CLI11::CLI11)

# Server executable
add_executable(lumen-server apps/server/main.cpp)
target_link_libraries(lumen-server PRIVATE lumen-core)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Installation
install(TARGETS lumen-core lumen-cli lumen-server
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
```

---

## Design Patterns

### 1. Strategy Pattern (Solver Selection)

```cpp
// BaseSolver defines interface; HighsOptimizer, DWaveClient implement it
class BaseSolver {
    virtual SolverResult solve(...) = 0;
};

class HighsOptimizer : public BaseSolver { ... };
class DWaveClient : public BaseSolver { ... };

// SolverDispatcher selects strategy at runtime
SolverDispatcher::dispatch() {
    if (tier == TIER_1) return classical_solver_->solve(...);
    if (tier == TIER_3) return quantum_solver_->solve(...);
}
```

### 2. Factory Pattern (Constraint Creation)

```cpp
class ConstraintFactory {
public:
    static std::unique_ptr<Constraint> create(ConstraintType type,
                                               const nlohmann::json& params) {
        switch (type) {
            case BUDGET: return std::make_unique<BudgetConstraint>(...);
            case ALLOCATION: return std::make_unique<AllocationConstraint>(...);
            // ...
        }
    }
};
```

### 3. Composite Pattern (ConstraintSet)

```cpp
class ConstraintSet {
    std::vector<std::unique_ptr<Constraint>> constraints_;

    void addAllToModel(HighsModel& model) {
        for (const auto& c : constraints_) {
            c->addToModel(model);  // Treat uniformly
        }
    }
};
```

### 4. Observer Pattern (Telemetry)

```cpp
class SolverObserver {
    virtual void onSolveStart(const ProblemCharacteristics&) = 0;
    virtual void onSolveComplete(const SolverResult&) = 0;
};

class PyFlareObserver : public SolverObserver {
    void onSolveComplete(const SolverResult& r) override {
        exporter_.recordSolverRun(...);
    }
};
```

### 5. Builder Pattern (MILP/QP Construction)

```cpp
MILPBuilder builder(portfolio, target);
builder.setObjective(MINIMIZE_DRIFT)
       .addBudgetConstraint(100000)
       .addAllocationBounds(bounds)
       .addIntegerConstraints(tickers);
HighsModel model = builder.build();
```

### 6. Adapter Pattern (External Libraries)

```cpp
// HighsOptimizer adapts HiGHS library to our BaseSolver interface
class HighsOptimizer : public BaseSolver {
    std::unique_ptr<Highs> highs_;  // External library

    SolverResult solve(...) override {
        // Translate our types to HiGHS types
        HighsLp lp = buildHighsLP(...);
        highs_->passLp(lp);
        highs_->run();
        // Translate back to our types
        return extractResult();
    }
};
```

---

## Security Architecture

### Data Protection

```
┌─────────────────────────────────────────────────────────────────┐
│                     LOCAL DEVICE                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Portfolio Data ──────► SQLite DB ──────► AES-256 Encryption    │
│  (user's holdings)      (lumen.db)      (optional, via SQLCipher)
│                                                                  │
│  API Keys ──────► Environment Variables                         │
│  (NEVER in config files, NEVER in logs)                         │
│                                                                  │
│  Telemetry Logs ──────► No PII                                  │
│  (PyFlare events)       (only solver metrics, no portfolio data)│
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ HTTPS only
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                     EXTERNAL SERVICES                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Market Data APIs:                                               │
│  ├─ Only ticker symbols sent (no portfolio context)             │
│  └─ Responses cached locally                                    │
│                                                                  │
│  Quantum Cloud (D-Wave, IBM):                                   │
│  ├─ Only QUBO matrix sent (anonymized optimization problem)     │
│  ├─ No ticker names, no dollar amounts                          │
│  └─ Binary decision variables only                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### API Key Handling

```cpp
// GOOD: Load from environment
std::string api_key = std::getenv("DWAVE_API_KEY");

// BAD: Never do this
std::string api_key = "sk-1234567890";  // NEVER hardcode
config["api_key"] = api_key;             // NEVER store in config
LOG_INFO("API key: " + api_key);         // NEVER log
```

### Input Validation

```cpp
// All user inputs validated before processing
bool Portfolio::addPosition(const std::string& ticker, double shares, double cost_basis) {
    // Validate ticker format
    if (!isValidTicker(ticker)) {
        throw InvalidInputException("Invalid ticker format: " + ticker);
    }

    // Validate numeric ranges
    if (shares <= 0 || shares > 1e9) {
        throw InvalidInputException("Shares must be positive and reasonable");
    }

    if (cost_basis < 0 || cost_basis > 1e6) {
        throw InvalidInputException("Cost basis must be non-negative");
    }

    // Sanitize ticker (uppercase, alphanumeric only)
    std::string sanitized = sanitizeTicker(ticker);

    // ... proceed with validated input
}
```

---

## Performance Specifications

### Response Time Targets

| Operation | Target | Implementation |
|-----------|--------|----------------|
| Small portfolio optimization (<20 positions) | <1 second | HiGHS MILP, no quantum |
| Medium portfolio (20-50 positions) | 1-5 seconds | HiGHS with presolve |
| Large portfolio (50+ positions) | 5-30 seconds | Quantum + classical hybrid |
| Market data fetch (single ticker) | <2 seconds | API + cache |
| Market data fetch (batch, 50 tickers) | <5 seconds | Parallel requests |
| Cache lookup | <10 ms | SQLite indexed query |

### Memory Targets

| Scenario | Target |
|----------|--------|
| Idle | <50 MB |
| Small optimization | <100 MB |
| Large optimization (100 positions) | <500 MB |
| Historical data (1 year, 50 tickers) | <200 MB |

### Solver Performance

```cpp
// HiGHS configuration for optimal performance
HighsOptimizer::HighsOptimizer() {
    highs_ = std::make_unique<Highs>();

    // Enable presolve (reduces problem size)
    highs_->setOptionValue("presolve", "on");

    // Enable parallel execution
    highs_->setOptionValue("parallel", "on");
    highs_->setOptionValue("threads", 4);

    // MIP settings
    highs_->setOptionValue("mip_rel_gap", 0.01);  // 1% optimality gap OK
    highs_->setOptionValue("mip_max_nodes", 100000);
}
```

### Cache Strategy

```cpp
// Market data caching policy
class DataCache {
    // Price quotes: 60 minute TTL (market hours)
    // Historical data: 24 hour TTL
    // Covariance matrices: Recompute on portfolio change

    void putPrice(const std::string& ticker, const PriceQuote& quote) {
        int ttl = isMarketHours() ? 5 : 60;  // More frequent during market
        // ...
    }
};
```

---

## References

- [Lumen_Project_Overview.md](Lumen_Project_Overview.md) - Business context and MVP features
- [Lumen_Top_Level_Architecture.md](Lumen_Top_Level_Architecture.md) - System overview
- [HiGHS Documentation](https://highs.dev/)
- [SymEngine Documentation](https://symengine.org/)
- [D-Wave Ocean SDK](https://docs.ocean.dwavesys.com/)
- [Eigen Linear Algebra](https://eigen.tuxfamily.org/)
