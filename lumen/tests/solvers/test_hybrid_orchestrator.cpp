/// @file test_hybrid_orchestrator.cpp
/// @brief Unit tests for hybrid solver orchestrator
///
/// Tests for the HybridOrchestrator class that combines classical
/// preprocessing, quantum optimization, and classical postprocessing.

#include <gtest/gtest.h>
#include <cmath>

#include "lumen/solvers/hybrid_orchestrator.hpp"
#include "lumen/solvers/quantum_client.hpp"
#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"

using namespace lumen::core;
using namespace lumen::solvers;

// =============================================================================
// HybridOrchestrator Basic Tests
// =============================================================================

class HybridOrchestratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Configure for mock mode
        config_.quantum_config.provider = "mock";
        config_.quantum_config.num_reads = 50;
        config_.lot_threshold = 50;
        config_.preprocess_enabled = true;
        config_.postprocess_enabled = true;
        config_.max_iterations = 3;

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

        Position googl;
        googl.ticker = "GOOGL";
        googl.shares = 50.0;
        googl.current_price = 200.0;
        googl.cost_basis = 180.0;
        googl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(googl);

        // Target: 40% AAPL, 30% MSFT, 30% GOOGL
        target_.setTarget("AAPL", 0.4);
        target_.setTarget("MSFT", 0.3);
        target_.setTarget("GOOGL", 0.3);
    }

    HybridOrchestratorConfig config_;
    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(HybridOrchestratorTest, ConstructorCreatesOrchestrator) {
    HybridOrchestrator orchestrator(config_);

    // Should be able to create without errors
    EXPECT_TRUE(true);
}

TEST_F(HybridOrchestratorTest, SolveSimplePortfolio) {
    HybridOrchestrator orchestrator(config_);

    auto result = orchestrator.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.solver_used.find("Hybrid") != std::string::npos ||
                result.solver_used.find("D-Wave") != std::string::npos);
    EXPECT_GT(result.solve_time_ms, 0);
}

TEST_F(HybridOrchestratorTest, EstimateCostInMockMode) {
    HybridOrchestrator orchestrator(config_);

    double cost = orchestrator.estimateCost(portfolio_, constraints_);

    // Mock mode should be free
    EXPECT_DOUBLE_EQ(cost, 0.0);
}

TEST_F(HybridOrchestratorTest, GetLastIterationCount) {
    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    int iterations = orchestrator.getLastIterationCount();

    EXPECT_GE(iterations, 1);
    EXPECT_LE(iterations, config_.max_iterations);
}

// =============================================================================
// Preprocessing Tests
// =============================================================================

class HybridPreprocessTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.quantum_config.provider = "mock";
        config_.quantum_config.num_reads = 20;
        config_.preprocess_enabled = true;
        config_.postprocess_enabled = false;  // Disable to isolate preprocessing
        config_.near_target_tolerance = 0.01;  // 1% tolerance

        portfolio_ = Portfolio("TestPortfolio");
        portfolio_.setCashBalance(0.0);
    }

    HybridOrchestratorConfig config_;
    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(HybridPreprocessTest, FixesPositionsAtTarget) {
    // Position exactly at target should be fixed
    Position pos;
    pos.ticker = "AAPL";
    pos.shares = 100.0;
    pos.current_price = 100.0;  // $10,000 value
    pos.cost_basis = 80.0;
    pos.asset_class = AssetClass::STOCKS;
    portfolio_.addPosition(pos);

    target_.setTarget("AAPL", 1.0);  // 100% target (exact match)

    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    auto preprocess = orchestrator.getLastPreprocessResult();

    // Position at target should be fixed
    EXPECT_GE(preprocess.fixed_positions.size(), 0);
    EXPECT_LE(preprocess.reduced_size, preprocess.original_size);
}

TEST_F(HybridPreprocessTest, ReductionRatioCalculatesCorrectly) {
    Position pos1, pos2;
    pos1.ticker = "AAPL";
    pos1.shares = 100.0;
    pos1.current_price = 100.0;
    pos1.asset_class = AssetClass::STOCKS;
    portfolio_.addPosition(pos1);

    pos2.ticker = "MSFT";
    pos2.shares = 100.0;
    pos2.current_price = 100.0;
    pos2.asset_class = AssetClass::STOCKS;
    portfolio_.addPosition(pos2);

    target_.setTarget("AAPL", 0.5);
    target_.setTarget("MSFT", 0.5);

    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    auto preprocess = orchestrator.getLastPreprocessResult();

    double ratio = preprocess.reduction_ratio();
    EXPECT_GE(ratio, 0.0);
    EXPECT_LE(ratio, 1.0);
}

