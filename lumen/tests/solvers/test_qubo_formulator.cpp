/// @file test_qubo_formulator.cpp
/// @brief Unit tests for QUBO formulation
///
/// Tests for the QuboFormulator class that converts portfolio optimization
/// problems into QUBO format for quantum solvers.

#include <gtest/gtest.h>
#include <cmath>

#include "lumen/solvers/quantum_client.hpp"
#include "lumen/solvers/qubo_types.hpp"
#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"

using namespace lumen::core;
using namespace lumen::solvers;

// =============================================================================
// EncodingConfig Tests
// =============================================================================

class EncodingConfigTest : public ::testing::Test {
protected:
    EncodingConfig config_;
};

TEST_F(EncodingConfigTest, DefaultValues) {
    EXPECT_EQ(config_.precision_bits, 8);
    EXPECT_DOUBLE_EQ(config_.min_value, 0.0);
    EXPECT_DOUBLE_EQ(config_.max_value, 1.0);
}

TEST_F(EncodingConfigTest, GetNumLevels) {
    EXPECT_EQ(config_.getNumLevels(), 256);  // 2^8

    config_.precision_bits = 4;
    EXPECT_EQ(config_.getNumLevels(), 16);  // 2^4
}

TEST_F(EncodingConfigTest, GetStepSize) {
    // With 8 bits: 256 levels, step = 1/255 ≈ 0.00392
    double expected_step = 1.0 / 255.0;
    EXPECT_NEAR(config_.getStepSize(), expected_step, 1e-6);
}

TEST_F(EncodingConfigTest, EncodeDecodeRoundTrip) {
    // Test several values
    std::vector<double> test_values = {0.0, 0.25, 0.5, 0.75, 1.0};

    for (double val : test_values) {
        auto binary = config_.encode(val);
        double decoded = config_.decode(binary);

        // Should be within one step size due to discretization
        EXPECT_NEAR(decoded, val, config_.getStepSize() + 1e-6)
            << "Failed for value: " << val;
    }
}

TEST_F(EncodingConfigTest, EncodeClampsBelowMin) {
    auto binary = config_.encode(-0.5);
    double decoded = config_.decode(binary);
    EXPECT_DOUBLE_EQ(decoded, 0.0);
}

TEST_F(EncodingConfigTest, EncodeClampsAboveMax) {
    auto binary = config_.encode(1.5);
    double decoded = config_.decode(binary);
    EXPECT_DOUBLE_EQ(decoded, 1.0);
}

TEST_F(EncodingConfigTest, LowPrecisionEncoding) {
    config_.precision_bits = 2;  // 4 levels: 0, 0.333, 0.667, 1.0

    auto bin0 = config_.encode(0.0);
    auto bin1 = config_.encode(0.5);
    auto bin2 = config_.encode(1.0);

    EXPECT_NEAR(config_.decode(bin0), 0.0, 0.01);
    EXPECT_NEAR(config_.decode(bin1), 0.333, 0.2);  // Will snap to nearest
    EXPECT_NEAR(config_.decode(bin2), 1.0, 0.01);
}

TEST_F(EncodingConfigTest, JsonRoundTrip) {
    config_.precision_bits = 6;
    config_.min_value = 0.1;
    config_.max_value = 0.9;

    auto json = config_.toJSON();
    auto restored = EncodingConfig::fromJSON(json);

    EXPECT_EQ(restored.precision_bits, config_.precision_bits);
    EXPECT_DOUBLE_EQ(restored.min_value, config_.min_value);
    EXPECT_DOUBLE_EQ(restored.max_value, config_.max_value);
}

// =============================================================================
// PenaltyConfig Tests
// =============================================================================

class PenaltyConfigTest : public ::testing::Test {
protected:
    PenaltyConfig config_;
};

TEST_F(PenaltyConfigTest, DefaultValues) {
    EXPECT_DOUBLE_EQ(config_.constraint_penalty, 1000.0);
    EXPECT_TRUE(config_.auto_tune);
    EXPECT_DOUBLE_EQ(config_.tune_factor, 10.0);
}

TEST_F(PenaltyConfigTest, AutoTuneDisabled) {
    config_.auto_tune = false;
    double penalty = config_.computeAutoTunedPenalty(100.0, 5);
    EXPECT_DOUBLE_EQ(penalty, 1000.0);  // Returns base penalty
}

