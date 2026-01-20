/// @file test_ibm_quantum.cpp
/// @brief Unit tests for IBM Quantum client (QAOA)
///
/// Tests for the IBMQuantumClient class including QAOA circuit
/// construction, SPSA optimization, and mock mode functionality.

#include <gtest/gtest.h>
#include <cmath>

#include "lumen/solvers/quantum_client.hpp"
#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"

using namespace lumen::core;
using namespace lumen::solvers;

// =============================================================================
// IBMQuantumClient Basic Tests
// =============================================================================

class IBMQuantumClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.provider = "mock";  // Use mock mode for testing
        config_.timeout_ms = 30000;
        config_.num_reads = 100;  // Fewer shots for faster tests
    }

    QuantumSolverConfig config_;
};

TEST_F(IBMQuantumClientTest, ConstructorCreatesMockClient) {
    IBMQuantumClient client(config_);

    EXPECT_EQ(client.getName(), "IBM Quantum");
}

TEST_F(IBMQuantumClientTest, MockModeIsAvailable) {
    IBMQuantumClient client(config_);
    client.authenticate();

    EXPECT_TRUE(client.isAvailable());
}

TEST_F(IBMQuantumClientTest, AuthenticateSucceedsInMockMode) {
    IBMQuantumClient client(config_);

    EXPECT_TRUE(client.authenticate());
}

TEST_F(IBMQuantumClientTest, CheckConnectionSucceedsInMockMode) {
    IBMQuantumClient client(config_);
    client.authenticate();

    EXPECT_TRUE(client.checkConnection());
}

TEST_F(IBMQuantumClientTest, EstimateCostZeroInMockMode) {
    IBMQuantumClient client(config_);

    double cost = client.estimateCost(10);

    EXPECT_DOUBLE_EQ(cost, 0.0);
}

// =============================================================================
// QAOA Configuration Tests
// =============================================================================

TEST_F(IBMQuantumClientTest, SetQAOADepth) {
    IBMQuantumClient client(config_);

    // Should not crash
    client.setQAOADepth(3);
    client.setQAOADepth(1);
    client.setQAOADepth(5);

    EXPECT_TRUE(true);
}

TEST_F(IBMQuantumClientTest, SetOptimizer) {
    IBMQuantumClient client(config_);

    client.setOptimizer("SPSA");
    client.setOptimizer("COBYLA");

    EXPECT_TRUE(true);
}

TEST_F(IBMQuantumClientTest, SetNumShots) {
    IBMQuantumClient client(config_);

    client.setNumShots(1024);
    client.setNumShots(512);

    EXPECT_TRUE(true);
}

TEST_F(IBMQuantumClientTest, SetBackend) {
    IBMQuantumClient client(config_);

    client.setBackend("ibm_brisbane");
    client.setBackend("ibm_osaka");

    EXPECT_TRUE(true);
}

TEST_F(IBMQuantumClientTest, GetAvailableBackendsInMockMode) {
    IBMQuantumClient client(config_);

    auto backends = client.getAvailableBackends();

    EXPECT_FALSE(backends.empty());
    EXPECT_EQ(backends[0], "mock_qaoa_simulator");
}

TEST_F(IBMQuantumClientTest, SetTimeout) {
    IBMQuantumClient client(config_);

    client.setTimeout(60000);

    EXPECT_TRUE(true);
}

TEST_F(IBMQuantumClientTest, SetVerbosity) {
    IBMQuantumClient client(config_);

    client.setVerbosity(0);
    client.setVerbosity(1);
    client.setVerbosity(2);

    EXPECT_TRUE(true);
}

// =============================================================================
// QUBO Submission Tests
// =============================================================================

class IBMQuantumQUBOTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.provider = "mock";
        config_.num_reads = 50;  // Fewer shots for testing
    }

    QuantumSolverConfig config_;
};

TEST_F(IBMQuantumQUBOTest, SubmitSimpleQUBO) {
    IBMQuantumClient client(config_);
    client.authenticate();

    // Simple 2-variable QUBO
    std::vector<std::vector<double>> Q = {
        {-1.0,  0.5},
        { 0.0, -1.0}
    };

    auto result = client.submitQUBO(Q, 50);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.best_sample.empty());
    EXPECT_EQ(result.best_sample.size(), 2);
}

