/// @file test_portfolio.cpp
/// @brief Unit tests for Portfolio classes

#include <gtest/gtest.h>
#include "lumen/core/portfolio.hpp"

using namespace lumen::core;

// =============================================================================
// Position Tests
// =============================================================================

class PositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        position_.ticker = "AAPL";
        position_.shares = 100.0;
        position_.current_price = 150.0;
        position_.cost_basis = 120.0;
        position_.asset_class = AssetClass::STOCKS;
        position_.supports_fractional = false;
        position_.purchase_date = std::chrono::system_clock::now() -
                                  std::chrono::hours(24 * 400);  // Over 1 year ago
    }

    Position position_;
};

TEST_F(PositionTest, GetCurrentValueCalculatesCorrectly) {
    EXPECT_DOUBLE_EQ(position_.getCurrentValue(), 15000.0);
}

TEST_F(PositionTest, GetCostBasisTotalCalculatesCorrectly) {
    EXPECT_DOUBLE_EQ(position_.getCostBasisTotal(), 12000.0);
}

TEST_F(PositionTest, GetUnrealizedGainCalculatesCorrectly) {
    // Current value: 15000, Cost basis: 12000, Gain: 3000
    EXPECT_DOUBLE_EQ(position_.getUnrealizedGain(), 3000.0);
}

TEST_F(PositionTest, GetUnrealizedGainPercentCalculatesCorrectly) {
    // Gain: 3000, Basis: 12000, Percent: 25%
    EXPECT_NEAR(position_.getUnrealizedGainPercent(), 25.0, 0.01);
}

TEST_F(PositionTest, IsLongTermReturnsTrueAfterOneYear) {
    EXPECT_TRUE(position_.isLongTerm());
}

TEST_F(PositionTest, IsLongTermReturnsFalseBeforeOneYear) {
    position_.purchase_date = std::chrono::system_clock::now() -
                              std::chrono::hours(24 * 100);  // 100 days ago
    EXPECT_FALSE(position_.isLongTerm());
}

TEST_F(PositionTest, ToJsonWorks) {
    auto json = position_.toJSON();
    EXPECT_EQ(json["ticker"], "AAPL");
    EXPECT_DOUBLE_EQ(json["shares"].get<double>(), 100.0);
    EXPECT_DOUBLE_EQ(json["current_price"].get<double>(), 150.0);
    EXPECT_DOUBLE_EQ(json["cost_basis"].get<double>(), 120.0);
    EXPECT_EQ(json["asset_class"], "stocks");
}

TEST_F(PositionTest, FromJsonRoundTrips) {
    auto json = position_.toJSON();
    auto restored = Position::fromJSON(json);

    EXPECT_EQ(restored.ticker, position_.ticker);
    EXPECT_DOUBLE_EQ(restored.shares, position_.shares);
    EXPECT_DOUBLE_EQ(restored.current_price, position_.current_price);
    EXPECT_DOUBLE_EQ(restored.cost_basis, position_.cost_basis);
    EXPECT_EQ(restored.asset_class, position_.asset_class);
}

// =============================================================================
// Portfolio Tests
// =============================================================================

class PortfolioTest : public ::testing::Test {
protected:
    void SetUp() override {
        portfolio_ = Portfolio("TestPortfolio");

        // Add positions
        Position aapl;
        aapl.ticker = "AAPL";
        aapl.shares = 100.0;
        aapl.current_price = 150.0;
        aapl.cost_basis = 120.0;
        aapl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(aapl);

        Position googl;
        googl.ticker = "GOOGL";
        googl.shares = 50.0;
        googl.current_price = 2800.0;
        googl.cost_basis = 2500.0;
        googl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(googl);

        Position bnd;
        bnd.ticker = "BND";
        bnd.shares = 200.0;
        bnd.current_price = 80.0;
        bnd.cost_basis = 85.0;
        bnd.asset_class = AssetClass::BONDS;
        portfolio_.addPosition(bnd);

        portfolio_.setCashBalance(5000.0);
    }

    Portfolio portfolio_;
};

TEST_F(PortfolioTest, GetPositionCountReturnsCorrectCount) {
    EXPECT_EQ(portfolio_.getPositionCount(), 3);
}

TEST_F(PortfolioTest, GetTotalValueIncludesCash) {
    // AAPL: 100 * 150 = 15000
    // GOOGL: 50 * 2800 = 140000
    // BND: 200 * 80 = 16000
    // Cash: 5000
    // Total: 176000
    EXPECT_DOUBLE_EQ(portfolio_.getTotalValue(), 176000.0);
}

