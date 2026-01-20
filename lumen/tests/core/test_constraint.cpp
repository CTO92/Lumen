/// @file test_constraint.cpp
/// @brief Unit tests for Constraint classes

#include <gtest/gtest.h>
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"

using namespace lumen::core;

// =============================================================================
// BudgetConstraint Tests
// =============================================================================

class BudgetConstraintTest : public ::testing::Test {
protected:
    void SetUp() override {
        constraint_ = std::make_unique<BudgetConstraint>(100000.0, 5000.0);
    }

    std::unique_ptr<BudgetConstraint> constraint_;
};

TEST_F(BudgetConstraintTest, GetTypeReturnsBudget) {
    EXPECT_EQ(constraint_->getType(), ConstraintType::BUDGET);
}

TEST_F(BudgetConstraintTest, GetNameReturnsCorrectName) {
    EXPECT_EQ(constraint_->getName(), "Budget");
}

TEST_F(BudgetConstraintTest, IsValidReturnsTrueForValidConstraint) {
    EXPECT_TRUE(constraint_->isValid());
}

TEST_F(BudgetConstraintTest, ConstructorThrowsForZeroBudget) {
    EXPECT_THROW(BudgetConstraint(0.0, 0.0), std::invalid_argument);
}

TEST_F(BudgetConstraintTest, ConstructorThrowsForReserveExceedsBudget) {
    EXPECT_THROW(BudgetConstraint(100000.0, 150000.0), std::invalid_argument);
}

TEST_F(BudgetConstraintTest, GettersReturnCorrectValues) {
    EXPECT_DOUBLE_EQ(constraint_->getTotalBudget(), 100000.0);
    EXPECT_DOUBLE_EQ(constraint_->getMinCashReserve(), 5000.0);
}

TEST_F(BudgetConstraintTest, ToJsonWorks) {
    auto json = constraint_->toJSON();
    EXPECT_EQ(json["type"], "budget");
    EXPECT_DOUBLE_EQ(json["total_budget"].get<double>(), 100000.0);
    EXPECT_DOUBLE_EQ(json["min_cash_reserve"].get<double>(), 5000.0);
}

// =============================================================================
// AllocationConstraint Tests
// =============================================================================

class AllocationConstraintTest : public ::testing::Test {
protected:
    void SetUp() override {
        constraint_ = std::make_unique<AllocationConstraint>("AAPL", 0.05, 0.15, false);
    }

    std::unique_ptr<AllocationConstraint> constraint_;
};

TEST_F(AllocationConstraintTest, GetTypeReturnsAllocation) {
    EXPECT_EQ(constraint_->getType(), ConstraintType::ALLOCATION);
}

TEST_F(AllocationConstraintTest, GetNameIncludesIdentifier) {
    EXPECT_EQ(constraint_->getName(), "Allocation[AAPL]");
}

TEST_F(AllocationConstraintTest, GettersReturnCorrectValues) {
    EXPECT_EQ(constraint_->getIdentifier(), "AAPL");
    EXPECT_DOUBLE_EQ(constraint_->getLowerBound(), 0.05);
    EXPECT_DOUBLE_EQ(constraint_->getUpperBound(), 0.15);
    EXPECT_FALSE(constraint_->isAssetClass());
}

TEST_F(AllocationConstraintTest, IsValidReturnsTrueForValidBounds) {
    EXPECT_TRUE(constraint_->isValid());
}

TEST_F(AllocationConstraintTest, ConstructorThrowsForInvertedBounds) {
    EXPECT_THROW(AllocationConstraint("AAPL", 0.20, 0.10, false), std::invalid_argument);
}

TEST_F(AllocationConstraintTest, ConstructorThrowsForNegativeBounds) {
    EXPECT_THROW(AllocationConstraint("AAPL", -0.10, 0.10, false), std::invalid_argument);
}