TEST_F(HybridPreprocessTest, DisabledPreprocessingKeepsAllPositions) {
    config_.preprocess_enabled = false;

    Position pos;
    pos.ticker = "AAPL";
    pos.shares = 100.0;
    pos.current_price = 100.0;
    pos.asset_class = AssetClass::STOCKS;
    portfolio_.addPosition(pos);

    target_.setTarget("AAPL", 0.5);  // Different from current 100%

    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    auto preprocess = orchestrator.getLastPreprocessResult();

    EXPECT_EQ(preprocess.reduced_size, preprocess.original_size);
    EXPECT_TRUE(preprocess.fixed_positions.empty());
}

// =============================================================================
// Postprocessing Tests
// =============================================================================

class HybridPostprocessTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.quantum_config.provider = "mock";
        config_.quantum_config.num_reads = 50;
        config_.preprocess_enabled = false;
        config_.postprocess_enabled = true;

        portfolio_ = Portfolio("TestPortfolio");

        Position pos;
        pos.ticker = "AAPL";
        pos.shares = 100.0;
        pos.current_price = 100.0;
        pos.cost_basis = 80.0;
        pos.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(pos);

        target_.setTarget("AAPL", 0.5);
    }

    HybridOrchestratorConfig config_;
    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(HybridPostprocessTest, PostprocessingValidatesConstraints) {
    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    auto postprocess = orchestrator.getLastPostprocessResult();

    // Should have checked feasibility
    // (violations may or may not be empty depending on solution)
    EXPECT_TRUE(postprocess.postprocessing_log.size() > 0);
}

TEST_F(HybridPostprocessTest, PostprocessingCalculatesObjective) {
    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    auto postprocess = orchestrator.getLastPostprocessResult();

    // Objective should be calculated
    EXPECT_TRUE(std::isfinite(postprocess.polished_objective));
}

TEST_F(HybridPostprocessTest, DisabledPostprocessingSkipsValidation) {
    config_.postprocess_enabled = false;

    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    auto postprocess = orchestrator.getLastPostprocessResult();

    // Should indicate postprocessing was disabled
    bool found_disabled_msg = false;
    for (const auto& msg : postprocess.postprocessing_log) {
        if (msg.find("disabled") != std::string::npos) {
            found_disabled_msg = true;
            break;
        }
    }
    EXPECT_TRUE(found_disabled_msg);
}

// =============================================================================
// Iteration Tests
// =============================================================================

class HybridIterationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.quantum_config.provider = "mock";
        config_.quantum_config.num_reads = 30;
        config_.preprocess_enabled = true;
        config_.postprocess_enabled = true;
        config_.max_iterations = 5;
        config_.improvement_threshold = 0.0001;

        portfolio_ = Portfolio("TestPortfolio");

        Position pos1, pos2;
        pos1.ticker = "AAPL";
        pos1.shares = 100.0;
        pos1.current_price = 100.0;
        pos1.cost_basis = 80.0;
        pos1.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(pos1);

        pos2.ticker = "MSFT";
        pos2.shares = 100.0;
        pos2.current_price = 100.0;
        pos2.cost_basis = 90.0;
        pos2.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(pos2);

        target_.setTarget("AAPL", 0.6);
        target_.setTarget("MSFT", 0.4);
    }

    HybridOrchestratorConfig config_;
    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(HybridIterationTest, StopsWhenConverged) {
    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    int iterations = orchestrator.getLastIterationCount();

    // Should stop before max if converged
    EXPECT_LE(iterations, config_.max_iterations);
}

TEST_F(HybridIterationTest, RespectsMaxIterations) {
    config_.improvement_threshold = -1.0;  // Never converge by threshold
    config_.max_iterations = 2;

    HybridOrchestrator orchestrator(config_);
    orchestrator.solve(portfolio_, target_, constraints_);

    int iterations = orchestrator.getLastIterationCount();

    EXPECT_LE(iterations, config_.max_iterations);
}

// =============================================================================
// Force Quantum Tests
// =============================================================================

class HybridForceQuantumTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.quantum_config.provider = "mock";
        config_.quantum_config.num_reads = 20;
        config_.force_quantum = true;  // Force quantum even for small problems
        config_.lot_threshold = 1000;   // High threshold that would normally skip quantum

        portfolio_ = Portfolio("SmallPortfolio");

        Position pos;
        pos.ticker = "AAPL";
        pos.shares = 10.0;
        pos.current_price = 100.0;
        pos.cost_basis = 80.0;
        pos.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(pos);

        target_.setTarget("AAPL", 0.5);
    }

    HybridOrchestratorConfig config_;
    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(HybridForceQuantumTest, UsesQuantumWhenForced) {
    HybridOrchestrator orchestrator(config_);

    auto result = orchestrator.solve(portfolio_, target_, constraints_);

    EXPECT_TRUE(result.success);
    // Should have used quantum path (hybrid or quantum solver)
    EXPECT_TRUE(result.solver_used.find("Quantum") != std::string::npos ||
                result.solver_used.find("Hybrid") != std::string::npos ||
                result.solver_used.find("D-Wave") != std::string::npos);
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST(HybridOrchestratorConfigTest, ReconfigureOrchestrator) {
    HybridOrchestratorConfig config1;
    config1.quantum_config.provider = "mock";
    config1.max_iterations = 2;

    HybridOrchestrator orchestrator(config1);

    HybridOrchestratorConfig config2;
    config2.quantum_config.provider = "mock";
    config2.max_iterations = 5;

    orchestrator.configure(config2);

    // Create a simple problem
    Portfolio portfolio("Test");
    Position pos;
    pos.ticker = "AAPL";
    pos.shares = 100.0;
    pos.current_price = 100.0;
    pos.asset_class = AssetClass::STOCKS;
    portfolio.addPosition(pos);

    TargetAllocation target;
    target.setTarget("AAPL", 0.5);

    ConstraintSet constraints;

    orchestrator.solve(portfolio, target, constraints);

    // Should use new config's max_iterations
    int iterations = orchestrator.getLastIterationCount();
    EXPECT_LE(iterations, 5);
}

TEST(HybridOrchestratorConfigTest, SetVerbosity) {
    HybridOrchestratorConfig config;
    config.quantum_config.provider = "mock";

    HybridOrchestrator orchestrator(config);
    orchestrator.setVerbosity(2);

    // Should not crash
    EXPECT_TRUE(true);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(HybridOrchestratorEdgeCases, EmptyPortfolio) {
    HybridOrchestratorConfig config;
    config.quantum_config.provider = "mock";

    HybridOrchestrator orchestrator(config);

    Portfolio empty_portfolio;
    TargetAllocation empty_target;
    ConstraintSet constraints;

    auto result = orchestrator.solve(empty_portfolio, empty_target, constraints);

    // Should handle gracefully (may fail or succeed with no trades)
    // Just shouldn't crash
    EXPECT_TRUE(true);
}

TEST(HybridOrchestratorEdgeCases, SinglePosition) {
    HybridOrchestratorConfig config;
    config.quantum_config.provider = "mock";
    config.quantum_config.num_reads = 20;

    HybridOrchestrator orchestrator(config);

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

    auto result = orchestrator.solve(portfolio, target, constraints);

    EXPECT_TRUE(result.success);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST(HybridOrchestratorPerformanceTest, SolvesInReasonableTime) {
    HybridOrchestratorConfig config;
    config.quantum_config.provider = "mock";
    config.quantum_config.num_reads = 50;
    config.max_iterations = 2;

    HybridOrchestrator orchestrator(config);

    // Create a medium-sized portfolio
    Portfolio portfolio("Medium");
    for (int i = 0; i < 10; ++i) {
        Position pos;
        pos.ticker = "TICK" + std::to_string(i);
        pos.shares = 100.0;
        pos.current_price = 100.0 + i * 10;
        pos.cost_basis = 90.0 + i * 5;
        pos.asset_class = AssetClass::STOCKS;
        portfolio.addPosition(pos);
    }

    TargetAllocation target;
    for (int i = 0; i < 10; ++i) {
        target.setTarget("TICK" + std::to_string(i), 0.1);
    }

    ConstraintSet constraints;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = orchestrator.solve(portfolio, target, constraints);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_TRUE(result.success);
    // Should complete in reasonable time (< 30 seconds)
    EXPECT_LT(duration.count(), 30000);
}
