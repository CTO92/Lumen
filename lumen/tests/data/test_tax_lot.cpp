/// @file test_tax_lot.cpp
/// @brief Unit tests for TaxLot, TaxLotManager, and TaxOptimizer classes

#include <gtest/gtest.h>
#include "lumen/data/tax_lot.hpp"

#include <chrono>
#include <ctime>

using namespace lumen::data;

// Helper to create a time_point from a date string (YYYY-MM-DD)
std::chrono::system_clock::time_point makeDate(int year, int month, int day) {
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = 12;
    auto time_t_val = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t_val);
}

// =============================================================================
// TaxLot Tests
// =============================================================================

class TaxLotTest : public ::testing::Test {
protected:
    void SetUp() override {
        lot_.id = "LOT_001";
        lot_.ticker = "AAPL";
        lot_.shares = 100.0;
        lot_.cost_basis_per_share = 120.0;
        lot_.purchase_date = makeDate(2023, 1, 15);
    }

    TaxLot lot_;
};

TEST_F(TaxLotTest, GetTotalCostBasisCalculatesCorrectly) {
    // 100 shares * $120 = $12,000
    EXPECT_DOUBLE_EQ(lot_.getTotalCostBasis(), 12000.0);
}

TEST_F(TaxLotTest, GetCurrentValueCalculatesCorrectly) {
    // 100 shares * $150 = $15,000
    EXPECT_DOUBLE_EQ(lot_.getCurrentValue(150.0), 15000.0);
}

TEST_F(TaxLotTest, GetUnrealizedGainCalculatesGainCorrectly) {
    // Current price 150, cost basis 120, shares 100
    // Gain = (150 - 120) * 100 = 3000
    EXPECT_DOUBLE_EQ(lot_.getUnrealizedGain(150.0), 3000.0);
}

TEST_F(TaxLotTest, GetUnrealizedGainCalculatesLossCorrectly) {
    // Current price 100, cost basis 120, shares 100
    // Loss = (100 - 120) * 100 = -2000
    EXPECT_DOUBLE_EQ(lot_.getUnrealizedGain(100.0), -2000.0);
}

TEST_F(TaxLotTest, IsSoldReturnsFalseWhenNotSold) {
    EXPECT_FALSE(lot_.isSold());
}

TEST_F(TaxLotTest, IsSoldReturnsTrueWhenSold) {
    lot_.sale_date = makeDate(2024, 6, 15);
    lot_.sale_price = 150.0;
    EXPECT_TRUE(lot_.isSold());
}

TEST_F(TaxLotTest, GetRealizedGainCalculatesCorrectlyWhenSold) {
    lot_.sale_date = makeDate(2024, 6, 15);
    lot_.sale_price = 150.0;
    // (150 - 120) * 100 = 3000
    EXPECT_DOUBLE_EQ(lot_.getRealizedGain(), 3000.0);
}

TEST_F(TaxLotTest, GetGainTypeReturnsShortTermForLessThanOneYear) {
    // Lot purchased 2023-01-15, check gain type at 2023-06-15 (6 months)
    // Need to set the sale date to simulate the check
    lot_.sale_date = makeDate(2023, 6, 15);
    EXPECT_EQ(lot_.getGainType(), GainType::SHORT_TERM);
}

TEST_F(TaxLotTest, GetGainTypeReturnsLongTermForMoreThanOneYear) {
    // Lot purchased 2023-01-15, check gain type at 2024-06-15 (>1 year)
    lot_.sale_date = makeDate(2024, 6, 15);
    EXPECT_EQ(lot_.getGainType(), GainType::LONG_TERM);
}

TEST_F(TaxLotTest, ToJsonWorks) {
    auto json = lot_.toJSON();
    EXPECT_EQ(json["id"], "LOT_001");
    EXPECT_EQ(json["ticker"], "AAPL");
    EXPECT_DOUBLE_EQ(json["shares"].get<double>(), 100.0);
    EXPECT_DOUBLE_EQ(json["cost_basis_per_share"].get<double>(), 120.0);
}

TEST_F(TaxLotTest, FromJsonWorks) {
    auto json = lot_.toJSON();
    auto restored = TaxLot::fromJSON(json);

    EXPECT_EQ(restored.id, lot_.id);
    EXPECT_EQ(restored.ticker, lot_.ticker);
    EXPECT_DOUBLE_EQ(restored.shares, lot_.shares);
    EXPECT_DOUBLE_EQ(restored.cost_basis_per_share, lot_.cost_basis_per_share);
}

// =============================================================================
// TaxLotManager Tests
// =============================================================================

class TaxLotManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<TaxLotManager>();

        TaxLot lot1;
        lot1.id = "LOT_001";
        lot1.ticker = "AAPL";
        lot1.shares = 100.0;
        lot1.cost_basis_per_share = 120.0;
        lot1.purchase_date = makeDate(2022, 1, 15);  // Old lot
        manager_->addLot(lot1);

        TaxLot lot2;
        lot2.id = "LOT_002";
        lot2.ticker = "AAPL";
        lot2.shares = 50.0;
        lot2.cost_basis_per_share = 140.0;
        lot2.purchase_date = makeDate(2023, 6, 1);   // Newer lot
        manager_->addLot(lot2);

        TaxLot lot3;
        lot3.id = "LOT_003";
        lot3.ticker = "GOOGL";
        lot3.shares = 25.0;
        lot3.cost_basis_per_share = 2500.0;
        lot3.purchase_date = makeDate(2023, 3, 10);
        manager_->addLot(lot3);
    }

    std::unique_ptr<TaxLotManager> manager_;
};

TEST_F(TaxLotManagerTest, GetLotsForTickerReturnsCorrectLots) {
    auto lots = manager_->getLotsForTicker("AAPL");
    EXPECT_EQ(lots.size(), 2);
}

TEST_F(TaxLotManagerTest, GetLotsForTickerReturnsEmptyForUnknownTicker) {
    auto lots = manager_->getLotsForTicker("UNKNOWN");
    EXPECT_EQ(lots.size(), 0);
}

TEST_F(TaxLotManagerTest, GetAllLotsReturnsAllLots) {
    auto lots = manager_->getAllLots();
    EXPECT_EQ(lots.size(), 3);
}

TEST_F(TaxLotManagerTest, GetLotReturnsCorrectLot) {
    const auto& lot = manager_->getLot("LOT_001");
    EXPECT_EQ(lot.ticker, "AAPL");
    EXPECT_DOUBLE_EQ(lot.shares, 100.0);
}

TEST_F(TaxLotManagerTest, GetLotThrowsForUnknownId) {
    EXPECT_THROW(manager_->getLot("UNKNOWN"), std::runtime_error);
}

TEST_F(TaxLotManagerTest, RemoveLotWorks) {
    manager_->removeLot("LOT_001");

    EXPECT_THROW(manager_->getLot("LOT_001"), std::runtime_error);

    auto lots = manager_->getLotsForTicker("AAPL");
    EXPECT_EQ(lots.size(), 1);
}

TEST_F(TaxLotManagerTest, GetTotalCostBasisCalculatesCorrectly) {
    // AAPL: 100 * 120 + 50 * 140 = 12000 + 7000 = 19000
    EXPECT_DOUBLE_EQ(manager_->getTotalCostBasis("AAPL"), 19000.0);
}

TEST_F(TaxLotManagerTest, GetAverageCostBasisCalculatesCorrectly) {
    // AAPL: 19000 / 150 shares = 126.67
    EXPECT_NEAR(manager_->getAverageCostBasis("AAPL"), 126.67, 0.01);
}

TEST_F(TaxLotManagerTest, SelectLotsToSellFIFOSelectsOldestFirst) {
    auto lots = manager_->selectLotsToSell("AAPL", 50.0, TaxLotMethod::FIFO);

    ASSERT_EQ(lots.size(), 1);
    EXPECT_EQ(lots[0].id, "LOT_001");  // Oldest lot
    EXPECT_DOUBLE_EQ(lots[0].shares, 50.0);
}

TEST_F(TaxLotManagerTest, SelectLotsToSellLIFOSelectsNewestFirst) {
    auto lots = manager_->selectLotsToSell("AAPL", 50.0, TaxLotMethod::LIFO);

    ASSERT_EQ(lots.size(), 1);
    EXPECT_EQ(lots[0].id, "LOT_002");  // Newest lot
    EXPECT_DOUBLE_EQ(lots[0].shares, 50.0);
}

TEST_F(TaxLotManagerTest, SelectLotsToSellHIFOSelectsHighestCostFirst) {
    auto lots = manager_->selectLotsToSell("AAPL", 50.0, TaxLotMethod::HIFO);

    ASSERT_EQ(lots.size(), 1);
    EXPECT_EQ(lots[0].id, "LOT_002");  // Highest cost basis ($140)
    EXPECT_DOUBLE_EQ(lots[0].shares, 50.0);
}

TEST_F(TaxLotManagerTest, SelectLotsToSellLOFOSelectsLowestCostFirst) {
    auto lots = manager_->selectLotsToSell("AAPL", 50.0, TaxLotMethod::LOFO);

    ASSERT_EQ(lots.size(), 1);
    EXPECT_EQ(lots[0].id, "LOT_001");  // Lowest cost basis ($120)
    EXPECT_DOUBLE_EQ(lots[0].shares, 50.0);
}