TEST_F(AllocationConstraintTest, ConstructorThrowsForBoundsOver100Percent) {
    EXPECT_THROW(AllocationConstraint("AAPL", 0.50, 1.50, false), std::invalid_argument);
}

TEST_F(AllocationConstraintTest, ToJsonWorks) {
    auto json = constraint_->toJSON();
    EXPECT_EQ(json["type"], "allocation");
    EXPECT_EQ(json["identifier"], "AAPL");
    EXPECT_DOUBLE_EQ(json["lower_bound"].get<double>(), 0.05);
    EXPECT_DOUBLE_EQ(json["upper_bound"].get<double>(), 0.15);
}

// =============================================================================
// MinTradeSizeConstraint Tests
// =============================================================================

class MinTradeSizeConstraintTest : public ::testing::Test {
protected:
    void SetUp() override {
        constraint_ = std::make_unique<MinTradeSizeConstraint>(100.0, 1.0);
    }

    std::unique_ptr<MinTradeSizeConstraint> constraint_;
};

TEST_F(MinTradeSizeConstraintTest, GetTypeReturnsMinTrade) {
    EXPECT_EQ(constraint_->getType(), ConstraintType::MIN_TRADE);
}

TEST_F(MinTradeSizeConstraintTest, GettersReturnCorrectValues) {
    EXPECT_DOUBLE_EQ(constraint_->getMinAmount(), 100.0);
    EXPECT_DOUBLE_EQ(constraint_->getMinShares(), 1.0);
}

TEST_F(MinTradeSizeConstraintTest, IsValidReturnsTrueForValidValues) {
    EXPECT_TRUE(constraint_->isValid());
}

TEST_F(MinTradeSizeConstraintTest, IsValidReturnsFalseForNegativeAmount) {
    MinTradeSizeConstraint invalid(-100.0, 1.0);
    EXPECT_FALSE(invalid.isValid());
}

TEST_F(MinTradeSizeConstraintTest, IsSatisfiedReturnsTrueForTradesAboveMinimum) {
    Portfolio portfolio;
    std::vector<Trade> trades;

    Trade buy_trade;
    buy_trade.ticker = "AAPL";
    buy_trade.action = Trade::Action::BUY;
    buy_trade.shares = 5.0;
    buy_trade.amount = 500.0;  // Above $100 minimum
    trades.push_back(buy_trade);

    EXPECT_TRUE(constraint_->isSatisfied(portfolio, trades));
}

TEST_F(MinTradeSizeConstraintTest, IsSatisfiedReturnsFalseForTradesBelowMinimum) {
    Portfolio portfolio;
    std::vector<Trade> trades;

    Trade buy_trade;
    buy_trade.ticker = "AAPL";
    buy_trade.action = Trade::Action::BUY;
    buy_trade.shares = 0.5;
    buy_trade.amount = 50.0;  // Below $100 minimum
    trades.push_back(buy_trade);

    EXPECT_FALSE(constraint_->isSatisfied(portfolio, trades));
}

// =============================================================================
// IntegerShareConstraint Tests
// =============================================================================

class IntegerShareConstraintTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::set<std::string> exempt = {"BRK.A", "SPY"};
        constraint_ = std::make_unique<IntegerShareConstraint>(true, exempt);
    }

    std::unique_ptr<IntegerShareConstraint> constraint_;
};

TEST_F(IntegerShareConstraintTest, GetTypeReturnsIntegerShares) {
    EXPECT_EQ(constraint_->getType(), ConstraintType::INTEGER_SHARES);
}

TEST_F(IntegerShareConstraintTest, IsEnforcedReturnsTrue) {
    EXPECT_TRUE(constraint_->isEnforced());
}

TEST_F(IntegerShareConstraintTest, IsExemptReturnsTrueForExemptTicker) {
    EXPECT_TRUE(constraint_->isExempt("BRK.A"));
    EXPECT_TRUE(constraint_->isExempt("SPY"));
}

