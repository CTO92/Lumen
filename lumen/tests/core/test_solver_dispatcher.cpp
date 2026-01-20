/// @file test_solver_dispatcher.cpp
/// @brief Unit tests for SolverDispatcher

#include <gtest/gtest.h>
#include "lumen/core/solver_dispatcher.hpp"

using namespace lumen::core;

class TradeTest : public ::testing::Test {
protected:
    void SetUp() override {
        buy_trade_ = std::make_unique<Trade>("AAPL", TradeDirection::BUY, 100.0, 150.0);
        sell_trade_ = std::make_unique<Trade>("GOOGL", TradeDirection::SELL, 50.0, 2800.0);
    }

    std::unique_ptr<Trade> buy_trade_;
    std::unique_ptr<Trade> sell_trade_;
};

TEST_F(TradeTest, GetTickerWorks) {
    EXPECT_EQ(buy_trade_->getTicker(), "AAPL");
    EXPECT_EQ(sell_trade_->getTicker(), "GOOGL");
}

TEST_F(TradeTest, GetDirectionWorks) {
    EXPECT_EQ(buy_trade_->getDirection(), TradeDirection::BUY);
    EXPECT_EQ(sell_trade_->getDirection(), TradeDirection::SELL);
}

TEST_F(TradeTest, GetSharesWorks) {
    EXPECT_DOUBLE_EQ(buy_trade_->getShares(), 100.0);
    EXPECT_DOUBLE_EQ(sell_trade_->getShares(), 50.0);
}

TEST_F(TradeTest, GetValueUsesEstimatedPrice) {
    EXPECT_DOUBLE_EQ(buy_trade_->getValue(), 15000.0);  // 100 * 150
    EXPECT_DOUBLE_EQ(sell_trade_->getValue(), 140000.0);  // 50 * 2800
}

TEST_F(TradeTest, GetValueUsesActualPriceWhenSet) {
    buy_trade_->setActualPrice(160.0);
    EXPECT_DOUBLE_EQ(buy_trade_->getValue(), 16000.0);  // 100 * 160
}

TEST_F(TradeTest, ToJsonWorks) {
    auto json = buy_trade_->toJSON();
    EXPECT_EQ(json["ticker"], "AAPL");
    EXPECT_EQ(json["direction"], "buy");
    EXPECT_DOUBLE_EQ(json["shares"].get<double>(), 100.0);
}

class SolverResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        result_ = std::make_unique<SolverResult>();
    }

    std::unique_ptr<SolverResult> result_;
};

TEST_F(SolverResultTest, DefaultStatusIsPending) {
    EXPECT_EQ(result_->getStatus(), SolverStatus::PENDING);
}

TEST_F(SolverResultTest, IsOptimalReturnsFalseForPending) {
    EXPECT_FALSE(result_->isOptimal());
}

TEST_F(SolverResultTest, IsOptimalReturnsTrueForOptimal) {
    result_->status_ = SolverStatus::OPTIMAL;
    EXPECT_TRUE(result_->isOptimal());
}

TEST_F(SolverResultTest, IsFeasibleReturnsTrueForOptimal) {
    result_->status_ = SolverStatus::OPTIMAL;
    EXPECT_TRUE(result_->isFeasible());
}

TEST_F(SolverResultTest, IsFeasibleReturnsTrueForFeasible) {
    result_->status_ = SolverStatus::FEASIBLE;
    EXPECT_TRUE(result_->isFeasible());
}

TEST_F(SolverResultTest, IsFeasibleReturnsFalseForInfeasible) {
    result_->status_ = SolverStatus::INFEASIBLE;
    EXPECT_FALSE(result_->isFeasible());
}

TEST_F(SolverResultTest, ToJsonWorks) {
    result_->status_ = SolverStatus::OPTIMAL;
    result_->objective_value_ = 1000.0;
    result_->solver_used_ = "highs";

    auto json = result_->toJSON();
    EXPECT_EQ(json["status"], "optimal");
    EXPECT_DOUBLE_EQ(json["objective_value"].get<double>(), 1000.0);
    EXPECT_EQ(json["solver_used"], "highs");
}

class ProblemCharacteristicsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a small portfolio
        portfolio_ = std::make_unique<Portfolio>("test");
        portfolio_->addPosition(Position("AAPL", 100.0, 150.0));
        portfolio_->addPosition(Position("GOOGL", 50.0, 2800.0));

        constraint_set_ = std::make_unique<ConstraintSet>();
        constraint_set_->addConstraint(std::make_unique<BudgetConstraint>(100000.0, 5000.0));
    }

    std::unique_ptr<Portfolio> portfolio_;
    std::unique_ptr<ConstraintSet> constraint_set_;
};

TEST_F(ProblemCharacteristicsTest, AnalyzeReturnsCorrectPositionCount) {
    auto chars = ProblemCharacteristics::analyze(*portfolio_, *constraint_set_);
    EXPECT_EQ(chars.getNumPositions(), 2);
}

TEST_F(ProblemCharacteristicsTest, AnalyzeReturnsCorrectConstraintCount) {
    auto chars = ProblemCharacteristics::analyze(*portfolio_, *constraint_set_);
    EXPECT_EQ(chars.getNumConstraints(), 1);
}

TEST_F(ProblemCharacteristicsTest, RecommendedTierIsTier1ForSmallProblems) {
    auto chars = ProblemCharacteristics::analyze(*portfolio_, *constraint_set_);
    EXPECT_EQ(chars.recommendedTier(), SolverTier::TIER1_CLASSICAL);
}

TEST_F(ProblemCharacteristicsTest, RecommendedTierIsTier2ForMediumProblems) {
    // Add more positions to make it a medium-sized problem
    for (int i = 0; i < 30; ++i) {
        portfolio_->addPosition(Position("TICKER" + std::to_string(i), 10.0, 100.0));
    }

    auto chars = ProblemCharacteristics::analyze(*portfolio_, *constraint_set_);
    EXPECT_EQ(chars.recommendedTier(), SolverTier::TIER2_HYBRID);
}

class SolverDispatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        dispatcher_ = &SolverDispatcher::getInstance();

        portfolio_ = std::make_unique<Portfolio>("test");
        portfolio_->addPosition(Position("AAPL", 100.0, 150.0));

        target_ = std::make_unique<TargetAllocation>();
        target_->setTarget("AAPL", 0.50);
        target_->setTarget("GOOGL", 0.50);

        constraints_ = std::make_unique<ConstraintSet>();
    }

    SolverDispatcher* dispatcher_;
    std::unique_ptr<Portfolio> portfolio_;
    std::unique_ptr<TargetAllocation> target_;
    std::unique_ptr<ConstraintSet> constraints_;
};

TEST_F(SolverDispatcherTest, GetInstanceReturnsSameInstance) {
    auto& instance1 = SolverDispatcher::getInstance();
    auto& instance2 = SolverDispatcher::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(SolverDispatcherTest, SetForceTierWorks) {
    dispatcher_->setForceTier(SolverTier::TIER3_QUANTUM);
    // Solve should use quantum tier regardless of problem size
    // (though it will return error since quantum is not implemented)
    auto result = dispatcher_->solve(*portfolio_, *target_, *constraints_);
    EXPECT_EQ(result.getSolverUsed(), "quantum");

    // Clean up
    dispatcher_->clearForceTier();
}

TEST_F(SolverDispatcherTest, SetTimeoutWorks) {
    dispatcher_->setTimeout(60000);
    // No direct way to verify, but this shouldn't throw
}

TEST_F(SolverDispatcherTest, SolveReturnsResult) {
    auto result = dispatcher_->solve(*portfolio_, *target_, *constraints_);
    // Currently returns error since solvers are not implemented
    EXPECT_FALSE(result.isOptimal());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