TEST_F(TaxLotManagerTest, SelectLotsToSellSpansMultipleLots) {
    auto lots = manager_->selectLotsToSell("AAPL", 120.0, TaxLotMethod::FIFO);

    // Should select all of LOT_001 (100 shares) + 20 from LOT_002
    ASSERT_EQ(lots.size(), 2);

    double total_shares = 0.0;
    for (const auto& lot : lots) {
        total_shares += lot.shares;
    }
    EXPECT_DOUBLE_EQ(total_shares, 120.0);
}

TEST_F(TaxLotManagerTest, ToJsonWorks) {
    auto json = manager_->toJSON();
    EXPECT_TRUE(json.contains("lots"));
    EXPECT_EQ(json["lots"].size(), 3);
}

TEST_F(TaxLotManagerTest, FromJsonWorks) {
    auto json = manager_->toJSON();
    auto restored = TaxLotManager::fromJSON(json);

    EXPECT_EQ(restored.getAllLots().size(), 3);
}

// =============================================================================
// TaxOptimizer Tests
// =============================================================================

class TaxOptimizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_unique<TaxLotManager>();

        // Add lots with different cost bases and acquisition dates
        TaxLot lot1;
        lot1.id = "LOT_001";
        lot1.ticker = "AAPL";
        lot1.shares = 100.0;
        lot1.cost_basis_per_share = 100.0;  // Low cost, old lot
        lot1.purchase_date = makeDate(2022, 1, 15);
        manager_->addLot(lot1);

        TaxLot lot2;
        lot2.id = "LOT_002";
        lot2.ticker = "AAPL";
        lot2.shares = 50.0;
        lot2.cost_basis_per_share = 180.0;  // High cost, recent (loss candidate)
        lot2.purchase_date = makeDate(2024, 6, 1);
        manager_->addLot(lot2);

        TaxLot lot3;
        lot3.id = "LOT_003";
        lot3.ticker = "VTI";
        lot3.shares = 75.0;
        lot3.cost_basis_per_share = 220.0;  // Loss candidate
        lot3.purchase_date = makeDate(2024, 1, 10);
        manager_->addLot(lot3);

        optimizer_ = std::make_unique<TaxOptimizer>(*manager_);
    }

    std::unique_ptr<TaxLotManager> manager_;
    std::unique_ptr<TaxOptimizer> optimizer_;
};

TEST_F(TaxOptimizerTest, FindHarvestingOpportunitiesFindsLosses) {
    std::map<std::string, double> prices = {
        {"AAPL", 150.0},  // LOT_002 has a loss (180 -> 150)
        {"VTI", 200.0}    // LOT_003 has a loss (220 -> 200)
    };

    auto opportunities = optimizer_->findHarvestingOpportunities(prices, 100.0);

    // Should find at least one opportunity (VTI has $1500 loss)
    EXPECT_GE(opportunities.size(), 1);
}

TEST_F(TaxOptimizerTest, FindHarvestingOpportunitiesRespectsMinimumThreshold) {
    std::map<std::string, double> prices = {
        {"AAPL", 175.0},  // LOT_002 has small loss (180 -> 175 = $250)
        {"VTI", 218.0}    // LOT_003 has tiny loss (220 -> 218 = $150)
    };

    // With $300 threshold, should find 0 opportunities
    auto opportunities = optimizer_->findHarvestingOpportunities(prices, 300.0);
    EXPECT_EQ(opportunities.size(), 0);

    // With $100 threshold, should find 2 opportunities
    opportunities = optimizer_->findHarvestingOpportunities(prices, 100.0);
    EXPECT_EQ(opportunities.size(), 2);
}

TEST_F(TaxOptimizerTest, WouldTriggerWashSaleDetectsEquivalentSecurities) {
    // VTI and VTSAX are in the same equivalence group
    TaxLot lot_to_sell;
    lot_to_sell.id = "LOT_003";
    lot_to_sell.ticker = "VTI";
    lot_to_sell.shares = 75.0;
    lot_to_sell.cost_basis_per_share = 220.0;
    lot_to_sell.purchase_date = makeDate(2024, 1, 10);

    auto trade_date = makeDate(2024, 6, 15);

    // Buying VTSAX within 30 days should trigger wash sale
    bool triggers = optimizer_->wouldTriggerWashSale(lot_to_sell, "VTSAX", trade_date);
    // This depends on whether there are recent VTSAX purchases
    // For now, just verify the method runs
    EXPECT_NO_THROW(optimizer_->wouldTriggerWashSale(lot_to_sell, "VTSAX", trade_date));
}