TEST_F(IntegerShareConstraintTest, IsExemptReturnsFalseForNonExemptTicker) {
    EXPECT_FALSE(constraint_->isExempt("AAPL"));
}

TEST_F(IntegerShareConstraintTest, IsSatisfiedReturnsTrueForWholeShares) {
    Portfolio portfolio;
    std::vector<Trade> trades;

    Trade trade;
    trade.ticker = "AAPL";
    trade.action = Trade::Action::BUY;
    trade.shares = 10.0;  // Whole number
    trades.push_back(trade);

    EXPECT_TRUE(constraint_->isSatisfied(portfolio, trades));
}

TEST_F(IntegerShareConstraintTest, IsSatisfiedReturnsFalseForFractionalShares) {
    Portfolio portfolio;
    std::vector<Trade> trades;

    Trade trade;
    trade.ticker = "AAPL";
    trade.action = Trade::Action::BUY;
    trade.shares = 10.5;  // Fractional
    trades.push_back(trade);

    EXPECT_FALSE(constraint_->isSatisfied(portfolio, trades));
}

TEST_F(IntegerShareConstraintTest, IsSatisfiedReturnsTrueForFractionalExemptTickers) {
    Portfolio portfolio;
    std::vector<Trade> trades;

    Trade trade;
    trade.ticker = "SPY";  // Exempt
    trade.action = Trade::Action::BUY;
    trade.shares = 10.5;  // Fractional but exempt
    trades.push_back(trade);

    EXPECT_TRUE(constraint_->isSatisfied(portfolio, trades));
}

// =============================================================================
// PositionLimitConstraint Tests
// =============================================================================

class PositionLimitConstraintTest : public ::testing::Test {
protected:
    void SetUp() override {
        constraint_ = std::make_unique<PositionLimitConstraint>(0.25);
    }

    std::unique_ptr<PositionLimitConstraint> constraint_;
};

TEST_F(PositionLimitConstraintTest, GetTypeReturnsPositionLimit) {
    EXPECT_EQ(constraint_->getType(), ConstraintType::POSITION_LIMIT);
}

TEST_F(PositionLimitConstraintTest, GetMaxPositionPercentReturnsCorrectValue) {
    EXPECT_DOUBLE_EQ(constraint_->getMaxPositionPercent(), 0.25);
}

TEST_F(PositionLimitConstraintTest, IsValidReturnsTrueForValidPercent) {
    EXPECT_TRUE(constraint_->isValid());
}

TEST_F(PositionLimitConstraintTest, ConstructorThrowsForZeroPercent) {
    EXPECT_THROW(PositionLimitConstraint(0.0), std::invalid_argument);
}

TEST_F(PositionLimitConstraintTest, ConstructorThrowsForOverOneHundredPercent) {
    EXPECT_THROW(PositionLimitConstraint(1.5), std::invalid_argument);
}

// =============================================================================
// ConstraintSet Tests
// =============================================================================

class ConstraintSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        constraint_set_ = std::make_unique<ConstraintSet>();
        constraint_set_->addConstraint(std::make_unique<BudgetConstraint>(100000.0, 5000.0));
        constraint_set_->addConstraint(std::make_unique<AllocationConstraint>("AAPL", 0.05, 0.15, false));
        constraint_set_->addConstraint(std::make_unique<PositionLimitConstraint>(0.25));
    }

    std::unique_ptr<ConstraintSet> constraint_set_;
};

TEST_F(ConstraintSetTest, GetConstraintCountReturnsCorrectCount) {
    EXPECT_EQ(constraint_set_->getConstraintCount(), 3);
}

TEST_F(ConstraintSetTest, GetAllConstraintsWorks) {
    auto constraints = constraint_set_->getAllConstraints();
    EXPECT_EQ(constraints.size(), 3);
}

