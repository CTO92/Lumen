#pragma once

/// @file constraint.hpp
/// @brief Constraint definitions for portfolio optimization
///
/// This module defines the constraint system used to formulate portfolio
/// optimization problems. Constraints are translated into mathematical
/// expressions for the solver.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/core/portfolio.hpp"

namespace lumen::data {
// Forward declarations for tax types
class TaxLotManager;
class TaxOptimizer;
enum class TaxLotMethod;
}  // namespace lumen::data

namespace lumen::core {

// Forward declarations
struct Trade;

/// Types of constraints supported by the optimizer
enum class ConstraintType {
    BUDGET,          ///< Total portfolio value constraint
    ALLOCATION,      ///< Min/max allocation bounds
    MIN_TRADE,       ///< Minimum trade size
    INTEGER_SHARES,  ///< Whole shares only
    TAX_LOT,         ///< Tax lot selection constraint
    WASH_SALE,       ///< Wash sale rule compliance
    POSITION_LIMIT,  ///< Maximum position size
    SECTOR_LIMIT,    ///< Sector exposure limit
    CUSTOM           ///< User-defined constraint
};

/// Status of a constraint after solving
enum class ConstraintStatus {
    ACTIVE,    ///< Constraint is enforced
    BINDING,   ///< Constraint is at its limit (slack = 0)
    SLACK,     ///< Constraint has room (not at limit)
    VIOLATED,  ///< Constraint could not be satisfied
    DISABLED   ///< Constraint is turned off
};

/// Convert ConstraintType to string
std::string constraintTypeToString(ConstraintType type);

/// Convert ConstraintStatus to string
std::string constraintStatusToString(ConstraintStatus status);

/// Abstract base class for all constraints
class Constraint {
public:
    virtual ~Constraint() = default;

    /// Get the type of this constraint
    virtual ConstraintType getType() const = 0;

    /// Get a human-readable name for this constraint
    virtual std::string getName() const = 0;

    /// Get a description of what this constraint does
    virtual std::string getDescription() const = 0;

    /// Check if the constraint parameters are valid
    virtual bool isValid() const = 0;

    /// Check if a set of trades satisfies this constraint
    virtual bool isSatisfied(const Portfolio& portfolio,
                             const std::vector<Trade>& trades) const = 0;

    /// Serialize to JSON
    virtual nlohmann::json toJSON() const = 0;

    /// Get current status
    ConstraintStatus getStatus() const { return status_; }

    /// Set status (called after solving)
    void setStatus(ConstraintStatus s) { status_ = s; }

    /// Get slack value (distance from binding)
    double getSlackValue() const { return slack_value_; }

    /// Set slack value (called after solving)
    void setSlackValue(double s) { slack_value_ = s; }

protected:
    ConstraintStatus status_ = ConstraintStatus::ACTIVE;
    double slack_value_ = 0.0;
};

/// Budget constraint: enforces total portfolio value
class BudgetConstraint : public Constraint {
public:
    BudgetConstraint(double total_budget, double min_cash_reserve = 0.0);

    ConstraintType getType() const override { return ConstraintType::BUDGET; }
    std::string getName() const override { return "Budget"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    double getTotalBudget() const { return total_budget_; }
    double getMinCashReserve() const { return min_cash_reserve_; }

private:
    double total_budget_;
    double min_cash_reserve_;
};

/// Allocation constraint: enforces min/max bounds on a position or asset class
class AllocationConstraint : public Constraint {
public:
    AllocationConstraint(const std::string& identifier, double lower_bound,
                         double upper_bound, bool is_asset_class = false);

    ConstraintType getType() const override { return ConstraintType::ALLOCATION; }
    std::string getName() const override;
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    const std::string& getIdentifier() const { return identifier_; }
    double getLowerBound() const { return lower_bound_; }
    double getUpperBound() const { return upper_bound_; }
    bool isAssetClass() const { return is_asset_class_; }

private:
    std::string identifier_;
    double lower_bound_;
    double upper_bound_;
    bool is_asset_class_;
};

/// Minimum trade size constraint: prevents small trades
class MinTradeSizeConstraint : public Constraint {
public:
    MinTradeSizeConstraint(double min_amount = 0.0, double min_shares = 0.0);

    ConstraintType getType() const override { return ConstraintType::MIN_TRADE; }
    std::string getName() const override { return "Minimum Trade Size"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    double getMinAmount() const { return min_trade_amount_; }
    double getMinShares() const { return min_trade_shares_; }

private:
    double min_trade_amount_;
    double min_trade_shares_;
};

/// Integer share constraint: enforces whole shares
class IntegerShareConstraint : public Constraint {
public:
    IntegerShareConstraint(bool enforce = true,
                           std::set<std::string> exempt_tickers = {});

