/// @file constraint.cpp
/// @brief Constraint implementation
///
/// Implementation of constraint classes for portfolio optimization.

#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/data/tax_lot.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace lumen::core {

std::string constraintTypeToString(ConstraintType type) {
    switch (type) {
        case ConstraintType::BUDGET: return "budget";
        case ConstraintType::ALLOCATION: return "allocation";
        case ConstraintType::MIN_TRADE: return "min_trade";
        case ConstraintType::INTEGER_SHARES: return "integer_shares";
        case ConstraintType::TAX_LOT: return "tax_lot";
        case ConstraintType::WASH_SALE: return "wash_sale";
        case ConstraintType::POSITION_LIMIT: return "position_limit";
        case ConstraintType::SECTOR_LIMIT: return "sector_limit";
        case ConstraintType::CUSTOM: return "custom";
        default: return "unknown";
    }
}

std::string constraintStatusToString(ConstraintStatus status) {
    switch (status) {
        case ConstraintStatus::ACTIVE: return "active";
        case ConstraintStatus::BINDING: return "binding";
        case ConstraintStatus::SLACK: return "slack";
        case ConstraintStatus::VIOLATED: return "violated";
        case ConstraintStatus::DISABLED: return "disabled";
        default: return "unknown";
    }
}

// =============================================================================
// BudgetConstraint
// =============================================================================

BudgetConstraint::BudgetConstraint(double total_budget, double min_cash_reserve)
    : total_budget_(total_budget), min_cash_reserve_(min_cash_reserve) {
    // Validate at construction time
    if (total_budget <= 0.0) {
        throw std::invalid_argument("BudgetConstraint: total_budget must be positive");
    }
    if (min_cash_reserve < 0.0) {
        throw std::invalid_argument("BudgetConstraint: min_cash_reserve cannot be negative");
    }
    if (min_cash_reserve >= total_budget) {
        throw std::invalid_argument("BudgetConstraint: min_cash_reserve must be less than total_budget");
    }
    // Sanity check for reasonable values
    if (total_budget > 1e15) {  // $1 quadrillion
        throw std::invalid_argument("BudgetConstraint: total_budget exceeds reasonable limit");
    }
}

std::string BudgetConstraint::getDescription() const {
    return "Total portfolio value must equal $" + std::to_string(total_budget_) +
           " with minimum cash reserve of $" + std::to_string(min_cash_reserve_);
}

bool BudgetConstraint::isValid() const {
    return total_budget_ > 0 && min_cash_reserve_ >= 0 && min_cash_reserve_ < total_budget_;
}

bool BudgetConstraint::isSatisfied(const Portfolio& portfolio,
                                    const std::vector<Trade>& trades) const {
    // Calculate portfolio value after trades
    double current_value = portfolio.getTotalValue();
    double trade_cost = 0.0;
    double cash_change = 0.0;

    for (const auto& trade : trades) {
        trade_cost += trade.transaction_cost;
        if (trade.action == Trade::Action::BUY) {
            cash_change -= trade.amount;
        } else if (trade.action == Trade::Action::SELL) {
            cash_change += trade.amount;
        }
    }

    // Self-funding assumption: buys should equal sells (approximately)
    // Final cash = current cash + cash_change - transaction_costs
    double final_cash = portfolio.getCashBalance() + cash_change - trade_cost;

    // Check cash reserve requirement
    if (final_cash < min_cash_reserve_) {
        return false;
    }

    // Check total budget (with small tolerance for floating point)
    constexpr double tolerance = 1e-6;
    if (std::abs(current_value - total_budget_) > tolerance * total_budget_) {
        // Portfolio value doesn't match budget
        return false;
    }

    return true;
}

nlohmann::json BudgetConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "budget"},
        {"total_budget", total_budget_},
        {"min_cash_reserve", min_cash_reserve_}
    };
}

// =============================================================================
// AllocationConstraint
// =============================================================================

