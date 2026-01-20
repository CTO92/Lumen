# Lumen Top-Level Architecture

**Version:** 1.0
**Date:** January 19, 2026
**Organization:** OA Quantum Labs
**Status:** Architecture Reference

---

## Overview

This document defines the high-level architecture for Lumen, an edge-deployed reasoning engine for financial portfolio optimization. Lumen uses deterministic constraint-based solving with optional quantum enhancement for complex scenarios.

**Core Principles:**
- Deterministic, explainable AI reasoning
- Edge-first computation with quantum cloud escalation
- Open-source core (MIT licensed)
- Privacy-preserving local computation

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           USER INTERFACES                               │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────────────┐    │
│  │ CLI          │  │ REST API     │  │ Desktop GUI                │    │
│  │ (primary)    │  │ (localhost)  │  │ (Qt/Electron, future)      │    │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬─────────────────┘    │
└─────────┼──────────────────┼─────────────────────┼──────────────────────┘
          │                  │                     │
          └──────────────────┼─────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────────────┐
│                      LUMEN CORE ENGINE (C++)                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ┌───────────────────────────────────────────────────────────────┐     │
│  │ 1. INPUT PROCESSING                                           │     │
│  │    ├─ Portfolio data parser (CSV, JSON, broker exports)       │     │
│  │    ├─ Target allocation specification                         │     │
│  │    ├─ Constraint validation                                   │     │
│  │    └─ Tax lot data management                                 │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                              │                                          │
│  ┌───────────────────────────▼───────────────────────────────────┐     │
│  │ 2. CONSTRAINT FORMULATION                                     │     │
│  │    ├─ Symbolic constraint building (SymEngine)                │     │
│  │    ├─ MILP/QP problem construction                            │     │
│  │    ├─ Tax optimization objective functions                    │     │
│  │    └─ Multi-objective formulation (Pareto analysis)           │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                              │                                          │
│  ┌───────────────────────────▼───────────────────────────────────┐     │
│  │ 3. MARKET DATA RETRIEVAL                                      │     │
│  │    ├─ HTTP client (cpp-httplib)                               │     │
│  │    ├─ Real-time prices (Alpha Vantage, Yahoo Finance)         │     │
│  │    ├─ Historical data for covariance/returns                  │     │
│  │    └─ Data caching with TTL                                   │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                              │                                          │
│  ┌───────────────────────────▼───────────────────────────────────┐     │
│  │ 4. SOLVER DISPATCHER                                          │     │
│  │    ├─ Problem classifier (size, complexity, type)             │     │
│  │    ├─ Classical solver: HiGHS (LP/MILP/QP)                    │     │
│  │    ├─ Quantum solver: QUBO/QAOA formulation                   │     │
│  │    │   ├─ D-Wave API (quantum annealing)                      │     │
│  │    │   ├─ IBM Quantum / IonQ (gate-based QAOA)                │     │
│  │    │   └─ Fallback to classical if quantum unavailable        │     │
│  │    └─ Linear algebra: Eigen                                   │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                              │                                          │
│  ┌───────────────────────────▼───────────────────────────────────┐     │
│  │ 5. SOLUTION EXPLAINER                                         │     │
│  │    ├─ Provenance tracking                                     │     │
│  │    │   ├─ Which constraints were active/binding?              │     │
│  │    │   ├─ Which data sources were used?                       │     │
│  │    │   └─ Sensitivity analysis                                │     │
│  │    ├─ Trade recommendation formatting                         │     │
│  │    ├─ Before/after allocation comparison                      │     │
│  │    └─ Cost-benefit analysis output                            │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                              │                                          │
│  ┌───────────────────────────▼───────────────────────────────────┐     │
│  │ 6. OBSERVABILITY (PyFlare Integration)                        │     │
│  │    ├─ Telemetry logging (JSON events)                         │     │
│  │    │   ├─ Solver performance (time, iterations)               │     │
│  │    │   ├─ Quantum API usage (calls, latency, cost)            │     │
│  │    │   └─ Constraint conflicts/violations                     │     │
│  │    └─ PyFlare-compatible log files                            │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                              │                                          │
│  ┌───────────────────────────▼───────────────────────────────────┐     │
│  │ 7. PERSISTENCE LAYER                                          │     │
│  │    ├─ SQLite embedded database                                │     │
│  │    │   ├─ Portfolio history                                   │     │
│  │    │   ├─ Cost basis / tax lot records                        │     │
│  │    │   ├─ Cached market data (with TTL)                       │     │
│  │    │   └─ User preferences/settings                           │     │
│  │    └─ Local encrypted storage                                 │     │
│  └───────────────────────────────────────────────────────────────┘     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────────────────┐
│                       EXTERNAL SERVICES                                 │
├─────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐  ┌─────────────────────────────────────────┐  │
│  │ Market Data APIs    │  │ Quantum Cloud Services                  │  │
│  │ ├─ Alpha Vantage    │  │ ├─ D-Wave (quantum annealing)           │  │
│  │ ├─ Yahoo Finance    │  │ ├─ IBM Quantum (gate-based)             │  │
│  │ └─ Broker APIs      │  │ └─ IonQ (gate-based)                    │  │
│  └─────────────────────┘  └─────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Component Descriptions