TEST_F(PenaltyConfigTest, AutoTuneEnabled) {
    config_.auto_tune = true;
    config_.tune_factor = 10.0;

    double max_coeff = 100.0;
    int num_constraints = 5;
    double penalty = config_.computeAutoTunedPenalty(max_coeff, num_constraints);

    // Should be max(1000, 100 * 10 * 5) = max(1000, 5000) = 5000
    EXPECT_DOUBLE_EQ(penalty, 5000.0);
}

TEST_F(PenaltyConfigTest, AutoTuneUsesBaseWhenLarger) {
    config_.auto_tune = true;
    config_.constraint_penalty = 10000.0;

    double penalty = config_.computeAutoTunedPenalty(10.0, 2);

    // Should be max(10000, 10 * 10 * 2) = max(10000, 200) = 10000
    EXPECT_DOUBLE_EQ(penalty, 10000.0);
}

TEST_F(PenaltyConfigTest, JsonRoundTrip) {
    config_.constraint_penalty = 500.0;
    config_.auto_tune = false;
    config_.tune_factor = 5.0;

    auto json = config_.toJSON();
    auto restored = PenaltyConfig::fromJSON(json);

    EXPECT_DOUBLE_EQ(restored.constraint_penalty, config_.constraint_penalty);
    EXPECT_EQ(restored.auto_tune, config_.auto_tune);
    EXPECT_DOUBLE_EQ(restored.tune_factor, config_.tune_factor);
}

// =============================================================================
// QuboMatrix Tests
// =============================================================================

class QuboMatrixTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Simple 3-variable QUBO
        qubo_.num_variables = 3;
        qubo_.Q = {{1.0, 2.0, 0.0},
                   {0.0, -1.0, 3.0},
                   {0.0, 0.0, 2.0}};
        qubo_.offset = 5.0;
    }

    QuboMatrix qubo_;
};

TEST_F(QuboMatrixTest, EvaluateAllZeros) {
    std::vector<int> solution = {0, 0, 0};
    double energy = qubo_.evaluate(solution);
    EXPECT_DOUBLE_EQ(energy, 5.0);  // Just the offset
}

TEST_F(QuboMatrixTest, EvaluateAllOnes) {
    std::vector<int> solution = {1, 1, 1};
    // Energy = 5 + Q[0][0]*1 + Q[1][1]*1 + Q[2][2]*1
    //        + Q[0][1]*1*1 + Q[0][2]*1*1 + Q[1][2]*1*1
    //        = 5 + 1 + (-1) + 2 + 2 + 0 + 3 = 12
    double energy = qubo_.evaluate(solution);
    EXPECT_DOUBLE_EQ(energy, 12.0);
}

TEST_F(QuboMatrixTest, EvaluateMixedSolution) {
    std::vector<int> solution = {1, 0, 1};
    // Energy = 5 + Q[0][0]*1 + Q[2][2]*1 + Q[0][2]*1*1
    //        = 5 + 1 + 2 + 0 = 8
    double energy = qubo_.evaluate(solution);
    EXPECT_DOUBLE_EQ(energy, 8.0);
}

TEST_F(QuboMatrixTest, EvaluateThrowsOnWrongSize) {
    std::vector<int> solution = {1, 0};  // Wrong size
    EXPECT_THROW(qubo_.evaluate(solution), std::invalid_argument);
}

TEST_F(QuboMatrixTest, IsValidReturnsTrueForValidMatrix) {
    EXPECT_TRUE(qubo_.isValid());
}

TEST_F(QuboMatrixTest, IsValidReturnsFalseForEmptyMatrix) {
    QuboMatrix empty;
    EXPECT_FALSE(empty.isValid());
}

TEST_F(QuboMatrixTest, IsValidReturnsFalseForMismatchedSize) {
    qubo_.Q.push_back({0.0, 0.0, 0.0});  // Now 4 rows, 3 columns
    EXPECT_FALSE(qubo_.isValid());
}

TEST_F(QuboMatrixTest, GetNonZeroCount) {
    // Non-zeros in upper triangle: (0,0)=1, (0,1)=2, (1,1)=-1, (1,2)=3, (2,2)=2
    EXPECT_EQ(qubo_.getNonZeroCount(), 5);
}