AllocationConstraint::AllocationConstraint(const std::string& identifier, double lower_bound,
                                            double upper_bound, bool is_asset_class)
    : identifier_(identifier), lower_bound_(lower_bound), upper_bound_(upper_bound),
      is_asset_class_(is_asset_class) {
    // Validate bounds at construction time
    if (lower_bound < 0.0) {
        throw std::invalid_argument("AllocationConstraint: lower_bound cannot be negative");
    }
    if (upper_bound > 1.0) {
        throw std::invalid_argument("AllocationConstraint: upper_bound cannot exceed 1.0 (100%)");
    }
    if (lower_bound > upper_bound) {
        throw std::invalid_argument("AllocationConstraint: lower_bound cannot exceed upper_bound");
    }
    if (identifier.empty()) {
        throw std::invalid_argument("AllocationConstraint: identifier cannot be empty");
    }
    // Limit identifier length
    if (identifier.length() > 100) {
        throw std::invalid_argument("AllocationConstraint: identifier too long (max 100 chars)");
    }
}

std::string AllocationConstraint::getName() const {
    return "Allocation[" + identifier_ + "]";
}

std::string AllocationConstraint::getDescription() const {
    return identifier_ + " allocation must be between " +
           std::to_string(lower_bound_ * 100) + "% and " +
           std::to_string(upper_bound_ * 100) + "%";
}

bool AllocationConstraint::isValid() const {
    return lower_bound_ >= 0 && upper_bound_ <= 1 && lower_bound_ <= upper_bound_;
}

bool AllocationConstraint::isSatisfied(const Portfolio& portfolio,
                                        const std::vector<Trade>& trades) const {
    // Build a map of final positions after trades
    std::map<std::string, double> final_values;
    double total_value = portfolio.getTotalValue();

    // Start with current positions
    for (const auto& pos : portfolio.getAllPositions()) {
        final_values[pos.ticker] = pos.getCurrentValue();
    }

    // Apply trades
    for (const auto& trade : trades) {
        if (trade.action == Trade::Action::BUY) {
            final_values[trade.ticker] += trade.amount;
        } else if (trade.action == Trade::Action::SELL) {
            final_values[trade.ticker] -= trade.amount;
        }
    }

    double allocation = 0.0;

    if (is_asset_class_) {
        // Calculate allocation for the asset class
        AssetClass ac = assetClassFromString(identifier_);
        for (const auto& pos : portfolio.getAllPositions()) {
            if (pos.asset_class == ac) {
                auto it = final_values.find(pos.ticker);
                if (it != final_values.end()) {
                    allocation += it->second;
                }
            }
        }
    } else {
        // Calculate allocation for specific ticker
        auto it = final_values.find(identifier_);
        if (it != final_values.end()) {
            allocation = it->second;
        }
    }

    // Convert to percentage
    if (total_value > 0) {
        allocation /= total_value;
    }

    // Check bounds with small tolerance
    constexpr double tolerance = 1e-6;
    return (allocation >= lower_bound_ - tolerance) &&
           (allocation <= upper_bound_ + tolerance);
}

nlohmann::json AllocationConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "allocation"},
        {"identifier", identifier_},
        {"lower_bound", lower_bound_},
        {"upper_bound", upper_bound_},
        {"is_asset_class", is_asset_class_}
    };
}

// =============================================================================
// MinTradeSizeConstraint
// =============================================================================

MinTradeSizeConstraint::MinTradeSizeConstraint(double min_amount, double min_shares)
    : min_trade_amount_(min_amount), min_trade_shares_(min_shares) {}

std::string MinTradeSizeConstraint::getDescription() const {
    return "Minimum trade size: $" + std::to_string(min_trade_amount_) +
           " or " + std::to_string(min_trade_shares_) + " shares";
}

bool MinTradeSizeConstraint::isValid() const {
    return min_trade_amount_ >= 0 && min_trade_shares_ >= 0;
}

bool MinTradeSizeConstraint::isSatisfied(const Portfolio& portfolio,
                                          const std::vector<Trade>& trades) const {
    for (const auto& trade : trades) {
        // HOLD actions don't need to meet minimum
        if (trade.action == Trade::Action::HOLD) {
            continue;
        }

        // Check minimum amount
        if (min_trade_amount_ > 0 && trade.amount < min_trade_amount_) {
            return false;
        }

        // Check minimum shares
        if (min_trade_shares_ > 0 && trade.shares < min_trade_shares_) {
            return false;
        }
    }
    return true;
}

nlohmann::json MinTradeSizeConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "min_trade"},
        {"min_amount", min_trade_amount_},
        {"min_shares", min_trade_shares_}
    };
}

// =============================================================================
// IntegerShareConstraint
// =============================================================================

IntegerShareConstraint::IntegerShareConstraint(bool enforce, std::set<std::string> exempt_tickers)
    : enforce_(enforce), exempt_tickers_(std::move(exempt_tickers)) {}