TEST_F(PortfolioTest, GetPositionReturnsCorrectPosition) {
    const auto& pos = portfolio_.getPosition("AAPL");
    EXPECT_EQ(pos.ticker, "AAPL");
    EXPECT_DOUBLE_EQ(pos.shares, 100.0);
}

TEST_F(PortfolioTest, GetPositionThrowsForUnknownTicker) {
    EXPECT_THROW(portfolio_.getPosition("UNKNOWN"), std::out_of_range);
}

TEST_F(PortfolioTest, HasPositionReturnsCorrectly) {
    EXPECT_TRUE(portfolio_.hasPosition("AAPL"));
    EXPECT_FALSE(portfolio_.hasPosition("UNKNOWN"));
}

TEST_F(PortfolioTest, RemovePositionWorks) {
    portfolio_.removePosition("AAPL");
    EXPECT_EQ(portfolio_.getPositionCount(), 2);
    EXPECT_FALSE(portfolio_.hasPosition("AAPL"));
}

TEST_F(PortfolioTest, GetAllocationPercentCalculatesCorrectly) {
    double total = portfolio_.getTotalValue();
    double expected_aapl = 15000.0 / total;
    EXPECT_NEAR(portfolio_.getAllocationPercent("AAPL"), expected_aapl, 0.0001);
}

TEST_F(PortfolioTest, GetAssetClassExposureWorks) {
    auto exposure = portfolio_.getAssetClassExposure();
    // Stocks: 15000 + 140000 = 155000
    // Bonds: 16000
    // Total equity: 171000 (excluding cash)
    // Total: 176000

    double total = portfolio_.getTotalValue();
    double stocks_pct = 155000.0 / total;
    double bonds_pct = 16000.0 / total;

    EXPECT_NEAR(exposure[AssetClass::STOCKS], stocks_pct, 0.0001);
    EXPECT_NEAR(exposure[AssetClass::BONDS], bonds_pct, 0.0001);
}

TEST_F(PortfolioTest, UpdatePriceWorks) {
    portfolio_.updatePrice("AAPL", 200.0);
    EXPECT_DOUBLE_EQ(portfolio_.getPosition("AAPL").current_price, 200.0);
}

TEST_F(PortfolioTest, ToJsonAndFromJsonRoundTrip) {
    auto json = portfolio_.toJSON();
    auto restored = Portfolio::fromJSON(json);

    EXPECT_EQ(restored.getName(), portfolio_.getName());
    EXPECT_EQ(restored.getPositionCount(), portfolio_.getPositionCount());
    EXPECT_DOUBLE_EQ(restored.getCashBalance(), portfolio_.getCashBalance());
}

// =============================================================================
// TargetAllocation Tests
// =============================================================================

class TargetAllocationTest : public ::testing::Test {
protected:
    void SetUp() override {
        target_ = TargetAllocation(AllocationMode::PERCENTAGE);
        target_.setTarget("AAPL", 0.30, 0.05);   // 30% +/- 5%
        target_.setTarget("GOOGL", 0.40, 0.05);  // 40% +/- 5%
        target_.setTarget("BND", 0.30, 0.05);    // 30% +/- 5%
    }

    TargetAllocation target_;
};

TEST_F(TargetAllocationTest, GetTargetReturnsCorrectValue) {
    EXPECT_DOUBLE_EQ(target_.getTarget("AAPL"), 0.30);
    EXPECT_DOUBLE_EQ(target_.getTarget("GOOGL"), 0.40);
    EXPECT_DOUBLE_EQ(target_.getTarget("BND"), 0.30);
}

TEST_F(TargetAllocationTest, GetTargetReturnsZeroForUnknown) {
    EXPECT_DOUBLE_EQ(target_.getTarget("UNKNOWN"), 0.0);
}

TEST_F(TargetAllocationTest, GetFullTargetReturnsCorrectBounds) {
    auto full = target_.getFullTarget("AAPL");
    EXPECT_DOUBLE_EQ(full.target_value, 0.30);
    EXPECT_DOUBLE_EQ(full.lower_bound, 0.25);  // 30% - 5%
    EXPECT_DOUBLE_EQ(full.upper_bound, 0.35);  // 30% + 5%
}

TEST_F(TargetAllocationTest, SetTargetWithBoundsWorks) {
    target_.setTargetWithBounds("TSLA", 0.10, 0.05, 0.15, false);
    auto full = target_.getFullTarget("TSLA");
    EXPECT_DOUBLE_EQ(full.target_value, 0.10);
    EXPECT_DOUBLE_EQ(full.lower_bound, 0.05);
    EXPECT_DOUBLE_EQ(full.upper_bound, 0.15);
}