    ConstraintType getType() const override { return ConstraintType::INTEGER_SHARES; }
    std::string getName() const override { return "Integer Shares"; }
    std::string getDescription() const override;

    bool isValid() const override { return true; }
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    bool isEnforced() const { return enforce_; }
    bool isExempt(const std::string& ticker) const;

private:
    bool enforce_;
    std::set<std::string> exempt_tickers_;
};

/// Position limit constraint: maximum allocation per position
class PositionLimitConstraint : public Constraint {
public:
    explicit PositionLimitConstraint(double max_position_percent = 0.25);

    ConstraintType getType() const override { return ConstraintType::POSITION_LIMIT; }
    std::string getName() const override { return "Max Position Size"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    double getMaxPositionPercent() const { return max_position_percent_; }

private:
    double max_position_percent_;
};

/// Tax lot selection constraint: enforces specific lot selection method for sales
class TaxLotConstraint : public Constraint {
public:
    /// Create a tax lot constraint
    /// @param lot_manager Reference to the tax lot manager
    /// @param method The lot selection method to enforce
    /// @param prefer_long_term If true, prefer long-term lots when multiple qualify
    TaxLotConstraint(data::TaxLotManager* lot_manager,
                     data::TaxLotMethod method,
                     bool prefer_long_term = true);

    ConstraintType getType() const override { return ConstraintType::TAX_LOT; }
    std::string getName() const override { return "Tax Lot Selection"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    data::TaxLotMethod getMethod() const { return method_; }
    bool prefersLongTerm() const { return prefer_long_term_; }

    /// Get the recommended lots for a sale
    std::vector<std::string> getRecommendedLotIds(const std::string& ticker,
                                                   double shares_to_sell) const;

private:
    data::TaxLotManager* lot_manager_;
    data::TaxLotMethod method_;
    bool prefer_long_term_;
};

/// Wash sale rule constraint: prevents trades that would trigger wash sales
class WashSaleConstraint : public Constraint {
public:
    /// Create a wash sale constraint
    /// @param optimizer Reference to the tax optimizer (for wash sale checking)
    /// @param block_violations If true, disallow trades that trigger wash sales
    /// @param warn_only If true, only warn but don't block
    WashSaleConstraint(data::TaxOptimizer* optimizer,
                       bool block_violations = true,
                       bool warn_only = false);

    ConstraintType getType() const override { return ConstraintType::WASH_SALE; }
    std::string getName() const override { return "Wash Sale Rule"; }
    std::string getDescription() const override;

    bool isValid() const override;
    bool isSatisfied(const Portfolio& portfolio,
                     const std::vector<Trade>& trades) const override;

    nlohmann::json toJSON() const override;

    bool blocksViolations() const { return block_violations_; }
    bool isWarnOnly() const { return warn_only_; }

    /// Get potential wash sale violations for proposed trades
    std::vector<std::string> getPotentialViolations(
        const std::vector<Trade>& trades) const;

private:
    data::TaxOptimizer* optimizer_;
    bool block_violations_;
    bool warn_only_;
};

/// Collection of constraints for an optimization problem
class ConstraintSet {
public:
    ConstraintSet() = default;

    /// Add a constraint to the set
    void addConstraint(std::unique_ptr<Constraint> constraint);

    /// Remove a constraint by name
    void removeConstraint(const std::string& name);

    /// Clear all constraints
    void clearAll();

    /// Get a constraint by name
    const Constraint* getConstraint(const std::string& name) const;

    /// Get all constraints
    std::vector<const Constraint*> getAllConstraints() const;

    /// Get constraints of a specific type
    std::vector<const Constraint*> getConstraintsByType(ConstraintType type) const;

    /// Get number of constraints
    size_t getConstraintCount() const { return constraints_.size(); }

    /// Check if constraints are consistent (no conflicts)
    bool validateConsistency() const;

    /// Get list of inconsistencies
    std::vector<std::string> getInconsistencies() const;

    /// Check if all constraints are satisfied by the trades
    bool areAllSatisfied(const Portfolio& portfolio,
                         const std::vector<Trade>& trades) const;

    /// Get constraints that are currently active
    std::vector<const Constraint*> getActiveConstraints() const;

    /// Get constraints that are binding (at limit)
    std::vector<const Constraint*> getBindingConstraints() const;

    /// Get constraints that were violated
    std::vector<const Constraint*> getViolatedConstraints() const;

    /// Serialize to JSON
    nlohmann::json toJSON() const;

    /// Deserialize from JSON
    static ConstraintSet fromJSON(const nlohmann::json& j);

private:
    std::vector<std::unique_ptr<Constraint>> constraints_;
};

}  // namespace lumen::core