std::string IntegerShareConstraint::getDescription() const {
    if (!enforce_) return "Integer share constraint disabled";
    return "All trades must be for whole shares (except exempt tickers)";
}

bool IntegerShareConstraint::isSatisfied(const Portfolio& portfolio,
                                          const std::vector<Trade>& trades) const {
    if (!enforce_) {
        return true;  // Constraint disabled
    }

    for (const auto& trade : trades) {
        // Skip HOLD actions
        if (trade.action == Trade::Action::HOLD) {
            continue;
        }

        // Skip exempt tickers
        if (isExempt(trade.ticker)) {
            continue;
        }

        // Check if shares is an integer (with small tolerance)
        double fractional_part = trade.shares - std::floor(trade.shares);
        constexpr double tolerance = 1e-6;
        if (fractional_part > tolerance && fractional_part < (1.0 - tolerance)) {
            return false;
        }
    }
    return true;
}

nlohmann::json IntegerShareConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "integer_shares"},
        {"enforce", enforce_},
        {"exempt_tickers", exempt_tickers_}
    };
}

bool IntegerShareConstraint::isExempt(const std::string& ticker) const {
    return exempt_tickers_.find(ticker) != exempt_tickers_.end();
}

// =============================================================================
// PositionLimitConstraint
// =============================================================================

PositionLimitConstraint::PositionLimitConstraint(double max_position_percent)
    : max_position_percent_(max_position_percent) {
    // Validate at construction time
    if (max_position_percent <= 0.0) {
        throw std::invalid_argument("PositionLimitConstraint: max_position_percent must be positive");
    }
    if (max_position_percent > 1.0) {
        throw std::invalid_argument("PositionLimitConstraint: max_position_percent cannot exceed 1.0 (100%)");
    }
}

std::string PositionLimitConstraint::getDescription() const {
    return "No single position may exceed " +
           std::to_string(max_position_percent_ * 100) + "% of portfolio";
}

bool PositionLimitConstraint::isValid() const {
    return max_position_percent_ > 0 && max_position_percent_ <= 1;
}

bool PositionLimitConstraint::isSatisfied(const Portfolio& portfolio,
                                           const std::vector<Trade>& trades) const {
    // Build a map of final positions after trades
    std::map<std::string, double> final_values;
    double total_value = portfolio.getTotalValue();

    if (total_value <= 0) {
        return true;  // Empty portfolio
    }

    // Start with current positions
    for (const auto& pos : portfolio.getAllPositions()) {
        final_values[pos.ticker] = pos.getCurrentValue();
    }

    // Apply trades
    for (const auto& trade : trades) {
        if (trade.action == Trade::Action::BUY) {
            final_values[trade.ticker] += trade.amount;
        } else if (trade.action == Trade::Action::SELL) {
            final_values[trade.ticker] -= trade.amount;
        }
    }

    // Check each position against limit
    constexpr double tolerance = 1e-6;
    for (const auto& [ticker, value] : final_values) {
        double allocation = value / total_value;
        if (allocation > max_position_percent_ + tolerance) {
            return false;
        }
    }

    return true;
}

nlohmann::json PositionLimitConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "position_limit"},
        {"max_position_percent", max_position_percent_}
    };
}

// =============================================================================
// TaxLotConstraint
// =============================================================================

TaxLotConstraint::TaxLotConstraint(data::TaxLotManager* lot_manager,
                                   data::TaxLotMethod method,
                                   bool prefer_long_term)
    : lot_manager_(lot_manager), method_(method), prefer_long_term_(prefer_long_term) {
    if (!lot_manager) {
        throw std::invalid_argument("TaxLotConstraint: lot_manager cannot be null");
    }
}

std::string TaxLotConstraint::getDescription() const {
    std::string method_str = data::taxLotMethodToString(method_);
    std::string desc = "Tax lot selection using " + method_str + " method";
    if (prefer_long_term_) {
        desc += " (preferring long-term lots)";
    }
    return desc;
}

bool TaxLotConstraint::isValid() const {
    return lot_manager_ != nullptr;
}

