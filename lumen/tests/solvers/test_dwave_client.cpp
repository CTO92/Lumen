/// @file test_dwave_client.cpp
/// @brief Unit tests for D-Wave client and mock quantum solver
///
/// Tests for the DWaveClient class including mock mode for testing
/// without actual D-Wave API access.

#include <gtest/gtest.h>
#include <cmath>

#include "lumen/solvers/quantum_client.hpp"
#include "lumen/solvers/qubo_types.hpp"
#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"

using namespace lumen::core;
using namespace lumen::solvers;

// =============================================================================
// Mock Mode Tests
// =============================================================================

class DWaveClientMockTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure for mock mode
        config_.provider = "mock";
        config_.num_reads = 100;  // Fewer reads for faster tests
        config_.timeout_ms = 5000;
        config_.use_hybrid = true;
    }

    QuantumSolverConfig config_;
};

TEST_F(DWaveClientMockTest, ConstructorInMockMode) {
    DWaveClient client(config_);

    EXPECT_TRUE(client.authenticate());
    EXPECT_TRUE(client.isAvailable());
}

TEST_F(DWaveClientMockTest, GetAvailableSolversInMockMode) {
    DWaveClient client(config_);
    client.authenticate();

    auto solvers = client.getAvailableSolvers();

    EXPECT_EQ(solvers.size(), 1);
    EXPECT_EQ(solvers[0], "mock_simulated_annealing");
}

TEST_F(DWaveClientMockTest, EstimateCostInMockMode) {
    DWaveClient client(config_);

    // Mock mode should be free
    EXPECT_DOUBLE_EQ(client.estimateCost(100), 0.0);
    EXPECT_DOUBLE_EQ(client.estimateCost(1000), 0.0);
}

TEST_F(DWaveClientMockTest, SubmitSimpleQUBO) {
    DWaveClient client(config_);
    client.authenticate();

    // Simple 2-variable QUBO: minimize x0 + x1 - 2*x0*x1
    // Optimal solution: x0 = x1 = 1 (energy = 0)
    std::vector<std::vector<double>> Q = {
        {1.0, -2.0},
        {0.0, 1.0}
    };

    auto result = client.submitQUBO(Q, 100);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status, "MOCK_COMPLETED");
    EXPECT_EQ(result.best_sample.size(), 2);
    EXPECT_EQ(result.all_samples.size(), 100);
    EXPECT_EQ(result.all_energies.size(), 100);

    // The optimal solution should have low energy
    EXPECT_LE(result.best_energy, 1.0);
}

TEST_F(DWaveClientMockTest, SubmitLargerQUBO) {
    DWaveClient client(config_);
    client.authenticate();

    // 10-variable random QUBO
    int n = 10;
    std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));

    // Fill with some values
    for (int i = 0; i < n; ++i) {
        Q[i][i] = static_cast<double>(i) - 5.0;  // Some negative, some positive
        for (int j = i + 1; j < n; ++j) {
            Q[i][j] = ((i + j) % 3 == 0) ? 1.0 : -1.0;
        }
    }

    auto result = client.submitQUBO(Q, 50);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.best_sample.size(), 10);

    // Best energy should be one of the sample energies
    bool found_best = false;
    for (double e : result.all_energies) {
        if (std::abs(e - result.best_energy) < 1e-10) {
            found_best = true;
            break;
        }
    }
    EXPECT_TRUE(found_best);
}

TEST_F(DWaveClientMockTest, SubmitEmptyQUBO) {
    DWaveClient client(config_);
    client.authenticate();

    std::vector<std::vector<double>> Q;  // Empty

    auto result = client.submitQUBO(Q, 10);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status, "EMPTY_QUBO");
}

TEST_F(DWaveClientMockTest, SubmitInvalidQUBO) {
    DWaveClient client(config_);
    client.authenticate();

    // Non-square matrix
    std::vector<std::vector<double>> Q = {
        {1.0, 2.0, 3.0},
        {0.0, 1.0}  // Wrong size
    };

    auto result = client.submitQUBO(Q, 10);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status, "INVALID_QUBO_DIMENSIONS");
}

TEST_F(DWaveClientMockTest, SetNumReads) {
    DWaveClient client(config_);
    client.authenticate();

    client.setNumReads(50);

    std::vector<std::vector<double>> Q = {{1.0}};
    auto result = client.submitQUBO(Q, 0);  // Use default num_reads

    EXPECT_EQ(result.all_samples.size(), 50);
}

// =============================================================================
// Solve with Portfolio Tests
// =============================================================================

class DWaveClientSolveTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.provider = "mock";
        config_.num_reads = 50;

        // Create a simple portfolio
        portfolio_ = Portfolio("TestPortfolio");

        Position aapl;
        aapl.ticker = "AAPL";
        aapl.shares = 100.0;
        aapl.current_price = 100.0;
        aapl.cost_basis = 80.0;
        aapl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(aapl);

        Position msft;
        msft.ticker = "MSFT";
        msft.shares = 100.0;
        msft.current_price = 100.0;
        msft.cost_basis = 90.0;
        msft.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(msft);

        // Target: 60% AAPL, 40% MSFT
        target_.setTarget("AAPL", 0.6);
        target_.setTarget("MSFT", 0.4);
    }

    QuantumSolverConfig config_;
    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(DWaveClientSolveTest, SolveSimplePortfolio) {
    DWaveClient client(config_);
    client.authenticate();

    auto result = client.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status, "optimal");
    EXPECT_TRUE(result.solver_used.find("D-Wave") != std::string::npos);
    EXPECT_GT(result.solve_time_ms, 0);
}