TEST_F(TargetAllocationTest, ValidateReturnsTrueForValidAllocation) {
    EXPECT_TRUE(target_.validate());
}

TEST_F(TargetAllocationTest, ValidateReturnsFalseForOverAllocation) {
    target_.setTarget("TSLA", 0.50, 0.05);  // Now totals 1.50
    EXPECT_FALSE(target_.validate());
}

TEST_F(TargetAllocationTest, GetAllTargetsReturnsAllTargets) {
    auto targets = target_.getAllTargets();
    EXPECT_EQ(targets.size(), 3);
}

TEST_F(TargetAllocationTest, RemoveTargetWorks) {
    target_.removeTarget("AAPL");
    auto targets = target_.getAllTargets();
    EXPECT_EQ(targets.size(), 2);
    EXPECT_DOUBLE_EQ(target_.getTarget("AAPL"), 0.0);
}

TEST_F(TargetAllocationTest, ToJsonAndFromJsonRoundTrip) {
    auto json = target_.toJSON();
    auto restored = TargetAllocation::fromJSON(json);

    EXPECT_DOUBLE_EQ(restored.getTarget("AAPL"), target_.getTarget("AAPL"));
    EXPECT_DOUBLE_EQ(restored.getTarget("GOOGL"), target_.getTarget("GOOGL"));
    EXPECT_EQ(restored.getAllTargets().size(), target_.getAllTargets().size());
}

// =============================================================================
// Drift Calculation Tests
// =============================================================================

class DriftCalculationTest : public ::testing::Test {
protected:
    void SetUp() override {
        portfolio_ = Portfolio("DriftTest");

        // Create a portfolio worth $100,000
        Position aapl;
        aapl.ticker = "AAPL";
        aapl.shares = 200.0;
        aapl.current_price = 150.0;  // $30,000 = 30%
        aapl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(aapl);

        Position googl;
        googl.ticker = "GOOGL";
        googl.shares = 20.0;
        googl.current_price = 2000.0;  // $40,000 = 40%
        googl.asset_class = AssetClass::STOCKS;
        portfolio_.addPosition(googl);

        Position bnd;
        bnd.ticker = "BND";
        bnd.shares = 300.0;
        bnd.current_price = 100.0;  // $30,000 = 30%
        bnd.asset_class = AssetClass::BONDS;
        portfolio_.addPosition(bnd);

        // Target: 40% AAPL, 30% GOOGL, 30% BND
        target_ = TargetAllocation(AllocationMode::PERCENTAGE);
        target_.setTarget("AAPL", 0.40, 0.05);
        target_.setTarget("GOOGL", 0.30, 0.05);
        target_.setTarget("BND", 0.30, 0.05);
    }

    Portfolio portfolio_;
    TargetAllocation target_;
};

TEST_F(DriftCalculationTest, GetDeviationMapCalculatesCorrectly) {
    auto deviations = portfolio_.getDeviationMap(target_);

    // Current: AAPL 30%, GOOGL 40%, BND 30%
    // Target: AAPL 40%, GOOGL 30%, BND 30%
    // Deviation: AAPL -10%, GOOGL +10%, BND 0%

    EXPECT_NEAR(deviations["AAPL"], -0.10, 0.001);
    EXPECT_NEAR(deviations["GOOGL"], 0.10, 0.001);
    EXPECT_NEAR(deviations["BND"], 0.0, 0.001);
}

TEST_F(DriftCalculationTest, CalculateDriftReturnsTotalAbsoluteDeviation) {
    double drift = portfolio_.calculateDrift(target_);

    // Total drift = |−0.10| + |0.10| + |0| = 0.20
    EXPECT_NEAR(drift, 0.20, 0.001);
}

TEST_F(DriftCalculationTest, NeedsRebalancingReturnsTrueWhenOutsideBands) {
    // AAPL is at 30%, target is 40% with 5% tolerance (band: 35%-45%)
    // 30% < 35%, so rebalancing is needed
    EXPECT_TRUE(portfolio_.needsRebalancing(target_));
}

TEST_F(DriftCalculationTest, NeedsRebalancingReturnsFalseWhenWithinBands) {
    // Adjust targets to match current allocations within tolerance
    TargetAllocation loose_target(AllocationMode::PERCENTAGE);
    loose_target.setTarget("AAPL", 0.30, 0.10);   // 20%-40% band
    loose_target.setTarget("GOOGL", 0.40, 0.10);  // 30%-50% band
    loose_target.setTarget("BND", 0.30, 0.10);    // 20%-40% band

    EXPECT_FALSE(portfolio_.needsRebalancing(loose_target));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