TEST_F(ConstraintSetTest, GetConstraintsByTypeWorks) {
    auto budget_constraints = constraint_set_->getConstraintsByType(ConstraintType::BUDGET);
    EXPECT_EQ(budget_constraints.size(), 1);

    auto alloc_constraints = constraint_set_->getConstraintsByType(ConstraintType::ALLOCATION);
    EXPECT_EQ(alloc_constraints.size(), 1);
}

TEST_F(ConstraintSetTest, GetConstraintByNameWorks) {
    auto constraint = constraint_set_->getConstraint("Budget");
    ASSERT_NE(constraint, nullptr);
    EXPECT_EQ(constraint->getType(), ConstraintType::BUDGET);
}

TEST_F(ConstraintSetTest, GetConstraintReturnsNullForUnknown) {
    auto constraint = constraint_set_->getConstraint("Unknown");
    EXPECT_EQ(constraint, nullptr);
}

TEST_F(ConstraintSetTest, RemoveConstraintWorks) {
    constraint_set_->removeConstraint("Budget");
    EXPECT_EQ(constraint_set_->getConstraintCount(), 2);
    EXPECT_EQ(constraint_set_->getConstraint("Budget"), nullptr);
}

TEST_F(ConstraintSetTest, ClearAllWorks) {
    constraint_set_->clearAll();
    EXPECT_EQ(constraint_set_->getConstraintCount(), 0);
}

TEST_F(ConstraintSetTest, ToJsonWorks) {
    auto json = constraint_set_->toJSON();
    EXPECT_EQ(json.size(), 3);
}

TEST_F(ConstraintSetTest, FromJsonWorks) {
    auto json = constraint_set_->toJSON();
    auto restored = ConstraintSet::fromJSON(json);

    EXPECT_EQ(restored.getConstraintCount(), 3);
}

TEST_F(ConstraintSetTest, ValidateConsistencyReturnsTrueForValidSet) {
    EXPECT_TRUE(constraint_set_->validateConsistency());
}

TEST_F(ConstraintSetTest, ValidateConsistencyReturnsFalseForConflictingAllocations) {
    // Add conflicting allocation constraints
    constraint_set_->addConstraint(std::make_unique<AllocationConstraint>("AAPL", 0.50, 0.60, false));

    // Both constraints for AAPL (5%-15% and 50%-60%) - no overlap
    auto inconsistencies = constraint_set_->getInconsistencies();
    EXPECT_FALSE(inconsistencies.empty());
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST(ConstraintTypeToStringTest, AllTypesConvertCorrectly) {
    EXPECT_EQ(constraintTypeToString(ConstraintType::BUDGET), "budget");
    EXPECT_EQ(constraintTypeToString(ConstraintType::ALLOCATION), "allocation");
    EXPECT_EQ(constraintTypeToString(ConstraintType::MIN_TRADE), "min_trade");
    EXPECT_EQ(constraintTypeToString(ConstraintType::INTEGER_SHARES), "integer_shares");
    EXPECT_EQ(constraintTypeToString(ConstraintType::TAX_LOT), "tax_lot");
    EXPECT_EQ(constraintTypeToString(ConstraintType::WASH_SALE), "wash_sale");
    EXPECT_EQ(constraintTypeToString(ConstraintType::POSITION_LIMIT), "position_limit");
    EXPECT_EQ(constraintTypeToString(ConstraintType::SECTOR_LIMIT), "sector_limit");
    EXPECT_EQ(constraintTypeToString(ConstraintType::CUSTOM), "custom");
}

TEST(ConstraintStatusToStringTest, AllStatusesConvertCorrectly) {
    EXPECT_EQ(constraintStatusToString(ConstraintStatus::ACTIVE), "active");
    EXPECT_EQ(constraintStatusToString(ConstraintStatus::BINDING), "binding");
    EXPECT_EQ(constraintStatusToString(ConstraintStatus::SLACK), "slack");
    EXPECT_EQ(constraintStatusToString(ConstraintStatus::VIOLATED), "violated");
    EXPECT_EQ(constraintStatusToString(ConstraintStatus::DISABLED), "disabled");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
