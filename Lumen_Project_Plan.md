# Lumen Project Implementation Plan

**Version:** 1.0
**Date:** January 19, 2026
**Organization:** OA Quantum Labs
**Project Lead:** Danny (Founder, CEO & CTO)
**Status:** Implementation Roadmap

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Project Timeline Overview](#project-timeline-overview)
3. [Phase 0: Project Setup](#phase-0-project-setup-week-0)
4. [Phase 1: Core Infrastructure](#phase-1-core-infrastructure-weeks-1-4)
5. [Phase 2: MVP Portfolio Optimizer](#phase-2-mvp-portfolio-optimizer-weeks-5-8)
6. [Phase 3: Tax Optimization](#phase-3-tax-optimization-weeks-9-12)
7. [Phase 4: Quantum Integration](#phase-4-quantum-integration-weeks-13-16)
8. [Phase 5: GUI & User Experience](#phase-5-gui--user-experience-weeks-17-20)
9. [Phase 6: Mobile & Cross-Platform](#phase-6-mobile--cross-platform-weeks-21-24)
10. [Ongoing: Quality & Observability](#ongoing-quality--observability)
11. [Detailed Task Breakdown](#detailed-task-breakdown)
12. [Dependency Graph](#dependency-graph)
13. [Testing Strategy](#testing-strategy)
14. [Risk Management](#risk-management)
15. [Success Criteria](#success-criteria)
16. [Resource Requirements](#resource-requirements)

---

## Executive Summary

This document provides a detailed, task-level implementation plan for the Lumen project. It breaks down the 6-month development timeline into weekly sprints with specific deliverables, acceptance criteria, and dependencies.

**Key Milestones:**
| Milestone | Target Date | Description |
|-----------|-------------|-------------|
| M1: First Optimization | Week 4 | End-to-end classical optimization working |
| M2: MVP Release | Week 8 | CLI tool with full rebalancing capability |
| M3: Tax Features | Week 12 | Tax-loss harvesting and lot selection |
| M4: Quantum Live | Week 16 | D-Wave integration operational |
| M5: Desktop App | Week 20 | Qt-based GUI application |
| M6: Public Launch | Week 24 | Full release with mobile foundation |

---

## Project Timeline Overview

```
Week:  0    4    8    12   16   20   24
       │    │    │    │    │    │    │
       ▼    ▼    ▼    ▼    ▼    ▼    ▼
       ┌────┬────┬────┬────┬────┬────┐
Phase: │ 0  │ 1  │ 2  │ 3  │ 4  │5/6 │
       │Setup│Infra│MVP │Tax │Qntm│GUI │
       └────┴────┴────┴────┴────┴────┘
             │    │    │    │    │
             M1   M2   M3   M4   M5/M6
```

---

## Phase 0: Project Setup (Week 0)

**Goal:** Establish development environment and project foundation.

### 0.1 Repository & Tooling Setup

| Task ID | Task | Priority | Acceptance Criteria |
|---------|------|----------|---------------------|
| 0.1.1 | Create GitHub repository | P0 | Repo created with MIT license |
| 0.1.2 | Initialize directory structure | P0 | Matches architecture spec |
| 0.1.3 | Configure CMake build system | P0 | `cmake .. && make` succeeds |
| 0.1.4 | Setup vcpkg integration | P0 | Dependencies install via vcpkg |
| 0.1.5 | Configure CI/CD (GitHub Actions) | P1 | Build runs on push |
| 0.1.6 | Setup code formatting (clang-format) | P1 | `.clang-format` committed |
| 0.1.7 | Configure static analysis (clang-tidy) | P2 | Runs in CI |

### 0.2 Documentation Foundation

| Task ID | Task | Priority | Acceptance Criteria |
|---------|------|----------|---------------------|
| 0.2.1 | Write README.md | P0 | Build instructions, overview |
| 0.2.2 | Create CONTRIBUTING.md | P1 | Contribution guidelines |
| 0.2.3 | Setup docs/ structure | P1 | Placeholder docs created |
| 0.2.4 | Create CHANGELOG.md | P2 | Semantic versioning format |

### 0.3 Development Environment

| Task ID | Task | Priority | Acceptance Criteria |
|---------|------|----------|---------------------|
| 0.3.1 | Document dev setup | P0 | New dev can build in <30 min |
| 0.3.2 | Create .env.example | P1 | API key placeholders documented |
| 0.3.3 | Setup pre-commit hooks | P2 | Format check on commit |

**Directory Structure to Create:**
```
lumen/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── .gitignore
├── .clang-format
├── .github/
│   └── workflows/
│       └── ci.yml
├── cmake/
│   └── vcpkg.cmake
├── include/
│   └── lumen/
│       ├── core/
│       ├── solvers/
│       ├── data/
│       ├── explain/
│       └── utils/
├── src/
│   ├── core/
│   ├── solvers/
│   ├── data/
│   ├── explain/
│   └── utils/
├── apps/
│   ├── cli/
│   └── server/
├── test/
│   ├── unit/
│   ├── integration/
│   └── benchmarks/
├── docs/
└── scripts/
```

**Deliverables:**
- [ ] GitHub repo with CI passing
- [ ] Empty project compiles
- [ ] README with setup instructions

---

## Phase 1: Core Infrastructure (Weeks 1-4)

**Goal:** Build foundational data structures and solver integration.

### Week 1: Core Data Structures

#### 1.1 Portfolio Module

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 1.1.1 | Implement `Position` struct | `core/portfolio.hpp/cpp` | All fields, computed properties |
| 1.1.2 | Implement `Portfolio` class | `core/portfolio.hpp/cpp` | CRUD for positions |
| 1.1.3 | Implement `TargetAllocation` class | `core/portfolio.hpp/cpp` | Target management, validation |
| 1.1.4 | JSON serialization for Portfolio | `core/portfolio.cpp` | `toJSON()`, `fromJSON()` work |
| 1.1.5 | CSV parsing for Portfolio | `core/portfolio.cpp` | `fromCSV()` parses standard format |
| 1.1.6 | Unit tests for Portfolio | `test/unit/core/portfolio_test.cpp` | 90%+ coverage |

**Position Implementation Details:**
```cpp
struct Position {
    std::string ticker;
    double shares;
    double current_price;
    double cost_basis;
    std::chrono::system_clock::time_point purchase_date;
    AssetClass asset_class;
    std::string exchange;
    bool supports_fractional;

    // Implement these computed properties:
    double getCurrentValue() const;
    double getCostBasisTotal() const;
    double getUnrealizedGain() const;
    double getUnrealizedGainPercent() const;
    bool isLongTerm() const;
};
```

**Test Cases Required:**
- [ ] Create position with valid data
- [ ] Calculate current value correctly
- [ ] Calculate unrealized gain correctly
- [ ] Determine long-term status correctly
- [ ] Serialize to JSON and back
- [ ] Parse from CSV row

### Week 2: Constraint System

#### 1.2 Constraint Module

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 1.2.1 | Implement `Constraint` base class | `core/constraint.hpp` | Abstract interface defined |
| 1.2.2 | Implement `BudgetConstraint` | `core/constraint.cpp` | Budget limits enforced |
| 1.2.3 | Implement `AllocationConstraint` | `core/constraint.cpp` | Min/max bounds work |
| 1.2.4 | Implement `MinTradeSizeConstraint` | `core/constraint.cpp` | Filters small trades |
| 1.2.5 | Implement `IntegerShareConstraint` | `core/constraint.cpp` | Whole shares only |
| 1.2.6 | Implement `PositionLimitConstraint` | `core/constraint.cpp` | Max % per position |
| 1.2.7 | Implement `ConstraintSet` | `core/constraint.cpp` | Collection management |
| 1.2.8 | Constraint validation logic | `core/constraint.cpp` | Detects conflicts |
| 1.2.9 | Unit tests for Constraints | `test/unit/core/constraint_test.cpp` | All constraint types tested |

**Constraint Interface:**
```cpp
class Constraint {
public:
    virtual ConstraintType getType() const = 0;
    virtual std::string getName() const = 0;
    virtual bool isValid() const = 0;
    virtual bool isSatisfied(const Portfolio&, const std::vector<Trade>&) const = 0;
    virtual void addToModel(HighsModel&, const std::vector<int>&) const = 0;
};
```

**Test Cases Required:**
- [ ] BudgetConstraint enforces total value
- [ ] AllocationConstraint rejects out-of-bounds
- [ ] MinTradeSize filters $10 trades correctly
- [ ] IntegerShare rounds correctly
- [ ] PositionLimit caps at 25%
- [ ] ConstraintSet detects conflicting constraints

### Week 3: HiGHS Solver Integration

#### 1.3 Solver Module (Classical)

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 1.3.1 | HiGHS library integration | `CMakeLists.txt`, vcpkg | HiGHS compiles and links |
| 1.3.2 | Implement `BaseSolver` interface | `solvers/base_solver.hpp` | Abstract interface |
| 1.3.3 | Implement `MILPBuilder` | `solvers/highs_wrapper.hpp/cpp` | Builds LP/MILP models |
| 1.3.4 | Implement `HighsOptimizer` | `solvers/highs_wrapper.cpp` | Solves models |
| 1.3.5 | Objective function construction | `solvers/highs_wrapper.cpp` | Drift minimization works |
| 1.3.6 | Constraint translation to HiGHS | `solvers/highs_wrapper.cpp` | All constraint types |
| 1.3.7 | Solution extraction | `solvers/highs_wrapper.cpp` | Extracts trades |
| 1.3.8 | Integration test: simple optimization | `test/integration/solver_test.cpp` | 5-position portfolio |

**MILPBuilder API:**
```cpp
MILPBuilder builder(portfolio, target);
builder.setObjective(ObjectiveType::MINIMIZE_DRIFT);
builder.addConstraintSet(constraints);
HighsModel model = builder.build();

HighsOptimizer optimizer;
SolverResult result = optimizer.solveModel(model);
```

**Test Cases Required:**
- [ ] Solve trivial 2-position rebalance
- [ ] Solve 5-position with all constraint types
- [ ] Handle infeasible problem gracefully
- [ ] Respect timeout setting
- [ ] Integer constraints produce whole numbers

### Week 4: Solver Dispatcher & End-to-End

#### 1.4 Dispatcher & Integration

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 1.4.1 | Implement `ProblemCharacteristics` | `core/solver_dispatcher.hpp` | Classification logic |
| 1.4.2 | Implement `SolverDispatcher` | `core/solver_dispatcher.cpp` | Routes to HiGHS |
| 1.4.3 | Implement `SolverResult` struct | `core/solver_dispatcher.hpp` | All result fields |
| 1.4.4 | Implement `Trade` struct | `core/solver_dispatcher.hpp` | Trade representation |
| 1.4.5 | Tier selection logic | `core/solver_dispatcher.cpp` | Tier 1/2/3 rules |
| 1.4.6 | End-to-end integration test | `test/integration/e2e_test.cpp` | Full pipeline works |
| 1.4.7 | Performance benchmark | `test/benchmarks/solver_bench.cpp` | <1s for 20 positions |

**Tier Selection Logic:**
```cpp
SolverTier SolverDispatcher::selectTier(const ProblemCharacteristics& c) const {
    if (c.num_positions < 20 && !c.has_tax_optimization) {
        return SolverTier::TIER_1;  // Classical only
    } else if (c.num_positions < 50) {
        return SolverTier::TIER_2;  // Hybrid
    } else {
        return SolverTier::TIER_3;  // Quantum
    }
}
```

**Milestone M1 Criteria:**
- [ ] Can optimize a 20-position portfolio
- [ ] Result includes recommended trades
- [ ] Constraints are satisfied in solution
- [ ] Solve time <1 second

---

## Phase 2: MVP Portfolio Optimizer (Weeks 5-8)

**Goal:** Build complete CLI application with market data and explainability.

### Week 5: Market Data Integration

#### 2.1 Market Data Module

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 2.1.1 | Implement `PriceQuote` struct | `data/market_data.hpp` | All quote fields |
| 2.1.2 | Implement `TimeSeries` struct | `data/market_data.hpp` | OHLCV data |
| 2.1.3 | Implement `MarketDataProvider` interface | `data/market_data.hpp` | Abstract provider |
| 2.1.4 | Implement `AlphaVantageProvider` | `data/market_data.cpp` | Fetches real quotes |
| 2.1.5 | Implement `YahooFinanceProvider` | `data/market_data.cpp` | Fallback provider |
| 2.1.6 | Implement `DataCache` (SQLite) | `data/market_data.cpp` | Caches with TTL |
| 2.1.7 | Implement `MarketDataClient` facade | `data/market_data.cpp` | Cache + providers |
| 2.1.8 | HTTP client integration (cpp-httplib) | `CMakeLists.txt` | HTTP calls work |
| 2.1.9 | Unit tests for market data | `test/unit/data/market_data_test.cpp` | Mock API tests |

**Alpha Vantage Integration:**
```cpp
class AlphaVantageProvider : public MarketDataProvider {
public:
    PriceQuote getCurrentPrice(const std::string& ticker) override {
        // GET https://www.alphavantage.co/query
        //   ?function=GLOBAL_QUOTE
        //   &symbol={ticker}
        //   &apikey={API_KEY}
    }
};
```

**Test Cases Required:**
- [ ] Fetch single ticker quote
- [ ] Batch fetch 10 tickers
- [ ] Cache hit returns cached data
- [ ] Cache miss fetches from API
- [ ] Expired cache refetches
- [ ] Fallback to Yahoo on Alpha Vantage failure

### Week 6: Persistence & Configuration

#### 2.2 Utils Module

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 2.2.1 | Implement `Logger` class | `utils/logging.hpp/cpp` | Structured logging |
| 2.2.2 | Implement `Configuration` class | `utils/config.hpp/cpp` | YAML parsing |
| 2.2.3 | Implement config file loading | `utils/config.cpp` | `~/.lumen/config.yaml` |
| 2.2.4 | Implement environment variable loading | `utils/config.cpp` | API keys from env |
| 2.2.5 | SQLite database setup | `data/persistence.cpp` | Schema creation |
| 2.2.6 | Portfolio persistence | `data/persistence.cpp` | Save/load portfolios |
| 2.2.7 | Optimization run history | `data/persistence.cpp` | Store past results |
| 2.2.8 | Unit tests for config | `test/unit/utils/config_test.cpp` | Config parsing |

**Configuration File Structure:**
```yaml
# ~/.lumen/config.yaml
solver:
  classical:
    default: highs
    timeout_ms: 30000
  quantum:
    enabled: false

market_data:
  providers:
    - name: alpha_vantage
      api_key_env: ALPHA_VANTAGE_API_KEY
  cache_ttl_minutes: 60

persistence:
  database_path: ~/.lumen/data/lumen.db
```

**Database Schema:**
```sql
CREATE TABLE portfolio_snapshots (
    id INTEGER PRIMARY KEY,
    snapshot_id TEXT UNIQUE,
    timestamp DATETIME,
    data_json TEXT
);

CREATE TABLE market_cache (
    ticker TEXT PRIMARY KEY,
    price REAL,
    fetched_at DATETIME,
    expires_at DATETIME
);

CREATE TABLE optimization_runs (
    id INTEGER PRIMARY KEY,
    session_id TEXT,
    timestamp DATETIME,
    result_json TEXT
);
```

### Week 7: Explainability Module

#### 2.3 Explain Module

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 2.3.1 | Implement `Provenance` class | `explain/provenance.hpp/cpp` | Tracks data sources |
| 2.3.2 | Implement `DataSourceRecord` | `explain/provenance.hpp` | Source metadata |
| 2.3.3 | Implement `ConstraintRecord` | `explain/provenance.hpp` | Constraint status |
| 2.3.4 | Implement `TradeRationale` struct | `explain/explainer.hpp` | Trade reasoning |
| 2.3.5 | Implement `AllocationComparison` | `explain/explainer.hpp` | Before/after |
| 2.3.6 | Implement `Explainer` class | `explain/explainer.cpp` | Generates explanations |
| 2.3.7 | Implement `ExplanationDocument` | `explain/explainer.cpp` | Multi-format output |
| 2.3.8 | JSON output format | `explain/explainer.cpp` | Machine-readable |
| 2.3.9 | Plain text output format | `explain/explainer.cpp` | Human-readable |
| 2.3.10 | Unit tests for explainer | `test/unit/explain/explainer_test.cpp` | Output validation |

**Trade Rationale Generation:**
```cpp
TradeRationale Explainer::explainTrade(const Trade& trade, ...) {
    TradeRationale rationale;
    rationale.trade = trade;

    // Generate reasons based on:
    // 1. Deviation from target allocation
    // 2. Constraint satisfaction
    // 3. Tax impact (if applicable)

    if (before_alloc > target + tolerance) {
        rationale.reasons.push_back(
            "Reduces " + ticker + " from " + pct(before) +
            " to " + pct(after) + " (target: " + pct(target) + ")");
    }

    return rationale;
}
```

**Output Example:**
```
=== Portfolio Rebalancing Recommendation ===

BEFORE: Stocks 65.2%, Bonds 34.8%
AFTER:  Stocks 60.0%, Bonds 40.0%
TARGET: Stocks 60.0%, Bonds 40.0%

Recommended Trades:
1. SELL 8 shares of VTI @ $250.00 = $2,000
   Reason: Reduces stock allocation from 65.2% to 60.0%

2. BUY 20 shares of BND @ $100.00 = $2,000
   Reason: Increases bond allocation from 34.8% to 40.0%

Transaction Cost: $4.95
Constraints Satisfied: Yes
Solver: HiGHS (234ms)
```

### Week 8: CLI Application

#### 2.4 CLI Application

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 2.4.1 | CLI11 integration | `CMakeLists.txt` | Parses arguments |
| 2.4.2 | `optimize` subcommand | `apps/cli/main.cpp` | Core functionality |
| 2.4.3 | `import` subcommand | `apps/cli/main.cpp` | CSV/JSON import |
| 2.4.4 | `explain` subcommand | `apps/cli/main.cpp` | Show past results |
| 2.4.5 | `config` subcommand | `apps/cli/main.cpp` | View/edit config |
| 2.4.6 | Output formatting (JSON/text) | `apps/cli/main.cpp` | `--format` flag |
| 2.4.7 | Verbose/quiet modes | `apps/cli/main.cpp` | `-v`, `-q` flags |
| 2.4.8 | Error handling & messages | `apps/cli/main.cpp` | User-friendly errors |
| 2.4.9 | Integration tests for CLI | `test/integration/cli_test.cpp` | Command tests |
| 2.4.10 | User documentation | `docs/cli-usage.md` | Command reference |

**CLI Commands:**
```bash
# Basic optimization
lumen optimize --portfolio portfolio.csv --target 60-40-stocks-bonds

# With constraints
lumen optimize -p portfolio.csv -t 60-40 --max-position 0.20 --min-trade 100

# Import from broker
lumen import --broker schwab --file export.csv

# View past results
lumen explain --session abc123

# Configuration
lumen config --show
lumen config --set market_data.cache_ttl_minutes=120
```

**Milestone M2 Criteria (MVP):**
- [ ] CLI tool installs and runs
- [ ] Can import portfolio from CSV
- [ ] Fetches real-time prices from Alpha Vantage
- [ ] Generates optimized trades
- [ ] Explains reasoning for each trade
- [ ] Results persist in SQLite database
- [ ] Documentation complete

---

## Phase 3: Tax Optimization (Weeks 9-12)

**Goal:** Implement tax-aware optimization with lot selection.

### Week 9: Tax Lot Data Structures

#### 3.1 Tax Lot Module

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 3.1.1 | Implement `TaxLot` struct | `data/tax_lot.hpp` | All lot fields |
| 3.1.2 | Implement `GainType` enum | `data/tax_lot.hpp` | SHORT_TERM, LONG_TERM |
| 3.1.3 | Implement `TaxLotMethod` enum | `data/tax_lot.hpp` | FIFO, LIFO, HIFO, SPEC_ID |
| 3.1.4 | Implement `TaxLotManager` | `data/tax_lot.cpp` | Lot CRUD operations |
| 3.1.5 | Implement lot selection (FIFO) | `data/tax_lot.cpp` | First in, first out |
| 3.1.6 | Implement lot selection (LIFO) | `data/tax_lot.cpp` | Last in, first out |
| 3.1.7 | Implement lot selection (HIFO) | `data/tax_lot.cpp` | Highest cost first |
| 3.1.8 | Implement lot selection (SpecID) | `data/tax_lot.cpp` | Specific identification |
| 3.1.9 | Database persistence for lots | `data/tax_lot.cpp` | SQLite storage |
| 3.1.10 | Unit tests for tax lots | `test/unit/data/tax_lot_test.cpp` | All methods tested |

**TaxLot Structure:**
```cpp
struct TaxLot {
    std::string id;
    std::string ticker;
    double shares;
    double cost_basis_per_share;
    std::chrono::system_clock::time_point purchase_date;

    GainType getGainType() const {
        auto days = daysSincePurchase();
        return (days > 365) ? GainType::LONG_TERM : GainType::SHORT_TERM;
    }

    double getUnrealizedGain(double current_price) const {
        return (current_price - cost_basis_per_share) * shares;
    }
};
```

**Lot Selection Example (HIFO):**
```cpp
// For tax-loss harvesting, sell highest cost basis lots first
std::vector<TaxLot> TaxLotManager::selectLotsHIFO(
    const std::string& ticker, double shares_to_sell) {

    auto lots = getLotsForTicker(ticker);
    std::sort(lots.begin(), lots.end(),
        [](const TaxLot& a, const TaxLot& b) {
            return a.cost_basis_per_share > b.cost_basis_per_share;
        });

    return selectFromSorted(lots, shares_to_sell);
}
```

### Week 10: Wash Sale & Tax Rules

#### 3.2 Tax Optimizer

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 3.2.1 | Implement `TaxOptimizer` class | `data/tax_lot.cpp` | Tax optimization |
| 3.2.2 | Implement wash sale detection | `data/tax_lot.cpp` | 30-day rule |
| 3.2.3 | Implement wash sale prevention | `data/tax_lot.cpp` | Blocks invalid trades |
| 3.2.4 | Implement tax-loss harvesting finder | `data/tax_lot.cpp` | Identifies opportunities |
| 3.2.5 | Implement capital gains calculation | `data/tax_lot.cpp` | ST/LT gains |
| 3.2.6 | Implement tax impact estimation | `data/tax_lot.cpp` | Tax cost estimate |
| 3.2.7 | Implement `CapitalGainsReport` | `data/tax_lot.cpp` | Tax summary |
| 3.2.8 | Add tax constraint to solver | `core/constraint.cpp` | `TaxLotConstraint` |
| 3.2.9 | Unit tests for tax optimizer | `test/unit/data/tax_optimizer_test.cpp` | All rules tested |

**Wash Sale Rule Implementation:**
```cpp
bool TaxOptimizer::wouldTriggerWashSale(
    const TaxLot& lot_to_sell,
    const std::string& replacement_ticker,
    const std::chrono::system_clock::time_point& trade_date) const {

    // Wash sale: Cannot buy "substantially identical" security
    // within 30 days before or after selling at a loss

    if (lot_to_sell.getUnrealizedGain(current_price) >= 0) {
        return false;  // No loss, no wash sale
    }

    if (!isSubstantiallyIdentical(lot_to_sell.ticker, replacement_ticker)) {
        return false;  // Different security, OK
    }

    // Check 61-day window (30 before + sale day + 30 after)
    auto window_start = trade_date - std::chrono::days(30);
    auto window_end = trade_date + std::chrono::days(30);

    return hasReplacementPurchase(replacement_ticker, window_start, window_end);
}
```

**Tax-Loss Harvesting Finder:**
```cpp
std::vector<TaxHarvestingOpportunity> TaxOptimizer::findOpportunities(
    const std::map<std::string, double>& current_prices,
    double min_loss_threshold) const {

    std::vector<TaxHarvestingOpportunity> opportunities;

    for (const auto& lot : lot_manager_.getUnsoldLots()) {
        double current_price = current_prices.at(lot.ticker);
        double unrealized_loss = lot.getUnrealizedGain(current_price);

        if (unrealized_loss < -min_loss_threshold) {
            TaxHarvestingOpportunity opp;
            opp.lot = lot;
            opp.unrealized_loss = std::abs(unrealized_loss);
            opp.gain_type = lot.getGainType();
            opp.replacement_candidates = findReplacements(lot.ticker);
            opp.would_trigger_wash_sale = false;  // Calculated per replacement
            opportunities.push_back(opp);
        }
    }

    return opportunities;
}
```

### Week 11: Tax-Aware Optimization

#### 3.3 Tax Integration

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 3.3.1 | Add tax weight to objective | `solvers/highs_wrapper.cpp` | Tax term in objective |
| 3.3.2 | Implement multi-objective weights | `core/solver_dispatcher.cpp` | Drift + cost + tax |
| 3.3.3 | Implement tax-lot-aware solver | `solvers/highs_wrapper.cpp` | Lot-level decisions |
| 3.3.4 | Add wash sale constraints to model | `solvers/highs_wrapper.cpp` | Constraint rows |
| 3.3.5 | Tax impact in explainer | `explain/explainer.cpp` | Tax savings shown |
| 3.3.6 | Tax report generation | `explain/explainer.cpp` | `CapitalGainsReport` |
| 3.3.7 | CLI `tax-harvest` command | `apps/cli/main.cpp` | Tax harvesting CLI |
| 3.3.8 | Integration tests for tax | `test/integration/tax_test.cpp` | End-to-end tax |

**Multi-Objective Formulation:**
```
Minimize:
    w_drift * Σ(deviation_i²)
  + w_cost * Σ(transaction_cost_j)
  - w_tax * Σ(tax_loss_harvested_k)

Subject to:
    Budget constraints
    Allocation constraints
    Wash sale constraints
    Integer share constraints (if applicable)
```

**CLI Tax Harvest Command:**
```bash
lumen tax-harvest --portfolio portfolio.csv --tax-rate 0.22 --min-loss 100

Output:
=== Tax-Loss Harvesting Opportunities ===

1. AAPL Lot #3 (purchased 2025-06-15)
   Shares: 10 @ $180.00 cost basis
   Current: $150.00
   Unrealized Loss: $300.00 (Long-term)
   Potential Tax Savings: $45.00 (at 15% LTCG rate)
   Replacement: Consider QQQ, VGT (no wash sale)

2. TSLA Lot #1 (purchased 2025-11-01)
   Shares: 5 @ $250.00 cost basis
   Current: $200.00
   Unrealized Loss: $250.00 (Short-term)
   Potential Tax Savings: $55.00 (at 22% ordinary rate)
   Replacement: Consider ARKK (no wash sale)

Total Harvestable Losses: $550.00
Estimated Tax Savings: $100.00
```

### Week 12: Tax Testing & Polish

#### 3.4 Tax Finalization

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 3.4.1 | Comprehensive tax unit tests | `test/unit/` | All edge cases |
| 3.4.2 | Tax integration tests | `test/integration/` | Real-world scenarios |
| 3.4.3 | Tax documentation | `docs/tax-optimization.md` | User guide |
| 3.4.4 | Performance testing with tax | `test/benchmarks/` | <5s for 50 lots |
| 3.4.5 | Tax disclaimers and warnings | `apps/cli/main.cpp` | Legal notices |
| 3.4.6 | Tax lot import from brokers | `data/tax_lot.cpp` | Schwab, Fidelity format |
| 3.4.7 | Tax lot export for records | `data/tax_lot.cpp` | CSV export |
| 3.4.8 | Bug fixes and edge cases | Various | All tests pass |

**Milestone M3 Criteria (Tax Features):**
- [ ] Tax lots can be imported and managed
- [ ] Wash sale rules are enforced
- [ ] Tax-loss harvesting opportunities identified
- [ ] FIFO/LIFO/HIFO/SpecID selection works
- [ ] Tax impact shown in optimization results
- [ ] Capital gains report generated
- [ ] Performance acceptable for 50+ lots

---

## Phase 4: Quantum Integration (Weeks 13-16)

**Goal:** Integrate D-Wave quantum annealing for complex problems.

### Week 13: QUBO Formulation

#### 4.1 Quantum Module Foundation

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 4.1.1 | Implement `QuboFormulator` class | `solvers/quantum_client.hpp/cpp` | QUBO matrix generation |
| 4.1.2 | Implement portfolio to QUBO mapping | `solvers/quantum_client.cpp` | Variable encoding |
| 4.1.3 | Implement constraint penalties | `solvers/quantum_client.cpp` | Penalty terms |
| 4.1.4 | Implement tax-lot QUBO extension | `solvers/quantum_client.cpp` | Lot selection |
| 4.1.5 | QUBO validation | `solvers/quantum_client.cpp` | Matrix verification |
| 4.1.6 | Solution interpretation | `solvers/quantum_client.cpp` | Binary to trades |
| 4.1.7 | Unit tests for QUBO | `test/unit/solvers/qubo_test.cpp` | Formulation tests |

**QUBO Formulation for Tax-Lot Selection:**
```
Variables:
    x_i ∈ {0, 1} for each tax lot i (1 = sell, 0 = hold)

Objective (minimize):
    Σ_i,j Q_ij * x_i * x_j

Where Q encodes:
    - Diagonal: benefit of selling lot i (tax loss) minus cost
    - Off-diagonal: interaction terms for constraints

Penalties added for:
    - Allocation deviation from target
    - Budget violation
    - Wash sale violation
```

**Variable Encoding:**
```cpp
int QuboFormulator::getNumQubits() const {
    // One binary variable per tax lot
    // Plus auxiliary variables for constraints
    int lot_vars = tax_lots_.size();
    int aux_vars = estimateAuxiliaryVariables();
    return lot_vars + aux_vars;
}

std::map<int, std::string> QuboFormulator::getVariableMapping() const {
    std::map<int, std::string> mapping;
    for (int i = 0; i < tax_lots_.size(); ++i) {
        mapping[i] = "sell_lot_" + tax_lots_[i].id;
    }
    return mapping;
}
```

### Week 14: D-Wave Integration

#### 4.2 D-Wave Client

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 4.2.1 | Implement `DWaveClient` class | `solvers/quantum_client.cpp` | API client |
| 4.2.2 | D-Wave authentication | `solvers/quantum_client.cpp` | API key auth |
| 4.2.3 | QUBO submission to D-Wave | `solvers/quantum_client.cpp` | HTTP POST |
| 4.2.4 | Result polling | `solvers/quantum_client.cpp` | Async waiting |
| 4.2.5 | Response parsing | `solvers/quantum_client.cpp` | JSON to result |
| 4.2.6 | Error handling | `solvers/quantum_client.cpp` | Retry, fallback |
| 4.2.7 | Hybrid solver support | `solvers/quantum_client.cpp` | hybrid_v2 solver |
| 4.2.8 | Integration test with D-Wave | `test/integration/dwave_test.cpp` | Real API call |

**D-Wave API Integration:**
```cpp
QuantumResult DWaveClient::submitQUBO(const QuboMatrix& Q, int num_reads) {
    // Build request body
    nlohmann::json request;
    request["solver"] = config_.use_hybrid ? "hybrid_binary_quadratic_model_version2"
                                           : "Advantage_system6.1";
    request["data"]["format"] = "qp";
    request["data"]["lin"] = extractLinear(Q);
    request["data"]["quad"] = extractQuadratic(Q);
    request["params"]["num_reads"] = num_reads;

    // Submit to D-Wave SAPI
    auto response = http_client_->Post(
        "/sapi/v2/problems",
        {{"Authorization", "Bearer " + api_key_}},
        request.dump(),
        "application/json"
    );

    // Poll for completion
    std::string problem_id = nlohmann::json::parse(response->body)["id"];
    return pollForResult(problem_id);
}
```

**Test with Real D-Wave:**
```cpp
TEST(DWaveIntegration, SolveSmallQUBO) {
    // Skip if no API key
    if (!hasEnvVar("DWAVE_API_KEY")) {
        GTEST_SKIP() << "No D-Wave API key";
    }

    DWaveClient client(config);
    ASSERT_TRUE(client.authenticate());

    // Simple 4-qubit QUBO
    QuboMatrix Q = Eigen::MatrixXd::Zero(4, 4);
    Q(0, 0) = -1; Q(1, 1) = -1;  // Prefer x0=1, x1=1
    Q(0, 1) = 2;                  // Penalize both on

    auto result = client.submitQUBO(Q, 100);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.best_sample.size(), 4);
}
```

### Week 15: Hybrid Solver Orchestration

#### 4.3 Hybrid Classical-Quantum

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 4.3.1 | Implement hybrid orchestration | `core/solver_dispatcher.cpp` | Classical + quantum |
| 4.3.2 | Problem decomposition | `core/solver_dispatcher.cpp` | Split large problems |
| 4.3.3 | Classical preprocessing | `core/solver_dispatcher.cpp` | Reduce problem size |
| 4.3.4 | Quantum result validation | `core/solver_dispatcher.cpp` | Verify constraints |
| 4.3.5 | Fallback to classical | `core/solver_dispatcher.cpp` | On quantum failure |
| 4.3.6 | Cost tracking | `core/solver_dispatcher.cpp` | Quantum API costs |
| 4.3.7 | Update tier escalation | `core/solver_dispatcher.cpp` | Route to quantum |
| 4.3.8 | Integration tests | `test/integration/hybrid_test.cpp` | Hybrid workflow |

**Hybrid Workflow:**
```cpp
SolverResult SolverDispatcher::solveHybrid(const Portfolio& portfolio, ...) {
    // Step 1: Classical preprocessing
    auto reduced = classicalPreprocess(portfolio, constraints);

    // Step 2: Check if quantum needed
    if (reduced.canSolveClassically()) {
        LOG_INFO("Problem reduced sufficiently for classical solver");
        return solveClassical(reduced);
    }

    // Step 3: Formulate QUBO
    QuboFormulator formulator(reduced.portfolio, reduced.target);
    formulator.enableTaxLotOptimization(reduced.tax_lots);
    auto Q = formulator.formulate(reduced.constraints);

    // Step 4: Submit to quantum
    try {
        auto quantum_result = quantum_solver_->submitQUBO(Q);

        // Step 5: Validate and post-process
        auto trades = formulator.interpretSolution(quantum_result.best_sample);
        if (validateConstraints(trades, constraints)) {
            return buildResult(trades, quantum_result);
        } else {
            LOG_WARN("Quantum solution violated constraints, falling back");
            return solveClassical(portfolio, target, constraints);
        }
    } catch (const QuantumAPIException& e) {
        LOG_ERROR("Quantum API failed: " + e.what());
        return solveClassical(portfolio, target, constraints);
    }
}
```

### Week 16: Quantum Testing & Benchmarking

#### 4.4 Quantum Finalization

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 4.4.1 | Benchmark: classical vs quantum | `test/benchmarks/` | Speedup measured |
| 4.4.2 | Solution quality comparison | `test/benchmarks/` | Optimality gap |
| 4.4.3 | Cost analysis | `docs/quantum-costs.md` | API cost per run |
| 4.4.4 | IBM Quantum stub | `solvers/quantum_client.cpp` | Future provider |
| 4.4.5 | Quantum telemetry | `utils/pyflare.cpp` | Quantum metrics |
| 4.4.6 | CLI quantum flags | `apps/cli/main.cpp` | `--quantum` flag |
| 4.4.7 | Quantum documentation | `docs/quantum-integration.md` | Usage guide |
| 4.4.8 | Edge case testing | `test/` | Error scenarios |

**Benchmark Framework:**
```cpp
static void BM_Classical50Positions(benchmark::State& state) {
    auto portfolio = generateRandomPortfolio(50);
    auto target = createBalancedTarget();
    auto constraints = createStandardConstraints();

    SolverDispatcher dispatcher(config);
    dispatcher.enableQuantum(false);  // Classical only

    for (auto _ : state) {
        auto result = dispatcher.dispatch(portfolio, target, constraints);
        benchmark::DoNotOptimize(result);
    }
}

static void BM_Quantum50Positions(benchmark::State& state) {
    // Same setup, quantum enabled
    SolverDispatcher dispatcher(config);
    dispatcher.enableQuantum(true);

    for (auto _ : state) {
        auto result = dispatcher.dispatch(portfolio, target, constraints);
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(BM_Classical50Positions)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Quantum50Positions)->Unit(benchmark::kMillisecond);
```

**Milestone M4 Criteria (Quantum Live):**
- [ ] D-Wave API integration working
- [ ] QUBO formulation for tax-lot selection
- [ ] Hybrid solver orchestration
- [ ] Fallback to classical on failure
- [ ] Benchmark data: classical vs quantum
- [ ] Quantum API cost tracking
- [ ] Documentation complete

---

## Phase 5: GUI & User Experience (Weeks 17-20)

**Goal:** Build desktop GUI application using Qt.

### Week 17: Qt Foundation

#### 5.1 GUI Framework Setup

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 5.1.1 | Qt6 integration in CMake | `CMakeLists.txt` | Qt links correctly |
| 5.1.2 | Main window scaffolding | `apps/gui/mainwindow.cpp` | Window opens |
| 5.1.3 | Menu bar structure | `apps/gui/mainwindow.cpp` | File, Edit, Help |
| 5.1.4 | Application icon and metadata | `apps/gui/` | Branding |
| 5.1.5 | Settings dialog | `apps/gui/settings.cpp` | Config UI |
| 5.1.6 | Status bar | `apps/gui/mainwindow.cpp` | Status messages |
| 5.1.7 | About dialog | `apps/gui/about.cpp` | Version info |
| 5.1.8 | Basic styling/theme | `apps/gui/style.qss` | Consistent look |

### Week 18: Portfolio Views

#### 5.2 Portfolio UI

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 5.2.1 | Portfolio table view | `apps/gui/portfolio_view.cpp` | Shows holdings |
| 5.2.2 | Position editing | `apps/gui/portfolio_view.cpp` | Add/remove/edit |
| 5.2.3 | CSV import dialog | `apps/gui/import_dialog.cpp` | File picker |
| 5.2.4 | Real-time price updates | `apps/gui/portfolio_view.cpp` | Auto-refresh |
| 5.2.5 | Allocation pie chart | `apps/gui/charts.cpp` | Visual breakdown |
| 5.2.6 | Target allocation editor | `apps/gui/target_editor.cpp` | Set targets |
| 5.2.7 | Constraint configuration | `apps/gui/constraint_editor.cpp` | UI for constraints |
| 5.2.8 | Portfolio summary panel | `apps/gui/summary_panel.cpp` | Key metrics |

**Portfolio Table Columns:**
- Ticker
- Shares
- Current Price
- Current Value
- Cost Basis
- Gain/Loss
- Gain/Loss %
- Asset Class
- Allocation %

### Week 19: Optimization UI

#### 5.3 Optimization Interface

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 5.3.1 | Optimize button and workflow | `apps/gui/optimize_view.cpp` | Triggers solve |
| 5.3.2 | Progress indicator | `apps/gui/optimize_view.cpp` | Shows solving |
| 5.3.3 | Results table | `apps/gui/results_view.cpp` | Recommended trades |
| 5.3.4 | Before/after comparison | `apps/gui/results_view.cpp` | Side-by-side |
| 5.3.5 | Trade rationale display | `apps/gui/results_view.cpp` | Explanations |
| 5.3.6 | Constraint status view | `apps/gui/results_view.cpp` | Which binding |
| 5.3.7 | Tax impact panel | `apps/gui/results_view.cpp` | Tax summary |
| 5.3.8 | Export results | `apps/gui/results_view.cpp` | CSV/JSON export |

### Week 20: Polish & Packaging

#### 5.4 GUI Finalization

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 5.4.1 | Error dialogs | `apps/gui/` | User-friendly errors |
| 5.4.2 | Keyboard shortcuts | `apps/gui/mainwindow.cpp` | Common shortcuts |
| 5.4.3 | Tooltips and help | `apps/gui/` | Hover help |
| 5.4.4 | Windows installer (NSIS) | `packaging/` | .exe installer |
| 5.4.5 | macOS bundle | `packaging/` | .app bundle |
| 5.4.6 | Linux AppImage | `packaging/` | Portable binary |
| 5.4.7 | Auto-update check | `apps/gui/updater.cpp` | Version check |
| 5.4.8 | User documentation | `docs/gui-guide.md` | Screenshots, walkthrough |

**Milestone M5 Criteria (Desktop App):**
- [ ] Qt GUI launches on Windows/macOS/Linux
- [ ] Portfolio can be imported and viewed
- [ ] Optimization runs from GUI
- [ ] Results displayed with explanations
- [ ] Installers for all platforms
- [ ] User documentation complete

---

## Phase 6: Mobile & Cross-Platform (Weeks 21-24)

**Goal:** Prepare mobile foundation and complete public launch.

### Week 21-22: REST API Server

#### 6.1 Server Application

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 6.1.1 | REST server scaffolding | `apps/server/main.cpp` | cpp-httplib server |
| 6.1.2 | `/api/v1/optimize` endpoint | `apps/server/` | POST optimization |
| 6.1.3 | `/api/v1/portfolios` endpoints | `apps/server/` | CRUD portfolios |
| 6.1.4 | `/api/v1/market/:ticker` endpoint | `apps/server/` | Price lookup |
| 6.1.5 | Authentication (API keys) | `apps/server/` | Bearer token |
| 6.1.6 | Rate limiting | `apps/server/` | Protect API |
| 6.1.7 | OpenAPI specification | `docs/api.yaml` | API documentation |
| 6.1.8 | Server integration tests | `test/integration/` | API tests |

**API Endpoints:**
```
POST   /api/v1/optimize           - Run optimization
GET    /api/v1/portfolios         - List portfolios
POST   /api/v1/portfolios         - Create portfolio
GET    /api/v1/portfolios/:id     - Get portfolio
PUT    /api/v1/portfolios/:id     - Update portfolio
DELETE /api/v1/portfolios/:id     - Delete portfolio
GET    /api/v1/market/:ticker     - Get current price
GET    /api/v1/results/:session   - Get optimization results
GET    /api/v1/health             - Health check
```

### Week 23: PyFlare & Observability

#### 6.2 Observability Integration

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 6.2.1 | Implement `PyFlareExporter` | `utils/pyflare.cpp` | Telemetry export |
| 6.2.2 | Solver run events | `utils/pyflare.cpp` | Performance metrics |
| 6.2.3 | Quantum API events | `utils/pyflare.cpp` | Quantum metrics |
| 6.2.4 | Market data events | `utils/pyflare.cpp` | Data source metrics |
| 6.2.5 | Error events | `utils/pyflare.cpp` | Error tracking |
| 6.2.6 | Event schema documentation | `docs/telemetry.md` | Event format |
| 6.2.7 | PyFlare dashboard integration | External | Dashboard works |
| 6.2.8 | Privacy review | Documentation | No PII in logs |

**Telemetry Event Example:**
```json
{
    "timestamp": "2026-01-19T12:00:00Z",
    "event_type": "solver_run",
    "session_id": "abc-123",
    "problem": {
        "num_positions": 25,
        "num_constraints": 10,
        "problem_type": "MILP",
        "has_tax_optimization": true
    },
    "solver": {
        "type": "highs",
        "time_ms": 234,
        "status": "optimal"
    },
    "quantum": {
        "used": false,
        "reason": "classical_sufficient"
    }
}
```

### Week 24: Launch Preparation

#### 6.3 Public Launch

| Task ID | Task | File(s) | Acceptance Criteria |
|---------|------|---------|---------------------|
| 6.3.1 | Final testing pass | All tests | 100% pass rate |
| 6.3.2 | Security audit | Code review | No vulnerabilities |
| 6.3.3 | Performance benchmarks | Documentation | Published results |
| 6.3.4 | README finalization | `README.md` | Complete, polished |
| 6.3.5 | License headers | All files | MIT header |
| 6.3.6 | GitHub release | GitHub | v1.0.0 tag |
| 6.3.7 | Hacker News post | External | Submission |
| 6.3.8 | Product Hunt launch | External | Listing |
| 6.3.9 | Blog post | External | Technical overview |
| 6.3.10 | Demo video | External | YouTube walkthrough |

**Milestone M6 Criteria (Public Launch):**
- [ ] All features working
- [ ] All tests passing
- [ ] Documentation complete
- [ ] Installers available
- [ ] GitHub release published
- [ ] Marketing materials ready

---

## Ongoing: Quality & Observability

### Continuous Tasks

| Task | Frequency | Description |
|------|-----------|-------------|
| Code review | Per PR | All code reviewed before merge |
| Unit tests | Per commit | Must maintain >80% coverage |
| Integration tests | Daily | Full suite runs in CI |
| Performance monitoring | Weekly | Check for regressions |
| Dependency updates | Monthly | Security patches |
| Documentation review | Bi-weekly | Keep docs current |
| User feedback triage | Weekly | Address issues |

### Quality Gates

Each phase must pass before proceeding:

1. **All tests pass** (unit, integration)
2. **Code coverage >80%**
3. **No critical static analysis warnings**
4. **Performance targets met**
5. **Documentation updated**
6. **Code reviewed and approved**

---

## Detailed Task Breakdown

### Task Numbering Convention

```
[Phase].[Week].[Category].[Sequence]

Example: 1.3.2.5
- Phase 1 (Core Infrastructure)
- Week 3 (of phase)
- Category 2 (Solver Module)
- Sequence 5 (5th task in category)
```

### Task Priorities

| Priority | Meaning | SLA |
|----------|---------|-----|
| P0 | Blocker | Must complete to proceed |
| P1 | Critical | Required for milestone |
| P2 | Important | Needed for quality |
| P3 | Nice-to-have | Can defer if needed |

### Task States

```
[ ] Not Started
[~] In Progress
[x] Complete
[!] Blocked
[-] Deferred
```

---

## Dependency Graph

```
Phase 0: Setup
    │
    ▼
Phase 1: Core Infrastructure
    ├── Week 1: Portfolio Module ─────────────────────┐
    ├── Week 2: Constraint Module ────────────────────┤
    ├── Week 3: HiGHS Integration ────────────────────┤
    └── Week 4: Solver Dispatcher ────────────────────┤
                                                      │
    ▼                                                 │
Phase 2: MVP                                          │
    ├── Week 5: Market Data ◄─────────────────────────┤
    ├── Week 6: Persistence ◄─────────────────────────┤
    ├── Week 7: Explainability ◄──────────────────────┤
    └── Week 8: CLI App ◄─────────────────────────────┘
                │
    ▼           │
Phase 3: Tax   │
    ├── Week 9: Tax Lots ◄────────────────────────────┘
    ├── Week 10: Wash Sale Rules
    ├── Week 11: Tax-Aware Solver
    └── Week 12: Tax Testing
                │
    ▼           │
Phase 4: Quantum│
    ├── Week 13: QUBO Formulation ◄───────────────────┘
    ├── Week 14: D-Wave Client
    ├── Week 15: Hybrid Orchestration
    └── Week 16: Quantum Benchmarks
                │
    ▼           │
Phase 5: GUI   │
    ├── Week 17: Qt Foundation ◄──────────────────────┘
    ├── Week 18: Portfolio Views
    ├── Week 19: Optimization UI
    └── Week 20: Packaging
                │
    ▼           │
Phase 6: Launch│
    ├── Week 21-22: REST API ◄────────────────────────┘
    ├── Week 23: Observability
    └── Week 24: Public Launch
```

---

## Testing Strategy

### Test Pyramid

```
                    ┌─────────────┐
                    │   E2E (5%)  │  <- Full system tests
                   ─┼─────────────┼─
                  ┌─┴─────────────┴─┐
                  │ Integration(20%)│  <- Module integration
                 ─┼─────────────────┼─
               ┌──┴─────────────────┴──┐
               │    Unit Tests (75%)    │  <- Individual functions
               └───────────────────────┘
```

### Test Categories

| Category | Purpose | Tools |
|----------|---------|-------|
| Unit | Test individual functions | Google Test |
| Integration | Test module interactions | Google Test |
| E2E | Test full workflows | Custom scripts |
| Performance | Benchmark speed | Google Benchmark |
| Security | Check vulnerabilities | Static analysis |

### Coverage Requirements

| Module | Minimum Coverage |
|--------|------------------|
| Core | 90% |
| Solvers | 85% |
| Data | 85% |
| Explain | 80% |
| Utils | 75% |
| Apps | 70% |

---

## Risk Management

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| HiGHS insufficient for large problems | Low | High | Modular solver architecture, commercial solver fallback |
| Quantum API costs exceed budget | Medium | Medium | Smart routing, classical fallback, cost caps |
| D-Wave API changes | Low | Medium | Abstraction layer, version pinning |
| Performance targets not met | Medium | High | Early benchmarking, optimization sprints |
| Cross-platform build issues | Medium | Medium | CI on all platforms from day 1 |

### Schedule Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Scope creep | High | High | Strict MVP definition, change control |
| Integration complexity | Medium | Medium | Early integration, continuous testing |
| External API changes | Low | Medium | Abstraction layers, API versioning |
| Resource availability | Medium | Medium | Realistic scheduling, buffer time |

### Mitigation Actions

1. **Weekly risk review**: Assess new risks, update mitigation
2. **Buffer time**: 20% schedule buffer built in
3. **Feature flags**: Disable problematic features if needed
4. **Fallback options**: Classical solver always available

---

## Success Criteria

### Phase 1 Success (Week 4)
- [ ] Optimize 20-position portfolio in <1 second
- [ ] All constraint types working
- [ ] 80%+ unit test coverage

### Phase 2 Success (Week 8)
- [ ] CLI tool fully functional
- [ ] Real market data integration
- [ ] Explainable output generated
- [ ] User can complete full workflow

### Phase 3 Success (Week 12)
- [ ] Tax-loss harvesting working
- [ ] Wash sale rules enforced
- [ ] Lot selection methods implemented
- [ ] Tax impact shown in results

### Phase 4 Success (Week 16)
- [ ] D-Wave integration working
- [ ] Hybrid solver orchestration
- [ ] Benchmark data available
- [ ] Quantum adds value for complex cases

### Phase 5 Success (Week 20)
- [ ] Desktop app on all platforms
- [ ] User-friendly interface
- [ ] Installers working
- [ ] Documentation complete

### Phase 6 Success (Week 24)
- [ ] Public release available
- [ ] REST API working
- [ ] Observability integrated
- [ ] Community engagement started

---

## Resource Requirements

### Development Tools

| Tool | Purpose | License |
|------|---------|---------|
| CLion / VS Code | IDE | Commercial / Free |
| CMake | Build system | BSD |
| vcpkg | Package manager | MIT |
| Git | Version control | GPL |
| GitHub Actions | CI/CD | Free tier |

### External Services

| Service | Purpose | Estimated Cost |
|---------|---------|----------------|
| Alpha Vantage | Market data | Free tier (5 calls/min) |
| D-Wave Leap | Quantum computing | $400/month (development) |
| GitHub | Repository, CI | Free (open source) |

### Libraries (All MIT/BSD Compatible)

| Library | Version | Purpose |
|---------|---------|---------|
| HiGHS | Latest | Optimization solver |
| SymEngine | Latest | Symbolic math |
| Eigen | 3.4+ | Linear algebra |
| nlohmann/json | 3.11+ | JSON parsing |
| cpp-httplib | Latest | HTTP client |
| SQLite | 3.35+ | Database |
| CLI11 | 2.3+ | CLI parsing |
| Qt6 | 6.5+ | GUI framework |
| Google Test | Latest | Testing |

---

## Appendix A: File Implementation Order

```
Week 1:
  include/lumen/core/portfolio.hpp
  src/core/portfolio.cpp
  test/unit/core/portfolio_test.cpp

Week 2:
  include/lumen/core/constraint.hpp
  src/core/constraint.cpp
  test/unit/core/constraint_test.cpp

Week 3:
  include/lumen/solvers/base_solver.hpp
  include/lumen/solvers/highs_wrapper.hpp
  src/solvers/highs_wrapper.cpp
  test/unit/solvers/highs_test.cpp

Week 4:
  include/lumen/core/solver_dispatcher.hpp
  src/core/solver_dispatcher.cpp
  test/integration/e2e_test.cpp

Week 5:
  include/lumen/data/market_data.hpp
  src/data/market_data.cpp
  test/unit/data/market_data_test.cpp

Week 6:
  include/lumen/utils/logging.hpp
  include/lumen/utils/config.hpp
  src/utils/logging.cpp
  src/utils/config.cpp
  src/data/persistence.cpp

Week 7:
  include/lumen/explain/provenance.hpp
  include/lumen/explain/explainer.hpp
  src/explain/provenance.cpp
  src/explain/explainer.cpp

Week 8:
  apps/cli/main.cpp
  docs/cli-usage.md

Week 9:
  include/lumen/data/tax_lot.hpp
  src/data/tax_lot.cpp

Week 10-11:
  src/data/tax_lot.cpp (tax optimizer)
  src/solvers/highs_wrapper.cpp (tax constraints)

Week 12:
  test/integration/tax_test.cpp
  docs/tax-optimization.md

Week 13:
  include/lumen/solvers/quantum_client.hpp
  src/solvers/quantum_client.cpp (QUBO)

Week 14-15:
  src/solvers/quantum_client.cpp (D-Wave)
  src/core/solver_dispatcher.cpp (hybrid)

Week 16:
  test/benchmarks/quantum_bench.cpp
  docs/quantum-integration.md

Week 17-20:
  apps/gui/*

Week 21-22:
  apps/server/*

Week 23:
  src/utils/pyflare.cpp

Week 24:
  Final polish, documentation, release
```

---

## Appendix B: Definition of Done

A task is considered "Done" when:

1. **Code Complete**
   - Implementation matches specification
   - No TODO comments left
   - Code compiles without warnings

2. **Tested**
   - Unit tests written and passing
   - Integration tests updated if needed
   - Manual testing performed

3. **Documented**
   - Header comments on public APIs
   - README updated if user-facing
   - Design decisions recorded

4. **Reviewed**
   - Code review completed
   - Feedback addressed
   - Approved by reviewer

5. **Integrated**
   - Merged to main branch
   - CI passing
   - No regressions introduced

---

## Appendix C: Communication & Reporting

### Daily
- Update task status
- Flag blockers immediately

### Weekly
- Progress summary
- Risk updates
- Next week priorities

### Per Milestone
- Milestone completion report
- Demo to stakeholders
- Retrospective

---

**Document Version Control:**
- v1.0 (2026-01-19): Initial project implementation plan
