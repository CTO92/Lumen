# Lumen (pre-release V1.0.0=alpha)

**Quantum-Enhanced Edge Reasoning Engine for Portfolio Optimization**

[![CI](https://github.com/oaqlabs/lumen/actions/workflows/ci.yml/badge.svg)](https://github.com/oaqlabs/lumen/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Lumen is an open-source edge reasoning engine that brings deterministic AI to financial portfolio optimization. Unlike statistical machine learning approaches, Lumen uses explicit constraint-based reasoning to solve optimization problems with mathematical rigor and full explainability.

## Key Features

- **Deterministic, Explainable AI** - Every recommendation traceable to specific constraints
- **Edge-First Computation** - Runs on personal computers without cloud dependency
- **Hybrid Classical-Quantum** - Optional quantum enhancement for complex scenarios
- **Tax-Aware Optimization** - Tax-loss harvesting with wash sale compliance
- **Open Source Core** - MIT licensed for transparency and trust

## Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16 or higher
- vcpkg package manager (recommended)

### Installation

#### Using vcpkg (Recommended)

```bash
# Clone the repository
git clone https://github.com/oaqlabs/lumen.git
cd lumen/lumen

# Install dependencies via vcpkg
vcpkg install

# Configure and build
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Run tests
cd build && ctest --output-on-failure
```

#### Manual Build

```bash
# Install system dependencies (Ubuntu/Debian)
sudo apt-get install -y cmake ninja-build libsqlite3-dev libeigen3-dev

# Clone and build
git clone https://github.com/oaqlabs/lumen.git
cd lumen/lumen

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Install
sudo cmake --install build
```

### Basic Usage

```bash
# Optimize a portfolio
lumen optimize --portfolio portfolio.csv --target 60-40-stocks-bonds

# Find tax-loss harvesting opportunities
lumen tax-harvest --portfolio portfolio.csv --tax-rate 0.22

# View configuration
lumen config --show
```

## Documentation

- [CLI Usage Guide](docs/cli-usage.md)
- [API Reference](docs/api-reference.md)
- [Architecture Overview](../Lumen_Top_Level_Architecture.md)
- [Tax Optimization Guide](docs/tax-optimization.md)
- [Quantum Integration](docs/quantum-integration.md)

## Project Structure

```
lumen/
├── include/lumen/       # Public header files
│   ├── core/            # Portfolio, constraints, solver dispatcher
│   ├── solvers/         # HiGHS wrapper, quantum clients
│   ├── data/            # Market data, tax lots
│   ├── explain/         # Provenance, explainability
│   └── utils/           # Logging, configuration
├── src/                 # Implementation files
├── apps/
│   ├── cli/             # Command-line interface
│   ├── server/          # REST API server
│   └── gui/             # Qt desktop application
├── test/                # Test suite
│   ├── unit/            # Unit tests
│   ├── integration/     # Integration tests
│   └── benchmarks/      # Performance benchmarks
└── docs/                # Documentation
```

## Technology Stack

| Component | Technology | Purpose |
|-----------|------------|---------|
| Language | C++17 | Performance, cross-platform |
| Build | CMake | Cross-platform builds |
| Optimization | HiGHS | LP/MILP/QP solving |
| Linear Algebra | Eigen | Matrix operations |
| Database | SQLite | Local persistence |
| JSON | nlohmann/json | Data serialization |
| HTTP | cpp-httplib | API client/server |
| CLI | CLI11 | Command-line parsing |
| Testing | Google Test | Unit testing |

## Configuration

Create `~/.lumen/config.yaml`:

```yaml
solver:
  classical:
    default: highs
    timeout_ms: 30000
  quantum:
    enabled: false
    providers:
      - name: dwave
        api_key_env: DWAVE_API_KEY

market_data:
  providers:
    - name: alpha_vantage
      api_key_env: ALPHA_VANTAGE_API_KEY
  cache_ttl_minutes: 60

persistence:
  database_path: ~/.lumen/data/lumen.db
```

## Environment Variables

```bash
# Market data
export ALPHA_VANTAGE_API_KEY=your_key_here

# Quantum providers (optional)
export DWAVE_API_KEY=your_dwave_key
export IBM_QUANTUM_API_KEY=your_ibm_key
```

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build test suite |
| `BUILD_BENCHMARKS` | OFF | Build performance benchmarks |
| `BUILD_CLI` | ON | Build command-line interface |
| `BUILD_SERVER` | ON | Build REST API server |
| `BUILD_GUI` | OFF | Build Qt desktop application |
| `ENABLE_QUANTUM` | ON | Enable quantum solver support |
| `ENABLE_PYFLARE` | ON | Enable telemetry integration |

Example:

```bash
cmake -B build \
  -DBUILD_GUI=ON \
  -DENABLE_QUANTUM=OFF \
  -DCMAKE_BUILD_TYPE=Release
```

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```bash
# Clone with submodules
git clone --recursive https://github.com/oaqlabs/lumen.git
cd lumen/lumen

# Install development dependencies
vcpkg install

# Build with tests
cmake -B build -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run tests
cd build && ctest --output-on-failure

# Format code
find include src apps test -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
```

## Roadmap

- [x] Phase 0: Project setup
- [ ] Phase 1: Core infrastructure (Weeks 1-4)
- [ ] Phase 2: MVP portfolio optimizer (Weeks 5-8)
- [ ] Phase 3: Tax optimization (Weeks 9-12)
- [ ] Phase 4: Quantum integration (Weeks 13-16)
- [ ] Phase 5: Desktop GUI (Weeks 17-20)
- [ ] Phase 6: Public launch (Weeks 21-24)

See [Lumen_Project_Plan.md](../Lumen_Project_Plan.md) for detailed implementation plan.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Disclaimer

Lumen is a decision support tool for portfolio analysis. It does not provide investment advice. Always consult with a qualified financial advisor before making investment decisions.

## Contact

- **Organization:** OA Quantum Labs
- **Project Lead:** Danny
- **Email:** dwall@oaqlabs.com
- **GitHub:** https://github.com/oaqlabs/lumen

---

Built with quantum in mind, powered by classical performance.