bool TaxLotConstraint::isSatisfied(const Portfolio& portfolio,
                                    const std::vector<Trade>& trades) const {
    // For each SELL trade, verify that the lots being sold match the constraint method
    // This is a soft constraint that guides the optimizer
    // In practice, the optimizer should use getRecommendedLotIds() to select lots

    for (const auto& trade : trades) {
        if (trade.action != Trade::Action::SELL) {
            continue;
        }

        // Get the recommended lots for this sale
        auto recommended = getRecommendedLotIds(trade.ticker, trade.shares);

        // If trade has lot_ids specified (via metadata), check they match
        // For now, we consider this satisfied as long as lots are available
        auto lots = lot_manager_->getLotsForTicker(trade.ticker);
        double available_shares = 0.0;
        for (const auto& lot : lots) {
            if (!lot.isSold()) {
                available_shares += lot.shares;
            }
        }

        if (available_shares < trade.shares) {
            return false;  // Not enough shares available
        }
    }

    return true;
}

nlohmann::json TaxLotConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "tax_lot"},
        {"method", data::taxLotMethodToString(method_)},
        {"prefer_long_term", prefer_long_term_}
    };
}

std::vector<std::string> TaxLotConstraint::getRecommendedLotIds(
    const std::string& ticker, double shares_to_sell) const {

    auto lots = lot_manager_->selectLotsToSell(ticker, shares_to_sell, method_);

    std::vector<std::string> lot_ids;
    lot_ids.reserve(lots.size());
    for (const auto& lot : lots) {
        lot_ids.push_back(lot.id);
    }

    return lot_ids;
}

// =============================================================================
// WashSaleConstraint
// =============================================================================

WashSaleConstraint::WashSaleConstraint(data::TaxOptimizer* optimizer,
                                       bool block_violations,
                                       bool warn_only)
    : optimizer_(optimizer), block_violations_(block_violations), warn_only_(warn_only) {
    if (!optimizer && block_violations) {
        throw std::invalid_argument("WashSaleConstraint: optimizer cannot be null when blocking violations");
    }
}

std::string WashSaleConstraint::getDescription() const {
    if (warn_only_) {
        return "Wash sale rule: warn about potential violations";
    } else if (block_violations_) {
        return "Wash sale rule: block trades that would trigger wash sales";
    } else {
        return "Wash sale rule: monitoring only";
    }
}

bool WashSaleConstraint::isValid() const {
    if (block_violations_ && !optimizer_) {
        return false;
    }
    return true;
}

bool WashSaleConstraint::isSatisfied(const Portfolio& portfolio,
                                      const std::vector<Trade>& trades) const {
    if (!optimizer_ || !block_violations_) {
        return true;  // No enforcement if optimizer not set or not blocking
    }

    // Check for wash sale violations
    auto violations = optimizer_->checkWashSaleViolations(trades);

    if (warn_only_) {
        // Just log/track violations, don't fail
        return true;
    }

    // If blocking violations and any found, constraint is not satisfied
    return violations.empty();
}

nlohmann::json WashSaleConstraint::toJSON() const {
    return nlohmann::json{
        {"type", "wash_sale"},
        {"block_violations", block_violations_},
        {"warn_only", warn_only_}
    };
}

std::vector<std::string> WashSaleConstraint::getPotentialViolations(
    const std::vector<Trade>& trades) const {

    std::vector<std::string> descriptions;

    if (!optimizer_) {
        return descriptions;
    }

    auto violations = optimizer_->checkWashSaleViolations(trades);
    descriptions.reserve(violations.size());

    for (const auto& violation : violations) {
        descriptions.push_back(violation.description);
    }

    return descriptions;
}

// =============================================================================
// ConstraintSet
// =============================================================================

void ConstraintSet::addConstraint(std::unique_ptr<Constraint> constraint) {
    constraints_.push_back(std::move(constraint));
}

void ConstraintSet::removeConstraint(const std::string& name) {
    constraints_.erase(
        std::remove_if(constraints_.begin(), constraints_.end(),
                       [&name](const std::unique_ptr<Constraint>& c) {
                           return c->getName() == name;
                       }),
        constraints_.end());
}

void ConstraintSet::clearAll() {
    constraints_.clear();
}

const Constraint* ConstraintSet::getConstraint(const std::string& name) const {
    for (const auto& c : constraints_) {
        if (c->getName() == name) return c.get();
    }
    return nullptr;
}

std::vector<const Constraint*> ConstraintSet::getAllConstraints() const {
    std::vector<const Constraint*> result;
    result.reserve(constraints_.size());
    for (const auto& c : constraints_) {
        result.push_back(c.get());
    }
    return result;
}