### 1. Input Processing
Handles all data ingestion and validation:
- **Portfolio Parser:** Reads portfolio data from CSV, JSON, or broker export formats
- **Target Allocation:** Accepts percentage or dollar-based allocation targets with tolerance bands
- **Constraint Validator:** Ensures user constraints are consistent and satisfiable
- **Tax Lot Manager:** Tracks cost basis, purchase dates, and lot-specific data for tax optimization

### 2. Constraint Formulation
Converts user requirements into mathematical optimization problems:
- **SymEngine Integration:** Symbolic constraint formulation and manipulation
- **Problem Types:** Linear programs (LP), mixed-integer linear programs (MILP), quadratic programs (QP)
- **Tax Objectives:** Short-term vs. long-term gains, wash sale compliance, tax-loss harvesting
- **Multi-Objective:** Balances allocation drift, transaction costs, and tax efficiency

### 3. Market Data Retrieval
Fetches real-time and historical financial data:
- **HTTP Client:** cpp-httplib for API communication
- **Data Sources:** Alpha Vantage (primary), Yahoo Finance, broker APIs
- **Derived Data:** Computes expected returns and covariance matrices from historical data
- **Caching:** SQLite-backed cache with configurable TTL to reduce API calls

### 4. Solver Dispatcher
Routes optimization problems to appropriate solvers:
- **Classical Path (Tier 1-2):** HiGHS for LP/MILP/QP problems
- **Quantum Path (Tier 3):** QUBO formulation for D-Wave annealing or QAOA for gate-based quantum
- **Decision Logic:** Automatically selects quantum when problem complexity exceeds classical thresholds
- **Fallback:** Graceful degradation to classical if quantum services unavailable

### 5. Solution Explainer
Generates human-readable explanations for optimization results:
- **Provenance Tracking:** Records data sources, active constraints, and solver decisions
- **Trade Rationale:** Explains why each trade was recommended
- **Constraint Satisfaction:** Shows which constraints were binding vs. slack
- **Sensitivity Analysis:** Indicates how results would change with different parameters

### 6. Observability (PyFlare)
Comprehensive monitoring and logging:
- **Telemetry Events:** Structured JSON logs for all optimization runs
- **Performance Metrics:** Solve time, iterations, memory usage
- **Quantum Metrics:** API latency, queue times, solution quality
- **PyFlare Export:** Compatible log format for analysis in PyFlare dashboard

### 7. Persistence Layer
Local data storage for privacy and offline operation:
- **SQLite Database:** Embedded, zero-config, battle-tested
- **Portfolio History:** Track portfolio changes over time
- **Tax Lots:** Cost basis records for tax optimization
- **Data Cache:** Reduce API calls with cached market data
- **Encryption:** Local storage encryption for sensitive financial data

---

## Technology Stack

