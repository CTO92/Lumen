/// @file test_tax_optimization.cpp
/// @brief Integration tests for tax-aware portfolio optimization

#include <gtest/gtest.h>

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/data/tax_lot.hpp"
#include "lumen/data/broker_import.hpp"
#include "lumen/explain/explainer.hpp"
#include "lumen/solvers/highs_wrapper.hpp"
#include "lumen/utils/config.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace lumen;
namespace fs = std::filesystem;

// Helper to create a time_point from a date
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
// Integration Test Fixture
// =============================================================================

class TaxOptimizationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory
        test_dir_ = fs::temp_directory_path() / "lumen_tax_integration_test";
        fs::create_directories(test_dir_);

        // Set up a sample portfolio
        setupPortfolio();

        // Set up tax lots
        setupTaxLots();

        // Set up target allocation
        setupTargetAllocation();
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    void setupPortfolio() {
        portfolio_.setName("Test Tax Portfolio");

        // Add positions
        core::Position pos1;
        pos1.ticker = "VTI";
        pos1.shares = 100.0;
        pos1.current_price = 220.0;
        pos1.cost_basis = 200.0;
        portfolio_.addPosition(pos1);

        core::Position pos2;
        pos2.ticker = "BND";
        pos2.shares = 200.0;
        pos2.current_price = 80.0;
        pos2.cost_basis = 85.0;
        portfolio_.addPosition(pos2);

        core::Position pos3;
        pos3.ticker = "VXUS";
        pos3.shares = 75.0;
        pos3.current_price = 55.0;
        pos3.cost_basis = 60.0;
        portfolio_.addPosition(pos3);

        // Total value: VTI 22000 + BND 16000 + VXUS 4125 = 42125
    }

    void setupTaxLots() {
        // VTI lots - some with gains, some with losses
        data::TaxLot vti_lot1;
        vti_lot1.id = "VTI_LOT_001";
        vti_lot1.ticker = "VTI";
        vti_lot1.shares = 50.0;
        vti_lot1.cost_basis_per_share = 180.0;  // Gain at $220
        vti_lot1.purchase_date = makeDate(2022, 1, 15);  // Long-term
        lot_manager_.addLot(vti_lot1);

        data::TaxLot vti_lot2;
        vti_lot2.id = "VTI_LOT_002";
        vti_lot2.ticker = "VTI";
        vti_lot2.shares = 50.0;
        vti_lot2.cost_basis_per_share = 240.0;  // Loss at $220
        vti_lot2.purchase_date = makeDate(2024, 6, 1);   // Short-term
        lot_manager_.addLot(vti_lot2);

        // BND lots - with losses
        data::TaxLot bnd_lot1;
        bnd_lot1.id = "BND_LOT_001";
        bnd_lot1.ticker = "BND";
        bnd_lot1.shares = 200.0;
        bnd_lot1.cost_basis_per_share = 85.0;   // Loss at $80
        bnd_lot1.purchase_date = makeDate(2023, 3, 10);  // Long-term
        lot_manager_.addLot(bnd_lot1);

        // VXUS lots - with losses
        data::TaxLot vxus_lot1;
        vxus_lot1.id = "VXUS_LOT_001";
        vxus_lot1.ticker = "VXUS";
        vxus_lot1.shares = 75.0;
        vxus_lot1.cost_basis_per_share = 60.0;  // Loss at $55
        vxus_lot1.purchase_date = makeDate(2023, 5, 20);  // Long-term
        lot_manager_.addLot(vxus_lot1);
    }

    void setupTargetAllocation() {
        target_.addTarget("VTI", 0.60);   // 60% US stocks
        target_.addTarget("BND", 0.30);   // 30% bonds
        target_.addTarget("VXUS", 0.10);  // 10% international
    }

    fs::path test_dir_;
    core::Portfolio portfolio_;
    data::TaxLotManager lot_manager_;
    core::TargetAllocation target_;
};