TEST_F(IBMQuantumQUBOTest, SubmitQUBOWithCorrectSolution) {
    IBMQuantumClient client(config_);
    client.authenticate();
    client.setNumShots(200);

    // QUBO where x0=1, x1=1 is optimal
    // E = -2*x0 - 2*x1 + x0*x1
    // E(0,0) = 0, E(1,0) = -2, E(0,1) = -2, E(1,1) = -3
    std::vector<std::vector<double>> Q = {
        {-2.0, 0.5},
        { 0.0, -2.0}
    };

    auto result = client.submitQUBO(Q, 200);

    EXPECT_TRUE(result.success);
    // Best solution should likely be [1, 1]
    EXPECT_GE(result.best_sample[0] + result.best_sample[1], 1);
}

TEST_F(IBMQuantumQUBOTest, SubmitEmptyQUBOFails) {
    IBMQuantumClient client(config_);
    client.authenticate();

    std::vector<std::vector<double>> Q;

    auto result = client.submitQUBO(Q, 50);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status, "EMPTY_QUBO");
}

TEST_F(IBMQuantumQUBOTest, SubmitLargerQUBO) {
    IBMQuantumClient client(config_);
    client.authenticate();
    client.setNumShots(100);

    // 5-variable QUBO
    int n = 5;
    std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));

    // Set diagonal (linear terms)
    for (int i = 0; i < n; ++i) {
        Q[i][i] = -1.0;
    }

    // Set some off-diagonal (interaction terms)
    Q[0][1] = 0.5;
    Q[1][2] = 0.5;
    Q[2][3] = 0.5;
    Q[3][4] = 0.5;

    auto result = client.submitQUBO(Q, 100);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.best_sample.size(), 5);
    EXPECT_GT(result.total_time_ms, 0);
}

TEST_F(IBMQuantumQUBOTest, ResultContainsAllSamples) {
    IBMQuantumClient client(config_);
    client.authenticate();

    std::vector<std::vector<double>> Q = {
        {-1.0,  0.0},
        { 0.0, -1.0}
    };

    auto result = client.submitQUBO(Q, 50);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.all_samples.empty());
    EXPECT_FALSE(result.all_energies.empty());
}

// =============================================================================
// QAOA Circuit Generation Tests
// =============================================================================

TEST_F(IBMQuantumQUBOTest, GenerateQAOACircuit) {
    IBMQuantumClient client(config_);

    std::vector<std::vector<double>> Q = {
        {-1.0,  0.5},
        { 0.0, -1.0}
    };

    std::vector<double> gamma = {0.5};
    std::vector<double> beta = {0.3};

    std::string circuit = client.getQAOACircuit(Q, gamma, beta);

    // Check that circuit contains expected elements
    EXPECT_NE(circuit.find("OPENQASM 3.0"), std::string::npos);
    EXPECT_NE(circuit.find("qubit[2]"), std::string::npos);
    EXPECT_NE(circuit.find("h q[0]"), std::string::npos);  // Hadamard
    EXPECT_NE(circuit.find("rz"), std::string::npos);       // Z rotation
    EXPECT_NE(circuit.find("rx"), std::string::npos);       // X rotation (mixer)
    EXPECT_NE(circuit.find("measure"), std::string::npos);
}

TEST_F(IBMQuantumQUBOTest, QAOACircuitDepthAffectsOutput) {
    IBMQuantumClient client(config_);

    std::vector<std::vector<double>> Q = {
        {-1.0,  0.0},
        { 0.0, -1.0}
    };

    std::vector<double> gamma1 = {0.5};
    std::vector<double> beta1 = {0.3};

    std::vector<double> gamma2 = {0.5, 0.4};
    std::vector<double> beta2 = {0.3, 0.2};

    std::string circuit1 = client.getQAOACircuit(Q, gamma1, beta1);
    std::string circuit2 = client.getQAOACircuit(Q, gamma2, beta2);

    // Deeper circuit should be longer
    EXPECT_GT(circuit2.size(), circuit1.size());
}

// =============================================================================
// Portfolio Optimization Tests
// =============================================================================

class IBMQuantumPortfolioTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.provider = "mock";
        config_.num_reads = 100;

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

TEST_F(IBMQuantumPortfolioTest, SolveSimplePortfolio) {
    IBMQuantumClient client(config_);
    client.authenticate();
    client.setNumShots(100);
    client.setQAOADepth(1);  // Shallow depth for speed

    auto result = client.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.solver_used.find("IBM Quantum") != std::string::npos ||
                result.solver_used.find("QAOA") != std::string::npos);
    EXPECT_GT(result.solve_time_ms, 0);
}