TEST_F(DWaveClientSolveTest, SolveReturnsReasonableObjective) {
    DWaveClient client(config_);
    client.authenticate();

    auto result = client.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);

    // Objective value should be finite
    EXPECT_TRUE(std::isfinite(result.objective_value));
}

// =============================================================================
// Cost Estimation Tests (Non-Mock Mode)
// =============================================================================

class DWaveClientCostTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Non-mock configuration (but without real API key)
        config_.provider = "dwave";
        config_.use_hybrid = true;
    }

    QuantumSolverConfig config_;
};

TEST_F(DWaveClientCostTest, EstimateCostHybridSolver) {
    DWaveClient client(config_);

    double cost_small = client.estimateCost(100);
    double cost_large = client.estimateCost(10000);

    // Larger problems should cost more
    EXPECT_GT(cost_large, cost_small);

    // Should be in reasonable range (cents to dollars)
    EXPECT_GT(cost_small, 0.0);
    EXPECT_LT(cost_small, 10.0);
}

TEST_F(DWaveClientCostTest, EstimateCostPureQPU) {
    config_.use_hybrid = false;
    DWaveClient client(config_);

    double cost = client.estimateCost(100);

    // Pure QPU is typically cheaper per run but limited
    EXPECT_GT(cost, 0.0);
    EXPECT_LT(cost, 1.0);  // Should be very cheap for small problems
}

// =============================================================================
// Configuration Tests
// =============================================================================

class DWaveClientConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.provider = "mock";
        config_.num_reads = 100;
    }

    QuantumSolverConfig config_;
};

TEST_F(DWaveClientConfigTest, SetTimeout) {
    DWaveClient client(config_);
    client.setTimeout(120000);

    // No direct way to verify, but should not crash
    EXPECT_TRUE(true);
}

TEST_F(DWaveClientConfigTest, SetVerbosity) {
    DWaveClient client(config_);
    client.setVerbosity(2);

    // No direct way to verify, but should not crash
    EXPECT_TRUE(true);
}

TEST_F(DWaveClientConfigTest, UseHybridSolver) {
    DWaveClient client(config_);
    client.useHybridSolver(false);

    // Should still work in mock mode
    client.authenticate();
    EXPECT_TRUE(client.isAvailable());
}

// =============================================================================
// Authentication Tests
// =============================================================================

TEST(DWaveClientAuthTest, AuthenticateWithoutAPIKey) {
    QuantumSolverConfig config;
    config.provider = "dwave";
    config.api_key_env = "NONEXISTENT_API_KEY_VAR_12345";

    DWaveClient client(config);

    // Should fail without API key
    EXPECT_FALSE(client.authenticate());
    EXPECT_FALSE(client.isAvailable());
}

TEST(DWaveClientAuthTest, CheckConnectionWithoutAuth) {
    QuantumSolverConfig config;
    config.provider = "dwave";

    DWaveClient client(config);

    // Should fail without authentication
    EXPECT_FALSE(client.checkConnection());
}

// =============================================================================
// QUBO Energy Verification
// =============================================================================

class QuboEnergyTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.provider = "mock";
        config_.num_reads = 1000;  // More reads for better solutions
    }

    QuantumSolverConfig config_;
};

TEST_F(QuboEnergyTest, FindsOptimalForSimpleProblem) {
    DWaveClient client(config_);
    client.authenticate();

    // Simple problem: minimize -x0 - x1 + 2*x0*x1
    // Optimal: x0=1, x1=0 or x0=0, x1=1 (energy = -1)
    std::vector<std::vector<double>> Q = {
        {-1.0, 2.0},
        {0.0, -1.0}
    };

    auto result = client.submitQUBO(Q, 1000);

    EXPECT_TRUE(result.success);

    // Should find optimal or near-optimal
    EXPECT_LE(result.best_energy, -0.9);  // Allow some tolerance
}

TEST_F(QuboEnergyTest, AllOnesWhenNegativeDiagonal) {
    DWaveClient client(config_);
    client.authenticate();

    // All negative diagonal, no coupling
    // Optimal: all ones
    int n = 5;
    std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        Q[i][i] = -1.0;
    }

    auto result = client.submitQUBO(Q, 500);

    EXPECT_TRUE(result.success);

    // Best solution should be all ones with energy = -5
    EXPECT_NEAR(result.best_energy, -5.0, 0.1);

    int ones_count = 0;
    for (int x : result.best_sample) {
        ones_count += x;
    }
    EXPECT_EQ(ones_count, 5);
}

TEST_F(QuboEnergyTest, AllZerosWhenPositiveDiagonal) {
    DWaveClient client(config_);
    client.authenticate();

    // All positive diagonal, no coupling
    // Optimal: all zeros
    int n = 5;
    std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        Q[i][i] = 1.0;
    }

    auto result = client.submitQUBO(Q, 500);

    EXPECT_TRUE(result.success);

    // Best solution should be all zeros with energy = 0
    EXPECT_NEAR(result.best_energy, 0.0, 0.1);

    int ones_count = 0;
    for (int x : result.best_sample) {
        ones_count += x;
    }
    EXPECT_EQ(ones_count, 0);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST(DWaveClientPerformanceTest, MockModeSolvesQuickly) {
    QuantumSolverConfig config;
    config.provider = "mock";
    config.num_reads = 100;

    DWaveClient client(config);
    client.authenticate();

    // Medium-sized problem
    int n = 50;
    std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));
    for (int i = 0; i < n; ++i) {
        Q[i][i] = static_cast<double>(i - 25);
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto result = client.submitQUBO(Q, 100);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(result.success);
    // Should complete in reasonable time (< 5 seconds for 100 reads)
    EXPECT_LT(duration.count(), 5000);
}