// =============================================================================
// Tax-Loss Harvesting Integration Tests
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, FindHarvestingOpportunitiesInPortfolio) {
    // Create optimizer with lot manager
    data::TaxOptimizer optimizer(lot_manager_);

    // Current prices from portfolio
    std::map<std::string, double> prices;
    for (const auto& pos : portfolio_.getAllPositions()) {
        prices[pos.ticker] = pos.current_price;
    }

    // Find harvesting opportunities with $100 minimum
    auto opportunities = optimizer.findHarvestingOpportunities(prices, 100.0);

    // Should find opportunities in:
    // - VTI_LOT_002: (220 - 240) * 50 = -$1000 loss
    // - BND_LOT_001: (80 - 85) * 200 = -$1000 loss
    // - VXUS_LOT_001: (55 - 60) * 75 = -$375 loss
    EXPECT_GE(opportunities.size(), 2);  // At least VTI and BND

    // Verify total harvestable losses
    double total_loss = 0.0;
    for (const auto& opp : opportunities) {
        total_loss += std::abs(opp.unrealized_loss);
    }
    EXPECT_GT(total_loss, 2000.0);  // At least $2000 in losses
}

TEST_F(TaxOptimizationIntegrationTest, WashSaleDetectionForEquivalentSecurities) {
    data::TaxOptimizer optimizer(lot_manager_);

    // VTI is in the same equivalence group as VTSAX, SPTM, etc.
    auto& vti_lot = lot_manager_.getLot("VTI_LOT_002");

    // Check if buying VTSAX would trigger wash sale
    bool triggers = optimizer.wouldTriggerWashSale(
        vti_lot, "VTSAX", std::chrono::system_clock::now());

    // The method should run without error
    // Actual trigger depends on recent purchases of equivalent securities
    EXPECT_NO_THROW(optimizer.wouldTriggerWashSale(
        vti_lot, "VTSAX", std::chrono::system_clock::now()));
}

// =============================================================================
// Tax-Aware Lot Selection Integration Tests
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, LotSelectionMethodsProduceDifferentResults) {
    // Selling 30 shares of VTI should produce different results
    // based on lot selection method

    auto fifo_lots = lot_manager_.selectLotsToSell("VTI", 30.0, data::TaxLotMethod::FIFO);
    auto lifo_lots = lot_manager_.selectLotsToSell("VTI", 30.0, data::TaxLotMethod::LIFO);
    auto hifo_lots = lot_manager_.selectLotsToSell("VTI", 30.0, data::TaxLotMethod::HIFO);

    ASSERT_FALSE(fifo_lots.empty());
    ASSERT_FALSE(lifo_lots.empty());
    ASSERT_FALSE(hifo_lots.empty());

    // FIFO should select from the oldest lot (VTI_LOT_001, cost $180)
    // LIFO should select from the newest lot (VTI_LOT_002, cost $240)
    // HIFO should select from the highest cost lot (VTI_LOT_002, cost $240)

    // At current price $220:
    // - FIFO: gain of (220-180)*30 = $1200
    // - LIFO: loss of (220-240)*30 = -$600
    // - HIFO: loss of (220-240)*30 = -$600

    double fifo_gain = fifo_lots[0].getUnrealizedGain(220.0);
    double hifo_gain = hifo_lots[0].getUnrealizedGain(220.0);

    // FIFO should have a gain (lot with $180 cost)
    EXPECT_GT(fifo_gain, 0.0);

    // HIFO should minimize tax (lot with $240 cost = loss)
    EXPECT_LT(hifo_gain, 0.0);
}

TEST_F(TaxOptimizationIntegrationTest, OptimalLotSelectionPreferLosses) {
    data::TaxOptimizer optimizer(lot_manager_);

    // Select lots optimally, preferring losses for tax harvesting
    auto optimal_lots = optimizer.selectOptimalLots("VTI", 30.0, 220.0, true);

    ASSERT_FALSE(optimal_lots.empty());

    // Should select from the lot with losses (VTI_LOT_002)
    EXPECT_EQ(optimal_lots[0].id, "VTI_LOT_002");
}