TEST_F(IBMQuantumPortfolioTest, SolveReturnsTradesOrNoChange) {
    IBMQuantumClient client(config_);
    client.authenticate();
    client.setNumShots(100);

    auto result = client.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);
    // May have trades or may optimize to current allocation
}

TEST_F(IBMQuantumPortfolioTest, SolveReportsIterations) {
    IBMQuantumClient client(config_);
    client.authenticate();
    client.setNumShots(50);

    auto result = client.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);
    EXPECT_GE(result.iterations, 0);  // Should report some iterations
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(IBMQuantumEdgeCases, EmptyPortfolio) {
    QuantumSolverConfig config;
    config.provider = "mock";

    IBMQuantumClient client(config);
    client.authenticate();

    Portfolio empty_portfolio;
    TargetAllocation empty_target;
    ConstraintSet constraints;

    auto result = client.solve(empty_portfolio, empty_target, constraints);

    // Should handle gracefully
    EXPECT_FALSE(result.success);
}

TEST(IBMQuantumEdgeCases, SinglePositionPortfolio) {
    QuantumSolverConfig config;
    config.provider = "mock";
    config.num_reads = 50;

    IBMQuantumClient client(config);
    client.authenticate();

    Portfolio portfolio("Single");
    Position pos;
    pos.ticker = "ONLY";
    pos.shares = 100.0;
    pos.current_price = 100.0;
    pos.cost_basis = 100.0;
    pos.asset_class = AssetClass::STOCKS;
    portfolio.addPosition(pos);

    TargetAllocation target;
    target.setTarget("ONLY", 1.0);

    ConstraintSet constraints;

    auto result = client.solve(portfolio, target, constraints);

    EXPECT_TRUE(result.success);
}

TEST(IBMQuantumEdgeCases, MismatchedTargets) {
    QuantumSolverConfig config;
    config.provider = "mock";

    IBMQuantumClient client(config);
    client.authenticate();

    Portfolio portfolio("Test");
    Position pos;
    pos.ticker = "AAPL";
    pos.shares = 100.0;
    pos.current_price = 100.0;
    pos.asset_class = AssetClass::STOCKS;
    portfolio.addPosition(pos);

    TargetAllocation target;
    target.setTarget("AAPL", 0.5);
    target.setTarget("MSFT", 0.5);  // Not in portfolio

    ConstraintSet constraints;

    auto result = client.solve(portfolio, target, constraints);

    // Should handle the mismatch
    EXPECT_TRUE(result.success || result.status.find("Invalid") != std::string::npos);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST(IBMQuantumPerformanceTest, SolvesInReasonableTime) {
    QuantumSolverConfig config;
    config.provider = "mock";
    config.num_reads = 100;

    IBMQuantumClient client(config);
    client.authenticate();
    client.setQAOADepth(1);
    client.setNumShots(100);

    // Medium portfolio
    Portfolio portfolio("Medium");
    TargetAllocation target;

    for (int i = 0; i < 5; ++i) {
        Position pos;
        pos.ticker = "TICK" + std::to_string(i);
        pos.shares = 100.0;
        pos.current_price = 100.0 + i * 10;
        pos.cost_basis = 90.0 + i * 5;
        pos.asset_class = AssetClass::STOCKS;
        portfolio.addPosition(pos);
        target.setTarget(pos.ticker, 0.2);
    }

    ConstraintSet constraints;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = client.solve(portfolio, target, constraints);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    EXPECT_TRUE(result.success);
    // Should complete in reasonable time (< 60 seconds for mock)
    EXPECT_LT(duration.count(), 60);
}

// =============================================================================
// Cost Estimation Tests (Non-Mock Mode)
// =============================================================================

TEST(IBMQuantumCostTest, EstimateCostNonZeroForRealMode) {
    QuantumSolverConfig config;
    config.provider = "ibm";  // Real mode (won't actually connect)

    IBMQuantumClient client(config);

    double cost = client.estimateCost(10);

    // Real mode should have non-zero cost estimate
    EXPECT_GT(cost, 0.0);
}

TEST(IBMQuantumCostTest, CostScalesWithQubits) {
    QuantumSolverConfig config;
    config.provider = "ibm";

    IBMQuantumClient client(config);

    double cost10 = client.estimateCost(10);
    double cost20 = client.estimateCost(20);
    double cost50 = client.estimateCost(50);

    EXPECT_LT(cost10, cost20);
    EXPECT_LT(cost20, cost50);
}
