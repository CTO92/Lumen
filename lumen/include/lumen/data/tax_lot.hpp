#pragma once

/// @file tax_lot.hpp
/// @brief Tax lot management and tax optimization
///
/// This module provides data structures and algorithms for tax-aware
/// portfolio optimization, including lot selection and wash sale detection.

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/core/solver_dispatcher.hpp"

namespace lumen::data {

/// Tax lot selection methods
enum class TaxLotMethod {
    FIFO,     ///< First In, First Out
    LIFO,     ///< Last In, First Out
    HIFO,     ///< Highest In, First Out (tax loss harvesting)
    LOFO,     ///< Lowest In, First Out
    SPEC_ID   ///< Specific Identification
};

/// Capital gain type based on holding period
enum class GainType {
    SHORT_TERM,  ///< Held < 1 year (ordinary tax rates)
    LONG_TERM    ///< Held >= 1 year (preferential rates)
};

/// Convert TaxLotMethod to string
std::string taxLotMethodToString(TaxLotMethod method);

/// Convert GainType to string
std::string gainTypeToString(GainType type);

/// Represents a single tax lot (cost basis record)
struct TaxLot {
    std::string id;                  ///< Unique lot identifier
    std::string ticker;              ///< Security symbol
    double shares;                   ///< Number of shares in lot
    double cost_basis_per_share;     ///< Purchase price per share
    std::chrono::system_clock::time_point purchase_date;  ///< Acquisition date
    std::optional<std::chrono::system_clock::time_point> sale_date;  ///< Sale date (if sold)
    std::optional<double> sale_price;  ///< Sale price per share (if sold)

    /// Get total cost basis for the lot
    double getTotalCostBasis() const;

    /// Get current value at given price
    double getCurrentValue(double current_price) const;

    /// Get unrealized gain/loss at given price
    double getUnrealizedGain(double current_price) const;

    /// Get realized gain/loss (if sold)
    double getRealizedGain() const;

    /// Determine gain type based on holding period
    GainType getGainType() const;

    /// Check if lot has been sold
    bool isSold() const { return sale_date.has_value(); }

    /// Get number of days held
    int getDaysHeld() const;

    nlohmann::json toJSON() const;
    static TaxLot fromJSON(const nlohmann::json& j);
};

/// Represents a wash sale violation
struct WashSaleViolation {
    std::string lot_id;               ///< Lot that triggered violation
    std::string replacement_ticker;   ///< Replacement security purchased
    std::chrono::system_clock::time_point violation_date;  ///< Date of violation
    double disallowed_loss;           ///< Amount of loss disallowed
    std::string description;          ///< Human-readable description

    nlohmann::json toJSON() const;
};

/// Manages tax lots for a portfolio
class TaxLotManager {
public:
    TaxLotManager() = default;
    ~TaxLotManager();

    /// Add a new tax lot
    void addLot(const TaxLot& lot);

    /// Remove a lot by ID
    void removeLot(const std::string& lot_id);

    /// Update an existing lot
    void updateLot(const std::string& lot_id, const TaxLot& updated);

    /// Get a lot by ID
    const TaxLot& getLot(const std::string& lot_id) const;

    /// Get all lots for a ticker
    std::vector<TaxLot> getLotsForTicker(const std::string& ticker) const;

    /// Get all unsold lots
    std::vector<TaxLot> getUnsoldLots() const;

    /// Get all sold lots
    std::vector<TaxLot> getSoldLots() const;

    /// Get all lots
    std::vector<TaxLot> getAllLots() const;

    /// Select lots to sell using specified method
    std::vector<TaxLot> selectLotsToSell(const std::string& ticker, double shares_to_sell,
                                          TaxLotMethod method) const;

    /// Calculate total cost basis for a ticker
    double getTotalCostBasis(const std::string& ticker) const;

    /// Calculate average cost basis for a ticker
    double getAverageCostBasis(const std::string& ticker) const;

    /// Calculate total unrealized gain for a ticker
    double getTotalUnrealizedGain(double current_price, const std::string& ticker) const;

    /// Save lots to database
    void saveToDatabase(const std::string& db_path);

    /// Load lots from database
    void loadFromDatabase(const std::string& db_path);

    nlohmann::json toJSON() const;
    static TaxLotManager fromJSON(const nlohmann::json& j);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Represents a tax-loss harvesting opportunity
struct TaxHarvestingOpportunity {
    TaxLot lot;                       ///< Lot with unrealized loss
    double current_price;             ///< Current market price
    double unrealized_loss;           ///< Amount of loss available
    GainType gain_type;               ///< Short-term or long-term
    std::vector<std::string> replacement_candidates;  ///< Similar ETFs
    bool would_trigger_wash_sale;     ///< Whether replacement would trigger wash sale

    nlohmann::json toJSON() const;
};

/// Capital gains tax report
struct CapitalGainsReport {
    double short_term_gains;          ///< Total short-term gains
    double long_term_gains;           ///< Total long-term gains
    double short_term_losses;         ///< Total short-term losses
    double long_term_losses;          ///< Total long-term losses
    double net_short_term;            ///< Net short-term gain/loss
    double net_long_term;             ///< Net long-term gain/loss
    double harvested_losses;          ///< Losses intentionally harvested
    double estimated_tax_savings;     ///< Estimated tax savings from harvesting
    std::vector<core::Trade> trades;  ///< Trades that generated gains/losses

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Tax-aware optimization helper
class TaxOptimizer {
public:
    TaxOptimizer(TaxLotManager& lot_manager, double tax_rate_short = 0.35,
                 double tax_rate_long = 0.15);
    ~TaxOptimizer();

    /// Find tax-loss harvesting opportunities
    std::vector<TaxHarvestingOpportunity> findHarvestingOpportunities(
        const std::map<std::string, double>& current_prices,
        double min_loss_threshold = 100.0) const;

    /// Check if a trade would trigger wash sale
    bool wouldTriggerWashSale(const TaxLot& lot_to_sell,
                              const std::string& replacement_ticker,
                              const std::chrono::system_clock::time_point& trade_date) const;

    /// Check proposed trades for wash sale violations
    std::vector<WashSaleViolation> checkWashSaleViolations(
        const std::vector<core::Trade>& proposed_trades) const;

    /// Select optimal lots for tax efficiency
    std::vector<TaxLot> selectOptimalLots(const std::string& ticker, double shares_to_sell,
                                           double current_price, bool prefer_losses = true) const;

    /// Calculate capital gains report for trades
    CapitalGainsReport calculateCapitalGains(
        const std::vector<core::Trade>& trades,
        const std::map<std::string, double>& prices) const;

    /// Estimate tax impact of trades
    double estimateTaxImpact(const std::vector<core::Trade>& trades,
                             const std::map<std::string, double>& prices) const;

    /// Set tax rates
    void setTaxRates(double short_term, double long_term);

    /// Set replacement candidates for tickers (similar ETFs)
    void setReplacementCandidates(const std::string& ticker,
                                  const std::vector<std::string>& candidates);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lumen::data