TEST_F(QuboMatrixTest, JsonRoundTrip) {
    qubo_.var_names[0] = "x0";
    qubo_.var_names[1] = "x1";
    qubo_.var_names[2] = "x2";

    auto json = qubo_.toJSON();
    auto restored = QuboMatrix::fromJSON(json);

    EXPECT_EQ(restored.num_variables, qubo_.num_variables);
    EXPECT_DOUBLE_EQ(restored.offset, qubo_.offset);
    EXPECT_EQ(restored.Q, qubo_.Q);
    EXPECT_EQ(restored.var_names, qubo_.var_names);
}

// =============================================================================
// QuboFormulator Basic Tests
// =============================================================================

class QuboFormulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple portfolio with 2 positions
        portfolio_ = Portfolio("TestPortfolio");

        Position aapl;
        aapl.ticker = "AAPL";
        aapl.shares = 100.0;
        aapl.current_price = 150.0;
        aapl.cost_basis = 120.0;
        aapl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(aapl);

        Position googl;
        googl.ticker = "GOOGL";
        googl.shares = 10.0;
        googl.current_price = 2800.0;
        googl.cost_basis = 2500.0;
        googl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(googl);

        // Set up target allocation: 50% each
        target_.setTarget("AAPL", 0.5);
        target_.setTarget("GOOGL", 0.5);
    }

    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(QuboFormulatorTest, ConstructorCreatesValidFormulator) {
    QuboFormulator formulator(portfolio_, target_);
    EXPECT_TRUE(formulator.validateFormulation());
}

TEST_F(QuboFormulatorTest, GetNumQubitsBeforeFormulate) {
    QuboFormulator formulator(portfolio_, target_);
    // Before formulation, should be 0
    EXPECT_EQ(formulator.getNumQubits(), 0);
}

TEST_F(QuboFormulatorTest, FormulateCreatesCorrectSizeMatrix) {
    QuboFormulator formulator(portfolio_, target_);

    auto Q = formulator.formulate(constraints_);

    // 2 positions * 8 bits each = 16 qubits
    EXPECT_EQ(Q.size(), 16);
    for (const auto& row : Q) {
        EXPECT_EQ(row.size(), 16);
    }
}

TEST_F(QuboFormulatorTest, GetNumQubitsAfterFormulate) {
    QuboFormulator formulator(portfolio_, target_);
    formulator.formulate(constraints_);

    EXPECT_EQ(formulator.getNumQubits(), 16);  // 2 positions * 8 bits
}

TEST_F(QuboFormulatorTest, GetVariableMappingReturnsCorrectNames) {
    QuboFormulator formulator(portfolio_, target_);
    formulator.formulate(constraints_);

    auto mapping = formulator.getVariableMapping();

    EXPECT_EQ(mapping.size(), 16);

    // Check some specific mappings
    bool found_aapl = false;
    bool found_googl = false;
    for (const auto& [idx, name] : mapping) {
        if (name.find("AAPL") != std::string::npos) found_aapl = true;
        if (name.find("GOOGL") != std::string::npos) found_googl = true;
    }

    EXPECT_TRUE(found_aapl);
    EXPECT_TRUE(found_googl);
}