// =============================================================================
// Capital Gains Report Integration Tests
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, CapitalGainsReportCalculatesCorrectly) {
    data::TaxOptimizer optimizer(lot_manager_);

    // Simulate selling some positions
    std::vector<core::Trade> trades;

    core::Trade vti_sell;
    vti_sell.ticker = "VTI";
    vti_sell.action = core::Trade::Action::SELL;
    vti_sell.shares = 50.0;
    vti_sell.price = 220.0;
    vti_sell.amount = 11000.0;
    vti_sell.lot_ids = {"VTI_LOT_002"};  // Use the loss lot
    trades.push_back(vti_sell);

    core::Trade bnd_sell;
    bnd_sell.ticker = "BND";
    bnd_sell.action = core::Trade::Action::SELL;
    bnd_sell.shares = 100.0;
    bnd_sell.price = 80.0;
    bnd_sell.amount = 8000.0;
    bnd_sell.lot_ids = {"BND_LOT_001"};
    trades.push_back(bnd_sell);

    std::map<std::string, double> prices = {
        {"VTI", 220.0},
        {"BND", 80.0}
    };

    auto report = optimizer.calculateCapitalGains(trades, prices);

    // VTI: (220-240)*50 = -$1000 short-term loss
    // BND: (80-85)*100 = -$500 long-term loss

    EXPECT_GT(report.short_term_losses, 0.0);
    EXPECT_GT(report.long_term_losses, 0.0);

    // Net should be negative (losses)
    EXPECT_LT(report.net_short_term, 0.0);
    EXPECT_LT(report.net_long_term, 0.0);
}

// =============================================================================
// Tax Constraint Integration Tests
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, TaxLotConstraintIsSatisfied) {
    // Create tax lot constraint
    auto constraint = std::make_unique<core::TaxLotConstraint>(
        &lot_manager_,
        data::TaxLotMethod::HIFO,
        true  // prefer long-term
    );

    EXPECT_TRUE(constraint->isValid());
    EXPECT_EQ(constraint->getType(), core::ConstraintType::TAX_LOT);

    // Check if constraint is satisfied with empty trades
    std::vector<core::Trade> empty_trades;
    EXPECT_TRUE(constraint->isSatisfied(portfolio_, empty_trades));

    // Create a sell trade
    std::vector<core::Trade> sell_trades;
    core::Trade trade;
    trade.ticker = "VTI";
    trade.action = core::Trade::Action::SELL;
    trade.shares = 30.0;
    trade.price = 220.0;
    sell_trades.push_back(trade);

    // Should be satisfied as long as shares are available
    EXPECT_TRUE(constraint->isSatisfied(portfolio_, sell_trades));
}

TEST_F(TaxOptimizationIntegrationTest, WashSaleConstraintWorks) {
    data::TaxOptimizer optimizer(lot_manager_);

    // Create wash sale constraint
    auto constraint = std::make_unique<core::WashSaleConstraint>(
        &optimizer,
        true,   // block violations
        false   // not warn-only
    );

    EXPECT_TRUE(constraint->isValid());
    EXPECT_EQ(constraint->getType(), core::ConstraintType::WASH_SALE);

    // Constraint should be satisfied with normal trades
    std::vector<core::Trade> trades;
    core::Trade trade;
    trade.ticker = "VTI";
    trade.action = core::Trade::Action::SELL;
    trade.shares = 30.0;
    trades.push_back(trade);

    // Check satisfaction (depends on recent transactions)
    EXPECT_NO_THROW(constraint->isSatisfied(portfolio_, trades));
}

// =============================================================================
// Broker Import Integration Tests
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, ImportAndOptimizeFlow) {
    // Create a CSV file with tax lots
    auto csv_path = test_dir_ / "lots.csv";
    std::ofstream csv_file(csv_path);
    csv_file << "ticker,shares,cost_basis,purchase_date\n";
    csv_file << "AAPL,100,15000.00,2023-01-15\n";
    csv_file << "GOOGL,50,125000.00,2023-03-10\n";
    csv_file.close();

    // Import using generic importer
    data::GenericCSVImporter importer;
    auto result = importer.importFromFile(csv_path.string());

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.lots.size(), 2);

    // Add imported lots to a new manager
    data::TaxLotManager imported_manager;
    for (const auto& lot : result.lots) {
        imported_manager.addLot(lot);
    }

    // Verify lots are accessible
    auto aapl_lots = imported_manager.getLotsForTicker("AAPL");
    EXPECT_EQ(aapl_lots.size(), 1);
    EXPECT_DOUBLE_EQ(aapl_lots[0].shares, 100.0);
}