### Core Language & Build
| Component | Technology | License |
|-----------|------------|---------|
| Language | C++17/20 | - |
| Build System | CMake 3.16+ | BSD-3 |
| Package Manager | vcpkg | MIT |
| Compiler Support | GCC 9+, Clang 10+, MSVC 2019+ | - |

### Core Libraries
| Library | Purpose | License |
|---------|---------|---------|
| HiGHS | LP/MILP/QP optimization | MIT |
| SymEngine | Symbolic mathematics | BSD-3 |
| Eigen | Linear algebra | MPL2 |
| SQLite | Embedded database | Public Domain |
| cpp-httplib | HTTP client/server | MIT |
| nlohmann/json | JSON parsing | MIT |
| CLI11 | Command-line parsing | BSD-3 |

### Quantum Integration
| Service | Purpose | Use Case |
|---------|---------|----------|
| D-Wave Ocean SDK | Quantum annealing | Tax-lot selection (QUBO) |
| IBM Qiskit | Gate-based quantum | Large portfolio QAOA |
| IonQ | Gate-based quantum | Alternative QAOA provider |

### Testing & Quality
| Tool | Purpose |
|------|---------|
| Google Test | Unit testing |
| Google Benchmark | Performance benchmarking |
| clang-tidy | Static analysis |
| gcov/lcov | Code coverage |

---

## Data Flow: Portfolio Optimization

```
Input:
"Rebalance my $100k portfolio to 60/40 stocks/bonds, max 20% per position"

Step 1: Input Processing
├─ Parse portfolio holdings (ticker, shares, cost basis)
├─ Parse target allocation (60% stocks, 40% bonds)
├─ Parse constraints (max 20% per position)
└─ Validate constraint consistency

Step 2: Market Data Retrieval
├─ Fetch current prices from Alpha Vantage
├─ Check cache, update if stale (TTL expired)
└─ Store with provenance metadata

Step 3: Constraint Formulation (SymEngine)
├─ Variables: trade quantities for each position
├─ Objective: minimize deviation from target allocation
├─ Constraints:
│   ├─ Budget: total value = $100k
│   ├─ Bounds: 0 ≤ position ≤ 20%
│   ├─ Asset class: stocks = 60%, bonds = 40%
│   └─ Integer shares (if required)
└─ Classify problem: MILP (integer shares) or LP (continuous)

Step 4: Solver Dispatch
├─ Problem size check: <20 positions → Classical path
├─ Solve with HiGHS
└─ Solution: optimal trade quantities

Step 5: Explanation Generation
├─ Format trade recommendations
├─ List active constraints
├─ Calculate transaction costs
├─ Show before/after allocation
└─ Provide constraint satisfaction proof

Step 6: Telemetry Logging
├─ Log solver performance
├─ Record problem characteristics
└─ Write PyFlare-compatible JSON

Output:
{
  "trades": [
    {"ticker": "VTI", "action": "BUY", "shares": 15, "rationale": "..."},
    {"ticker": "BND", "action": "SELL", "shares": 8, "rationale": "..."}
  ],
  "before_allocation": {"stocks": 65%, "bonds": 35%},
  "after_allocation": {"stocks": 60%, "bonds": 40%},
  "transaction_cost": "$14.95",
  "constraints_satisfied": true
}
```

---

## Tiered Optimization Strategy

| Tier | Portfolio Size | Solver | Response Time | Cloud Dependency |
|------|----------------|--------|---------------|------------------|
| **Tier 1** | <20 positions | Classical (HiGHS) | <1 second | None |
| **Tier 2** | 20-50 positions | Hybrid classical/quantum | 1-5 seconds | Optional |
| **Tier 3** | 50+ positions + tax | Quantum (D-Wave/QAOA) | 5-30 seconds | Required |

**Tier Escalation Logic:**
```
if (num_positions < 20 && !tax_lot_optimization):
    use_classical_solver()
elif (num_positions < 50):
    if (quantum_available && user_premium):
        use_hybrid_solver()
    else:
        use_classical_solver()
else:
    if (quantum_available && user_enterprise):
        use_quantum_solver()
    else:
        warn_user_about_solution_quality()
        use_classical_solver()
```