std::vector<const Constraint*> ConstraintSet::getConstraintsByType(ConstraintType type) const {
    std::vector<const Constraint*> result;
    for (const auto& c : constraints_) {
        if (c->getType() == type) result.push_back(c.get());
    }
    return result;
}

bool ConstraintSet::validateConsistency() const {
    return getInconsistencies().empty();
}

std::vector<std::string> ConstraintSet::getInconsistencies() const {
    std::vector<std::string> issues;

    // Collect all allocation constraints to check for conflicts
    std::map<std::string, std::pair<double, double>> allocation_bounds;

    for (const auto& c : constraints_) {
        if (!c->isValid()) {
            issues.push_back("Invalid constraint: " + c->getName());
        }

        // Check for conflicting allocation constraints
        if (c->getType() == ConstraintType::ALLOCATION) {
            const auto* alloc = dynamic_cast<const AllocationConstraint*>(c.get());
            if (alloc) {
                const std::string& id = alloc->getIdentifier();
                auto it = allocation_bounds.find(id);
                if (it != allocation_bounds.end()) {
                    // Check for conflict with existing bounds
                    double existing_lb = it->second.first;
                    double existing_ub = it->second.second;
                    double new_lb = alloc->getLowerBound();
                    double new_ub = alloc->getUpperBound();

                    // Bounds are inconsistent if they don't overlap
                    if (new_lb > existing_ub || new_ub < existing_lb) {
                        std::ostringstream oss;
                        oss << "Conflicting allocation bounds for " << id
                            << ": [" << existing_lb << ", " << existing_ub << "] vs ["
                            << new_lb << ", " << new_ub << "]";
                        issues.push_back(oss.str());
                    } else {
                        // Tighten bounds
                        it->second.first = std::max(existing_lb, new_lb);
                        it->second.second = std::min(existing_ub, new_ub);
                    }
                } else {
                    allocation_bounds[id] = {alloc->getLowerBound(), alloc->getUpperBound()};
                }
            }
        }
    }

    // Check that allocation bounds sum is feasible
    double min_sum = 0.0;
    double max_sum = 0.0;
    for (const auto& [id, bounds] : allocation_bounds) {
        min_sum += bounds.first;
        max_sum += bounds.second;
    }

    // Sum of lower bounds should be <= 1.0, sum of upper bounds should be >= 1.0
    // to have a feasible allocation
    if (min_sum > 1.0 + 1e-6) {
        std::ostringstream oss;
        oss << "Sum of minimum allocations (" << min_sum * 100
            << "%) exceeds 100% - constraints are infeasible";
        issues.push_back(oss.str());
    }

    if (!allocation_bounds.empty() && max_sum < 1.0 - 1e-6) {
        std::ostringstream oss;
        oss << "Sum of maximum allocations (" << max_sum * 100
            << "%) is less than 100% - constraints are infeasible";
        issues.push_back(oss.str());
    }

    return issues;
}

bool ConstraintSet::areAllSatisfied(const Portfolio& portfolio,
                                     const std::vector<Trade>& trades) const {
    for (const auto& c : constraints_) {
        if (!c->isSatisfied(portfolio, trades)) return false;
    }
    return true;
}

std::vector<const Constraint*> ConstraintSet::getActiveConstraints() const {
    std::vector<const Constraint*> result;
    for (const auto& c : constraints_) {
        if (c->getStatus() == ConstraintStatus::ACTIVE ||
            c->getStatus() == ConstraintStatus::BINDING) {
            result.push_back(c.get());
        }
    }
    return result;
}

std::vector<const Constraint*> ConstraintSet::getBindingConstraints() const {
    std::vector<const Constraint*> result;
    for (const auto& c : constraints_) {
        if (c->getStatus() == ConstraintStatus::BINDING) {
            result.push_back(c.get());
        }
    }
    return result;
}

std::vector<const Constraint*> ConstraintSet::getViolatedConstraints() const {
    std::vector<const Constraint*> result;
    for (const auto& c : constraints_) {
        if (c->getStatus() == ConstraintStatus::VIOLATED) {
            result.push_back(c.get());
        }
    }
    return result;
}

nlohmann::json ConstraintSet::toJSON() const {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& c : constraints_) {
        j.push_back(c->toJSON());
    }
    return j;
}

