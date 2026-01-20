#pragma once

/// @file portfolio.hpp
/// @brief Portfolio data structures and management
///
/// This module defines the core data structures for representing investment
/// portfolios, individual positions, and target allocations.

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lumen::core {

/// Asset class categories for portfolio positions
enum class AssetClass {
    STOCKS,
    BONDS,
    CASH,
    COMMODITIES,
    REAL_ESTATE,
    CRYPTO,
    OTHER
};

/// Convert AssetClass to string representation
std::string assetClassToString(AssetClass ac);

/// Parse AssetClass from string
AssetClass assetClassFromString(const std::string& str);

/// Represents a single position (holding) in a portfolio
struct Position {
    std::string ticker;       ///< Security symbol (e.g., "AAPL")
    double shares;            ///< Number of shares held
    double current_price;     ///< Current market price per share
    double cost_basis;        ///< Average cost basis per share
    std::chrono::system_clock::time_point purchase_date;  ///< Date of purchase
    AssetClass asset_class;   ///< Asset classification
    std::string exchange;     ///< Exchange (e.g., "NASDAQ")
    bool supports_fractional; ///< Whether fractional shares are allowed

    /// Calculate current market value
    double getCurrentValue() const;

    /// Calculate total cost basis
    double getCostBasisTotal() const;

    /// Calculate unrealized gain/loss
    double getUnrealizedGain() const;

    /// Calculate unrealized gain/loss as percentage
    double getUnrealizedGainPercent() const;

    /// Check if position qualifies for long-term capital gains (held > 1 year)
    bool isLongTerm() const;

    /// Serialize to JSON
    nlohmann::json toJSON() const;

    /// Deserialize from JSON
    static Position fromJSON(const nlohmann::json& j);
};

/// Allocation mode for target specification
enum class AllocationMode {
    PERCENTAGE,    ///< Target as percentage of portfolio (0.0 to 1.0)
    DOLLAR_AMOUNT  ///< Target as fixed dollar amount
};

/// Represents a target allocation specification
struct AllocationTarget {
    std::string identifier;  ///< Ticker or asset class name
    double target_value;     ///< Target % or $ amount
    double lower_bound;      ///< Minimum acceptable value (for bands)
    double upper_bound;      ///< Maximum acceptable value (for bands)
    bool is_asset_class;     ///< true = asset class, false = specific ticker
};

/// Target allocation specification for portfolio rebalancing
class TargetAllocation {
public:
    TargetAllocation() = default;
    explicit TargetAllocation(AllocationMode mode);

    /// Set a target allocation with default tolerance
    void setTarget(const std::string& identifier, double target,
                   double tolerance = 0.05, bool is_asset_class = false);

    /// Set a target allocation with explicit bounds
    void setTargetWithBounds(const std::string& identifier, double target,
                             double lower, double upper, bool is_asset_class = false);

    /// Remove a target
    void removeTarget(const std::string& identifier);

    /// Get target value for identifier
    double getTarget(const std::string& identifier) const;

    /// Get full target specification
    AllocationTarget getFullTarget(const std::string& identifier) const;

    /// Get all targets
    std::vector<AllocationTarget> getAllTargets() const;

    /// Get allocation mode
    AllocationMode getMode() const { return mode_; }

    /// Validate that targets are consistent (e.g., sum to 100%)
    bool validate() const;

    /// Get validation error messages
    std::vector<std::string> getValidationErrors() const;

    /// Serialize to JSON
    nlohmann::json toJSON() const;

    /// Deserialize from JSON
    static TargetAllocation fromJSON(const nlohmann::json& j);

private:
    AllocationMode mode_ = AllocationMode::PERCENTAGE;
    std::map<std::string, AllocationTarget> targets_;
    double global_tolerance_ = 0.05;
};

/// Represents an investment portfolio
class Portfolio {
public:
    Portfolio() = default;
    explicit Portfolio(const std::string& name);

    // Position management
    void addPosition(const Position& pos);
    void addPosition(const std::string& ticker, double shares, double cost_basis);
    void removePosition(const std::string& ticker);
    void updatePosition(const std::string& ticker, double new_shares);
    void updatePrice(const std::string& ticker, double new_price);
    void updateAllPrices(const std::map<std::string, double>& prices);

    // Accessors
    const Position& getPosition(const std::string& ticker) const;
    std::vector<Position> getAllPositions() const;
    std::vector<std::string> getAllTickers() const;
    size_t getPositionCount() const { return positions_.size(); }
    bool hasPosition(const std::string& ticker) const;

    // Portfolio metrics
    double getTotalValue() const;
    double getTotalCostBasis() const;
    double getTotalUnrealizedGain() const;
    double getCashBalance() const { return cash_balance_; }
    void setCashBalance(double cash) { cash_balance_ = cash; }

    // Allocation analysis
    double getAllocationPercent(const std::string& ticker) const;
    std::map<std::string, double> getAllocationMap() const;
    std::map<AssetClass, double> getAssetClassExposure() const;
    double getAssetClassPercent(AssetClass ac) const;

    // Deviation from target
    double calculateDrift(const TargetAllocation& target) const;
    std::map<std::string, double> getDeviationMap(const TargetAllocation& target) const;

    // Check if rebalancing is needed
    bool needsRebalancing(const TargetAllocation& target) const;

    // I/O
    nlohmann::json toJSON() const;
    static Portfolio fromJSON(const nlohmann::json& j);
    static Portfolio fromCSV(const std::string& filepath);
    void saveToFile(const std::string& filepath) const;

    // Metadata
    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }
    const std::string& getCurrency() const { return currency_; }
    void setCurrency(const std::string& currency) { currency_ = currency; }

private:
    std::string name_;
    std::string currency_ = "USD";
    std::map<std::string, Position> positions_;
    double cash_balance_ = 0.0;
    std::chrono::system_clock::time_point last_updated_;
};

}  // namespace lumen::core