TEST_F(QuboFormulatorTest, SetWeightsAffectsFormulation) {
    QuboFormulator formulator1(portfolio_, target_);
    QuboFormulator formulator2(portfolio_, target_);

    formulator1.setWeights(1.0, 0.0, 0.0);  // Only drift
    formulator2.setWeights(0.0, 1.0, 0.0);  // Only cost

    auto Q1 = formulator1.formulate(constraints_);
    auto Q2 = formulator2.formulate(constraints_);

    // Matrices should be different
    bool different = false;
    for (size_t i = 0; i < Q1.size() && !different; ++i) {
        for (size_t j = 0; j < Q1[i].size() && !different; ++j) {
            if (std::abs(Q1[i][j] - Q2[i][j]) > 1e-6) {
                different = true;
            }
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(QuboFormulatorTest, SetConstraintPenaltyAffectsFormulation) {
    QuboFormulator formulator1(portfolio_, target_);
    QuboFormulator formulator2(portfolio_, target_);

    formulator1.setConstraintPenalty(100.0);
    formulator2.setConstraintPenalty(10000.0);

    auto Q1 = formulator1.formulate(constraints_);
    auto Q2 = formulator2.formulate(constraints_);

    // With higher penalty, constraint terms should be larger
    double max1 = 0, max2 = 0;
    for (const auto& row : Q1) {
        for (double val : row) {
            max1 = std::max(max1, std::abs(val));
        }
    }
    for (const auto& row : Q2) {
        for (double val : row) {
            max2 = std::max(max2, std::abs(val));
        }
    }

    EXPECT_GT(max2, max1);
}

// =============================================================================
// QuboFormulator Solution Interpretation Tests
// =============================================================================

class QuboFormulatorInterpretationTest : public ::testing::Test {
protected:
    void SetUp() override {
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

        // Equal portfolio value: $10k each = $20k total
        // Current allocation: 50% each

        target_.setTarget("AAPL", 0.5);
        target_.setTarget("MSFT", 0.5);
    }

    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(QuboFormulatorInterpretationTest, InterpretSolutionReturnsEmptyForEmptySolution) {
    QuboFormulator formulator(portfolio_, target_);
    formulator.formulate(constraints_);

    std::vector<int> empty_solution;
    auto trades = formulator.interpretSolution(empty_solution);

    EXPECT_TRUE(trades.empty());
}

TEST_F(QuboFormulatorInterpretationTest, InterpretSolutionForOptimalAllocation) {
    QuboFormulator formulator(portfolio_, target_);
    formulator.formulate(constraints_);

    // Create a solution that encodes 50% weight for each position
    // With 8-bit encoding, 50% = 127.5 ≈ 128 = 10000000 in binary (bit 7 set)
    std::vector<int> solution(16, 0);

    // For AAPL (bits 0-7): encode ~50%
    // 127 = 0111 1111 -> but we need 128 levels for 50%
    // 128 / 255 ≈ 0.502 which is close to 50%
    solution[7] = 1;  // 128 for first position

    // For MSFT (bits 8-15): encode ~50%
    solution[15] = 1;  // 128 for second position

    auto trades = formulator.interpretSolution(solution);

    // Current allocation is already ~50% each, so trades should be minimal
    // The exact behavior depends on how close the decoded value is to current
    for (const auto& trade : trades) {
        // Any trades should be small adjustments
        EXPECT_LT(trade.amount, portfolio_.getTotalValue() * 0.1);
    }
}

// =============================================================================
// QuboFormulator with Constraints
// =============================================================================

class QuboFormulatorConstraintTest : public ::testing::Test {
protected:
    void SetUp() override {
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

        target_.setTarget("AAPL", 0.6);
        target_.setTarget("MSFT", 0.4);
    }

    Portfolio portfolio_;
    TargetAllocation target_;
    ConstraintSet constraints_;
};

TEST_F(QuboFormulatorConstraintTest, AllocationConstraintsAreEncoded) {
    // Add allocation constraint: AAPL must be between 50% and 70%
    auto alloc_constraint = std::make_unique<AllocationConstraint>(
        "AAPL", 0.50, 0.70, false);
    constraints_.addConstraint(std::move(alloc_constraint));

    QuboFormulator formulator(portfolio_, target_);
    auto Q = formulator.formulate(constraints_);

    // The matrix should have penalty terms that discourage AAPL < 50% or > 70%
    // We verify by checking the matrix is non-trivial
    int non_zero = 0;
    for (const auto& row : Q) {
        for (double val : row) {
            if (std::abs(val) > 1e-10) non_zero++;
        }
    }

    EXPECT_GT(non_zero, 0);
}

TEST_F(QuboFormulatorConstraintTest, BudgetConstraintAlwaysIncluded) {
    // Even with no explicit constraints, budget constraint (sum=1) should be included
    QuboFormulator formulator(portfolio_, target_);
    auto Q = formulator.formulate(constraints_);

    // The budget constraint creates cross-terms between all position bits
    // Check that there are significant off-diagonal terms
    double off_diagonal_sum = 0.0;
    for (size_t i = 0; i < Q.size(); ++i) {
        for (size_t j = i + 1; j < Q[i].size(); ++j) {
            off_diagonal_sum += std::abs(Q[i][j]);
        }
    }

    EXPECT_GT(off_diagonal_sum, 0.0);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST(QuboFormulatorEdgeCases, EmptyPortfolio) {
    Portfolio empty_portfolio;
    TargetAllocation empty_target;
    ConstraintSet constraints;

    QuboFormulator formulator(empty_portfolio, empty_target);

    // Should not crash, but validation should fail
    EXPECT_FALSE(formulator.validateFormulation());
}

TEST(QuboFormulatorEdgeCases, SinglePosition) {
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

    QuboFormulator formulator(portfolio, target);
    EXPECT_TRUE(formulator.validateFormulation());

    auto Q = formulator.formulate(constraints);
    EXPECT_EQ(Q.size(), 8);  // 1 position * 8 bits
}

TEST(QuboFormulatorEdgeCases, TargetNotInPortfolio) {
    Portfolio portfolio("Test");
    Position pos;
    pos.ticker = "AAPL";
    pos.shares = 100.0;
    pos.current_price = 100.0;
    pos.cost_basis = 100.0;
    pos.asset_class = AssetClass::STOCKS;
    portfolio.addPosition(pos);

    TargetAllocation target;
    target.setTarget("AAPL", 0.5);
    target.setTarget("MSFT", 0.5);  // Not in portfolio!

    ConstraintSet constraints;

    QuboFormulator formulator(portfolio, target);
    auto Q = formulator.formulate(constraints);

    // Should include both tickers in the formulation
    EXPECT_EQ(Q.size(), 16);  // 2 positions * 8 bits
}

// =============================================================================
// DecodedSolution Tests
// =============================================================================

TEST(DecodedSolutionTest, IsFeasibleReturnsTrueWhenAllSatisfied) {
    DecodedSolution sol;
    sol.constraints_satisfied["budget"] = true;
    sol.constraints_satisfied["min_aapl"] = true;

    EXPECT_TRUE(sol.is_feasible());
}

TEST(DecodedSolutionTest, IsFeasibleReturnsFalseWhenAnySatisfied) {
    DecodedSolution sol;
    sol.constraints_satisfied["budget"] = true;
    sol.constraints_satisfied["min_aapl"] = false;

    EXPECT_FALSE(sol.is_feasible());
}

TEST(DecodedSolutionTest, IsFeasibleReturnsTrueForEmpty) {
    DecodedSolution sol;
    EXPECT_TRUE(sol.is_feasible());  // Vacuously true
}

TEST(DecodedSolutionTest, JsonRoundTrip) {
    DecodedSolution sol;
    sol.target_weights["AAPL"] = 0.5;
    sol.target_weights["MSFT"] = 0.5;
    sol.constraints_satisfied["budget"] = true;
    sol.total_violation = 0.01;
    sol.objective_value = 123.45;

    auto json = sol.toJSON();

    EXPECT_EQ(json["target_weights"]["AAPL"].get<double>(), 0.5);
    EXPECT_EQ(json["target_weights"]["MSFT"].get<double>(), 0.5);
    EXPECT_TRUE(json["constraints_satisfied"]["budget"].get<bool>());
    EXPECT_DOUBLE_EQ(json["total_violation"].get<double>(), 0.01);
    EXPECT_DOUBLE_EQ(json["objective_value"].get<double>(), 123.45);
}

// =============================================================================
// QuantumResult Tests
// =============================================================================

TEST(QuantumResultTest, JsonRoundTrip) {
    QuantumResult result;
    result.success = true;
    result.status = "COMPLETED";
    result.best_sample = {1, 0, 1, 1};
    result.best_energy = -42.5;
    result.all_samples = {{1, 0, 1, 1}, {0, 1, 0, 1}};
    result.all_energies = {-42.5, -40.0};
    result.qpu_access_time_us = 1234.56;
    result.total_time_ms = 5000.0;
    result.num_occurrences = 50;
    result.chain_break_fraction = 0.02;

    auto json = result.toJSON();
    auto restored = QuantumResult::fromJSON(json);

    EXPECT_EQ(restored.success, result.success);
    EXPECT_EQ(restored.status, result.status);
    EXPECT_EQ(restored.best_sample, result.best_sample);
    EXPECT_DOUBLE_EQ(restored.best_energy, result.best_energy);
    EXPECT_EQ(restored.all_samples, result.all_samples);
    EXPECT_EQ(restored.all_energies, result.all_energies);
    EXPECT_DOUBLE_EQ(restored.qpu_access_time_us, result.qpu_access_time_us);
    EXPECT_DOUBLE_EQ(restored.total_time_ms, result.total_time_ms);
    EXPECT_EQ(restored.num_occurrences, result.num_occurrences);
    EXPECT_DOUBLE_EQ(restored.chain_break_fraction, result.chain_break_fraction);
}

TEST(QuantumResultTest, FromJsonHandlesMissingFields) {
    nlohmann::json j;
    j["success"] = false;

    auto result = QuantumResult::fromJSON(j);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status, "");
    EXPECT_TRUE(result.best_sample.empty());
    EXPECT_DOUBLE_EQ(result.best_energy, 0.0);
}