ConstraintSet ConstraintSet::fromJSON(const nlohmann::json& j) {
    ConstraintSet set;

    // Limit number of constraints to prevent DoS
    constexpr size_t MAX_CONSTRAINTS = 1000;
    if (j.size() > MAX_CONSTRAINTS) {
        throw std::runtime_error("ConstraintSet::fromJSON: too many constraints (max " +
            std::to_string(MAX_CONSTRAINTS) + ")");
    }

    for (const auto& item : j) {
        if (!item.contains("type") || !item["type"].is_string()) {
            throw std::runtime_error("ConstraintSet::fromJSON: missing or invalid 'type' field");
        }

        std::string type = item.at("type").get<std::string>();

        try {
            if (type == "budget") {
                if (!item.contains("total_budget") || !item["total_budget"].is_number()) {
                    throw std::runtime_error("budget constraint requires numeric 'total_budget'");
                }
                double budget = item.at("total_budget").get<double>();
                double reserve = item.value("min_cash_reserve", 0.0);
                set.addConstraint(std::make_unique<BudgetConstraint>(budget, reserve));

            } else if (type == "allocation") {
                if (!item.contains("identifier") || !item["identifier"].is_string()) {
                    throw std::runtime_error("allocation constraint requires string 'identifier'");
                }
                if (!item.contains("lower_bound") || !item["lower_bound"].is_number()) {
                    throw std::runtime_error("allocation constraint requires numeric 'lower_bound'");
                }
                if (!item.contains("upper_bound") || !item["upper_bound"].is_number()) {
                    throw std::runtime_error("allocation constraint requires numeric 'upper_bound'");
                }

                std::string id = item.at("identifier").get<std::string>();
                double lb = item.at("lower_bound").get<double>();
                double ub = item.at("upper_bound").get<double>();
                bool is_asset_class = item.value("is_asset_class", false);
                set.addConstraint(std::make_unique<AllocationConstraint>(id, lb, ub, is_asset_class));

            } else if (type == "min_trade") {
                double min_amt = item.value("min_amount", 0.0);
                double min_shares = item.value("min_shares", 0.0);
                if (min_amt < 0 || min_shares < 0) {
                    throw std::runtime_error("min_trade values cannot be negative");
                }
                set.addConstraint(std::make_unique<MinTradeSizeConstraint>(min_amt, min_shares));

            } else if (type == "integer_shares") {
                bool enforce = item.value("enforce", true);
                std::set<std::string> exempt;
                if (item.contains("exempt_tickers")) {
                    if (!item["exempt_tickers"].is_array()) {
                        throw std::runtime_error("exempt_tickers must be an array");
                    }
                    // Limit number of exempt tickers
                    if (item["exempt_tickers"].size() > 1000) {
                        throw std::runtime_error("too many exempt_tickers (max 1000)");
                    }
                    for (const auto& t : item["exempt_tickers"]) {
                        if (!t.is_string()) {
                            throw std::runtime_error("exempt_tickers must contain strings");
                        }
                        exempt.insert(t.get<std::string>());
                    }
                }
                set.addConstraint(std::make_unique<IntegerShareConstraint>(enforce, exempt));

            } else if (type == "position_limit") {
                if (!item.contains("max_position_percent") || !item["max_position_percent"].is_number()) {
                    throw std::runtime_error("position_limit requires numeric 'max_position_percent'");
                }
                double max_pct = item.at("max_position_percent").get<double>();
                set.addConstraint(std::make_unique<PositionLimitConstraint>(max_pct));

            } else if (type == "tax_lot") {
                // Tax lot constraints require a TaxLotManager reference that can't be
                // deserialized from JSON. The JSON stores the configuration, but the
                // constraint must be created programmatically with a valid manager.
                // Skip during JSON deserialization - these are added programmatically.
                // Log a warning or store the config for later use if needed.
                continue;

            } else if (type == "wash_sale") {
                // Wash sale constraints require a TaxOptimizer reference that can't be
                // deserialized from JSON. The JSON stores the configuration, but the
                // constraint must be created programmatically with a valid optimizer.
                // Skip during JSON deserialization - these are added programmatically.
                continue;

            } else {
                // Unknown constraint type - skip with warning (or throw)
                // For security, we throw to avoid unexpected behavior
                throw std::runtime_error("unknown constraint type: " + type);
            }
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Invalid " + type + " constraint: " + e.what());
        }
    }

    return set;
}

}  // namespace lumen::core
