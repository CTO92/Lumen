# Contributing to Lumen

Thank you for your interest in contributing to Lumen! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Style](#code-style)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Issue Guidelines](#issue-guidelines)

## Code of Conduct

By participating in this project, you agree to maintain a respectful and inclusive environment. We expect all contributors to:

- Be respectful and constructive in discussions
- Welcome newcomers and help them get started
- Focus on what is best for the community and project
- Accept constructive criticism gracefully

## Getting Started

### Prerequisites

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.16+
- vcpkg package manager
- Git

### Development Setup

1. **Fork and clone the repository**

   ```bash
   git clone https://github.com/YOUR_USERNAME/lumen.git
   cd lumen/lumen
   ```

2. **Install dependencies**

   ```bash
   # Using vcpkg
   vcpkg install
   ```

3. **Build with debug flags**

   ```bash
   cmake -B build \
     -DCMAKE_BUILD_TYPE=Debug \
     -DBUILD_TESTS=ON \
     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake

   cmake --build build
   ```

4. **Run tests**

   ```bash
   cd build && ctest --output-on-failure
   ```

## Development Workflow

### Branch Naming

- `feature/description` - New features
- `fix/description` - Bug fixes
- `refactor/description` - Code refactoring
- `docs/description` - Documentation changes
- `test/description` - Test additions/changes

### Commit Messages

Follow conventional commit format:

```
type(scope): brief description

Longer description if needed.

Fixes #123
```

Types:
- `feat` - New feature
- `fix` - Bug fix
- `docs` - Documentation
- `style` - Formatting, no code change
- `refactor` - Code restructuring
- `test` - Adding tests
- `chore` - Maintenance tasks

Examples:
```
feat(core): add position limit constraint

Implements PositionLimitConstraint class that enforces maximum
allocation percentage per position.

Closes #42
```

```
fix(solver): handle infeasible problem gracefully

Return proper error status instead of throwing exception when
HiGHS reports infeasibility.
```

## Code Style

### Formatting

We use clang-format with the project's `.clang-format` configuration:

```bash
# Format all files
find include src apps test -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Check formatting (CI does this)
find include src apps test -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror
```

### Naming Conventions

```cpp
// Classes: PascalCase
class PortfolioManager {

// Functions: camelCase
void calculateAllocation();

// Variables: snake_case
double total_value;

// Member variables: snake_case with trailing underscore
double total_value_;

// Constants: SCREAMING_SNAKE_CASE
constexpr int MAX_POSITIONS = 100;

// Namespaces: lowercase
namespace lumen::core {

// Enums: PascalCase with PascalCase values
enum class AssetClass {
    Stocks,
    Bonds,
    Cash
};
```

### Header Files

```cpp
#pragma once

#include <system_headers>

#include <third_party_headers>

#include "lumen/project_headers.hpp"

namespace lumen::module {

// Forward declarations first
class ForwardDeclared;

// Then class definition
class MyClass {
public:
    // Public interface

private:
    // Private members
};

}  // namespace lumen::module
```

### Documentation

- All public APIs must have documentation comments
- Use `///` for single-line doc comments, `/** */` for multi-line
- Document parameters, return values, and exceptions

```cpp
/// Calculate the allocation percentage for a given ticker.
///
/// @param ticker The security symbol to look up
/// @return Allocation as a decimal (0.0 to 1.0)
/// @throws std::out_of_range if ticker not found in portfolio
double getAllocationPercent(const std::string& ticker) const;
```

## Testing

### Test Requirements

- All new features must include unit tests
- Bug fixes should include regression tests
- Maintain >80% code coverage for new code

### Writing Tests

```cpp
#include <gtest/gtest.h>

#include "lumen/core/portfolio.hpp"

namespace lumen::core::test {

class PortfolioTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }

    Portfolio portfolio_;
};

TEST_F(PortfolioTest, AddPositionIncreasesCount) {
    EXPECT_EQ(portfolio_.getPositionCount(), 0);

    portfolio_.addPosition("AAPL", 100, 150.0);

    EXPECT_EQ(portfolio_.getPositionCount(), 1);
}

TEST_F(PortfolioTest, GetTotalValueCalculatesCorrectly) {
    portfolio_.addPosition("AAPL", 10, 150.0);
    portfolio_.updatePrice("AAPL", 160.0);

    EXPECT_DOUBLE_EQ(portfolio_.getTotalValue(), 1600.0);
}

}  // namespace lumen::core::test
```

### Running Tests

```bash
# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./build/lumen-tests --gtest_filter="PortfolioTest.*"

# Run with verbose output
./build/lumen-tests --gtest_filter="*" --gtest_print_time=1
```

## Submitting Changes

### Pull Request Process

1. **Create a feature branch**
   ```bash
   git checkout -b feature/my-feature
   ```

2. **Make your changes**
   - Write code and tests
   - Ensure all tests pass
   - Format code with clang-format

3. **Commit your changes**
   ```bash
   git add .
   git commit -m "feat(module): description"
   ```

4. **Push and create PR**
   ```bash
   git push origin feature/my-feature
   ```

5. **Fill out the PR template**
   - Describe what changes were made
   - Link related issues
   - Note any breaking changes

### PR Checklist

- [ ] Code follows project style guidelines
- [ ] All tests pass locally
- [ ] New code has appropriate test coverage
- [ ] Documentation updated if needed
- [ ] Commit messages follow convention
- [ ] PR description is complete

### Code Review

- All PRs require at least one approval
- Address reviewer feedback promptly
- Keep discussions focused and constructive
- Squash commits before merging if requested

## Issue Guidelines

### Bug Reports

Include:
- Clear description of the bug
- Steps to reproduce
- Expected vs actual behavior
- System information (OS, compiler, version)
- Relevant logs or error messages

### Feature Requests

Include:
- Clear description of the feature
- Use case / motivation
- Proposed solution (if any)
- Alternatives considered

### Labels

- `bug` - Something isn't working
- `enhancement` - New feature request
- `documentation` - Documentation improvements
- `good first issue` - Good for newcomers
- `help wanted` - Extra attention needed
- `question` - Further information requested

## Architecture Guidelines

When making significant changes, consider:

1. **Modularity** - Keep modules loosely coupled
2. **Testability** - Design for easy testing
3. **Performance** - Profile before optimizing
4. **Compatibility** - Maintain backward compatibility
5. **Documentation** - Update architecture docs

See [Lumen_Detailed_Architecture.md](../Lumen_Detailed_Architecture.md) for detailed design specifications.

## Questions?

- Open a GitHub issue with the `question` label
- Email: dwall@oaqlabs.com

Thank you for contributing to Lumen!