---

## Directory Structure

```
lumen/
├── CMakeLists.txt              # Root CMake configuration
├── README.md                   # User documentation
├── LICENSE                     # MIT License
├── include/
│   └── lumen/
│       ├── core/
│       │   ├── portfolio.hpp
│       │   ├── constraint.hpp
│       │   └── solver_dispatcher.hpp
│       ├── solvers/
│       │   ├── highs_wrapper.hpp
│       │   └── quantum_client.hpp
│       ├── data/
│       │   ├── market_data.hpp
│       │   └── tax_lot.hpp
│       ├── explain/
│       │   ├── provenance.hpp
│       │   └── explainer.hpp
│       └── utils/
│           ├── logging.hpp
│           └── config.hpp
├── src/
│   ├── core/
│   ├── solvers/
│   ├── data/
│   ├── explain/
│   ├── utils/
│   └── main.cpp
├── apps/
│   ├── cli/                    # Command-line interface
│   └── server/                 # REST API server
├── test/
│   ├── unit/
│   ├── integration/
│   └── benchmarks/
├── docs/
└── scripts/
```

---

## Configuration

```yaml
# ~/.lumen/config.yaml

solver:
  classical:
    default: highs
    timeout_ms: 30000
  quantum:
    enabled: true
    providers:
      - name: dwave
        api_key_env: DWAVE_API_KEY
        timeout_ms: 60000
      - name: ibm
        api_key_env: IBM_QUANTUM_API_KEY
        timeout_ms: 60000

market_data:
  providers:
    - name: alpha_vantage
      api_key_env: ALPHA_VANTAGE_API_KEY
    - name: yahoo_finance
      enabled: true
  cache_ttl_minutes: 60

observability:
  pyflare:
    enabled: true
    log_directory: ~/.lumen/logs/pyflare/
  local_logging:
    enabled: true
    log_file: ~/.lumen/logs/lumen.log
    rotation_size_mb: 100

persistence:
  database_path: ~/.lumen/data/lumen.db
  encryption_enabled: true
```

---

## Telemetry Event Schema

```json
{
  "timestamp": "2026-01-19T12:00:00Z",
  "event_type": "solver_run",
  "session_id": "uuid",

  "problem": {
    "num_positions": 25,
    "num_constraints": 45,
    "problem_type": "MILP",
    "tax_optimization": true
  },

  "solver": {
    "type": "highs",
    "time_ms": 234,
    "iterations": 47,
    "status": "optimal",
    "objective_value": 0.0023
  },

  "quantum": {
    "used": false,
    "reason": "problem_size_below_threshold"
  },

  "constraints": {
    "active": ["budget", "max_position"],
    "binding": ["max_position[AAPL]"]
  }
}
```

---

## Platform Support

### Tier 1 (MVP)
- Windows 10/11 (x64)
- Linux (Ubuntu 20.04+)
- macOS (11+, Intel and Apple Silicon)

### Tier 2 (Phase 3)
- iOS (via C++ core)
- Android (via C++ core)

### Performance Targets
| Metric | Target |
|--------|--------|
| Small problems (<20 positions) | <1 second |
| Medium problems (20-50 positions) | <5 seconds |
| Large problems (50+ positions) | <60 seconds |
| Memory usage (typical) | <500 MB |

---

## Security Considerations

- **Local-First:** All portfolio data stored locally with encryption
- **API Keys:** Stored in environment variables, never in code or logs
- **Quantum Cloud:** Only anonymized problem formulations sent to quantum services
- **No Telemetry PII:** Observability logs contain no personally identifiable information
- **Financial Disclaimers:** Tool provides decision support, not investment advice

---

## References

- [Lumen_Project_Overview.md](Lumen_Project_Overview.md) - MVP features and business context
- [HiGHS Documentation](https://highs.dev/)
- [SymEngine Documentation](https://symengine.org/)
- [D-Wave Ocean SDK](https://docs.ocean.dwavesys.com/)
