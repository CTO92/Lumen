# Lumen User Guide

## Overview

Lumen is a quantum-enhanced portfolio optimization engine designed to help investors rebalance their portfolios efficiently. It combines classical mathematical optimization with optional quantum computing capabilities to find optimal trade recommendations while respecting your constraints.

**Key Features:**
- Intelligent portfolio rebalancing to target allocations
- Tax-aware optimization with tax-loss harvesting
- Support for multiple constraint types (budget, position limits, minimum trade sizes)
- Explainable results with clear rationale for each trade
- Edge-first architecture (your data stays local)
- Optional quantum computing integration for complex portfolios
- **Real-time market data integration** (Alpha Vantage, Yahoo Finance)
- **Portfolio persistence and optimization history tracking**
- **Comprehensive explainability with provenance tracking**
- **Multiple output formats** (JSON, text, Markdown, HTML)

---

## Table of Contents

1. [Installation](#installation)
2. [Quick Start](#quick-start)
3. [Configuration](#configuration)
4. [Market Data](#market-data)
5. [Portfolio Management](#portfolio-management)
6. [Setting Target Allocations](#setting-target-allocations)
7. [Constraints](#constraints)
8. [Running Optimization](#running-optimization)
9. [Understanding Results](#understanding-results)
10. [Explainability](#explainability)
11. [Tax Optimization](#tax-optimization)
12. [Quantum Optimization](#quantum-optimization)
13. [Command Line Interface](#command-line-interface)
14. [REST API Server](#rest-api-server)
15. [Desktop GUI Application](#desktop-gui-application)
16. [Persistence & History](#persistence--history)
17. [Troubleshooting](#troubleshooting)

---

## Installation

### System Requirements

- **Operating System:** Windows 11+ or Ubuntu 20.04+ (Linux)
- **Memory:** 4GB RAM minimum, 8GB recommended
- **Disk Space:** 500MB for installation

### Installing from Pre-built Binaries

Download the latest release for your platform from the releases page and extract:

```bash
# Linux/macOS
tar -xzf lumen-<version>-<platform>.tar.gz
cd lumen-<version>
./install.sh

# Windows
# Extract the ZIP file and run install.bat
```

### Building from Source

If pre-built binaries are not available for your platform:

```bash
# Install dependencies via vcpkg
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$PWD/vcpkg

# Clone and build Lumen
git clone https://github.com/your-org/lumen.git
cd lumen
vcpkg install
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

---

## Quick Start

### 1. Create Your Portfolio File

Create a CSV file with your current holdings (`portfolio.csv`):

```csv
ticker,shares,price,cost_basis,asset_class
AAPL,100,175.50,150.00,stocks
GOOGL,50,140.25,120.00,stocks
VTI,200,220.00,200.00,stocks
BND,300,72.50,75.00,bonds
CASH,1,10000.00,10000.00,cash
```

### 2. Define Your Target Allocation

Create a target allocation file (`targets.json`):

```json
{
  "mode": "percentage",
  "targets": [
    {"identifier": "AAPL", "target_value": 0.20, "lower_bound": 0.15, "upper_bound": 0.25, "is_asset_class": false},
    {"identifier": "GOOGL", "target_value": 0.15, "lower_bound": 0.10, "upper_bound": 0.20, "is_asset_class": false},
    {"identifier": "VTI", "target_value": 0.35, "lower_bound": 0.30, "upper_bound": 0.40, "is_asset_class": false},
    {"identifier": "BND", "target_value": 0.25, "lower_bound": 0.20, "upper_bound": 0.30, "is_asset_class": false},
    {"identifier": "CASH", "target_value": 0.05, "lower_bound": 0.02, "upper_bound": 0.10, "is_asset_class": false}
  ]
}
```

### 3. Run Optimization

```bash
# Basic optimization
lumen optimize --portfolio portfolio.csv --target targets.json --output trades.json

# With automatic price fetching
lumen optimize --portfolio portfolio.csv --target targets.json --output trades.json --fetch-prices

# With constraints and detailed explanation
lumen optimize --portfolio portfolio.csv --target targets.json --constraints constraints.json --output trades.json --format markdown
```

### 4. Review Results

The output file will contain recommended trades:

```json
{
  "success": true,
  "status": "optimal",
  "session_id": "opt_20240115_143052_a7b3c",
  "trades": [
    {"ticker": "AAPL", "action": "buy", "shares": 15, "amount": 2632.50, "rationale": "Increase allocation to target"},
    {"ticker": "BND", "action": "sell", "shares": 50, "amount": 3625.00, "rationale": "Reduce allocation to target"}
  ],
  "total_transaction_cost": 6.26
}
```

---

## Configuration

Lumen uses a configuration file located at `~/.lumen/config.yaml`. Create this file to customize behavior:

```yaml
# Solver Configuration
solver:
  classical:
    default: highs           # Classical solver to use
    timeout_ms: 30000        # Maximum solve time (30 seconds)
  quantum:
    enabled: false           # Enable quantum solvers
    provider: dwave          # Quantum provider (dwave or ibm)
    timeout_ms: 60000        # Quantum solver timeout
    num_reads: 1000          # Samples for quantum annealing
    fallback_to_classical: true  # Fall back if quantum fails

# Market Data Configuration
market_data:
  primary_provider: alpha_vantage
  fallback_providers:
    - yahoo_finance
  cache_ttl_minutes: 60      # Cache market data for 1 hour
  history_days_default: 252  # Default historical data period
  rate_limit:
    calls_per_minute: 5      # Alpha Vantage free tier limit
    retry_delay_ms: 12000    # Delay between retries

# Persistence Configuration
persistence:
  database_path: ~/.lumen/data/lumen.db
  encryption_enabled: true   # Encrypt sensitive data
  auto_save_history: true    # Automatically save optimization history

# Explainability Configuration
explain:
  default_format: text       # json, text, markdown, html
  include_sensitivity: true  # Include sensitivity analysis
  include_provenance: true   # Include data provenance tracking

# Logging Configuration
logging:
  level: info                # trace, debug, info, warn, error, fatal
  file: ~/.lumen/logs/lumen.log
  rotation_size_mb: 100      # Rotate logs at 100MB
```

### Environment Variables

You can also configure Lumen via environment variables:

| Variable | Description | Example |
|----------|-------------|---------|
| `LUMEN_HOME` | Configuration directory | `~/.lumen` |
| `LUMEN_LOG_LEVEL` | Log verbosity | `debug` |
| `LUMEN_DB_PATH` | Database file location | `~/.lumen/data/lumen.db` |
| `ALPHA_VANTAGE_API_KEY` | Market data API key | `your_api_key` |
| `YAHOO_FINANCE_API_KEY` | Yahoo Finance API key (optional) | `your_api_key` |
| `DWAVE_API_KEY` | D-Wave quantum API key | `your_api_key` |
| `IBM_QUANTUM_API_KEY` | IBM Quantum API key | `your_api_key` |

#### Security Environment Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `LUMEN_API_KEY` | API key for HTTP server authentication | `your_secure_api_key` |
| `LUMEN_DB_KEY` | Database encryption key (requires SQLCipher) | `your_encryption_key` |
| `LUMEN_PYFLARE_ENABLED` | Enable telemetry (opt-in, default: false) | `true` or `false` |

> **Security Note:** Store sensitive environment variables in a secure location. Never commit API keys or encryption keys to version control. Consider using a secrets manager for production deployments.

### Managing Configuration

Use the `lumen config` command to view and modify configuration:

```bash
# Show all configuration
lumen config show

# Show specific setting
lumen config get market_data.primary_provider

# Set a configuration value
lumen config set market_data.cache_ttl_minutes 120

# Show configuration as JSON
lumen config show --format json
```

---

## Market Data

Lumen integrates with market data providers to fetch real-time and historical prices.

### Supported Providers

| Provider | Free Tier | Rate Limit | Features |
|----------|-----------|------------|----------|
| **Alpha Vantage** | Yes | 5 calls/min | Quotes, historical data, fundamentals |
| **Yahoo Finance** | Yes | Varies | Quotes, historical data |

### Setting Up API Keys

#### Alpha Vantage (Recommended)

1. Sign up at [alphavantage.co](https://www.alphavantage.co/support/#api-key)
2. Get your free API key
3. Configure in Lumen:

```bash
# Via environment variable
export ALPHA_VANTAGE_API_KEY=your_api_key

# Or via config command
lumen config set market_data.alpha_vantage_api_key your_api_key
```

### Fetching Quotes

Use the `lumen quote` command to fetch current prices:

```bash
# Single ticker
lumen quote AAPL

# Multiple tickers
lumen quote AAPL GOOGL MSFT VTI

# Output as JSON
lumen quote AAPL GOOGL --format json

# Output as table (text)
lumen quote AAPL GOOGL --format text
```

Example output:

```
Stock Quotes
============
Ticker    Price      Change     Change %   Volume
------    -----      ------     --------   ------
AAPL      $178.72    +$2.15     +1.22%     52,341,200
GOOGL     $142.56    -$0.89     -0.62%     18,234,100
```

### Automatic Price Updates

When running optimization, use `--fetch-prices` to automatically update prices:

```bash
lumen optimize --portfolio portfolio.csv --target targets.json --fetch-prices
```

This will:
1. Fetch current prices for all tickers in your portfolio
2. Update the portfolio with current market prices
3. Run optimization with fresh data
4. Record data provenance for explainability

### Rate Limiting

The free tier of Alpha Vantage allows 5 API calls per minute. Lumen automatically:
- Queues requests to stay within rate limits
- Caches responses to minimize API calls
- Falls back to cached data when rate limited

To increase throughput, consider:
- Upgrading to a premium API plan
- Configuring a longer cache TTL
- Using the Yahoo Finance fallback provider

---

## Portfolio Management

### Portfolio File Formats

Lumen supports multiple portfolio file formats:

#### CSV Format

```csv
ticker,shares,price,cost_basis,asset_class,exchange,fractional
AAPL,100.5,175.50,150.00,stocks,NASDAQ,true
GOOGL,50,140.25,120.00,stocks,NASDAQ,false
```

**Required columns:** `ticker`, `shares`
**Optional columns:** `price`, `cost_basis`, `asset_class`, `exchange`, `fractional`

#### JSON Format

```json
{
  "name": "My Portfolio",
  "currency": "USD",
  "cash_balance": 5000.00,
  "positions": [
    {
      "ticker": "AAPL",
      "shares": 100.5,
      "current_price": 175.50,
      "cost_basis": 150.00,
      "asset_class": "stocks",
      "exchange": "NASDAQ",
      "supports_fractional": true
    }
  ]
}
```

### Importing Portfolios

Import portfolios from files and save them to the local database:

```bash
# Import from CSV
lumen import portfolio.csv --name "My Retirement Account"

# Import from JSON
lumen import portfolio.json --name "Brokerage Account" --format json

# Import and fetch current prices
lumen import portfolio.csv --name "Trading Account" --update-prices
```

### Listing Portfolios

View saved portfolios:

```bash
# List all portfolios
lumen list portfolios

# Output as JSON
lumen list portfolios --format json
```

Example output:

```
Saved Portfolios
================
ID          Name                    Positions  Total Value   Last Updated
--          ----                    ---------  -----------   ------------
pf_001      My Retirement Account   12         $125,432.50   2024-01-15 14:30
pf_002      Brokerage Account       8          $45,231.00    2024-01-14 09:15
pf_003      Trading Account         5          $10,500.00    2024-01-15 10:00
```

### Asset Classes

Lumen supports the following asset classes:

| Asset Class | Description |
|-------------|-------------|
| `stocks` | Equities and stock ETFs |
| `bonds` | Fixed income securities |
| `cash` | Cash and money market |
| `commodities` | Commodity funds and ETFs |
| `real_estate` | REITs and real estate funds |
| `crypto` | Cryptocurrency holdings |
| `other` | Any other asset type |

---

## Setting Target Allocations

### Percentage-Based Targets

Target allocations as percentages of total portfolio value (must sum to 100%):

```json
{
  "mode": "percentage",
  "targets": [
    {"identifier": "stocks", "target_value": 0.60, "lower_bound": 0.55, "upper_bound": 0.65, "is_asset_class": true},
    {"identifier": "bonds", "target_value": 0.30, "lower_bound": 0.25, "upper_bound": 0.35, "is_asset_class": true},
    {"identifier": "cash", "target_value": 0.10, "lower_bound": 0.05, "upper_bound": 0.15, "is_asset_class": true}
  ]
}
```

### Dollar Amount Targets

Target allocations as fixed dollar amounts:

```json
{
  "mode": "dollar_amount",
  "targets": [
    {"identifier": "AAPL", "target_value": 25000, "lower_bound": 20000, "upper_bound": 30000, "is_asset_class": false},
    {"identifier": "VTI", "target_value": 50000, "lower_bound": 45000, "upper_bound": 55000, "is_asset_class": false}
  ]
}
```

### Tolerance Bands

Each target has:
- **target_value:** The ideal allocation
- **lower_bound:** Minimum acceptable allocation
- **upper_bound:** Maximum acceptable allocation

The optimizer will try to reach the target_value but will accept any allocation within the bounds.

---

## Constraints

Constraints define the rules that your rebalancing must follow. Add constraints to limit how the optimizer can rebalance.

### Budget Constraint

Ensures total portfolio value equals a specific amount:

```json
{
  "type": "budget",
  "total_budget": 100000.00,
  "min_cash_reserve": 5000.00
}
```

### Allocation Constraint

Limits allocation to a specific ticker or asset class:

```json
{
  "type": "allocation",
  "identifier": "AAPL",
  "lower_bound": 0.05,
  "upper_bound": 0.15,
  "is_asset_class": false
}
```

### Minimum Trade Size

Prevents small trades that may not be worth the transaction costs:

```json
{
  "type": "min_trade",
  "min_amount": 100.00,
  "min_shares": 1.0
}
```

### Integer Shares

Requires whole share trades (no fractional shares):

```json
{
  "type": "integer_shares",
  "enforce": true,
  "exempt_tickers": ["SPY", "BRK.A"]
}
```

### Position Limit

Limits maximum allocation to any single position:

```json
{
  "type": "position_limit",
  "max_position_percent": 0.25
}
```

### Using Constraints File

Create a constraints file (`constraints.json`):

```json
[
  {"type": "budget", "total_budget": 100000.00, "min_cash_reserve": 2000.00},
  {"type": "min_trade", "min_amount": 50.00},
  {"type": "integer_shares", "enforce": true, "exempt_tickers": []},
  {"type": "position_limit", "max_position_percent": 0.30}
]
```

Use with the optimizer:

```bash
lumen optimize --portfolio portfolio.csv --target targets.json --constraints constraints.json
```

---

## Running Optimization

### Basic Optimization

```bash
lumen optimize \
  --portfolio portfolio.csv \
  --target targets.json \
  --output trades.json
```

### With Constraints and Price Fetching

```bash
lumen optimize \
  --portfolio portfolio.csv \
  --target targets.json \
  --constraints constraints.json \
  --output trades.json \
  --fetch-prices
```

### Optimization Options

| Option | Description | Default |
|--------|-------------|---------|
| `--portfolio`, `-p` | Path to portfolio file | Required |
| `--target`, `-t` | Path to target allocation file | Required |
| `--constraints`, `-c` | Path to constraints file | None |
| `--output`, `-o` | Output file for trades | stdout |
| `--format`, `-f` | Output format (json, text, markdown, html) | json |
| `--fetch-prices` | Fetch current prices before optimizing | false |
| `--verbose`, `-v` | Enable verbose output | false |

### Output Formats

Lumen supports multiple output formats:

```bash
# JSON output (default, machine-readable)
lumen optimize -p portfolio.csv -t targets.json --format json

# Plain text output (human-readable)
lumen optimize -p portfolio.csv -t targets.json --format text

# Markdown output (for documentation/reports)
lumen optimize -p portfolio.csv -t targets.json --format markdown

# HTML output (for web display)
lumen optimize -p portfolio.csv -t targets.json --format html
```

### Example with All Options

```bash
lumen optimize \
  --portfolio portfolio.csv \
  --target targets.json \
  --constraints constraints.json \
  --output trades.json \
  --format markdown \
  --fetch-prices \
  --verbose
```

---

## Understanding Results

### Result Structure

```json
{
  "success": true,
  "status": "optimal",
  "session_id": "opt_20240115_143052_a7b3c",
  "objective_value": 0.0234,
  "optimality_gap": 0.0,
  "trades": [...],
  "final_allocation": {...},
  "total_transaction_cost": 12.50,
  "solver_used": "HiGHS",
  "solve_time_ms": 145,
  "active_constraints": [...],
  "binding_constraints": [...],
  "provenance": {...}
}
```

### Status Values

| Status | Description |
|--------|-------------|
| `optimal` | Found the best possible solution |
| `feasible` | Found a valid solution (may not be optimal) |
| `infeasible` | No solution exists that satisfies all constraints |
| `unbounded` | Problem has no finite solution |
| `timeout` | Solver ran out of time |
| `error` | An error occurred during solving |

### Understanding Trades

Each trade recommendation includes:

```json
{
  "ticker": "AAPL",
  "action": "buy",
  "shares": 15.0,
  "price": 175.50,
  "amount": 2632.50,
  "transaction_cost": 2.63,
  "rationale": "Increase allocation from 18% to target 20%"
}
```

### Binding vs Active Constraints

- **Active constraints:** Constraints that are being enforced
- **Binding constraints:** Constraints that are at their limit (affecting the solution)

If a constraint is binding, it means changing it could improve the solution.

---

## Explainability

Lumen provides comprehensive explanations for optimization decisions, including provenance tracking and sensitivity analysis.

### Explaining Past Optimizations

Use the `explain` command to get detailed explanations of past optimizations:

```bash
# Explain by session ID
lumen explain opt_20240115_143052_a7b3c

# Output to file
lumen explain opt_20240115_143052_a7b3c --output explanation.md --format markdown

# Different output formats
lumen explain opt_20240115_143052_a7b3c --format json
lumen explain opt_20240115_143052_a7b3c --format html
lumen explain opt_20240115_143052_a7b3c --format text
```

### Explanation Contents

The explanation document includes:

#### 1. Executive Summary
High-level overview of what happened and why.

#### 2. Trade Rationales
Detailed explanation for each recommended trade:

```
Trade: BUY 15 shares of AAPL

Rationale:
  - Current allocation: 18.2% ($18,200)
  - Target allocation: 20.0% ($20,000)
  - Allocation drift: -1.8%

Why this trade:
  - Brings position closer to target allocation
  - Maintains diversification across asset classes
  - Minimizes transaction costs while achieving target

Constraints affecting this trade:
  - Budget constraint (binding): Limited available cash
  - Position limit: Within 25% maximum
```

#### 3. Allocation Comparison
Before/after comparison showing how allocations change:

```
Ticker    Before     After      Target     Drift
------    ------     -----      ------     -----
AAPL      18.2%      20.1%      20.0%      +0.1%
GOOGL     15.5%      15.0%      15.0%      0.0%
VTI       33.1%      35.2%      35.0%      +0.2%
BND       28.2%      24.7%      25.0%      -0.3%
CASH      5.0%       5.0%       5.0%       0.0%
```

#### 4. Cost Breakdown
Detailed breakdown of all costs:

```
Cost Type           Amount
---------           ------
Commission          $12.50
Bid-Ask Spread      $8.25
Market Impact       $3.50
Tax Implications    $45.00
                    -------
Total Cost          $69.25
```

#### 5. Sensitivity Analysis
How results would change under different scenarios:

```
Price Shock Analysis:
  If AAPL price increases 5%: Total cost increases $15.20
  If AAPL price decreases 5%: Total cost decreases $14.80

Constraint Relaxation:
  Relaxing position limit to 30%: Could reduce tracking error by 0.5%
  Relaxing min trade size: 3 additional trades possible
```

#### 6. Data Provenance
Complete audit trail of data sources:

```
Data Sources:
  - Portfolio: portfolio.csv (loaded 2024-01-15 14:25:30)
  - Prices: Alpha Vantage API (fetched 2024-01-15 14:30:00)
  - Targets: targets.json (loaded 2024-01-15 14:25:30)

Solver Information:
  - Solver: HiGHS 1.5.3
  - Mode: Mixed-Integer Linear Programming
  - Solve time: 145ms
  - Optimality gap: 0.0%

Assumptions:
  - Transaction cost: 0.1% of trade value
  - Short-term capital gains rate: 35%
  - Long-term capital gains rate: 15%
```

### Viewing Optimization History

```bash
# List recent optimizations
lumen list history

# Output as JSON
lumen list history --format json
```

Example output:

```
Optimization History
====================
Session ID                    Date                 Status    Trades
----------                    ----                 ------    ------
opt_20240115_143052_a7b3c     2024-01-15 14:30     optimal   5
opt_20240114_091522_f2e1d     2024-01-14 09:15     optimal   3
opt_20240113_162045_c8a4b     2024-01-13 16:20     feasible  8
```

---

## Tax Optimization

Lumen provides comprehensive tax-aware portfolio rebalancing to help minimize your tax liability while achieving your investment goals.

### Overview

Tax-aware optimization in Lumen includes:
- **Tax lot tracking** - Track individual purchase lots with cost basis and acquisition dates
- **Lot selection methods** - Choose how lots are selected for sales (FIFO, LIFO, HIFO, etc.)
- **Tax-loss harvesting** - Identify opportunities to realize losses for tax benefits
- **Wash sale detection** - Prevent violations of the wash sale rule
- **Capital gains reporting** - Generate tax reports for planning and filing
- **Broker import** - Import tax lots from Schwab, Fidelity, Vanguard, or CSV files

### Tax Lot Methods

When selling securities, Lumen can select which specific lots to sell:

| Method | Description | Best For |
|--------|-------------|----------|
| `FIFO` | First-In, First-Out (oldest lots sold first) | IRS default method |
| `LIFO` | Last-In, First-Out (newest lots sold first) | Deferring gains |
| `HIFO` | Highest-In, First-Out (highest cost basis first) | Minimizing current taxes |
| `LOFO` | Lowest-In, First-Out (lowest cost basis first) | Realizing gains strategically |
| `SPEC_ID` | Specific identification (choose which lots to sell) | Maximum control |

**Note:** HIFO is generally the best choice for tax optimization as it minimizes realized gains (or maximizes realized losses).

### Importing Tax Lots

Before using tax optimization features, you need to import your tax lots from your brokerage.

#### Import from Broker Files

Lumen supports importing from major brokerages:

```bash
# Auto-detect broker format
lumen import-lots transactions.csv --lots-db lots.db

# Specify broker explicitly
lumen import-lots schwab_lots.csv --broker schwab --lots-db lots.db
lumen import-lots fidelity_export.csv --broker fidelity --lots-db lots.db
lumen import-lots vanguard_positions.csv --broker vanguard --lots-db lots.db
```

Supported brokers:
- **Schwab** - Charles Schwab position export format
- **Fidelity** - Fidelity cost basis export
- **Vanguard** - Vanguard position/lot export
- **CSV** - Generic CSV (customize column mapping)

#### Generic CSV Format

For custom CSV files, use these column names:

```csv
ticker,shares,cost_basis,purchase_date
AAPL,100,15000.00,2023-01-15
VTI,50,11000.00,2023-03-20
```

Required columns:
- `ticker` - Stock symbol
- `shares` - Number of shares
- `cost_basis` - Total cost basis (not per share)
- `purchase_date` - Acquisition date (YYYY-MM-DD format recommended)

### Tax-Loss Harvesting

Identify opportunities to harvest tax losses and offset gains:

```bash
# Find harvesting opportunities
lumen tax-harvest -p portfolio.json --lots-db lots.db

# Set minimum loss threshold ($500 default)
lumen tax-harvest -p portfolio.json --lots-db lots.db --min-loss 500

# Fetch current prices for accurate calculations
lumen tax-harvest -p portfolio.json --lots-db lots.db --fetch-prices

# Output as JSON for further processing
lumen tax-harvest -p portfolio.json --lots-db lots.db -f json -o opportunities.json
```

**Example Output:**

```
=== Tax-Loss Harvesting Opportunities ===

Found 3 opportunities:

Ticker      Shares    Cost Basis  Curr Price   Loss Amount        Type
------------------------------------------------------------------------
VTI         50.00       $220.00     $200.00     $1,000.00   Short-term
    Replacement candidates: VTSAX, SPTM, ITOT
BND         100.00       $85.00      $80.00       $500.00   Long-term
    Replacement candidates: AGG, VBTLX, SCHZ
VXUS         75.00       $60.00      $55.00       $375.00   Long-term
    Replacement candidates: VEA, IEFA, SCHF

Total harvestable losses: $1,875.00
```

### Wash Sale Prevention

The IRS wash sale rule disallows loss deductions if you purchase "substantially identical" securities within 30 days before or after a sale at a loss.

Lumen automatically:
1. **Detects wash sale violations** when proposing trades
2. **Tracks equivalent securities** in the same equivalence group
3. **Warns about potential violations** in harvesting opportunities

#### Default Equivalence Groups

Lumen recognizes these securities as substantially identical:

| Category | Securities |
|----------|------------|
| Total US Stock | VTI, VTSAX, SPTM, ITOT, SCHB |
| S&P 500 | VOO, SPY, IVV, VFIAX, SWPPX |
| Total Bond | BND, AGG, VBTLX, SCHZ |
| International Developed | VXUS, VEA, IEFA, SCHF, VTIAX |

#### Wash Sale Constraint

Add a wash sale constraint to your optimization:

```json
{
  "constraints": [
    {
      "type": "wash_sale",
      "block_violations": true,
      "warn_only": false
    }
  ]
}
```

### Capital Gains Report

Generate detailed capital gains reports for tax planning:

```bash
# Generate report for current year
lumen tax-report --lots-db lots.db

# Specify tax year
lumen tax-report --lots-db lots.db --year 2024

# Export as JSON
lumen tax-report --lots-db lots.db -f json -o tax_report.json
```

**Example Output:**

```
=== Capital Gains Report for 2024 ===

Category              Amount
--------------------------------
Short-term gains      $2,500.00
Short-term losses    -$1,000.00
Long-term gains       $5,000.00
Long-term losses     -$2,000.00

Net short-term        $1,500.00
Net long-term         $3,000.00

Estimated tax savings   $450.00

Detailed Transactions:
--------------------------------------------------------------------------------
Ticker      Shares    Cost Basis    Proceeds     Gain/Loss        Type
--------------------------------------------------------------------------------
AAPL        50.00      $7,500.00   $10,000.00   $2,500.00   Short-term
VTI         30.00      $6,600.00    $6,000.00    -$600.00   Long-term
...
```

### Tax-Aware Optimization

Enable tax-aware optimization by adding tax constraints:

```json
{
  "constraints": [
    {
      "type": "budget",
      "amount": 100000.00
    },
    {
      "type": "tax_lot",
      "method": "HIFO",
      "prefer_long_term": true
    },
    {
      "type": "wash_sale",
      "block_violations": true
    }
  ]
}
```

Run optimization with tax awareness:

```bash
lumen optimize -p portfolio.json -t targets.json -c constraints.json \
  --fetch-prices -o results.json
```

The optimizer will:
1. Select lots using the specified method (HIFO minimizes taxes)
2. Prefer long-term lots when possible (lower tax rate)
3. Avoid trades that would trigger wash sale violations
4. Include tax impact in the optimization objective

### Tax Impact in Reports

When tax optimization is enabled, explanation reports include:

```
TAX IMPACT
----------------------------------------
  Short-term gain/loss: -$500.00
  Long-term gain/loss: $1,200.00
  Estimated tax: $180.00

RECOMMENDED TRADES
----------------------------------------
1. Sell 30.00 shares of VTI @ $200.00 ($6,000.00)
   Allocation: 52.20% -> 45.00% (target: 45.00%)
   Reasons:
     - Currently overweight by 7.20%
   Addresses constraints:
     - Target Allocation
   Tax impact:
     - Short-term gain/loss: -$600.00
     - Long-term gain/loss: $0.00
     - Estimated tax: -$210.00
   Lot IDs: VTI_LOT_002
```

### Best Practices

1. **Import lots before year-end** - Ensure your lot data is current for tax planning
2. **Use HIFO for tax efficiency** - Generally minimizes realized gains
3. **Check wash sales before harvesting** - Review replacement candidates carefully
4. **Harvest losses before year-end** - Losses can offset gains for the tax year
5. **Consider holding periods** - Long-term gains are taxed at lower rates
6. **Consult a tax professional** - Tax rules can be complex; seek professional advice

---

## Quantum Optimization

Lumen supports quantum computing for portfolio optimization using D-Wave quantum annealing and IBM Quantum gate-based computing. Quantum optimization can provide advantages for complex portfolios with many tax lots.

### Overview

Quantum computing in Lumen:
- **D-Wave Quantum Annealing** - Best for combinatorial optimization (tax lot selection)
- **IBM Quantum QAOA** - Gate-based quantum computing with the Quantum Approximate Optimization Algorithm
- **Hybrid Workflow** - Combines classical preprocessing, quantum optimization, and classical postprocessing
- **Mock Mode** - Test quantum workflows without API costs

### When to Use Quantum

Quantum optimization is most beneficial for:
- **Large tax lot selection** (50+ lots) - Combinatorial complexity
- **Multi-objective optimization** - Balancing drift, cost, and tax impact
- **Complex constraints** - Many interacting allocation rules

For simple portfolios, classical solvers (HiGHS) are typically faster and sufficient.

### Setting Up Quantum Access

#### D-Wave

1. Sign up at [D-Wave Leap](https://cloud.dwavesys.com/leap/)
2. Get your API key from the dashboard
3. Configure in Lumen:

```bash
# Via environment variable
export DWAVE_API_KEY=your_api_key

# Or via config command
lumen config set quantum.dwave.api_key your_api_key
```

#### IBM Quantum

1. Sign up at [IBM Quantum](https://quantum.ibm.com/)
2. Get your API key from your account settings
3. Configure in Lumen:

```bash
# Via environment variable
export IBM_QUANTUM_API_KEY=your_api_key

# Or via config command
lumen config set quantum.ibm.api_key your_api_key
```

### Using Mock Mode (Testing)

Mock mode simulates quantum computing using classical algorithms, allowing you to test quantum workflows without incurring API costs:

```bash
# Enable mock mode for D-Wave
lumen optimize -p portfolio.json -t targets.json --quantum --quantum-mock

# Enable mock mode for IBM Quantum
lumen optimize -p portfolio.json -t targets.json --quantum --quantum-provider ibm --quantum-mock
```

Mock mode is ideal for:
- Testing your optimization setup
- Development and debugging
- Understanding how quantum optimization affects results

### Running Quantum Optimization

#### Basic Quantum Optimization

```bash
# Use D-Wave quantum annealing
lumen optimize -p portfolio.json -t targets.json --quantum

# Use IBM Quantum QAOA
lumen optimize -p portfolio.json -t targets.json --quantum --quantum-provider ibm
```

#### With Cost Estimation

Before running, see the estimated cost:

```bash
# Show cost estimate before solving
lumen optimize -p portfolio.json -t targets.json --quantum --quantum-estimate
```

Example output:
```
Quantum Cost Estimate
=====================
Provider: D-Wave
Problem size: 156 qubits
Estimated cost: $0.42
Estimated time: 2-3 minutes

Proceed with quantum optimization? [y/N]
```

#### Quantum Optimization Options

| Option | Description | Default |
|--------|-------------|---------|
| `--quantum` | Enable quantum optimization | false |
| `--quantum-provider` | Provider: `dwave` or `ibm` | dwave |
| `--quantum-mock` | Use mock/simulator mode | false |
| `--quantum-estimate` | Show cost estimate before solving | false |

#### D-Wave Options

| Option | Description | Default |
|--------|-------------|---------|
| `--dwave-solver` | Solver name | hybrid_binary_quadratic_model_version2 |
| `--dwave-reads` | Number of samples | 1000 |
| `--dwave-timeout` | Timeout in seconds | 60 |

#### IBM Quantum Options

| Option | Description | Default |
|--------|-------------|---------|
| `--ibm-backend` | Backend name | ibm_brisbane |
| `--ibm-shots` | Number of measurement shots | 1024 |
| `--ibm-qaoa-depth` | QAOA circuit depth (p) | 2 |
| `--ibm-optimizer` | Optimizer: `SPSA` or `COBYLA` | SPSA |

#### Hybrid Options

| Option | Description | Default |
|--------|-------------|---------|
| `--hybrid-preprocess` | Enable classical preprocessing | true |
| `--hybrid-postprocess` | Enable classical postprocessing | true |
| `--hybrid-iterations` | Max classical-quantum iterations | 3 |

### Configuration File

Configure quantum settings in `~/.lumen/config.yaml`:

```yaml
quantum:
  enabled: false               # Enable quantum by default
  provider: dwave             # Default provider
  use_mock: false             # Use mock mode by default
  show_cost_estimate: true    # Show estimates before solving
  max_cost_per_solve: 0       # Maximum cost limit (0 = unlimited)

  dwave:
    api_key_env: DWAVE_API_KEY
    solver: hybrid_binary_quadratic_model_version2
    num_reads: 1000
    use_hybrid: true
    timeout_ms: 60000

  ibm:
    api_key_env: IBM_QUANTUM_API_KEY
    backend: ibm_brisbane
    num_shots: 1024
    qaoa_depth: 2
    optimizer: SPSA
    timeout_ms: 300000

  hybrid:
    lot_threshold: 50         # Use hybrid above this lot count
    preprocess_enabled: true
    postprocess_enabled: true
    max_iterations: 3
    improvement_threshold: 0.001
```

### Hybrid Workflow

The hybrid workflow combines classical and quantum computing for best results:

```
┌────────────────────┐     ┌────────────────────┐     ┌────────────────────┐
│  1. Preprocessing  │ ──► │  2. Quantum Solve  │ ──► │  3. Postprocessing │
└────────────────────┘     └────────────────────┘     └────────────────────┘
      Classical                   Quantum                   Classical
```

1. **Preprocessing (Classical)**
   - Fixes "obvious" positions already at target
   - Reduces problem size for quantum
   - Identifies independent subproblems

2. **Quantum Optimization**
   - Solves the reduced problem
   - D-Wave: Quantum annealing
   - IBM: QAOA with SPSA optimizer

3. **Postprocessing (Classical)**
   - Validates constraints
   - Repairs any violations using HiGHS
   - Polishes the solution for final result

### Understanding Quantum Results

Quantum optimization results include additional information:

```json
{
  "success": true,
  "status": "optimal",
  "solver_used": "Hybrid (D-Wave + HiGHS)",
  "trades": [...],
  "quantum_info": {
    "qubits_used": 156,
    "annealing_time_us": 20000,
    "total_time_ms": 2341,
    "samples_collected": 1000,
    "best_energy": -4523.67,
    "chain_break_fraction": 0.02
  },
  "hybrid_info": {
    "preprocessing_reduction": 0.35,
    "fixed_positions": 4,
    "iterations": 2,
    "postprocessing_repairs": 0
  }
}
```

Key metrics:
- **qubits_used** - Number of quantum variables
- **annealing_time_us** - Time on the quantum processor (microseconds)
- **chain_break_fraction** - Quality metric (lower is better, <0.05 is good)
- **preprocessing_reduction** - Problem size reduction (35% = 35% smaller)
- **postprocessing_repairs** - Number of constraint violations fixed

### Cost Management

Quantum computing can incur costs. Manage costs with:

#### Cost Estimation

```bash
# Always show cost before solving
lumen config set quantum.show_cost_estimate true
```

#### Cost Limits

```bash
# Set maximum cost per solve ($5 limit)
lumen config set quantum.max_cost_per_solve 5.00
```

If estimated cost exceeds the limit, Lumen will:
1. Fall back to classical solver
2. Or ask for confirmation (interactive mode)

#### Pricing Reference

| Provider | Approximate Pricing |
|----------|---------------------|
| D-Wave Hybrid | ~$0.20/minute of solver time |
| IBM Quantum | ~$1.60/second of quantum time |

**Note:** Mock mode is always free.

### Best Practices

1. **Start with mock mode** - Test your workflow before using real quantum
2. **Use cost estimates** - Always review costs before large optimizations
3. **Monitor chain breaks** - High chain break fractions indicate embedding issues
4. **Enable hybrid** - Classical preprocessing significantly improves results
5. **Increase reads for complex problems** - More samples = better solutions
6. **Use appropriate depth for QAOA** - p=2 is usually sufficient; higher adds cost

### Troubleshooting Quantum

#### "No quantum API key configured"

```bash
# Check configuration
lumen config get quantum.dwave.api_key

# Set API key
export DWAVE_API_KEY=your_key
```

#### "Quantum timeout"

The problem may be too large or complex:

```bash
# Increase timeout
lumen optimize -p portfolio.json -t targets.json --quantum --dwave-timeout 120

# Or enable preprocessing to reduce size
lumen optimize -p portfolio.json -t targets.json --quantum --hybrid-preprocess true
```

#### "High chain break fraction"

The problem may not embed well on the quantum hardware:

```bash
# Try more samples
lumen optimize -p portfolio.json -t targets.json --quantum --dwave-reads 2000

# Or use hybrid solver (handles large problems better)
lumen config set quantum.dwave.use_hybrid true
```

#### "Cost limit exceeded"

```bash
# Increase limit
lumen config set quantum.max_cost_per_solve 10.00

# Or use mock mode
lumen optimize -p portfolio.json -t targets.json --quantum --quantum-mock
```

---

## Command Line Interface

### Available Commands

```bash
lumen help                    # Show help
lumen version                 # Show version information
lumen status                  # Show system status

# Optimization
lumen optimize [options]      # Run portfolio optimization
lumen explain <session_id>    # Explain a past optimization

# Portfolio Management
lumen import <file>           # Import portfolio from file
lumen list portfolios         # List saved portfolios
lumen list history            # List optimization history

# Market Data
lumen quote <tickers...>      # Fetch stock quotes

# Configuration
lumen config show             # Show all configuration
lumen config get <key>        # Get a configuration value
lumen config set <key> <val>  # Set a configuration value

# Tax Planning
lumen tax-harvest [options]   # Find tax-loss harvesting opportunities
lumen tax-report [options]    # Generate capital gains report
lumen import-lots <file>      # Import tax lots from broker files
```

### Global Options

```bash
--config <path>       # Path to config file
--quiet               # Suppress non-essential output
--verbose             # Enable verbose output
--log-level <level>   # Set log level (trace/debug/info/warn/error)
--format <format>     # Output format (json/text/markdown/html)
```

### Command Reference

#### optimize

Run portfolio optimization:

```bash
lumen optimize -p portfolio.csv -t targets.json [options]

Options:
  -p, --portfolio <file>     Portfolio file (required)
  -t, --target <file>        Target allocation file (required)
  -c, --constraints <file>   Constraints file
  -o, --output <file>        Output file (default: stdout)
  -f, --format <format>      Output format: json, text, markdown, html
      --fetch-prices         Fetch current prices before optimizing
  -v, --verbose              Enable verbose output
```

#### import

Import a portfolio from file:

```bash
lumen import <file> [options]

Options:
  -n, --name <name>          Portfolio name
  -f, --format <format>      Input format: csv, json (auto-detected)
      --update-prices        Fetch and update current prices
```

#### explain

Explain a past optimization:

```bash
lumen explain <session_id> [options]

Options:
  -o, --output <file>        Output file (default: stdout)
  -f, --format <format>      Output format: json, text, markdown, html
```

#### list

List portfolios or history:

```bash
lumen list <what> [options]

Arguments:
  what                       What to list: portfolios, history

Options:
  -f, --format <format>      Output format: json, text
```

#### quote

Fetch stock quotes:

```bash
lumen quote <tickers...> [options]

Arguments:
  tickers                    One or more ticker symbols

Options:
  -f, --format <format>      Output format: json, text
```

#### config

Manage configuration:

```bash
lumen config <action> [key] [value] [options]

Actions:
  show                       Show all configuration
  get <key>                  Get a configuration value
  set <key> <value>          Set a configuration value

Options:
  -f, --format <format>      Output format: json, text
```

#### status

Show system status:

```bash
lumen status

Displays:
  - Database connection status
  - Market data provider status
  - API key configuration
  - Cache statistics
```

#### tax-harvest

Find tax-loss harvesting opportunities:

```bash
lumen tax-harvest -p portfolio.json [options]

Options:
  -p, --portfolio <file>     Portfolio JSON file (required)
      --lots-db <file>       Tax lots database file
      --min-loss <amount>    Minimum loss threshold (default: $100)
      --fetch-prices         Fetch current market prices
  -o, --output <file>        Output file (default: stdout)
  -f, --format <format>      Output format: json, text
```

Example:
```bash
# Find harvesting opportunities with at least $500 loss
lumen tax-harvest -p portfolio.json --lots-db lots.db --min-loss 500 --fetch-prices

# Export as JSON
lumen tax-harvest -p portfolio.json --lots-db lots.db -f json -o opportunities.json
```

#### tax-report

Generate capital gains tax report:

```bash
lumen tax-report --lots-db <file> [options]

Options:
      --lots-db <file>       Tax lots database file (required)
      --year <year>          Tax year (default: current year)
  -o, --output <file>        Output file (default: stdout)
  -f, --format <format>      Output format: json, text
```

Example:
```bash
# Generate report for 2024
lumen tax-report --lots-db lots.db --year 2024

# Export as JSON for tax software
lumen tax-report --lots-db lots.db --year 2024 -f json -o tax_2024.json
```

#### import-lots

Import tax lots from broker files:

```bash
lumen import-lots <file> [options]

Arguments:
  file                       Broker CSV file (required)

Options:
      --lots-db <file>       Tax lots database file (default: ~/.lumen/lots.db)
      --broker <name>        Broker format: schwab, fidelity, vanguard, csv
      --auto-detect          Auto-detect broker format (default: true)
      --no-auto-detect       Disable auto-detection
```

Examples:
```bash
# Import with auto-detection
lumen import-lots schwab_export.csv --lots-db lots.db

# Specify broker explicitly
lumen import-lots positions.csv --broker schwab --lots-db lots.db

# Import from Fidelity
lumen import-lots fidelity_cost_basis.csv --broker fidelity --lots-db lots.db

# Import from Vanguard
lumen import-lots vanguard_lots.csv --broker vanguard --lots-db lots.db
```

---

## REST API Server

Lumen includes a REST API server for integration with other applications.

### Starting the Server

```bash
# Basic usage (localhost only - secure default)
lumen-server --port 8080

# With TLS encryption (recommended for production)
lumen-server --port 8443 --tls --cert /path/to/cert.pem --key /path/to/key.pem

# Allow external connections (requires explicit flag)
lumen-server --port 8080 --allow-external

# Full production setup with TLS and external access
lumen-server --port 8443 --tls --cert /path/to/cert.pem --key /path/to/key.pem --allow-external
```

### Server Options

| Option | Description | Default |
|--------|-------------|---------|
| `-H, --host` | Host to bind to | `127.0.0.1` |
| `-p, --port` | Port to listen on | `8080` |
| `-c, --config` | Path to configuration file | None |
| `--tls` | Enable TLS/SSL encryption | Disabled |
| `--cert` | Path to TLS certificate file | Required with `--tls` |
| `--key` | Path to TLS private key file | Required with `--tls` |
| `--allow-external` | Bind to 0.0.0.0 for external access | Disabled |

### Security Features

The HTTP server includes several security features:

#### Authentication

All endpoints except `/health` require API key authentication. Set your API key:

```bash
# Set API key via environment variable
export LUMEN_API_KEY=your_secure_api_key_here

# Or add keys to ~/.lumen/api_keys.txt (one per line)
echo "your_api_key_here" >> ~/.lumen/api_keys.txt
```

Include the API key in requests using the `Authorization` header:

```bash
# Using Bearer token format
curl -H "Authorization: Bearer your_api_key_here" http://localhost:8080/version

# Using ApiKey format
curl -H "Authorization: ApiKey your_api_key_here" http://localhost:8080/status
```

If no API key is configured, the server will generate a temporary key and display it in the logs.

#### Rate Limiting

The server enforces rate limiting to prevent abuse:
- **100 requests per minute** per IP address
- Exceeding the limit returns HTTP 429 (Too Many Requests)

#### TLS/SSL Encryption

For production deployments, always enable TLS:

```bash
# Generate a self-signed certificate for testing
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes

# Start server with TLS
lumen-server --tls --cert cert.pem --key key.pem --port 8443
```

> **Warning:** Never expose the HTTP server to untrusted networks without TLS enabled.

#### Security Headers

All responses include security headers:
- `X-Content-Type-Options: nosniff`
- `X-Frame-Options: DENY`
- `Content-Security-Policy: default-src 'none'`
- `Strict-Transport-Security` (when TLS enabled)

### API Endpoints

#### Health Check

```
GET /health
```

Response:
```json
{"status": "healthy", "version": "1.0.0"}
```

#### Optimize Portfolio

```
POST /api/v1/optimize
Content-Type: application/json

{
  "portfolio": {...},
  "target": {...},
  "constraints": [...],
  "fetch_prices": true
}
```

Response:
```json
{
  "success": true,
  "session_id": "opt_20240115_143052_a7b3c",
  "trades": [...],
  "solve_time_ms": 145
}
```

#### Get Explanation

```
GET /api/v1/explain/{session_id}
```

Response:
```json
{
  "session_id": "opt_20240115_143052_a7b3c",
  "summary": "...",
  "trades": [...],
  "provenance": {...}
}
```

#### Fetch Quote

```
GET /api/v1/quote/{ticker}
```

Response:
```json
{
  "ticker": "AAPL",
  "price": 178.72,
  "change": 2.15,
  "change_percent": 1.22,
  "volume": 52341200,
  "timestamp": "2024-01-15T14:30:00Z"
}
```

#### Validate Portfolio

```
POST /api/v1/validate
Content-Type: application/json

{
  "portfolio": {...}
}
```

#### Get Tax Report

```
POST /api/v1/tax-report
Content-Type: application/json

{
  "portfolio": {...},
  "year": 2024
}
```

---

## Desktop GUI Application

Lumen includes a full-featured desktop GUI application for visual portfolio management and optimization. The GUI provides an intuitive interface for users who prefer graphical interaction over command-line tools.

### Supported Platforms

- **Windows 11** or later (64-bit)
- **Ubuntu 20.04** or later (64-bit)

**Note:** macOS is not supported in this version.

### Installing the GUI

#### Windows

1. Download the installer: `Lumen-<version>-Setup.exe`
2. Run the installer and follow the prompts
3. Launch Lumen from the Start Menu or Desktop shortcut

#### Linux (Ubuntu/Debian)

**AppImage (Recommended):**
```bash
# Download the AppImage
wget https://github.com/oaqlabs/lumen/releases/latest/download/Lumen-<version>-x86_64.AppImage

# Make executable
chmod +x Lumen-<version>-x86_64.AppImage

# Run
./Lumen-<version>-x86_64.AppImage
```

**DEB Package:**
```bash
# Download and install
sudo dpkg -i lumen_<version>_amd64.deb

# Install dependencies if needed
sudo apt-get install -f
```

### Launching the GUI

```bash
# From command line
lumen-gui

# With a portfolio file
lumen-gui --portfolio portfolio.csv

# With dark mode
lumen-gui --dark
```

### Main Window Overview

The GUI consists of several key areas:

```
┌─────────────────────────────────────────────────────────────────────┐
│  File   Edit   View   Portfolio   Optimization   Help               │
├─────────────────────────────────────────────────────────────────────┤
│  [New] [Open] [Save] [Import] │ [Optimize] [Stop]                   │
├──────────────────────────────────────────┬──────────────────────────┤
│                                          │     Allocation Chart     │
│         Portfolio Table                  │                          │
│                                          │         [Pie Chart]      │
│  Ticker  Shares  Price  Value  Alloc%    │                          │
│  ─────────────────────────────────────   │                          │
│  AAPL    100     175.50 17550  20.1%     │                          │
│  VTI     200     220.00 44000  50.3%     │                          │
│  BND     300     72.50  21750  24.9%     │                          │
│  CASH    1       4100   4100   4.7%      │                          │
│                                          │                          │
├──────────────────────────────────────────┴──────────────────────────┤
│  Constraints Panel  │  Results Panel   │  Comparison Panel          │
│  [✓] No short       │  Status: Optimal │  Before    After           │
│  [✓] Max 25% pos    │  Trades: 5       │  [Chart]   [Chart]         │
│  Max turnover: 50%  │  Cost: $12.50    │  Drift: 5% → 0.2%          │
└─────────────────────────────────────────────────────────────────────┘
```

### Portfolio Management

#### Creating a New Portfolio

1. Click **File > New Portfolio** or press `Ctrl+N`
2. Add positions using **Portfolio > Add Position** or the `+` button
3. Enter ticker symbol, shares, price, and cost basis
4. Save with **File > Save** or `Ctrl+S`

#### Opening an Existing Portfolio

1. Click **File > Open** or press `Ctrl+O`
2. Select a `.lumen`, `.json`, or `.csv` portfolio file
3. The portfolio loads into the table view

#### Importing from CSV

1. Click **File > Import CSV** or drag-drop a CSV file
2. The Import Dialog opens showing a preview
3. Map columns: Ticker, Shares, Price, Cost Basis, Asset Class
4. Click **Import** to load the data

Supported CSV formats:
- Auto-detected delimiter (comma, semicolon, tab)
- Header row auto-detection
- Column mapping suggestions based on common names

#### Editing Positions

- **Double-click** a cell to edit in place
- **Right-click** for context menu (Edit, Delete, View Details)
- Use **Portfolio > Edit Position** for a detailed dialog

### Setting Targets

#### Target Editor Panel

1. Open the Target Editor from **View > Target Editor** or the dock panel
2. Each position shows:
   - Current allocation (%)
   - Target allocation (editable)
   - Lower/Upper bounds (optional)

#### Quick Actions

- **Normalize**: Adjust targets to sum to 100%
- **Equal Weight**: Set all targets to equal percentages
- **Match Current**: Set targets to current allocations

### Configuring Constraints

The Constraints Panel allows you to configure optimization rules:

#### Preset Constraints

| Constraint | Description |
|------------|-------------|
| No short selling | All positions must be >= 0 |
| Respect allocation bands | Keep positions within min/max bounds |
| Tax-aware optimization | Optimize for tax efficiency |
| Min position | Minimum allocation for any position |
| Max position | Maximum allocation for any position |
| Max turnover | Maximum portfolio turnover percentage |

#### Custom Constraints

Click **Add...** to create custom constraints:

1. **Minimum/Maximum Weight**: Set bounds for specific tickers
2. **Sector Limit**: Limit allocation to a sector
3. **Minimum Cash**: Maintain a cash reserve

### Running Optimization

#### Starting Optimization

1. Ensure portfolio and targets are configured
2. Set desired constraints
3. Click **Optimization > Run** or the **Optimize** toolbar button
4. The progress widget shows:
   - Elapsed time
   - Current solver phase
   - Cancel button

#### Optimization Options

Access via **Optimization > Settings**:

| Option | Description |
|--------|-------------|
| Solver | HiGHS (classical) or Quantum |
| Timeout | Maximum solve time |
| Fetch Prices | Update prices before solving |
| Tax-Aware | Enable tax optimization |

#### Quantum Optimization

Enable quantum via **Optimization > Settings > Quantum**:

1. Check **Enable Quantum Optimization**
2. Select provider (D-Wave or IBM Quantum)
3. Configure API key (or use mock mode for testing)
4. Click **Estimate Cost** to see pricing before solving

### Viewing Results

#### Results Panel

After optimization completes, the Results Panel shows:

- **Summary**: Status, solver, time, objective value, trade count, cost
- **Trade Table**: List of recommended trades with:
  - Ticker, Action (BUY/SELL/HOLD), Shares, Amount, Tax Lots
- **Rationale**: Select a trade to see detailed explanation

#### Comparison Panel

Visual before/after comparison:

- **Side-by-side pie charts**: Current vs. proposed allocation
- **Drift analysis**: Current drift, proposed drift, improvement
- **Tax impact**: Estimated short-term gains, long-term gains, tax liability

### Exporting Results

#### Export Options

Click **File > Export** or **Export...** button in Results Panel:

| Format | Description |
|--------|-------------|
| CSV | Comma-separated values for spreadsheets |
| JSON | Machine-readable format with full details |
| Markdown | Formatted report for documentation |
| HTML | Web-ready report |

### Settings

Access via **Edit > Settings** or `Ctrl+,`:

#### General Tab

- **Auto-save**: Automatically save portfolio changes
- **Confirm on exit**: Prompt before closing with unsaved changes
- **Recent files**: Number of recent files to remember

#### Optimization Tab

- **Default solver**: HiGHS or Quantum
- **Default timeout**: Maximum solve time (seconds)
- **Transaction cost**: Default cost per trade (percentage)

#### Quantum Tab

- **Provider**: D-Wave or IBM Quantum
- **API Key**: Enter or manage API keys
- **Mock mode**: Use simulator for testing
- **Cost limit**: Maximum cost per solve

#### Appearance Tab

- **Theme**: Light, Dark, or System (auto-detect)
- **Font size**: Adjust text size
- **Chart colors**: Customize allocation chart colors

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New portfolio |
| `Ctrl+O` | Open portfolio |
| `Ctrl+S` | Save portfolio |
| `Ctrl+Shift+S` | Save as... |
| `Ctrl+I` | Import CSV |
| `Ctrl+E` | Export results |
| `Ctrl+R` | Run optimization |
| `Ctrl+,` | Settings |
| `Ctrl+Q` | Quit |
| `F1` | Help |
| `F5` | Refresh prices |
| `Escape` | Cancel optimization |

### Themes

Lumen supports light and dark themes:

- **Light**: Clean, bright interface for well-lit environments
- **Dark**: Reduced eye strain for low-light use
- **System**: Automatically matches your OS theme preference

Change via **Edit > Settings > Appearance** or **View > Theme**.

### Troubleshooting GUI

#### "Qt platform plugin could not be loaded"

**Windows:**
- Ensure Visual C++ Redistributable is installed
- Run the installer repair option

**Linux:**
- Install Qt dependencies: `sudo apt install libqt6widgets6 libqt6charts6`
- For AppImage, try: `./Lumen.AppImage --appimage-extract-and-run`

#### High DPI Scaling Issues

**Windows:**
- Right-click lumen-gui.exe > Properties > Compatibility
- Check "Override high DPI scaling" > Application

**Linux:**
```bash
export QT_AUTO_SCREEN_SCALE_FACTOR=1
./lumen-gui
```

#### Charts Not Displaying

Ensure Qt Charts module is installed:
- **Windows**: Included in installer
- **Linux**: `sudo apt install libqt6charts6`

#### Slow Performance

- Reduce chart animation: **Settings > Appearance > Disable animations**
- For large portfolios, consider CLI for batch processing

---

## Persistence & History

Lumen stores portfolios and optimization history in a local SQLite database for auditability and explainability.

### Database Location

By default, the database is stored at `~/.lumen/data/lumen.db`. Configure a different location:

```yaml
# In config.yaml
persistence:
  database_path: /path/to/your/lumen.db
```

Or via environment variable:

```bash
export LUMEN_DB_PATH=/path/to/your/lumen.db
```

### What Gets Stored

1. **Portfolios**: Imported portfolios with all positions
2. **Optimization History**: Every optimization run with:
   - Input portfolio snapshot
   - Target allocations
   - Constraints used
   - Recommended trades
   - Solver statistics
3. **Data Provenance**: Audit trail for all data sources
4. **Configuration Snapshots**: Settings at time of optimization

### Querying History

```bash
# List recent optimizations
lumen list history

# Get full explanation of past optimization
lumen explain opt_20240115_143052_a7b3c --format json
```

### Data Retention

By default, Lumen retains all history. To manage storage:

```yaml
# In config.yaml
persistence:
  retention_days: 365        # Delete history older than 1 year
  max_history_entries: 1000  # Keep at most 1000 optimization records
```

### Backup and Restore

The database is a standard SQLite file. Back it up by copying:

```bash
cp ~/.lumen/data/lumen.db ~/.lumen/data/lumen.db.backup
```

---

## Troubleshooting

### Common Issues

#### "Infeasible" Result

The optimizer couldn't find a solution that satisfies all constraints. Common causes:

1. **Conflicting constraints:** Check that allocation bounds don't conflict
2. **Impossible targets:** Ensure target allocations sum to 100%
3. **Insufficient funds:** Budget constraint may be too restrictive

**Solution:** Relax some constraints or widen tolerance bands.

#### "Timeout" Result

The solver ran out of time. Common causes:

1. **Large portfolio:** Many positions increase complexity
2. **Integer constraints:** Whole share requirements make the problem harder
3. **Tight bounds:** Narrow tolerance bands increase difficulty

**Solution:** Increase timeout, relax constraints, or enable hybrid solving.

#### Rate Limit Exceeded

```
Error: Rate limit exceeded for Alpha Vantage API
```

The free tier of Alpha Vantage limits you to 5 API calls per minute.

**Solutions:**
1. Wait and retry (Lumen caches data automatically)
2. Configure longer cache TTL: `lumen config set market_data.cache_ttl_minutes 120`
3. Upgrade to a premium API plan

#### Invalid API Key

```
Error: Invalid API key for Alpha Vantage
```

**Solutions:**
1. Verify your API key: `lumen config get market_data.alpha_vantage_api_key`
2. Set the correct key: `lumen config set market_data.alpha_vantage_api_key YOUR_KEY`
3. Or use environment variable: `export ALPHA_VANTAGE_API_KEY=YOUR_KEY`

#### Database Locked

```
Error: Database is locked
```

Another process may be using the database.

**Solutions:**
1. Close other Lumen instances
2. Check for zombie processes: `ps aux | grep lumen`
3. If safe, delete the lock file: `rm ~/.lumen/data/lumen.db-wal`

#### Invalid Portfolio File

```
Error: Failed to parse portfolio: Invalid character in ticker
```

Ensure your portfolio file:
- Uses valid ticker symbols (letters, numbers, dots, hyphens)
- Has no empty or malformed rows
- Contains required columns (ticker, shares)

#### Configuration Not Loading

```
Error: Cannot open config file
```

Check that:
- Config file exists at `~/.lumen/config.yaml`
- File has correct YAML syntax
- You have read permissions

### Getting Help

- Check the logs at `~/.lumen/logs/lumen.log`
- Run with `--verbose` for detailed output
- Use `lumen validate` to check inputs
- Use `lumen status` to check system configuration

### Reporting Issues

When reporting issues, please include:
- Lumen version (`lumen version`)
- Operating system
- Relevant configuration (sanitized)
- Steps to reproduce
- Full error message

---

## Glossary

| Term | Definition |
|------|------------|
| **Allocation** | The percentage or dollar amount invested in a position |
| **Binding constraint** | A constraint at its limit that affects the solution |
| **Cost basis** | The original purchase price of a security |
| **Drift** | The deviation from target allocation |
| **MILP** | Mixed-Integer Linear Program (optimization with integer variables) |
| **Provenance** | Audit trail tracking the origin and transformation of data |
| **QAOA** | Quantum Approximate Optimization Algorithm (IBM Quantum) |
| **QUBO** | Quadratic Unconstrained Binary Optimization (quantum problem format) |
| **Quantum annealing** | Quantum computing approach using physical annealing process |
| **Qubit** | Quantum bit, the basic unit of quantum information |
| **Rebalancing** | Adjusting portfolio to match target allocation |
| **Session ID** | Unique identifier for an optimization run |
| **SPSA** | Simultaneous Perturbation Stochastic Approximation optimizer |
| **Tax lot** | A record of a specific purchase for tax purposes |
| **Wash sale** | Selling at a loss and repurchasing within 30 days |

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 5.0.0 | TBD | Phase 5: Desktop GUI application (Windows 11+, Ubuntu 20.04+), visual portfolio management, interactive optimization |
| 4.0.0 | TBD | Phase 4: Quantum integration (D-Wave, IBM Quantum), hybrid orchestration |
| 3.0.0 | TBD | Phase 3: Tax optimization, tax lot management, broker import, wash sale detection |
| 2.0.0 | TBD | Phase 2: Market data, persistence, explainability, CLI |
| 1.0.0 | TBD | Initial release with Phase 1 features |

---

*For developer documentation, see [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md).*
