# Lumen Developer Guide

## Overview

This guide provides comprehensive documentation for developers who want to contribute to, extend, or integrate with the Lumen portfolio optimization engine. It covers architecture, code organization, APIs, and development workflows.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Project Structure](#project-structure)
3. [Core Modules](#core-modules)
4. [Market Data Module](#market-data-module)
5. [Persistence Module](#persistence-module)
6. [Explainability Module](#explainability-module)
7. [Tax Optimization Module](#tax-optimization-module)
8. [Broker Import Module](#broker-import-module)
9. [Quantum Integration Module](#quantum-integration-module)
10. [GUI Module](#gui-module)
11. [Building from Source](#building-from-source)
12. [Development Workflow](#development-workflow)
13. [API Reference](#api-reference)
14. [Adding New Features](#adding-new-features)
15. [Testing](#testing)
16. [Performance Optimization](#performance-optimization)
17. [Security Considerations](#security-considerations)
18. [Code Style Guide](#code-style-guide)

---

## Architecture Overview

### System Design

Lumen follows a modular, layered architecture:

```
┌─────────────────────────────────────────────────────────────┐
│                    Applications Layer                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │    CLI      │  │   Server    │  │    GUI      │          │
│  │ (lumen-cli) │  │(lumen-server│  │ (lumen-gui) │          │
│  └─────────────┘  └─────────────┘  └─────────────┘          │
├─────────────────────────────────────────────────────────────┤
│                      Core Library                            │
│  ┌─────────────────────────────────────────────────────┐    │
│  │                  lumen-core                          │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐           │    │
│  │  │   Core   │  │ Solvers  │  │   Data   │           │    │
│  │  │ Module   │  │  Module  │  │  Module  │           │    │
│  │  └──────────┘  └──────────┘  └──────────┘           │    │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐           │    │
│  │  │ Explain  │  │  Utils   │  │ Persist  │           │    │
│  │  │ Module   │  │  Module  │  │  Module  │           │    │
│  │  └──────────┘  └──────────┘  └──────────┘           │    │
│  └─────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│                    External Dependencies                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │  HiGHS   │  │ SQLite   │  │  Eigen   │  │  JSON    │    │
│  │ Solver   │  │ Database │  │  Linear  │  │ Library  │    │
│  └──────────┘  └──────────┘  │  Algebra │  └──────────┘    │
│  ┌──────────┐  ┌──────────┐  └──────────┘                   │
│  │cpp-httplib│  │ yaml-cpp │                                │
│  │  HTTP    │  │   YAML   │                                │
│  └──────────┘  └──────────┘                                │
└─────────────────────────────────────────────────────────────┘
```

### Key Design Principles

1. **Edge-First Computing:** Portfolio data and optimization run locally
2. **Modular Design:** Each module is self-contained with clear interfaces
3. **Extensibility:** Easy to add new constraints, solvers, or data sources
4. **Security:** Input validation, path sanitization, no data exfiltration
5. **Explainability:** Solutions include rationale and provenance tracking
6. **Persistence:** Full audit trail with optimization history

### Data Flow

```
                                  ┌─────────────────┐
                                  │   Market Data   │
                                  │    Provider     │
                                  └────────┬────────┘
                                           │
Portfolio Data ─┐                          ▼
                ├──► Problem Classification ──► Solver Selection
Target Alloc.  ─┤                                     │
                │                                     ▼
Constraints ────┘                              Optimization
                                                     │
                ┌────────────────────────────────────┤
                │                                    │
                ▼                                    ▼
          Persistence                         Trade Extraction
          (SQLite DB)                               │
                │                                   ▼
                │                            Result + Explanation
                │                                   │
                └───────────► Provenance ◄──────────┘
```

---

## Project Structure

```
lumen/
├── include/lumen/           # Public header files (API)
│   ├── core/               # Core domain types
│   │   ├── portfolio.hpp   # Portfolio, Position, TargetAllocation
│   │   ├── constraint.hpp  # Constraint system
│   │   └── solver_dispatcher.hpp  # Problem routing
│   ├── solvers/            # Solver interfaces
│   │   ├── highs_wrapper.hpp    # HiGHS LP/MILP/QP
│   │   └── quantum_client.hpp   # Quantum solver clients
│   ├── data/               # Data management
│   │   ├── market_data.hpp      # Market data retrieval
│   │   ├── market_data_client.hpp  # HTTP client for market APIs
│   │   ├── persistence.hpp      # Database persistence
│   │   ├── tax_lot.hpp          # Tax lot management and optimization
│   │   └── broker_import.hpp    # Broker-specific tax lot import
│   ├── explain/            # Explainability
│   │   ├── provenance.hpp       # Data provenance tracking
│   │   └── explainer.hpp        # Result explanation generator
│   └── utils/              # Utilities
│       ├── logging.hpp          # Structured logging
│       ├── config.hpp           # Configuration
│       └── pyflare.hpp          # Telemetry
│
├── src/                    # Implementation files
│   ├── core/               # Core implementations
│   ├── solvers/            # Solver implementations
│   ├── data/               # Data implementations
│   │   ├── market_data_client.cpp
│   │   └── persistence.cpp
│   ├── explain/            # Explain implementations
│   │   ├── provenance.cpp
│   │   └── explainer.cpp
│   ├── utils/              # Utility implementations
│   ├── cli/                # CLI application
│   │   └── main.cpp
│   └── server/             # REST API server
│       └── main.cpp
│
├── tests/                  # Unit tests (Google Test)
│   ├── core/
│   ├── data/
│   ├── explain/
│   └── utils/
│
├── test/                   # Additional tests
│   ├── unit/               # Unit test suites
│   ├── integration/        # Integration tests
│   └── benchmarks/         # Performance benchmarks
│
├── cmake/                  # CMake modules
├── docs/                   # Documentation
├── CMakeLists.txt          # Build configuration
├── vcpkg.json              # Dependencies
└── .clang-format           # Code style
```

### Namespace Organization

```cpp
namespace lumen {
    namespace core {
        // Portfolio, Position, Constraint, Trade, SolverResult
        // TaxLotConstraint, WashSaleConstraint (tax-specific constraints)
    }
    namespace solvers {
        // HighsOptimizer, QuantumClient, MILPBuilder, QPBuilder
        // TaxConfig (tax-aware optimization configuration)
    }
    namespace data {
        // MarketDataClient, MarketDataProvider
        // PersistenceManager, DatabaseConnection
        // TaxLot, TaxLotManager, TaxOptimizer, TaxLotMethod, GainType
        // TaxHarvestingOpportunity, CapitalGainsReport, WashSaleViolation
        // BrokerImporter, BrokerImportFactory, TaxLotExporter
        // GenericCSVImporter, SchwabImporter, FidelityImporter, VanguardImporter
    }
    namespace explain {
        // Provenance, DataSourceRecord, ConstraintRecord, SolverRecord
        // Explainer, TradeRationale, AllocationComparison
        // CostBreakdown, SensitivityAnalysis, ExplanationDocument
    }
    namespace utils {
        // Logger, Configuration
    }
}
```

---

## Core Modules

### Core Module (`lumen::core`)

The core module defines the fundamental domain types.

#### Portfolio and Position

```cpp
// include/lumen/core/portfolio.hpp

enum class AssetClass {
    STOCKS, BONDS, CASH, COMMODITIES, REAL_ESTATE, CRYPTO, OTHER
};

struct Position {
    std::string ticker;
    double shares;
    double current_price;
    double cost_basis;
    std::chrono::system_clock::time_point purchase_date;
    AssetClass asset_class;
    std::string exchange;
    bool supports_fractional;

    double getCurrentValue() const;
    double getUnrealizedGain() const;
    bool isLongTerm() const;

    nlohmann::json toJSON() const;
    static Position fromJSON(const nlohmann::json& j);
};

class Portfolio {
public:
    void addPosition(const Position& pos);
    void removePosition(const std::string& ticker);
    void updatePrice(const std::string& ticker, double price);

    double getTotalValue() const;
    double getAllocationPercent(const std::string& ticker) const;
    double calculateDrift(const TargetAllocation& target) const;
    bool needsRebalancing(const TargetAllocation& target) const;

    static Portfolio fromCSV(const std::string& filepath);
    static Portfolio fromJSON(const nlohmann::json& j);
    void saveToFile(const std::string& filepath) const;

private:
    std::map<std::string, Position> positions_;
    double cash_balance_ = 0.0;
};
```

#### Constraint System

```cpp
// include/lumen/core/constraint.hpp

enum class ConstraintType {
    BUDGET, ALLOCATION, MIN_TRADE, INTEGER_SHARES,
    TAX_LOT, WASH_SALE, POSITION_LIMIT, SECTOR_LIMIT, CUSTOM
};

// Abstract base class
class Constraint {
public:
    virtual ~Constraint() = default;
    virtual ConstraintType getType() const = 0;
    virtual std::string getName() const = 0;
    virtual bool isValid() const = 0;
    virtual bool isSatisfied(const Portfolio& portfolio,
                             const std::vector<Trade>& trades) const = 0;
    virtual nlohmann::json toJSON() const = 0;
};

// Concrete constraints
class BudgetConstraint : public Constraint { /* ... */ };
class AllocationConstraint : public Constraint { /* ... */ };
class MinTradeSizeConstraint : public Constraint { /* ... */ };
class IntegerShareConstraint : public Constraint { /* ... */ };
class PositionLimitConstraint : public Constraint { /* ... */ };
class TaxLotConstraint : public Constraint { /* ... */ };      // Tax lot selection method
class WashSaleConstraint : public Constraint { /* ... */ };    // Wash sale rule compliance

// Constraint collection
class ConstraintSet {
public:
    void addConstraint(std::unique_ptr<Constraint> constraint);
    void removeConstraint(const std::string& name);
    bool validateConsistency() const;
    std::vector<std::string> getInconsistencies() const;

    static ConstraintSet fromJSON(const nlohmann::json& j);
    nlohmann::json toJSON() const;
};
```

#### Solver Dispatcher

```cpp
// include/lumen/core/solver_dispatcher.hpp

enum class ProblemType { LP, MILP, QP, QUBO };
enum class SolverTier { TIER_1, TIER_2, TIER_3 };

struct Trade {
    std::string ticker;
    enum class Action { BUY, SELL, HOLD } action;
    double shares;
    double price;
    double amount;
    double transaction_cost;
    std::string rationale;

    static Trade fromJSON(const nlohmann::json& j);
    nlohmann::json toJSON() const;
};

struct SolverResult {
    bool success;
    std::string status;  // "optimal", "feasible", "infeasible", "timeout"
    std::string session_id;  // Unique ID for this optimization run
    double objective_value;
    std::vector<Trade> trades;
    std::map<std::string, double> final_allocation;
    double total_transaction_cost;
    std::string solver_used;
    long solve_time_ms;
};

class SolverDispatcher {
public:
    explicit SolverDispatcher(const utils::Configuration& config);

    SolverResult dispatch(const Portfolio& portfolio,
                          const TargetAllocation& target,
                          const ConstraintSet& constraints);

    ProblemCharacteristics classifyProblem(const Portfolio& portfolio,
                                           const ConstraintSet& constraints) const;

    // Generate unique session ID for tracking
    static std::string generateSessionId();
};
```

### Solvers Module (`lumen::solvers`)

The solvers module provides optimization engines.

#### MILP Builder

```cpp
// include/lumen/solvers/highs_wrapper.hpp

enum class ObjectiveType {
    MINIMIZE_DRIFT, MINIMIZE_COST, MINIMIZE_TAX, MULTI_OBJECTIVE
};

struct ObjectiveWeights {
    double drift_weight = 1.0;
    double cost_weight = 0.1;
    double tax_weight = 0.0;
    void normalize();
};

class MILPBuilder {
public:
    MILPBuilder(const core::Portfolio& portfolio,
                const core::TargetAllocation& target);

    void setObjective(ObjectiveType type, const ObjectiveWeights& weights = {});
    void addConstraintSet(const core::ConstraintSet& constraints);
    void addBudgetConstraint(double budget, double min_cash = 0.0);
    void addAllocationBounds(const std::map<std::string, std::pair<double, double>>& bounds);
    void addIntegerConstraints(const std::set<std::string>& tickers);

    int addVariable(const std::string& name, double lb, double ub, bool is_integer = false);
    bool isReady() const;
    int getNumVariables() const;
    int getNumConstraints() const;

    std::vector<core::Trade> extractTrades(const std::vector<double>& solution) const;

    // Model data accessors for solver integration
    const std::vector<double>& getVarLower() const;
    const std::vector<double>& getVarUpper() const;
    const std::vector<bool>& getVarInteger() const;
    const std::vector<double>& getObjCoeffs() const;
    const std::vector<std::vector<double>>& getConstraintMatrix() const;
};
```

#### HiGHS Optimizer

```cpp
class HighsOptimizer : public BaseSolver {
public:
    std::string getName() const override { return "HiGHS"; }
    bool isAvailable() const override;

    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints) override;

    void setTimeout(long timeout_ms) override;
    void setVerbosity(int level) override;
    void setPresolve(bool enable);
    void setParallel(bool enable);
    void setMIPGap(double gap);

    std::vector<double> getPrimalSolution() const;
    std::vector<double> getDualSolution() const;
    double getObjectiveValue() const;
};
```

---

## Market Data Module

The market data module (`lumen::data`) provides integration with external market data providers.

### Market Data Client

```cpp
// include/lumen/data/market_data_client.hpp

namespace lumen::data {

/// Quote data for a single ticker
struct Quote {
    std::string ticker;
    double price;
    double change;
    double change_percent;
    long volume;
    std::chrono::system_clock::time_point timestamp;

    nlohmann::json toJSON() const;
    static Quote fromJSON(const nlohmann::json& j);
};

/// Historical price data point
struct HistoricalPrice {
    std::chrono::system_clock::time_point date;
    double open;
    double high;
    double low;
    double close;
    double adjusted_close;
    long volume;
};

/// Rate limiter for API calls
class RateLimiter {
public:
    explicit RateLimiter(int calls_per_minute);

    /// Wait if necessary to stay within rate limit
    void waitIfNeeded();

    /// Check if we can make a call now
    bool canMakeCall() const;

    /// Get time until next allowed call (in ms)
    long getWaitTimeMs() const;

private:
    int calls_per_minute_;
    std::deque<std::chrono::steady_clock::time_point> call_times_;
    mutable std::mutex mutex_;
};

/// Market data provider interface
class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;

    virtual std::string getName() const = 0;
    virtual bool isAvailable() const = 0;
    virtual bool requiresApiKey() const = 0;

    virtual std::optional<Quote> getQuote(const std::string& ticker) = 0;
    virtual std::vector<Quote> getBatchQuotes(const std::vector<std::string>& tickers) = 0;
    virtual std::vector<HistoricalPrice> getHistoricalPrices(
        const std::string& ticker,
        int days) = 0;
};

/// Alpha Vantage provider implementation
class AlphaVantageProvider : public MarketDataProvider {
public:
    explicit AlphaVantageProvider(const std::string& api_key);

    std::string getName() const override { return "alpha_vantage"; }
    bool isAvailable() const override;
    bool requiresApiKey() const override { return true; }

    std::optional<Quote> getQuote(const std::string& ticker) override;
    std::vector<Quote> getBatchQuotes(const std::vector<std::string>& tickers) override;
    std::vector<HistoricalPrice> getHistoricalPrices(
        const std::string& ticker,
        int days) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Yahoo Finance provider implementation
class YahooFinanceProvider : public MarketDataProvider {
public:
    YahooFinanceProvider();

    std::string getName() const override { return "yahoo_finance"; }
    bool isAvailable() const override;
    bool requiresApiKey() const override { return false; }

    std::optional<Quote> getQuote(const std::string& ticker) override;
    std::vector<Quote> getBatchQuotes(const std::vector<std::string>& tickers) override;
    std::vector<HistoricalPrice> getHistoricalPrices(
        const std::string& ticker,
        int days) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Main market data client with caching and fallback support
class MarketDataClient {
public:
    explicit MarketDataClient(const utils::Configuration& config);
    ~MarketDataClient();

    /// Get quote for a single ticker (uses cache if available)
    std::optional<Quote> getQuote(const std::string& ticker);

    /// Get quotes for multiple tickers
    std::vector<Quote> getQuotes(const std::vector<std::string>& tickers);

    /// Get historical prices
    std::vector<HistoricalPrice> getHistoricalPrices(
        const std::string& ticker,
        int days = 252);

    /// Update portfolio prices from market data
    void updatePortfolioPrices(core::Portfolio& portfolio);

    /// Check if provider is available
    bool isProviderAvailable(const std::string& provider_name) const;

    /// Get cache statistics
    struct CacheStats {
        size_t entries;
        size_t hits;
        size_t misses;
        double hit_rate;
    };
    CacheStats getCacheStats() const;

    /// Clear the cache
    void clearCache();

    /// Set cache TTL in minutes
    void setCacheTTL(int minutes);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lumen::data
```

### Using Market Data

```cpp
#include <lumen/data/market_data_client.hpp>
#include <lumen/utils/config.hpp>

using namespace lumen;

// Load configuration with API keys
utils::Configuration config("~/.lumen/config.yaml");

// Create market data client
data::MarketDataClient client(config);

// Get single quote
if (auto quote = client.getQuote("AAPL")) {
    std::cout << quote->ticker << ": $" << quote->price
              << " (" << quote->change_percent << "%)" << std::endl;
}

// Get multiple quotes
auto quotes = client.getQuotes({"AAPL", "GOOGL", "MSFT"});
for (const auto& q : quotes) {
    std::cout << q.ticker << ": $" << q.price << std::endl;
}

// Get historical prices
auto history = client.getHistoricalPrices("AAPL", 30);  // 30 days
for (const auto& price : history) {
    std::cout << "Close: $" << price.close << std::endl;
}

// Update portfolio with current prices
core::Portfolio portfolio = loadPortfolio();
client.updatePortfolioPrices(portfolio);

// Check cache statistics
auto stats = client.getCacheStats();
std::cout << "Cache hit rate: " << (stats.hit_rate * 100) << "%" << std::endl;
```

---

## Persistence Module

The persistence module provides SQLite-based storage for portfolios and optimization history.

### Persistence Manager

```cpp
// include/lumen/data/persistence.hpp

namespace lumen::data {

/// Stored portfolio record
struct PortfolioRecord {
    std::string id;
    std::string name;
    std::string data_json;  // Serialized portfolio
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

/// Stored optimization history record
struct OptimizationRecord {
    std::string session_id;
    std::string portfolio_id;
    std::string input_json;       // Portfolio, targets, constraints
    std::string output_json;      // SolverResult
    std::string provenance_json;  // Provenance data
    std::chrono::system_clock::time_point created_at;
};

/// Database connection wrapper
class DatabaseConnection {
public:
    explicit DatabaseConnection(const std::string& db_path);
    ~DatabaseConnection();

    /// Execute a SQL statement
    void execute(const std::string& sql);

    /// Execute with parameters
    void execute(const std::string& sql,
                 const std::vector<std::string>& params);

    /// Query and return rows
    std::vector<std::map<std::string, std::string>> query(
        const std::string& sql,
        const std::vector<std::string>& params = {});

    /// Begin a transaction
    void beginTransaction();

    /// Commit current transaction
    void commit();

    /// Rollback current transaction
    void rollback();

    /// Check if database is connected
    bool isConnected() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Main persistence manager
class PersistenceManager {
public:
    explicit PersistenceManager(const utils::Configuration& config);
    explicit PersistenceManager(const std::string& db_path);
    ~PersistenceManager();

    // Portfolio operations
    /// Save a portfolio (creates or updates)
    std::string savePortfolio(const core::Portfolio& portfolio,
                              const std::string& name);

    /// Load a portfolio by ID
    std::optional<core::Portfolio> loadPortfolio(const std::string& id);

    /// Load a portfolio by name
    std::optional<core::Portfolio> loadPortfolioByName(const std::string& name);

    /// List all portfolios
    std::vector<PortfolioRecord> listPortfolios();

    /// Delete a portfolio
    bool deletePortfolio(const std::string& id);

    // Optimization history operations
    /// Save optimization result with provenance
    void saveOptimization(const std::string& session_id,
                          const core::Portfolio& portfolio,
                          const core::TargetAllocation& target,
                          const core::ConstraintSet& constraints,
                          const core::SolverResult& result,
                          const explain::Provenance& provenance);

    /// Load optimization by session ID
    std::optional<OptimizationRecord> loadOptimization(const std::string& session_id);

    /// List recent optimizations
    std::vector<OptimizationRecord> listOptimizations(int limit = 100);

    /// List optimizations for a portfolio
    std::vector<OptimizationRecord> listOptimizationsForPortfolio(
        const std::string& portfolio_id);

    // Database management
    /// Initialize database schema
    void initializeSchema();

    /// Check database connection
    bool isConnected() const;

    /// Get database file path
    std::string getDatabasePath() const;

    /// Vacuum database (compact)
    void vacuum();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lumen::data
```

### Using Persistence

```cpp
#include <lumen/data/persistence.hpp>

using namespace lumen;

// Create persistence manager
data::PersistenceManager pm("~/.lumen/data/lumen.db");

// Save a portfolio
core::Portfolio portfolio = createPortfolio();
std::string portfolio_id = pm.savePortfolio(portfolio, "My Retirement Account");

// List all portfolios
auto portfolios = pm.listPortfolios();
for (const auto& record : portfolios) {
    std::cout << record.id << ": " << record.name << std::endl;
}

// Load a portfolio
if (auto loaded = pm.loadPortfolio(portfolio_id)) {
    std::cout << "Loaded portfolio with "
              << loaded->getPositionCount() << " positions" << std::endl;
}

// Save optimization result
core::SolverResult result = runOptimization();
explain::Provenance provenance = buildProvenance();
pm.saveOptimization(
    result.session_id,
    portfolio,
    target,
    constraints,
    result,
    provenance
);

// List recent optimizations
auto history = pm.listOptimizations(10);
for (const auto& record : history) {
    std::cout << record.session_id << " at " << formatTime(record.created_at)
              << std::endl;
}

// Load specific optimization for explanation
if (auto opt = pm.loadOptimization("opt_20240115_143052_a7b3c")) {
    auto result = core::SolverResult::fromJSON(nlohmann::json::parse(opt->output_json));
    std::cout << "Found " << result.trades.size() << " trades" << std::endl;
}
```

---

## Explainability Module

The explainability module provides comprehensive explanation generation for optimization decisions.

### Provenance Tracking

```cpp
// include/lumen/explain/provenance.hpp

namespace lumen::explain {

/// Record of a data source used in optimization
struct DataSourceRecord {
    std::string source_type;  // "file", "api", "database"
    std::string source_name;  // e.g., "portfolio.csv", "Alpha Vantage"
    std::string identifier;   // e.g., file path, API endpoint
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> metadata;

    nlohmann::json toJSON() const;
    static DataSourceRecord fromJSON(const nlohmann::json& j);
};

/// Record of a constraint and its effect
struct ConstraintRecord {
    std::string name;
    std::string type;
    std::string description;
    bool was_binding;
    double slack_value;  // Distance from bound
    std::map<std::string, std::string> parameters;

    nlohmann::json toJSON() const;
    static ConstraintRecord fromJSON(const nlohmann::json& j);
};

/// Record of solver execution
struct SolverRecord {
    std::string solver_name;
    std::string solver_version;
    std::string problem_type;
    long solve_time_ms;
    int iterations;
    double optimality_gap;
    std::string termination_status;
    std::map<std::string, std::string> parameters;

    nlohmann::json toJSON() const;
    static SolverRecord fromJSON(const nlohmann::json& j);
};

/// Complete provenance record for an optimization
class Provenance {
public:
    Provenance();
    ~Provenance();

    // Add records
    void addDataSource(const DataSourceRecord& record);
    void addConstraint(const ConstraintRecord& record);
    void setSolverInfo(const SolverRecord& record);
    void addAssumption(const std::string& key, const std::string& value);
    void addWarning(const std::string& warning);

    // Retrieve records
    const std::vector<DataSourceRecord>& getDataSources() const;
    const std::vector<ConstraintRecord>& getConstraints() const;
    const SolverRecord& getSolverInfo() const;
    const std::map<std::string, std::string>& getAssumptions() const;
    const std::vector<std::string>& getWarnings() const;

    // Serialization
    nlohmann::json toJSON() const;
    static Provenance fromJSON(const nlohmann::json& j);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lumen::explain
```

### Explainer

```cpp
// include/lumen/explain/explainer.hpp

namespace lumen::explain {

/// Rationale for a single trade
struct TradeRationale {
    std::string ticker;
    core::Trade::Action action;
    double shares;
    double amount;

    double before_allocation;      // Percentage before trade
    double after_allocation;       // Percentage after trade
    double target_allocation;      // Target percentage
    double drift_before;           // Drift before this trade
    double drift_after;            // Drift after this trade

    std::vector<std::string> primary_reasons;
    std::vector<std::string> contributing_factors;
    std::vector<std::string> constraints_affecting;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Comparison of allocations before and after optimization
struct AllocationComparison {
    struct Entry {
        std::string ticker;
        double before_value;
        double before_percent;
        double after_value;
        double after_percent;
        double target_percent;
        double drift;  // after_percent - target_percent
    };

    std::vector<Entry> entries;
    double total_value_before;
    double total_value_after;
    double total_drift_before;
    double total_drift_after;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Breakdown of transaction costs
struct CostBreakdown {
    double commission_cost;
    double spread_cost;
    double market_impact_cost;
    double tax_cost;
    double total_cost;

    struct TradeBreakdown {
        std::string ticker;
        double commission;
        double spread;
        double market_impact;
        double tax;
        double total;
    };
    std::vector<TradeBreakdown> per_trade;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Sensitivity analysis results
struct SensitivityAnalysis {
    struct PriceShock {
        std::string ticker;
        double shock_percent;
        double impact_on_cost;
        double impact_on_drift;
    };
    std::vector<PriceShock> price_shocks;

    struct ConstraintRelaxation {
        std::string constraint_name;
        std::string relaxation;
        double potential_improvement;
    };
    std::vector<ConstraintRelaxation> constraint_relaxations;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Complete explanation document
class ExplanationDocument {
public:
    std::string session_id;
    std::chrono::system_clock::time_point timestamp;

    std::string executive_summary;
    std::vector<TradeRationale> trade_rationales;
    AllocationComparison allocation_comparison;
    CostBreakdown cost_breakdown;
    SensitivityAnalysis sensitivity_analysis;
    Provenance provenance;

    // Output methods
    nlohmann::json toJSON() const;
    std::string toPlainText() const;
    std::string toMarkdown() const;
    std::string toHTML() const;
};

/// Main explainer class
class Explainer {
public:
    Explainer();
    ~Explainer();

    /// Generate complete explanation for an optimization result
    ExplanationDocument explain(
        const core::Portfolio& portfolio_before,
        const core::Portfolio& portfolio_after,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints,
        const core::SolverResult& result,
        const Provenance& provenance);

    /// Generate explanation from stored optimization
    ExplanationDocument explainFromRecord(
        const data::OptimizationRecord& record);

    // Individual components
    TradeRationale explainTrade(
        const core::Trade& trade,
        const core::Portfolio& portfolio_before,
        const core::Portfolio& portfolio_after,
        const core::TargetAllocation& target);

    AllocationComparison compareAllocations(
        const core::Portfolio& before,
        const core::Portfolio& after,
        const core::TargetAllocation& target);

    CostBreakdown calculateCostBreakdown(
        const std::vector<core::Trade>& trades,
        const core::Portfolio& portfolio);

    SensitivityAnalysis runSensitivityAnalysis(
        const core::Portfolio& portfolio,
        const core::TargetAllocation& target,
        const core::ConstraintSet& constraints);

    std::string generateExecutiveSummary(
        const core::SolverResult& result,
        const AllocationComparison& comparison);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lumen::explain
```

### Using the Explainer

```cpp
#include <lumen/explain/explainer.hpp>
#include <lumen/explain/provenance.hpp>

using namespace lumen;

// Build provenance during optimization
explain::Provenance provenance;

// Track data sources
explain::DataSourceRecord portfolio_source;
portfolio_source.source_type = "file";
portfolio_source.source_name = "portfolio.csv";
portfolio_source.timestamp = std::chrono::system_clock::now();
provenance.addDataSource(portfolio_source);

explain::DataSourceRecord price_source;
price_source.source_type = "api";
price_source.source_name = "Alpha Vantage";
price_source.timestamp = std::chrono::system_clock::now();
provenance.addDataSource(price_source);

// Track constraints
for (const auto& constraint : constraints) {
    explain::ConstraintRecord rec;
    rec.name = constraint->getName();
    rec.type = toString(constraint->getType());
    rec.was_binding = /* determined during solve */;
    provenance.addConstraint(rec);
}

// Track solver info
explain::SolverRecord solver_rec;
solver_rec.solver_name = "HiGHS";
solver_rec.solver_version = "1.5.3";
solver_rec.solve_time_ms = result.solve_time_ms;
solver_rec.optimality_gap = 0.0;
provenance.setSolverInfo(solver_rec);

// Add assumptions
provenance.addAssumption("transaction_cost_rate", "0.001");
provenance.addAssumption("short_term_tax_rate", "0.35");

// Generate explanation
explain::Explainer explainer;
auto explanation = explainer.explain(
    portfolio_before,
    portfolio_after,
    target,
    constraints,
    result,
    provenance
);

// Output in different formats
std::cout << explanation.toPlainText() << std::endl;

// Save as JSON
std::ofstream json_out("explanation.json");
json_out << explanation.toJSON().dump(2);

// Save as Markdown
std::ofstream md_out("explanation.md");
md_out << explanation.toMarkdown();

// Save as HTML
std::ofstream html_out("explanation.html");
html_out << explanation.toHTML();
```

---

## Tax Optimization Module

The tax optimization module (`lumen::data`) provides comprehensive tax-aware portfolio management, including tax lot tracking, wash sale detection, and tax-loss harvesting.

### Tax Lot Data Structures

```cpp
// include/lumen/data/tax_lot.hpp

namespace lumen::data {

/// Tax lot selection methods
enum class TaxLotMethod {
    FIFO,     ///< First In, First Out
    LIFO,     ///< Last In, First Out
    HIFO,     ///< Highest In, First Out (tax loss harvesting)
    LOFO,     ///< Lowest In, First Out
    SPEC_ID   ///< Specific Identification
};

/// Capital gain type based on holding period
enum class GainType {
    SHORT_TERM,  ///< Held < 1 year (ordinary tax rates)
    LONG_TERM    ///< Held >= 1 year (preferential rates)
};

/// Represents a single tax lot (cost basis record)
struct TaxLot {
    std::string id;                  ///< Unique lot identifier
    std::string ticker;              ///< Security symbol
    double shares;                   ///< Number of shares in lot
    double cost_basis_per_share;     ///< Purchase price per share
    std::chrono::system_clock::time_point purchase_date;  ///< Acquisition date
    std::optional<std::chrono::system_clock::time_point> sale_date;  ///< Sale date (if sold)
    std::optional<double> sale_price;  ///< Sale price per share (if sold)

    double getTotalCostBasis() const;
    double getCurrentValue(double current_price) const;
    double getUnrealizedGain(double current_price) const;
    double getRealizedGain() const;
    GainType getGainType() const;
    bool isSold() const { return sale_date.has_value(); }
    int getDaysHeld() const;

    nlohmann::json toJSON() const;
    static TaxLot fromJSON(const nlohmann::json& j);
};

/// Represents a wash sale violation
struct WashSaleViolation {
    std::string lot_id;               ///< Lot that triggered violation
    std::string replacement_ticker;   ///< Replacement security purchased
    std::chrono::system_clock::time_point violation_date;
    double disallowed_loss;           ///< Amount of loss disallowed
    std::string description;          ///< Human-readable description

    nlohmann::json toJSON() const;
};

}  // namespace lumen::data
```

### TaxLotManager

```cpp
/// Manages tax lots for a portfolio
class TaxLotManager {
public:
    TaxLotManager() = default;
    ~TaxLotManager();

    /// Add a new tax lot
    void addLot(const TaxLot& lot);

    /// Remove a lot by ID
    void removeLot(const std::string& lot_id);

    /// Update an existing lot
    void updateLot(const std::string& lot_id, const TaxLot& updated);

    /// Get a lot by ID
    const TaxLot& getLot(const std::string& lot_id) const;

    /// Get all lots for a ticker
    std::vector<TaxLot> getLotsForTicker(const std::string& ticker) const;

    /// Get all unsold lots
    std::vector<TaxLot> getUnsoldLots() const;

    /// Get all sold lots
    std::vector<TaxLot> getSoldLots() const;

    /// Get all lots
    std::vector<TaxLot> getAllLots() const;

    /// Select lots to sell using specified method
    std::vector<TaxLot> selectLotsToSell(const std::string& ticker,
                                          double shares_to_sell,
                                          TaxLotMethod method) const;

    /// Calculate total cost basis for a ticker
    double getTotalCostBasis(const std::string& ticker) const;

    /// Calculate average cost basis for a ticker
    double getAverageCostBasis(const std::string& ticker) const;

    /// Calculate total unrealized gain for a ticker
    double getTotalUnrealizedGain(double current_price,
                                   const std::string& ticker) const;

    /// Save lots to database
    void saveToDatabase(const std::string& db_path);

    /// Load lots from database
    void loadFromDatabase(const std::string& db_path);

    nlohmann::json toJSON() const;
    static TaxLotManager fromJSON(const nlohmann::json& j);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

### TaxOptimizer

```cpp
/// Tax-loss harvesting opportunity
struct TaxHarvestingOpportunity {
    TaxLot lot;                       ///< Lot with unrealized loss
    double current_price;             ///< Current market price
    double unrealized_loss;           ///< Amount of loss available
    GainType gain_type;               ///< Short-term or long-term
    std::vector<std::string> replacement_candidates;  ///< Similar ETFs
    bool would_trigger_wash_sale;     ///< Whether replacement would trigger wash sale

    nlohmann::json toJSON() const;
};

/// Capital gains tax report
struct CapitalGainsReport {
    double short_term_gains;          ///< Total short-term gains
    double long_term_gains;           ///< Total long-term gains
    double short_term_losses;         ///< Total short-term losses
    double long_term_losses;          ///< Total long-term losses
    double net_short_term;            ///< Net short-term gain/loss
    double net_long_term;             ///< Net long-term gain/loss
    double harvested_losses;          ///< Losses intentionally harvested
    double estimated_tax_savings;     ///< Estimated tax savings from harvesting
    std::vector<core::Trade> trades;  ///< Trades that generated gains/losses

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Tax-aware optimization helper
class TaxOptimizer {
public:
    TaxOptimizer(TaxLotManager& lot_manager,
                 double tax_rate_short = 0.35,
                 double tax_rate_long = 0.15);
    ~TaxOptimizer();

    /// Find tax-loss harvesting opportunities
    std::vector<TaxHarvestingOpportunity> findHarvestingOpportunities(
        const std::map<std::string, double>& current_prices,
        double min_loss_threshold = 100.0) const;

    /// Check if a trade would trigger wash sale
    bool wouldTriggerWashSale(
        const TaxLot& lot_to_sell,
        const std::string& replacement_ticker,
        const std::chrono::system_clock::time_point& trade_date) const;

    /// Check proposed trades for wash sale violations
    std::vector<WashSaleViolation> checkWashSaleViolations(
        const std::vector<core::Trade>& proposed_trades) const;

    /// Select optimal lots for tax efficiency
    std::vector<TaxLot> selectOptimalLots(
        const std::string& ticker,
        double shares_to_sell,
        double current_price,
        bool prefer_losses = true) const;

    /// Calculate capital gains report for trades
    CapitalGainsReport calculateCapitalGains(
        const std::vector<core::Trade>& trades,
        const std::map<std::string, double>& prices) const;

    /// Estimate tax impact of trades
    double estimateTaxImpact(
        const std::vector<core::Trade>& trades,
        const std::map<std::string, double>& prices) const;

    /// Set tax rates
    void setTaxRates(double short_term, double long_term);

    /// Set replacement candidates for tickers (similar ETFs)
    void setReplacementCandidates(const std::string& ticker,
                                  const std::vector<std::string>& candidates);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
```

### Tax Constraints

The constraint system includes tax-specific constraints:

```cpp
// include/lumen/core/constraint.hpp

/// Tax lot selection constraint: enforces specific lot selection method for sales
class TaxLotConstraint : public Constraint {
public:
    /// Create a tax lot constraint
    /// @param lot_manager Reference to the tax lot manager
    /// @param method The lot selection method to enforce
    /// @param prefer_long_term If true, prefer long-term lots when multiple qualify
    TaxLotConstraint(data::TaxLotManager* lot_manager,
                     data::TaxLotMethod method,
                     bool prefer_long_term = true);

    ConstraintType getType() const override { return ConstraintType::TAX_LOT; }
    std::string getName() const override { return "Tax Lot Selection"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    data::TaxLotMethod getMethod() const { return method_; }
    bool prefersLongTerm() const { return prefer_long_term_; }

    /// Get the recommended lots for a sale
    std::vector<std::string> getRecommendedLotIds(const std::string& ticker,
                                                   double shares_to_sell) const;

private:
    data::TaxLotManager* lot_manager_;
    data::TaxLotMethod method_;
    bool prefer_long_term_;
};

/// Wash sale rule constraint: prevents trades that would trigger wash sales
class WashSaleConstraint : public Constraint {
public:
    /// Create a wash sale constraint
    /// @param optimizer Reference to the tax optimizer (for wash sale checking)
    /// @param block_violations If true, disallow trades that trigger wash sales
    /// @param warn_only If true, only warn but don't block
    WashSaleConstraint(data::TaxOptimizer* optimizer,
                       bool block_violations = true,
                       bool warn_only = false);

    ConstraintType getType() const override { return ConstraintType::WASH_SALE; }
    std::string getName() const override { return "Wash Sale Rule"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    bool blocksViolations() const { return block_violations_; }
    bool isWarnOnly() const { return warn_only_; }

    /// Get potential wash sale violations for proposed trades
    std::vector<std::string> getPotentialViolations(
        const std::vector<Trade>& trades) const;

private:
    data::TaxOptimizer* optimizer_;
    bool block_violations_;
    bool warn_only_;
};
```

### Using Tax Optimization

```cpp
#include <lumen/data/tax_lot.hpp>
#include <lumen/core/constraint.hpp>

using namespace lumen;

// Create tax lot manager
data::TaxLotManager lot_manager;

// Add tax lots from broker data
data::TaxLot lot1;
lot1.id = "LOT_001";
lot1.ticker = "AAPL";
lot1.shares = 100.0;
lot1.cost_basis_per_share = 150.0;
lot1.purchase_date = /* 2 years ago */;
lot_manager.addLot(lot1);

data::TaxLot lot2;
lot2.id = "LOT_002";
lot2.ticker = "AAPL";
lot2.shares = 50.0;
lot2.cost_basis_per_share = 180.0;  // Higher cost basis
lot2.purchase_date = /* 6 months ago */;
lot_manager.addLot(lot2);

// Create tax optimizer
data::TaxOptimizer optimizer(lot_manager, 0.35, 0.15);  // 35% short-term, 15% long-term

// Find harvesting opportunities
std::map<std::string, double> prices = {{"AAPL", 145.0}};
auto opportunities = optimizer.findHarvestingOpportunities(prices, 500.0);

for (const auto& opp : opportunities) {
    std::cout << opp.lot.ticker << " lot " << opp.lot.id
              << ": $" << opp.unrealized_loss << " unrealized loss"
              << " (" << (opp.gain_type == data::GainType::LONG_TERM ? "long-term" : "short-term") << ")"
              << std::endl;
}

// Select lots for a sale using HIFO (tax-loss harvesting)
auto lots_to_sell = lot_manager.selectLotsToSell("AAPL", 75.0, data::TaxLotMethod::HIFO);

// Check for wash sale violations
std::vector<core::Trade> proposed_trades = /* ... */;
auto violations = optimizer.checkWashSaleViolations(proposed_trades);
if (!violations.empty()) {
    for (const auto& v : violations) {
        std::cout << "Wash sale warning: " << v.description << std::endl;
    }
}

// Add tax constraints to optimization
core::ConstraintSet constraints;
constraints.addConstraint(
    std::make_unique<core::TaxLotConstraint>(&lot_manager, data::TaxLotMethod::HIFO, true));
constraints.addConstraint(
    std::make_unique<core::WashSaleConstraint>(&optimizer, true, false));

// Generate tax report
auto report = optimizer.calculateCapitalGains(result.trades, prices);
std::cout << report.toPlainText() << std::endl;
```

---

## Broker Import Module

The broker import module provides parsers for importing tax lot data from various broker export formats.

### Supported Formats

```cpp
// include/lumen/data/broker_import.hpp

namespace lumen::data {

/// Supported broker formats
enum class BrokerFormat {
    GENERIC_CSV,    ///< Generic CSV with configurable columns
    SCHWAB,         ///< Charles Schwab cost basis export
    FIDELITY,       ///< Fidelity Investments tax lot export
    VANGUARD,       ///< Vanguard cost basis export
    AUTO_DETECT     ///< Automatically detect format from file contents
};

/// Column mapping for generic CSV import
struct CSVColumnMapping {
    int ticker_column = 0;           ///< Column index for ticker symbol
    int shares_column = 1;           ///< Column index for share quantity
    int cost_basis_column = 2;       ///< Column index for cost basis per share
    int purchase_date_column = 3;    ///< Column index for purchase date
    int lot_id_column = -1;          ///< Column index for lot ID (-1 = auto-generate)
    int sale_date_column = -1;       ///< Column index for sale date (-1 = not present)
    int sale_price_column = -1;      ///< Column index for sale price (-1 = not present)

    bool has_header = true;          ///< Whether first row is header
    char delimiter = ',';            ///< Field delimiter
    std::string date_format = "%Y-%m-%d";  ///< Date format string

    nlohmann::json toJSON() const;
    static CSVColumnMapping fromJSON(const nlohmann::json& j);
};

/// Result of an import operation
struct ImportResult {
    bool success = false;
    std::vector<TaxLot> lots;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    int rows_processed = 0;
    int rows_imported = 0;
    int rows_skipped = 0;
    BrokerFormat detected_format = BrokerFormat::GENERIC_CSV;

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

}  // namespace lumen::data
```

### BrokerImporter Interface

```cpp
/// Abstract base class for broker import parsers
class BrokerImporter {
public:
    virtual ~BrokerImporter() = default;

    /// Get the broker format this importer handles
    virtual BrokerFormat getFormat() const = 0;

    /// Get human-readable name of the broker
    virtual std::string getBrokerName() const = 0;

    /// Check if this importer can handle the given file
    virtual bool canHandle(const std::string& file_path) const = 0;

    /// Import tax lots from file
    virtual ImportResult importFromFile(const std::string& file_path) = 0;

    /// Import tax lots from string content
    virtual ImportResult importFromString(const std::string& content) = 0;
};

/// Generic CSV importer with configurable column mapping
class GenericCSVImporter : public BrokerImporter {
public:
    GenericCSVImporter();
    explicit GenericCSVImporter(const CSVColumnMapping& mapping);
    ~GenericCSVImporter() override;

    BrokerFormat getFormat() const override { return BrokerFormat::GENERIC_CSV; }
    std::string getBrokerName() const override { return "Generic CSV"; }
    bool canHandle(const std::string& file_path) const override;
    ImportResult importFromFile(const std::string& file_path) override;
    ImportResult importFromString(const std::string& content) override;

    void setColumnMapping(const CSVColumnMapping& mapping);
    const CSVColumnMapping& getColumnMapping() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/// Charles Schwab cost basis importer
class SchwabImporter : public BrokerImporter { /* ... */ };

/// Fidelity Investments tax lot importer
class FidelityImporter : public BrokerImporter { /* ... */ };

/// Vanguard cost basis importer
class VanguardImporter : public BrokerImporter { /* ... */ };
```

### BrokerImportFactory

```cpp
/// Factory for creating broker importers
class BrokerImportFactory {
public:
    /// Create importer for specified format
    static std::unique_ptr<BrokerImporter> create(BrokerFormat format);

    /// Create importer by auto-detecting format from file
    static std::unique_ptr<BrokerImporter> createForFile(const std::string& file_path);

    /// Get all supported formats
    static std::vector<BrokerFormat> getSupportedFormats();

    /// Import from file with auto-detection
    static ImportResult importFile(const std::string& file_path,
                                   BrokerFormat format = BrokerFormat::AUTO_DETECT);

    /// Import from file and add to TaxLotManager
    static ImportResult importToManager(const std::string& file_path,
                                        TaxLotManager& manager,
                                        BrokerFormat format = BrokerFormat::AUTO_DETECT);
};

/// Export tax lots to CSV format
class TaxLotExporter {
public:
    /// Export lots to CSV string
    static std::string toCSV(const std::vector<TaxLot>& lots);

    /// Export lots to CSV file
    static bool toFile(const std::vector<TaxLot>& lots, const std::string& file_path);

    /// Export TaxLotManager contents to CSV file
    static bool exportManager(const TaxLotManager& manager, const std::string& file_path);
};
```

### Using Broker Import

```cpp
#include <lumen/data/broker_import.hpp>
#include <lumen/data/tax_lot.hpp>

using namespace lumen::data;

// Auto-detect and import from broker file
auto result = BrokerImportFactory::importFile("schwab_export.csv");

if (result.success) {
    std::cout << "Imported " << result.rows_imported << " lots from "
              << brokerFormatToString(result.detected_format) << std::endl;

    for (const auto& lot : result.lots) {
        std::cout << lot.ticker << ": " << lot.shares << " shares @ $"
                  << lot.cost_basis_per_share << std::endl;
    }
} else {
    for (const auto& error : result.errors) {
        std::cerr << "Error: " << error << std::endl;
    }
}

// Print any warnings
for (const auto& warning : result.warnings) {
    std::cout << "Warning: " << warning << std::endl;
}

// Import directly into TaxLotManager
TaxLotManager lot_manager;
auto result2 = BrokerImportFactory::importToManager("fidelity_lots.csv", lot_manager);

// Use custom column mapping for generic CSV
CSVColumnMapping mapping;
mapping.ticker_column = 0;
mapping.shares_column = 2;
mapping.cost_basis_column = 3;
mapping.date_format = "%m/%d/%Y";

GenericCSVImporter importer(mapping);
auto result3 = importer.importFromFile("custom_export.csv");

// Export lots back to CSV
TaxLotExporter::exportManager(lot_manager, "exported_lots.csv");
```

---

## Building from Source

### Prerequisites

- C++17 compatible compiler (GCC 11+, Clang 14+, MSVC 2019+)
- CMake 3.16+
- Ninja (recommended) or Make

### Dependencies

Install via vcpkg:

```bash
# Setup vcpkg
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$PWD/vcpkg

# Install dependencies
cd lumen
vcpkg install
```

Or install manually:
- **Required:** Eigen3, nlohmann-json, SQLite3
- **Optional:** HiGHS, yaml-cpp, CLI11, cpp-httplib, GTest

### Build Commands

```bash
# Configure
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build

# Test
cd build && ctest --output-on-failure

# Install
sudo cmake --install build
```

### Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build unit tests |
| `BUILD_BENCHMARKS` | OFF | Build performance benchmarks |
| `BUILD_CLI` | ON | Build command-line interface |
| `BUILD_SERVER` | ON | Build REST API server |
| `BUILD_GUI` | OFF | Build Qt GUI application |
| `ENABLE_QUANTUM` | ON | Enable quantum solver integration |
| `ENABLE_PYFLARE` | ON | Enable telemetry |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |

Example with custom options:

```bash
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DBUILD_BENCHMARKS=ON \
  -DENABLE_COVERAGE=ON
```

---

## Development Workflow

### Code Formatting

Lumen uses clang-format for consistent code style:

```bash
# Format all files
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Check formatting
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run -Werror
```

### Running Tests

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific test suite
./build/lumen-tests --gtest_filter="PortfolioTest.*"

# Run with verbose output
./build/lumen-tests --gtest_filter="*" -v
```

### Running Benchmarks

```bash
cmake -B build -DBUILD_BENCHMARKS=ON
cmake --build build
./build/lumen-benchmarks
```

### Code Coverage

```bash
cmake -B build -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ctest
gcovr --html --html-details -o coverage.html
```

### Git Workflow

1. Create a feature branch: `git checkout -b feature/my-feature`
2. Make changes and commit using conventional commits
3. Push and create a pull request
4. Ensure CI passes
5. Request review

Commit message format:
```
type(scope): description

[optional body]

[optional footer]
```

Types: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `chore`

---

## API Reference

### Creating a Portfolio

```cpp
#include <lumen/core/portfolio.hpp>

using namespace lumen::core;

// Create empty portfolio
Portfolio portfolio("My Portfolio");
portfolio.setCurrency("USD");
portfolio.setCashBalance(10000.0);

// Add positions
Position pos;
pos.ticker = "AAPL";
pos.shares = 100;
pos.current_price = 175.50;
pos.cost_basis = 150.00;
pos.asset_class = AssetClass::STOCKS;
portfolio.addPosition(pos);

// Load from file
auto portfolio2 = Portfolio::fromCSV("portfolio.csv");
auto portfolio3 = Portfolio::fromJSON(json_data);

// Save to file
portfolio.saveToFile("output.json");
```

### Creating Constraints

```cpp
#include <lumen/core/constraint.hpp>

using namespace lumen::core;

ConstraintSet constraints;

// Budget constraint
constraints.addConstraint(
    std::make_unique<BudgetConstraint>(100000.0, 5000.0));

// Allocation constraint
constraints.addConstraint(
    std::make_unique<AllocationConstraint>("AAPL", 0.05, 0.15, false));

// Integer shares
constraints.addConstraint(
    std::make_unique<IntegerShareConstraint>(true, {"SPY", "BRK.A"}));

// Position limit
constraints.addConstraint(
    std::make_unique<PositionLimitConstraint>(0.25));

// Validate consistency
if (!constraints.validateConsistency()) {
    auto issues = constraints.getInconsistencies();
    for (const auto& issue : issues) {
        std::cerr << "Constraint issue: " << issue << std::endl;
    }
}
```

### Running Optimization

```cpp
#include <lumen/core/solver_dispatcher.hpp>
#include <lumen/utils/config.hpp>

using namespace lumen;

// Load configuration
utils::Configuration config("~/.lumen/config.yaml");

// Create dispatcher
core::SolverDispatcher dispatcher(config);

// Define target allocation
core::TargetAllocation target(core::AllocationMode::PERCENTAGE);
target.setTarget("AAPL", 0.20, 0.05);  // 20% target, 5% tolerance
target.setTarget("VTI", 0.50, 0.05);
target.setTarget("BND", 0.30, 0.05);

// Run optimization
core::SolverResult result = dispatcher.dispatch(portfolio, target, constraints);

// Check result
if (result.success) {
    std::cout << "Session ID: " << result.session_id << std::endl;
    std::cout << "Found " << result.trades.size() << " trades" << std::endl;
    std::cout << "Objective value: " << result.objective_value << std::endl;

    for (const auto& trade : result.trades) {
        std::cout << trade.ticker << ": "
                  << core::tradeActionToString(trade.action)
                  << " " << trade.shares << " shares" << std::endl;
    }
} else {
    std::cerr << "Optimization failed: " << result.status << std::endl;
}
```

### Complete Workflow Example

```cpp
#include <lumen/core/portfolio.hpp>
#include <lumen/core/solver_dispatcher.hpp>
#include <lumen/data/market_data_client.hpp>
#include <lumen/data/persistence.hpp>
#include <lumen/explain/explainer.hpp>
#include <lumen/utils/config.hpp>

using namespace lumen;

int main() {
    // Load configuration
    utils::Configuration config("~/.lumen/config.yaml");

    // Initialize services
    data::MarketDataClient market_data(config);
    data::PersistenceManager persistence(config);
    explain::Explainer explainer;

    // Load portfolio
    auto portfolio = core::Portfolio::fromCSV("portfolio.csv");

    // Update prices from market data
    market_data.updatePortfolioPrices(portfolio);

    // Build provenance
    explain::Provenance provenance;
    provenance.addDataSource({"file", "portfolio.csv", "portfolio.csv",
                              std::chrono::system_clock::now(), {}});
    provenance.addDataSource({"api", "Alpha Vantage", "quotes",
                              std::chrono::system_clock::now(), {}});

    // Define targets and constraints
    core::TargetAllocation target(core::AllocationMode::PERCENTAGE);
    target.setTarget("AAPL", 0.20, 0.05);
    target.setTarget("VTI", 0.50, 0.05);
    target.setTarget("BND", 0.30, 0.05);

    core::ConstraintSet constraints;
    constraints.addConstraint(std::make_unique<core::IntegerShareConstraint>(true));
    constraints.addConstraint(std::make_unique<core::PositionLimitConstraint>(0.25));

    // Run optimization
    core::SolverDispatcher dispatcher(config);
    auto result = dispatcher.dispatch(portfolio, target, constraints);

    if (!result.success) {
        std::cerr << "Optimization failed: " << result.status << std::endl;
        return 1;
    }

    // Build portfolio after trades
    auto portfolio_after = portfolio;
    for (const auto& trade : result.trades) {
        // Apply trades to portfolio_after...
    }

    // Generate explanation
    auto explanation = explainer.explain(
        portfolio, portfolio_after, target, constraints, result, provenance);

    // Save to persistence
    persistence.saveOptimization(
        result.session_id, portfolio, target, constraints, result, provenance);

    // Output results
    std::cout << explanation.toMarkdown() << std::endl;

    return 0;
}
```

---

## Adding New Features

### Adding a New Constraint Type

1. **Define the constraint in the header:**

```cpp
// include/lumen/core/constraint.hpp

class SectorLimitConstraint : public Constraint {
public:
    SectorLimitConstraint(const std::string& sector, double max_percent);

    ConstraintType getType() const override { return ConstraintType::SECTOR_LIMIT; }
    std::string getName() const override;
    std::string getDescription() const override;
    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;
    nlohmann::json toJSON() const override;

    const std::string& getSector() const { return sector_; }
    double getMaxPercent() const { return max_percent_; }

private:
    std::string sector_;
    double max_percent_;
};
```

2. **Implement the constraint:**

```cpp
// src/core/constraint.cpp

SectorLimitConstraint::SectorLimitConstraint(const std::string& sector,
                                              double max_percent)
    : sector_(sector), max_percent_(max_percent) {
    if (sector.empty()) {
        throw std::invalid_argument("SectorLimitConstraint: sector cannot be empty");
    }
    if (max_percent <= 0.0 || max_percent > 1.0) {
        throw std::invalid_argument("SectorLimitConstraint: max_percent must be in (0, 1]");
    }
}

bool SectorLimitConstraint::isSatisfied(const Portfolio& portfolio,
                                         const std::vector<Trade>& trades) const {
    // Implementation...
}
```

3. **Add JSON deserialization in `ConstraintSet::fromJSON`**

4. **Add MILP formulation in `MILPBuilder`**

5. **Add tests**

### Adding a New Market Data Provider

1. **Create the provider class:**

```cpp
// include/lumen/data/my_provider.hpp

class MyProvider : public MarketDataProvider {
public:
    explicit MyProvider(const std::string& api_key);

    std::string getName() const override { return "my_provider"; }
    bool isAvailable() const override;
    bool requiresApiKey() const override { return true; }

    std::optional<Quote> getQuote(const std::string& ticker) override;
    std::vector<Quote> getBatchQuotes(const std::vector<std::string>& tickers) override;
    std::vector<HistoricalPrice> getHistoricalPrices(
        const std::string& ticker, int days) override;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
```

2. **Implement the provider:**

```cpp
// src/data/my_provider.cpp

class MyProvider::Impl {
public:
    Impl(const std::string& api_key) : api_key_(api_key) {}

    std::optional<Quote> getQuote(const std::string& ticker) {
        // Make HTTP request to API
        // Parse response
        // Return Quote
    }

private:
    std::string api_key_;
    RateLimiter rate_limiter_{60};  // 60 calls per minute
};
```

3. **Register the provider in `MarketDataClient`**

4. **Add configuration support**

5. **Add tests**

---

## Testing

### Test Organization

```
tests/
├── core/
│   ├── test_portfolio.cpp
│   ├── test_constraint.cpp
│   └── test_solver_dispatcher.cpp
├── data/
│   ├── test_market_data.cpp
│   ├── test_persistence.cpp
│   └── test_tax_lot.cpp
├── explain/
│   ├── test_provenance.cpp
│   └── test_explainer.cpp
└── utils/
    ├── test_logging.cpp
    └── test_config.cpp
```

### Writing Tests

```cpp
#include <gtest/gtest.h>
#include "lumen/explain/explainer.hpp"

using namespace lumen::explain;

class ExplainerTest : public ::testing::Test {
protected:
    void SetUp() override {
        explainer_ = std::make_unique<Explainer>();
        // Set up test portfolio, target, etc.
    }

    std::unique_ptr<Explainer> explainer_;
};

TEST_F(ExplainerTest, GeneratesValidTradeRationale) {
    auto rationale = explainer_->explainTrade(trade, portfolio_before,
                                               portfolio_after, target);

    EXPECT_EQ(rationale.ticker, "AAPL");
    EXPECT_FALSE(rationale.primary_reasons.empty());
    EXPECT_GE(rationale.after_allocation, 0.0);
    EXPECT_LE(rationale.after_allocation, 1.0);
}

TEST_F(ExplainerTest, OutputFormatsAreValid) {
    auto explanation = explainer_->explain(/* ... */);

    // JSON should be valid
    auto json = explanation.toJSON();
    EXPECT_TRUE(json.contains("session_id"));
    EXPECT_TRUE(json.contains("trade_rationales"));

    // Markdown should not be empty
    auto markdown = explanation.toMarkdown();
    EXPECT_FALSE(markdown.empty());
    EXPECT_NE(markdown.find("##"), std::string::npos);  // Has headers
}
```

---

## Performance Optimization

### Profiling

```bash
# Build with debug symbols
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Run with perf
perf record ./build/lumen-cli optimize ...
perf report

# Run with valgrind
valgrind --tool=callgrind ./build/lumen-cli optimize ...
```

### Optimization Guidelines

1. **Use sparse matrices:** The MILP builder uses sparse CSC format
2. **Limit allocations:** Pre-allocate vectors when size is known
3. **Cache computations:** Portfolio metrics and market data are cached
4. **Parallel solving:** Enable HiGHS parallel mode for large problems
5. **Rate limit handling:** Use async operations with market data

---

## Security Considerations

Lumen implements comprehensive security measures to protect sensitive financial data. This section documents security features and best practices for developers.

### Security Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Security Layers                             │
├─────────────────────────────────────────────────────────────────┤
│  Transport Security    │ TLS/SSL for HTTP server                │
│                        │ HTTPS for external API calls           │
├────────────────────────┼────────────────────────────────────────┤
│  Authentication        │ API key authentication (Bearer/ApiKey) │
│                        │ Key file support (~/.lumen/api_keys.txt)│
├────────────────────────┼────────────────────────────────────────┤
│  Data Protection       │ SQLCipher database encryption          │
│                        │ Secure file permissions (0600/0700)    │
├────────────────────────┼────────────────────────────────────────┤
│  Input Validation      │ Path traversal protection              │
│                        │ JSON depth/size limits                 │
│                        │ CSV injection sanitization             │
├────────────────────────┼────────────────────────────────────────┤
│  Rate Limiting         │ 100 requests/minute per IP             │
└─────────────────────────────────────────────────────────────────┘
```

### Security Environment Variables

| Variable | Purpose | Required |
|----------|---------|----------|
| `LUMEN_API_KEY` | HTTP server authentication | For server deployment |
| `LUMEN_DB_KEY` | Database encryption key | For encrypted databases |
| `LUMEN_PYFLARE_ENABLED` | Telemetry opt-in (default: false) | Optional |

### Input Validation

All public APIs validate inputs:

```cpp
Position Position::fromJSON(const nlohmann::json& j) {
    // Validate ticker
    if (!j.contains("ticker") || !j["ticker"].is_string()) {
        throw std::runtime_error("Position::fromJSON: missing or invalid 'ticker'");
    }
    std::string ticker = j.at("ticker").get<std::string>();
    if (ticker.empty() || ticker.length() > 20) {
        throw std::runtime_error("Position::fromJSON: ticker must be 1-20 characters");
    }
    // ... continue validation
}
```

### Path Traversal Protection

The configuration system enforces a strict directory whitelist:

```cpp
// Only these directories are allowed for file operations:
// - ~/.lumen (LUMEN_HOME)
// - /etc/lumen
// - /usr/local/etc/lumen

// Path validation rejects:
// - Null bytes in paths
// - Directory traversal sequences (..)
// - Paths outside the whitelist
```

### API Key Security

- API keys stored in environment variables or encrypted config
- Never logged or included in error messages
- Never sent to unauthorized endpoints
- Sanitized from all external error responses

```cpp
// CORRECT: Sanitize error messages
LOG_ERROR("Alpha Vantage error: API error (details redacted)");

// WRONG: Never log API keys
LOG_ERROR("Request failed with key: " + api_key_);  // DON'T DO THIS
```

### Database Encryption

Enable database encryption for sensitive financial data:

```cpp
// Open encrypted database (requires SQLCipher)
Database db;
db.openEncrypted("/path/to/lumen.db", encryption_key);

// Check encryption status
if (db.isEncrypted()) {
    LOG_INFO("Database encryption active");
}
```

Set up encryption via environment:

```bash
export LUMEN_DB_KEY="your-strong-encryption-key"
```

### Secure Random Generation

Always use cryptographically secure random generation for IDs and tokens:

```cpp
#include "lumen/data/persistence.hpp"

// Generate secure unique ID (128-bit random)
std::string id = lumen::data::generateUniqueId();

// Generate secure session ID (256-bit random)
std::string session = lumen::data::generateSecureSessionId();
```

### CSV Injection Protection

When importing CSV data, fields are automatically sanitized:

```cpp
// Dangerous prefixes (=, +, -, @, tab) are escaped with single quote
// Input: "=cmd|'/C calc.exe'"
// Output: "'=cmd|'/C calc.exe'"
```

### HTTP Server Security

The server includes multiple security layers:

```cpp
// Security constants
constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;  // 10 MB
constexpr size_t MAX_JSON_DEPTH = 32;
constexpr int RATE_LIMIT_REQUESTS = 100;
constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;

// All responses include security headers
void addSecurityHeaders(httplib::Response& res) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("Content-Security-Policy", "default-src 'none'");
    res.set_header("Strict-Transport-Security", "max-age=31536000");
}
```

### Telemetry Privacy

Telemetry is disabled by default and requires explicit opt-in:

```cpp
struct ObservabilityConfig {
    bool pyflare_enabled = false;           // Opt-in only
    bool telemetry_consent_given = false;   // Requires explicit consent
};
```

### Size Limits

```cpp
constexpr size_t MAX_POSITIONS = 10000;
constexpr size_t MAX_CONSTRAINTS = 1000;
constexpr size_t MAX_TARGETS = 1000;
constexpr size_t MAX_HISTORY_ENTRIES = 100000;
constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;  // 10 MB
constexpr size_t MAX_JSON_DEPTH = 32;
constexpr size_t MAX_TICKER_LENGTH = 10;
```

### Security Checklist for Contributors

When contributing code, ensure:

- [ ] All user inputs are validated before use
- [ ] File paths are sanitized through `Configuration::sanitizePath()`
- [ ] API keys are never logged or included in error messages
- [ ] Random IDs use `generateUniqueId()` or `generateSecureSessionId()`
- [ ] External error messages don't leak internal details
- [ ] New HTTP endpoints include authentication checks
- [ ] CSV data is sanitized for formula injection
- [ ] Database operations use prepared statements (no string concatenation)

---

## Code Style Guide

### Naming Conventions

```cpp
// Classes: PascalCase
class PortfolioManager { };

// Functions/methods: camelCase
void calculateDrift() { }

// Variables: snake_case
double total_value = 0.0;

// Constants: SCREAMING_SNAKE_CASE
constexpr size_t MAX_POSITIONS = 10000;

// Private members: trailing underscore
double cash_balance_;

// Namespaces: lowercase
namespace lumen::core { }
```

### Using pImpl Pattern

All modules with complex implementation use the pImpl idiom:

```cpp
// Header
class MyClass {
public:
    MyClass();
    ~MyClass();

    void doSomething();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

// Implementation
class MyClass::Impl {
public:
    void doSomething() { /* actual implementation */ }
private:
    // Private data members
};

MyClass::MyClass() : pImpl_(std::make_unique<Impl>()) {}
MyClass::~MyClass() = default;

void MyClass::doSomething() { pImpl_->doSomething(); }
```

### Error Handling

```cpp
// Use exceptions for errors
if (budget <= 0.0) {
    throw std::invalid_argument("BudgetConstraint: total_budget must be positive");
}

// Use optional for missing values
std::optional<Quote> MarketDataClient::getQuote(const std::string& ticker) {
    // Returns nullopt if quote not available
}

// Use result types for expected failures
struct SolverResult {
    bool success;
    std::string status;
};
```

---

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines on:

- Code of conduct
- Development workflow
- Pull request process
- Issue reporting

---

## License

Lumen is released under the MIT License. See [LICENSE](../LICENSE) for details.

---

## Quantum Integration Module

The quantum integration module provides quantum computing support for portfolio optimization using D-Wave quantum annealing and IBM Quantum gate-based computing (QAOA).

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Quantum Integration Layer                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  HybridOrchestrator                      │    │
│  │  ┌──────────────┐  ┌───────────────┐  ┌──────────────┐ │    │
│  │  │ Preprocessing │→│ Quantum Solve │→│ Postprocessing│ │    │
│  │  └──────────────┘  └───────────────┘  └──────────────┘ │    │
│  └─────────────────────────────────────────────────────────┘    │
│                            │                                     │
│           ┌────────────────┴────────────────┐                   │
│           ▼                                  ▼                   │
│  ┌─────────────────┐              ┌─────────────────┐           │
│  │   DWaveClient   │              │ IBMQuantumClient │           │
│  │  (Annealing)    │              │    (QAOA)        │           │
│  │  ┌───────────┐  │              │  ┌───────────┐  │           │
│  │  │   Mock    │  │              │  │   Mock    │  │           │
│  │  │ Simulator │  │              │  │ Simulator │  │           │
│  │  └───────────┘  │              │  └───────────┘  │           │
│  └─────────────────┘              └─────────────────┘           │
│                            │                                     │
│                            ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   QuboFormulator                         │    │
│  │  Portfolio Optimization → QUBO Matrix                    │    │
│  │  Binary Encoding | Constraint Penalties | Tax Lots       │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

### QUBO Formulation

QUBO (Quadratic Unconstrained Binary Optimization) is the mathematical format required by quantum solvers. The `QuboFormulator` converts portfolio optimization problems to QUBO format.

#### Data Structures

```cpp
// include/lumen/solvers/qubo_types.hpp

namespace lumen::solvers {

/// Upper triangular QUBO matrix
struct QuboMatrix {
    std::vector<std::vector<double>> Q;  ///< Coefficient matrix
    double offset = 0.0;                  ///< Constant offset
    int num_variables = 0;                ///< Number of binary variables
    std::map<int, std::string> var_names; ///< Variable index to name

    double evaluate(const std::vector<int>& solution) const;
    bool isValid() const;
    int getNonZeroCount() const;
    nlohmann::json toJSON() const;
    static QuboMatrix fromJSON(const nlohmann::json& j);
};

/// Binary encoding configuration
struct EncodingConfig {
    int precision_bits = 8;     ///< Bits per continuous variable (256 levels)
    double min_value = 0.0;     ///< Minimum portfolio weight
    double max_value = 1.0;     ///< Maximum portfolio weight

    int getNumLevels() const { return 1 << precision_bits; }
    double getStepSize() const;
    std::vector<int> encode(double value) const;
    double decode(const std::vector<int>& binary) const;
};

/// Constraint penalty configuration
struct PenaltyConfig {
    double constraint_penalty = 1000.0;  ///< Base penalty
    bool auto_tune = true;               ///< Enable auto-tuning
    double tune_factor = 10.0;           ///< Auto-tune multiplier

    double computeAutoTunedPenalty(double max_obj_coeff, int num_constraints) const;
};
}
```

#### Binary Encoding Strategy

Continuous portfolio weights (0-100%) are encoded as sums of binary variables:

```
weight = Σ(2^i × x_i) / (2^n - 1)

Example with 8 bits (256 levels):
- x = [1,0,1,0,0,0,0,0] → level = 5 → weight = 5/255 ≈ 1.96%
- x = [1,1,1,1,1,1,1,1] → level = 255 → weight = 100%
- Step size: 100% / 255 ≈ 0.39%
```

#### QUBO Objective Function

The multi-objective function combines:

1. **Drift Minimization**: `Σ (w_i - target_i)²`
2. **Transaction Costs**: `Σ cost_rate × |w_i - current_i|`
3. **Tax Impact**: For tax lots, penalty for realizing gains

```cpp
// Formulating drift minimization in QUBO
// For weight w encoded as Σ(2^k × x_k) × scale:
// (w - target)² = w² - 2×w×target + target²
//
// Expanding in binary:
// Q[i][i] += drift_weight × (coeff_i² - 2×target×coeff_i)
// Q[i][j] += drift_weight × 2×coeff_i×coeff_j  (for i < j)
```

#### Auto-Tuning Penalty Weights

```cpp
double PenaltyConfig::computeAutoTunedPenalty(double max_obj_coeff,
                                               int num_constraints) const {
    if (!auto_tune) return constraint_penalty;

    // Penalty must exceed any feasible objective improvement
    // to ensure constraint violations are never optimal
    return std::max(constraint_penalty,
                    max_obj_coeff * tune_factor * num_constraints);
}
```

### D-Wave Client

The D-Wave client uses quantum annealing to solve QUBO problems.

```cpp
// include/lumen/solvers/quantum_client.hpp

class DWaveClient : public QuantumClient {
public:
    explicit DWaveClient(const QuantumSolverConfig& config);

    // QuantumClient interface
    bool authenticate() override;
    bool checkConnection() override;
    double estimateCost(int num_qubits) const override;
    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q,
                             int num_reads = 1000) override;

    // BaseSolver interface
    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints) override;

    // D-Wave specific
    void setNumReads(int reads);
    void useHybridSolver(bool use);
    std::vector<std::string> getAvailableSolvers() const;
};
```

#### Mock Mode

For development and testing, use mock mode which simulates quantum annealing using classical simulated annealing:

```cpp
QuantumSolverConfig config;
config.provider = "mock";  // Use mock simulator
config.num_reads = 100;

DWaveClient client(config);
client.authenticate();  // Always succeeds in mock mode

auto result = client.solve(portfolio, target, constraints);
// result.solver_used == "D-Wave (Mock)"
```

#### Simulated Annealing Algorithm

The mock solver uses simulated annealing:

```cpp
// Pseudocode for mock quantum solver
for each read:
    solution = random_binary_vector(n)
    temperature = initial_temp

    for sweep in num_sweeps:
        for step in n:
            flip_index = random(0, n-1)
            delta_energy = calculate_flip_delta(Q, solution, flip_index)

            if delta_energy < 0 or random() < exp(-delta_energy / temperature):
                flip(solution, flip_index)

        temperature *= cooling_factor

    store(solution, energy)

return best_solution
```

### IBM Quantum Client (QAOA)

The IBM Quantum client uses the Quantum Approximate Optimization Algorithm (QAOA) for gate-based quantum computing.

```cpp
class IBMQuantumClient : public QuantumClient {
public:
    explicit IBMQuantumClient(const QuantumSolverConfig& config);

    // QuantumClient interface
    bool authenticate() override;
    double estimateCost(int num_qubits) const override;
    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q,
                             int num_shots = 1024) override;

    // IBM specific - QAOA parameters
    void setQAOADepth(int p);           // Circuit depth (default: 2)
    void setOptimizer(const std::string& optimizer);  // "SPSA" or "COBYLA"
    void setNumShots(int shots);
    void setBackend(const std::string& backend);
    std::vector<std::string> getAvailableBackends() const;

    // Get QAOA circuit as OpenQASM 3.0
    std::string getQAOACircuit(const std::vector<std::vector<double>>& Q,
                               const std::vector<double>& gamma,
                               const std::vector<double>& beta) const;
};
```

#### QAOA Circuit Structure

```
|ψ⟩ = U_M(β_p) U_C(γ_p) ... U_M(β_1) U_C(γ_1) |+⟩^n

Where:
- |+⟩^n = H⊗n |0⟩^n  (equal superposition)
- U_C(γ) = exp(-iγH_C)  (cost unitary from QUBO)
- U_M(β) = exp(-iβH_M)  (mixer unitary, X rotations)
```

OpenQASM 3.0 generation:

```cpp
std::string buildQAOACircuit(const QuboMatrix& Q,
                              const std::vector<double>& gamma,
                              const std::vector<double>& beta) {
    std::stringstream qasm;
    qasm << "OPENQASM 3.0;\n";
    qasm << "include \"stdgates.inc\";\n";
    qasm << "qubit[" << n << "] q;\n";
    qasm << "bit[" << n << "] c;\n\n";

    // Initial superposition
    for (int i = 0; i < n; ++i) {
        qasm << "h q[" << i << "];\n";
    }

    // QAOA layers
    for (int layer = 0; layer < p; ++layer) {
        // Cost unitary: Z rotations and ZZ interactions
        for (int i = 0; i < n; ++i) {
            if (Q[i][i] != 0)
                qasm << "rz(" << gamma[layer] * Q[i][i] << ") q[" << i << "];\n";
            for (int j = i + 1; j < n; ++j) {
                if (Q[i][j] != 0) {
                    qasm << "cx q[" << i << "], q[" << j << "];\n";
                    qasm << "rz(" << gamma[layer] * Q[i][j] << ") q[" << j << "];\n";
                    qasm << "cx q[" << i << "], q[" << j << "];\n";
                }
            }
        }

        // Mixer unitary: X rotations
        for (int i = 0; i < n; ++i) {
            qasm << "rx(" << 2 * beta[layer] << ") q[" << i << "];\n";
        }
    }

    qasm << "measure q -> c;\n";
    return qasm.str();
}
```

#### SPSA Optimizer

Simultaneous Perturbation Stochastic Approximation optimizes QAOA parameters:

```cpp
std::vector<double> optimizeSPSA(const QuboMatrix& Q) {
    std::vector<double> params(2 * p, 0.5);  // gamma and beta

    for (int k = 0; k < max_iterations; ++k) {
        // Decaying step sizes
        double a_k = a / pow(k + 1 + 100, alpha);
        double c_k = c / pow(k + 1, gamma);

        // Random perturbation (Rademacher)
        std::vector<double> delta = random_signs(2 * p);

        // Two-point gradient estimate
        auto params_plus = params + c_k * delta;
        auto params_minus = params - c_k * delta;
        double f_plus = evaluateQAOA(Q, params_plus);
        double f_minus = evaluateQAOA(Q, params_minus);

        // Update
        for (int i = 0; i < 2 * p; ++i) {
            double grad = (f_plus - f_minus) / (2 * c_k * delta[i]);
            params[i] -= a_k * grad;
        }
    }
    return params;
}
```

### Hybrid Orchestrator

The hybrid orchestrator combines classical preprocessing, quantum optimization, and classical postprocessing.

```cpp
// include/lumen/solvers/hybrid_orchestrator.hpp

struct HybridOrchestratorConfig {
    int lot_threshold = 50;              ///< Use hybrid above this lot count
    bool preprocess_enabled = true;
    bool postprocess_enabled = true;
    int max_iterations = 3;              ///< Classical-quantum iterations
    double improvement_threshold = 0.001; ///< Stop if below this
    double near_target_tolerance = 0.001; ///< Fix positions within this of target
    bool force_quantum = false;          ///< Force quantum for any size
    QuantumSolverConfig quantum_config;
};

class HybridOrchestrator {
public:
    explicit HybridOrchestrator(const HybridOrchestratorConfig& config);

    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints);

    // Results from last solve
    const PreprocessResult& getLastPreprocessResult() const;
    const PostprocessResult& getLastPostprocessResult() const;
    const QuantumResult& getLastQuantumResult() const;
    int getLastIterationCount() const;

    double estimateCost(const core::Portfolio& portfolio,
                        const core::ConstraintSet& constraints) const;
};
```

#### Preprocessing Stage

Reduces problem size by fixing "obvious" decisions:

```cpp
PreprocessResult preprocess(const Portfolio& portfolio,
                            const TargetAllocation& target,
                            const ConstraintSet& constraints) {
    PreprocessResult result;
    result.original_size = portfolio.getPositionCount();

    for (const auto& position : portfolio.positions) {
        double drift = abs(position.current_weight - position.target_weight);

        if (drift < near_target_tolerance) {
            // Position is already at target - fix it
            result.fixed_positions.push_back({position.ticker, position.target_weight});
        }
    }

    result.reduced_portfolio = applyFixedPositions(portfolio, result.fixed_positions);
    result.reduced_size = result.reduced_portfolio.getPositionCount();
    return result;
}
```

#### Postprocessing Stage

Validates and repairs quantum solutions:

```cpp
PostprocessResult postprocess(const QuantumResult& quantum_result,
                               const PreprocessResult& preprocess,
                               const Portfolio& portfolio,
                               const TargetAllocation& target,
                               const ConstraintSet& constraints) {
    PostprocessResult result;

    // Decode quantum solution
    result.trades = qubo_formulator_->interpretSolution(quantum_result.best_sample);

    // Validate constraints
    result.violations = validateConstraints(result.trades, portfolio, constraints);

    if (!result.violations.empty()) {
        // Repair using classical solver (HiGHS)
        result.trades = repairSolution(result.trades, result.violations,
                                        portfolio, target, constraints);
    }

    result.is_feasible = result.violations.empty();
    result.polished_objective = calculateObjective(result.trades, portfolio, target);
    return result;
}
```

#### Iterative Refinement

```cpp
core::SolverResult HybridOrchestrator::solve(...) {
    PreprocessResult preprocess = this->preprocess(portfolio, target, constraints);

    double best_objective = std::numeric_limits<double>::max();
    std::vector<core::Trade> best_trades;

    for (int iter = 0; iter < config_.max_iterations; ++iter) {
        // Quantum optimization on reduced problem
        QuantumResult quantum = quantumOptimize(preprocess.reduced_portfolio,
                                                 target, constraints);

        // Classical postprocessing
        PostprocessResult post = postprocess(quantum, preprocess, portfolio,
                                              target, constraints);

        // Track best solution
        if (post.polished_objective < best_objective) {
            double improvement = best_objective - post.polished_objective;
            best_objective = post.polished_objective;
            best_trades = post.trades;

            // Check convergence
            if (improvement < config_.improvement_threshold && iter > 0) {
                break;
            }
        }
    }

    return buildResult(best_trades, best_objective);
}
```

### Configuration

#### Environment Variables

```bash
# D-Wave
export DWAVE_API_KEY="your-dwave-api-key"

# IBM Quantum
export IBM_QUANTUM_API_KEY="your-ibm-quantum-api-key"
```

#### Configuration File

```json
{
  "quantum": {
    "enabled": true,
    "provider": "dwave",
    "use_mock": false,
    "show_cost_estimate": true,
    "max_cost_per_solve": 0,

    "dwave": {
      "api_key_env": "DWAVE_API_KEY",
      "solver": "hybrid_binary_quadratic_model_version2",
      "num_reads": 1000,
      "use_hybrid": true,
      "timeout_ms": 60000
    },

    "ibm": {
      "api_key_env": "IBM_QUANTUM_API_KEY",
      "backend": "ibm_brisbane",
      "num_shots": 1024,
      "qaoa_depth": 2,
      "optimizer": "SPSA",
      "timeout_ms": 300000
    },

    "hybrid": {
      "lot_threshold": 50,
      "preprocess_enabled": true,
      "postprocess_enabled": true,
      "max_iterations": 3,
      "improvement_threshold": 0.001
    },

    "qubo": {
      "precision_bits": 8,
      "auto_tune_penalty": true,
      "penalty_factor": 10.0
    }
  }
}
```

### Cost Estimation

Both clients provide cost estimates before solving:

```cpp
// D-Wave cost estimation
double DWaveClient::estimateCost(int num_qubits) const {
    if (use_mock_) return 0.0;

    // Hybrid solver: ~$0.20/minute
    double estimated_minutes = 1.0 + num_qubits / 1000.0;
    return 0.20 * estimated_minutes;
}

// IBM Quantum cost estimation
double IBMQuantumClient::estimateCost(int num_qubits) const {
    if (use_mock_) return 0.0;

    // ~$1.60/second of quantum time
    int circuit_depth = num_qubits * qaoa_depth_ * 3;
    double time_per_shot_us = circuit_depth * 0.5;
    double total_time_s = (time_per_shot_us * num_shots_) / 1e6;
    double optimization_factor = spsa_iterations_ * 2.0;

    return 1.60 * total_time_s * optimization_factor;
}
```

### Adding a New Quantum Backend

1. **Create the client class:**

```cpp
// include/lumen/solvers/ionq_client.hpp

class IonQClient : public QuantumClient {
public:
    explicit IonQClient(const QuantumSolverConfig& config);

    std::string getName() const override { return "IonQ"; }
    bool authenticate() override;
    double estimateCost(int num_qubits) const override;
    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q,
                             int num_shots) override;

    // IonQ specific
    void setTargetMachine(const std::string& machine);  // "simulator", "harmony", "aria"
};
```

2. **Implement the client with mock mode**

3. **Register in `SolverDispatcher::createQuantumClient()`**

4. **Add configuration options**

5. **Add tests**

### Testing Quantum Code

```cpp
// tests/solvers/test_ibm_quantum.cpp

TEST(IBMQuantumTest, MockModeProducesValidResults) {
    QuantumSolverConfig config;
    config.provider = "mock";

    IBMQuantumClient client(config);
    client.authenticate();

    std::vector<std::vector<double>> Q = {{-1, 0.5}, {0, -1}};
    auto result = client.submitQUBO(Q, 100);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.best_sample.size(), 2);
    EXPECT_FALSE(result.all_samples.empty());
}

TEST(IBMQuantumTest, QAOACircuitIsValid) {
    IBMQuantumClient client(config);
    std::vector<double> gamma = {0.5};
    std::vector<double> beta = {0.3};

    std::string circuit = client.getQAOACircuit(Q, gamma, beta);

    EXPECT_NE(circuit.find("OPENQASM 3.0"), std::string::npos);
    EXPECT_NE(circuit.find("h q["), std::string::npos);
    EXPECT_NE(circuit.find("rz("), std::string::npos);
    EXPECT_NE(circuit.find("measure"), std::string::npos);
}
```

---

## GUI Module

The GUI module provides a Qt6-based desktop application for visual portfolio management and optimization. It targets **Windows 11+** and **Ubuntu 20.04+** (no macOS support in this version).

### Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                           Qt Application                                │
├────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                       MainWindow                                  │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │  │
│  │  │ Menu Bar     │  │  Tool Bar    │  │  Status Bar  │           │  │
│  │  └──────────────┘  └──────────────┘  └──────────────┘           │  │
│  │  ┌──────────────────────────────────────────────────────────┐   │  │
│  │  │                    Central Widget                         │   │  │
│  │  │  ┌────────────────────┐  ┌────────────────────────────┐  │   │  │
│  │  │  │ PortfolioTableView │  │ AllocationChartView        │  │   │  │
│  │  │  │ (QTableView)       │  │ (QChartView)               │  │   │  │
│  │  │  └────────────────────┘  └────────────────────────────┘  │   │  │
│  │  └──────────────────────────────────────────────────────────┘   │  │
│  │  ┌──────────────────────────────────────────────────────────┐   │  │
│  │  │                     Dock Widgets                          │   │  │
│  │  │  ┌───────────────┐ ┌───────────────┐ ┌───────────────┐   │   │  │
│  │  │  │ConstraintPanel│ │ ResultsPanel  │ │ComparisonPanel│   │   │  │
│  │  │  └───────────────┘ └───────────────┘ └───────────────┘   │   │  │
│  │  └──────────────────────────────────────────────────────────┘   │  │
│  └─────────────────────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                    ApplicationState                              │  │
│  │  - Portfolio data (Q_PROPERTY)                                   │  │
│  │  - Target allocation (Q_PROPERTY)                                │  │
│  │  - Constraints (Q_PROPERTY)                                      │  │
│  │  - Solver result (Q_PROPERTY)                                    │  │
│  │  - Settings cache                                                │  │
│  │  - Signals: portfolioChanged, resultAvailable, etc.             │  │
│  └─────────────────────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │                    lumen-core Library                            │  │
│  └─────────────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────────────┘
```

### Directory Structure

```
apps/gui/
├── main.cpp                    # Application entry point
├── mainwindow.hpp/cpp          # Main window with menus, toolbar, docks
├── applicationstate.hpp/cpp    # Centralized state management
├── settingsdialog.hpp/cpp      # Settings dialog (General, Optimization, Quantum, Appearance)
├── aboutdialog.hpp/cpp         # About dialog
├── models/
│   └── portfoliomodel.hpp/cpp  # QAbstractTableModel for portfolio data
├── views/
│   ├── portfoliotableview.hpp/cpp     # Table view with sorting
│   ├── allocationchartview.hpp/cpp    # Pie chart visualization
│   └── targeteditor.hpp/cpp           # Target allocation editor
├── dialogs/
│   ├── positiondialog.hpp/cpp  # Add/edit position dialog
│   └── importdialog.hpp/cpp    # CSV import dialog with column mapping
├── panels/
│   ├── constraintpanel.hpp/cpp # Constraint configuration panel
│   ├── resultspanel.hpp/cpp    # Optimization results display
│   └── comparisonpanel.hpp/cpp # Before/after comparison
├── workers/
│   └── optimizationworker.hpp/cpp     # Background optimization thread
├── widgets/
│   └── progresswidget.hpp/cpp  # Progress bar with cancel
└── resources/
    ├── lumen.qrc               # Qt resource file
    ├── lumen.rc                # Windows resource file (icon, version)
    ├── lumen.desktop.in        # Linux desktop entry template
    ├── styles/
    │   ├── light.qss           # Light theme stylesheet
    │   └── dark.qss            # Dark theme stylesheet
    └── icons/
        └── lumen.png           # Application icon
```

### Key Components

#### ApplicationState

Centralized state management using Qt's property system:

```cpp
class ApplicationState : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY unsavedChangesChanged)

public:
    // Portfolio access
    lumen::core::Portfolio& portfolio();
    const lumen::core::Portfolio& portfolio() const;
    void setPortfolio(const lumen::core::Portfolio& portfolio);

    // Target allocation
    lumen::core::TargetAllocation& targetAllocation();
    void setTargetAllocation(const lumen::core::TargetAllocation& target);

    // Constraints
    lumen::core::ConstraintSet& constraints();
    void setConstraints(const lumen::core::ConstraintSet& constraints);

    // Results
    void setResult(const lumen::core::SolverResult& result);
    void clearResult();

signals:
    void portfolioChanged();
    void targetAllocationChanged();
    void constraintsChanged();
    void resultAvailable(const lumen::core::SolverResult& result);
    void unsavedChangesChanged();
};
```

#### PortfolioModel

Qt model for displaying portfolio in table views:

```cpp
class PortfolioModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        Ticker = 0,
        Shares,
        Price,
        Value,
        CostBasis,
        UnrealizedGain,
        GainPercent,
        Allocation,
        AssetClass,
        ColumnCount
    };

    // Required overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation, int role) const override;

    // Editing support
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // Sorting
    void sort(int column, Qt::SortOrder order) override;
};
```

#### OptimizationWorker

Background thread for running optimization:

```cpp
class OptimizationWorker : public QObject {
    Q_OBJECT

public:
    void setPortfolio(const lumen::core::Portfolio& portfolio);
    void setTarget(const lumen::core::TargetAllocation& target);
    void setConstraints(const lumen::core::ConstraintSet& constraints);
    void setConfig(const lumen::core::SolverConfig& config);

public slots:
    void run();
    void cancel();

signals:
    void started();
    void progress(int percent, const QString& message);
    void finished(const lumen::core::SolverResult& result);
    void error(const QString& message);
    void cancelled();
};
```

### State Management Pattern

The GUI uses a unidirectional data flow pattern:

```
┌──────────────────────────────────────────────────────────────────────┐
│                          User Action                                  │
└────────────────────────────────┬─────────────────────────────────────┘
                                 │
                                 ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      ApplicationState                                 │
│  (Portfolio, Targets, Constraints, Results)                          │
└────────────────────────────────┬─────────────────────────────────────┘
                                 │ emit signals
                                 ▼
┌──────────────────────────────────────────────────────────────────────┐
│                      UI Components                                    │
│  (Views, Panels, Dialogs)                                            │
│  - Connect to signals                                                 │
│  - Update display                                                     │
└──────────────────────────────────────────────────────────────────────┘
```

### Threading Model

- **Main Thread**: UI rendering, user interaction
- **Worker Thread**: Optimization via `OptimizationWorker`
- **Qt Signals/Slots**: Thread-safe communication

```cpp
// Launch optimization in background
auto* worker = new OptimizationWorker();
auto* thread = new QThread();

worker->moveToThread(thread);
connect(thread, &QThread::started, worker, &OptimizationWorker::run);
connect(worker, &OptimizationWorker::finished, this, &MainWindow::onOptimizationFinished);
connect(worker, &OptimizationWorker::finished, thread, &QThread::quit);
connect(thread, &QThread::finished, worker, &QObject::deleteLater);
connect(thread, &QThread::finished, thread, &QObject::deleteLater);

thread->start();
```

### Theming

Stylesheets in QSS format with system dark mode detection:

```cpp
// Detect system dark mode (Windows/Linux)
bool isDarkMode() {
#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       QSettings::NativeFormat);
    return settings.value("AppsUseLightTheme", 1).toInt() == 0;
#else
    // Linux: check GTK or KDE settings
    QPalette palette = QApplication::palette();
    return palette.color(QPalette::Window).lightness() < 128;
#endif
}

// Apply stylesheet
void applyTheme(const QString& theme) {
    QString path = (theme == "dark") ? ":/styles/dark.qss" : ":/styles/light.qss";
    QFile file(path);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(file.readAll());
    }
}
```

### Building the GUI

```bash
# Enable GUI in CMake
cmake -B build -DBUILD_GUI=ON

# Build
cmake --build build --target lumen-gui

# Run
./build/lumen-gui
```

### Packaging

#### Windows (NSIS)

```bash
# Build release
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON
cmake --build build --config Release

# Create installer
cd build
cpack -G NSIS
```

#### Linux (AppImage)

```bash
# Use the provided script
./scripts/build-appimage.sh

# Or manual
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=ON
cmake --build build
./linuxdeploy --appdir AppDir --plugin qt --output appimage
```

### Adding New UI Components

1. **Create the widget class**:

```cpp
// widgets/mywidget.hpp
class MyWidget : public QWidget {
    Q_OBJECT

public:
    explicit MyWidget(ApplicationState* state, QWidget* parent = nullptr);

signals:
    void userAction();

private:
    void setupUI();
    void setupConnections();

    ApplicationState* state_;
};
```

2. **Connect to ApplicationState**:

```cpp
void MyWidget::setupConnections() {
    connect(state_, &ApplicationState::portfolioChanged,
            this, &MyWidget::updateDisplay);
}
```

3. **Add to CMakeLists.txt**:

```cmake
set(GUI_SOURCES
    # ...
    apps/gui/widgets/mywidget.cpp
)
set(GUI_HEADERS
    # ...
    apps/gui/widgets/mywidget.hpp
)
```

4. **Integrate into MainWindow** as dock widget or central area

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 5.0.0 | TBD | Phase 5: Desktop GUI (Windows 11+, Ubuntu 20.04+), visual portfolio management, interactive optimization |
| 4.0.0 | TBD | Phase 4: Quantum integration, D-Wave annealing, IBM Quantum QAOA, hybrid orchestration |
| 3.0.0 | TBD | Phase 3: Tax optimization, tax lot management, broker import, wash sale detection |
| 2.0.0 | TBD | Phase 2: Market data, persistence, explainability, CLI |
| 1.0.0 | TBD | Initial release with Phase 1 features |

---

*For user documentation, see [USER_GUIDE.md](USER_GUIDE.md).*