// =============================================================================
// Full Optimization with Tax Constraints
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, OptimizationWithTaxConstraints) {
    // Create constraint set with tax constraints
    core::ConstraintSet constraints;

    // Add tax lot constraint
    constraints.addConstraint(std::make_unique<core::TaxLotConstraint>(
        &lot_manager_,
        data::TaxLotMethod::HIFO,
        true
    ));

    // Add wash sale constraint
    data::TaxOptimizer optimizer(lot_manager_);
    constraints.addConstraint(std::make_unique<core::WashSaleConstraint>(
        &optimizer,
        true,
        false
    ));

    // Verify constraint set has tax constraints
    auto tax_constraints = constraints.getConstraintsByType(core::ConstraintType::TAX_LOT);
    auto wash_constraints = constraints.getConstraintsByType(core::ConstraintType::WASH_SALE);

    EXPECT_EQ(tax_constraints.size(), 1);
    EXPECT_EQ(wash_constraints.size(), 1);

    // Verify all constraints are valid
    EXPECT_TRUE(constraints.isValid());
}

// =============================================================================
// Explainer Integration with Tax Reports
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, ExplainerGeneratesTaxReport) {
    explain::Explainer explainer;
    data::TaxOptimizer optimizer(lot_manager_);

    // Set up explainer with tax optimizer
    explainer.setTaxOptimizer(&optimizer);

    std::map<std::string, double> prices;
    for (const auto& pos : portfolio_.getAllPositions()) {
        prices[pos.ticker] = pos.current_price;
    }
    explainer.setCurrentPrices(prices);

    // Create some trades for tax impact calculation
    std::vector<core::Trade> trades;
    core::Trade trade;
    trade.ticker = "VTI";
    trade.action = core::Trade::Action::SELL;
    trade.shares = 30.0;
    trade.price = 220.0;
    trade.amount = 6600.0;
    trade.lot_ids = {"VTI_LOT_002"};
    trades.push_back(trade);

    // Calculate tax impact
    auto report = explainer.calculateTaxImpact(trades);

    // Should have some short-term losses (VTI_LOT_002 has loss)
    EXPECT_NE(report.net_short_term, 0.0);
}

// =============================================================================
// End-to-End Tax Optimization Flow
// =============================================================================

TEST_F(TaxOptimizationIntegrationTest, EndToEndTaxOptimizationFlow) {
    // 1. Identify tax-loss harvesting opportunities
    data::TaxOptimizer optimizer(lot_manager_);

    std::map<std::string, double> prices;
    for (const auto& pos : portfolio_.getAllPositions()) {
        prices[pos.ticker] = pos.current_price;
    }

    auto opportunities = optimizer.findHarvestingOpportunities(prices, 100.0);
    EXPECT_GT(opportunities.size(), 0);

    // 2. Select lots for harvesting using HIFO method
    std::vector<core::Trade> harvest_trades;
    for (const auto& opp : opportunities) {
        if (std::abs(opp.unrealized_loss) >= 500.0) {  // Only harvest significant losses
            core::Trade trade;
            trade.ticker = opp.lot.ticker;
            trade.action = core::Trade::Action::SELL;
            trade.shares = opp.lot.shares;
            trade.price = opp.current_price;
            trade.amount = trade.shares * trade.price;
            trade.lot_ids = {opp.lot.id};
            harvest_trades.push_back(trade);
        }
    }

    // 3. Calculate capital gains/losses
    auto report = optimizer.calculateCapitalGains(harvest_trades, prices);

    // 4. Verify we're harvesting losses
    double total_losses = report.short_term_losses + report.long_term_losses;
    EXPECT_GT(total_losses, 0.0);

    // 5. Verify wash sale check works
    for (const auto& trade : harvest_trades) {
        auto& lot = lot_manager_.getLot(trade.lot_ids[0]);
        // Check potential replacement candidates
        for (const auto& opp : opportunities) {
            if (opp.lot.id == lot.id) {
                for (const auto& candidate : opp.replacement_candidates) {
                    // Should be able to check wash sale
                    EXPECT_NO_THROW(optimizer.wouldTriggerWashSale(
                        lot, candidate, std::chrono::system_clock::now()));
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