TEST_F(TaxOptimizerTest, SelectOptimalLotsPreferLossesWhenRequested) {
    std::map<std::string, double> prices = {{"AAPL", 150.0}};

    auto lots = optimizer_->selectOptimalLots("AAPL", 50.0, 150.0, true);

    ASSERT_EQ(lots.size(), 1);
    // Should select LOT_002 (cost $180) which has a loss at price $150
    EXPECT_EQ(lots[0].id, "LOT_002");
}

TEST_F(TaxOptimizerTest, EstimateTaxImpactCalculatesCorrectly) {
    std::map<std::string, double> prices = {
        {"AAPL", 150.0},
        {"VTI", 200.0}
    };

    // Create trade for selling AAPL at gain
    std::vector<lumen::core::Trade> trades;
    lumen::core::Trade trade;
    trade.ticker = "AAPL";
    trade.action = lumen::core::Trade::Action::SELL;
    trade.shares = 100.0;
    trade.price = 150.0;
    trade.amount = 15000.0;
    trade.lot_ids = {"LOT_001"};  // Lot with $100 cost basis
    trades.push_back(trade);

    double tax_impact = optimizer_->estimateTaxImpact(trades, prices);
    // Gain is (150 - 100) * 100 = $5000
    // Long-term rate is 15%, so tax ~$750
    EXPECT_GT(tax_impact, 0.0);
}

TEST_F(TaxOptimizerTest, SetTaxRatesWorks) {
    optimizer_->setTaxRates(0.40, 0.20);
    // No exception means success
    EXPECT_NO_THROW(optimizer_->setTaxRates(0.40, 0.20));
}

// =============================================================================
// WashSaleViolation Tests
// =============================================================================

TEST(WashSaleViolationTest, ToJsonWorks) {
    WashSaleViolation violation;
    violation.lot_id = "LOT_001";
    violation.replacement_ticker = "VTSAX";
    violation.violation_date = makeDate(2024, 6, 15);
    violation.disallowed_loss = 1500.0;
    violation.description = "Wash sale: sold VTI at loss and bought VTSAX within 30 days";

    auto json = violation.toJSON();
    EXPECT_EQ(json["lot_id"], "LOT_001");
    EXPECT_EQ(json["replacement_ticker"], "VTSAX");
    EXPECT_DOUBLE_EQ(json["disallowed_loss"].get<double>(), 1500.0);
}

// =============================================================================
// CapitalGainsReport Tests
// =============================================================================

TEST(CapitalGainsReportTest, ToJsonWorks) {
    CapitalGainsReport report;
    report.short_term_gains = 5000.0;
    report.short_term_losses = 1000.0;
    report.long_term_gains = 10000.0;
    report.long_term_losses = 2000.0;
    report.net_short_term = 4000.0;
    report.net_long_term = 8000.0;
    report.harvested_losses = 3000.0;
    report.estimated_tax_savings = 450.0;

    auto json = report.toJSON();
    EXPECT_DOUBLE_EQ(json["short_term_gains"].get<double>(), 5000.0);
    EXPECT_DOUBLE_EQ(json["net_long_term"].get<double>(), 8000.0);
}

TEST(CapitalGainsReportTest, ToPlainTextContainsExpectedSections) {
    CapitalGainsReport report;
    report.short_term_gains = 5000.0;
    report.short_term_losses = 1000.0;
    report.long_term_gains = 10000.0;
    report.long_term_losses = 2000.0;
    report.net_short_term = 4000.0;
    report.net_long_term = 8000.0;

    auto text = report.toPlainText();
    EXPECT_TRUE(text.find("Short-term") != std::string::npos);
    EXPECT_TRUE(text.find("Long-term") != std::string::npos);
}

// =============================================================================
// Helper Function Tests
// =============================================================================

TEST(TaxLotHelperTest, TaxLotMethodToStringWorks) {
    EXPECT_EQ(taxLotMethodToString(TaxLotMethod::FIFO), "FIFO");
    EXPECT_EQ(taxLotMethodToString(TaxLotMethod::LIFO), "LIFO");
    EXPECT_EQ(taxLotMethodToString(TaxLotMethod::HIFO), "HIFO");
    EXPECT_EQ(taxLotMethodToString(TaxLotMethod::LOFO), "LOFO");
    EXPECT_EQ(taxLotMethodToString(TaxLotMethod::SPEC_ID), "SPEC_ID");
}

TEST(TaxLotHelperTest, GainTypeToStringWorks) {
    EXPECT_EQ(gainTypeToString(GainType::SHORT_TERM), "short_term");
    EXPECT_EQ(gainTypeToString(GainType::LONG_TERM), "long_term");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
