/// @file highs_wrapper.cpp
/// @brief HiGHS solver wrapper implementation
///
/// Implementation of the HiGHS optimization solver wrapper for LP, MILP, and QP problems.
/// Based on Phase 1 technical decisions for portfolio optimization.

#include "lumen/solvers/highs_wrapper.hpp"
#include "lumen/data/tax_lot.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#ifdef LUMEN_HAS_HIGHS
#include <Highs.h>
#endif

namespace lumen::solvers {

// =============================================================================
// ObjectiveWeights
// =============================================================================

void ObjectiveWeights::normalize() {
    double sum = drift_weight + cost_weight + tax_weight;
    if (sum > 0) {
        drift_weight /= sum;
        cost_weight /= sum;
        tax_weight /= sum;
    }
}

// =============================================================================
// MILPBuilder::Impl
// =============================================================================

class MILPBuilder::Impl {
public:
    Impl(const core::Portfolio& portfolio, const core::TargetAllocation& target)
        : portfolio_(portfolio), target_(target) {
        initializeVariables();
    }

    void initializeVariables() {
        // Create buy and sell variables for each position
        // Variable naming: buy_TICKER, sell_TICKER, drift_TICKER
        auto tickers = portfolio_.getAllTickers();

        for (const auto& ticker : tickers) {
            const auto& pos = portfolio_.getPosition(ticker);
            double max_shares = pos.shares * 10;  // Allow buying up to 10x current holding

            // Buy variable: buy_i >= 0
            int buy_idx = static_cast<int>(var_names_.size());
            var_names_.push_back("buy_" + ticker);
            var_lower_.push_back(0.0);
            var_upper_.push_back(max_shares);
            var_integer_.push_back(false);
            buy_var_index_[ticker] = buy_idx;

            // Sell variable: 0 <= sell_i <= current_shares
            int sell_idx = static_cast<int>(var_names_.size());
            var_names_.push_back("sell_" + ticker);
            var_lower_.push_back(0.0);
            var_upper_.push_back(pos.shares);  // Can't sell more than owned
            var_integer_.push_back(false);
            sell_var_index_[ticker] = sell_idx;

            // Drift variable for linearized absolute value
            int drift_idx = static_cast<int>(var_names_.size());
            var_names_.push_back("drift_" + ticker);
            var_lower_.push_back(0.0);
            var_upper_.push_back(1.0);  // Drift is a percentage
            var_integer_.push_back(false);
            drift_var_index_[ticker] = drift_idx;
        }

        // Also create variables for tickers in target that might not be in portfolio
        for (const auto& alloc_target : target_.getAllTargets()) {
            if (!alloc_target.is_asset_class &&
                buy_var_index_.find(alloc_target.identifier) == buy_var_index_.end()) {
                const std::string& ticker = alloc_target.identifier;

                int buy_idx = static_cast<int>(var_names_.size());
                var_names_.push_back("buy_" + ticker);
                var_lower_.push_back(0.0);
                var_upper_.push_back(std::numeric_limits<double>::infinity());
                var_integer_.push_back(false);
                buy_var_index_[ticker] = buy_idx;

                int sell_idx = static_cast<int>(var_names_.size());
                var_names_.push_back("sell_" + ticker);
                var_lower_.push_back(0.0);
                var_upper_.push_back(0.0);  // No shares to sell
                var_integer_.push_back(false);
                sell_var_index_[ticker] = sell_idx;

                int drift_idx = static_cast<int>(var_names_.size());
                var_names_.push_back("drift_" + ticker);
                var_lower_.push_back(0.0);
                var_upper_.push_back(1.0);
                var_integer_.push_back(false);
                drift_var_index_[ticker] = drift_idx;
            }
        }
    }

    void setObjective(ObjectiveType type, const ObjectiveWeights& weights) {
        objective_type_ = type;
        weights_ = weights;

        // Initialize objective coefficients to zero
        obj_coeffs_.assign(var_names_.size(), 0.0);

        double total_value = portfolio_.getTotalValue();
        if (total_value <= 0) total_value = 1.0;

        auto tickers = getAllTickers();

        for (const auto& ticker : tickers) {
            double price = getTickerPrice(ticker);

            if (type == ObjectiveType::MINIMIZE_DRIFT || type == ObjectiveType::MULTI_OBJECTIVE) {
                // Add drift variables to objective
                auto drift_it = drift_var_index_.find(ticker);
                if (drift_it != drift_var_index_.end()) {
                    obj_coeffs_[drift_it->second] += weights.drift_weight;
                }
            }

            if (type == ObjectiveType::MINIMIZE_COST || type == ObjectiveType::MULTI_OBJECTIVE) {
                // Add transaction cost terms: cost_rate * price * (buy + sell)
                constexpr double cost_rate = 0.001;  // 0.1% transaction cost

                auto buy_it = buy_var_index_.find(ticker);
                if (buy_it != buy_var_index_.end()) {
                    obj_coeffs_[buy_it->second] += weights.cost_weight * cost_rate * price;
                }

                auto sell_it = sell_var_index_.find(ticker);
                if (sell_it != sell_var_index_.end()) {
                    obj_coeffs_[sell_it->second] += weights.cost_weight * cost_rate * price;
                }
            }
        }

        // Build drift linearization constraints
        buildDriftConstraints();

        // Add tax terms if tax optimization is enabled
        if (type == ObjectiveType::MINIMIZE_TAX || type == ObjectiveType::MULTI_OBJECTIVE) {
            addTaxObjectiveTerms();
        }
    }

    void setCustomObjective(const std::vector<double>& coefficients) {
        obj_coeffs_ = coefficients;
        obj_coeffs_.resize(var_names_.size(), 0.0);
    }

    void addConstraintSet(const core::ConstraintSet& constraints) {
        for (const auto* c : constraints.getAllConstraints()) {
            switch (c->getType()) {
                case core::ConstraintType::BUDGET: {
                    const auto* bc = dynamic_cast<const core::BudgetConstraint*>(c);
                    if (bc) {
                        addBudgetConstraint(bc->getTotalBudget(), bc->getMinCashReserve());
                    }
                    break;
                }
                case core::ConstraintType::ALLOCATION: {
                    const auto* ac = dynamic_cast<const core::AllocationConstraint*>(c);
                    if (ac && !ac->isAssetClass()) {
                        std::map<std::string, std::pair<double, double>> bounds;
                        bounds[ac->getIdentifier()] = {ac->getLowerBound(), ac->getUpperBound()};
                        addAllocationBounds(bounds);
                    }
                    break;
                }
                case core::ConstraintType::MIN_TRADE: {
                    const auto* mtc = dynamic_cast<const core::MinTradeSizeConstraint*>(c);
                    if (mtc) {
                        addMinTradeConstraint(mtc->getMinAmount());
                    }
                    break;
                }
                case core::ConstraintType::INTEGER_SHARES: {
                    const auto* isc = dynamic_cast<const core::IntegerShareConstraint*>(c);
                    if (isc && isc->isEnforced()) {
                        std::set<std::string> tickers;
                        for (const auto& ticker : getAllTickers()) {
                            if (!isc->isExempt(ticker)) {
                                tickers.insert(ticker);
                            }
                        }
                        addIntegerConstraints(tickers);
                    }
                    break;
                }
                case core::ConstraintType::POSITION_LIMIT: {
                    const auto* plc = dynamic_cast<const core::PositionLimitConstraint*>(c);
                    if (plc) {
                        addPositionLimitConstraint(plc->getMaxPositionPercent());
                    }
                    break;
                }
                case core::ConstraintType::TAX_LOT: {
                    const auto* tlc = dynamic_cast<const core::TaxLotConstraint*>(c);
                    if (tlc) {
                        addTaxLotConstraint(tlc);
                    }
                    break;
                }
                case core::ConstraintType::WASH_SALE: {
                    const auto* wsc = dynamic_cast<const core::WashSaleConstraint*>(c);
                    if (wsc) {
                        addWashSaleConstraint(wsc);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    void addBudgetConstraint(double budget, double min_cash) {
        // Self-funding constraint: Σ(price_i × sell_i) = Σ(price_i × buy_i)
        // This ensures trades are self-funding

        std::vector<double> coeffs(var_names_.size(), 0.0);

        for (const auto& ticker : getAllTickers()) {
            double price = getTickerPrice(ticker);

            auto buy_it = buy_var_index_.find(ticker);
            if (buy_it != buy_var_index_.end()) {
                coeffs[buy_it->second] = price;  // Cost of buying
            }

            auto sell_it = sell_var_index_.find(ticker);
            if (sell_it != sell_var_index_.end()) {
                coeffs[sell_it->second] = -price;  // Proceeds from selling
            }
        }

        // Equality constraint: buys - sells = 0 (self-funding)
        addConstraint(coeffs, 0.0, 0.0, "self_funding");

        budget_ = budget;
        min_cash_ = min_cash;
    }

    void addAllocationBounds(const std::map<std::string, std::pair<double, double>>& bounds) {
        double total_value = portfolio_.getTotalValue();
        if (total_value <= 0) return;

        for (const auto& [ticker, bound] : bounds) {
            double lower_pct = bound.first;
            double upper_pct = bound.second;

            // Get current position value
            double current_shares = 0.0;
            double price = 0.0;

            if (portfolio_.hasPosition(ticker)) {
                const auto& pos = portfolio_.getPosition(ticker);
                current_shares = pos.shares;
                price = pos.current_price;
            }

            if (price <= 0) {
                // Need to get price from somewhere - use default
                price = 100.0;  // TODO: Get from market data
            }

            // Constraint: lb * V <= (shares + buy - sell) * price <= ub * V
            // Rearranged: lb * V / price - shares <= buy - sell <= ub * V / price - shares

            std::vector<double> coeffs(var_names_.size(), 0.0);

            auto buy_it = buy_var_index_.find(ticker);
            if (buy_it != buy_var_index_.end()) {
                coeffs[buy_it->second] = price;
            }

            auto sell_it = sell_var_index_.find(ticker);
            if (sell_it != sell_var_index_.end()) {
                coeffs[sell_it->second] = -price;
            }

            double current_value = current_shares * price;
            double lb = lower_pct * total_value - current_value;
            double ub = upper_pct * total_value - current_value;

            addConstraint(coeffs, lb, ub, "alloc_" + ticker);
        }
    }

    void addIntegerConstraints(const std::set<std::string>& tickers) {
        for (const auto& ticker : tickers) {
            auto buy_it = buy_var_index_.find(ticker);
            if (buy_it != buy_var_index_.end()) {
                var_integer_[buy_it->second] = true;
            }

            auto sell_it = sell_var_index_.find(ticker);
            if (sell_it != sell_var_index_.end()) {
                var_integer_[sell_it->second] = true;
            }
        }
    }

    void addMinTradeConstraint(double min_amount) {
        // Minimum trade size requires binary indicators (makes it MILP)
        // For Phase 1, we'll use a simpler approach: post-process to remove small trades
        min_trade_amount_ = min_amount;

        // Full MILP formulation would add:
        // buy_i <= M * z_buy_i  (if z=0, buy=0)
        // buy_i * price >= min_amount * z_buy_i  (if z=1, trade >= min)
        // This is deferred to avoid complexity in Phase 1
    }

    void addPositionLimitConstraint(double max_pct) {
        double total_value = portfolio_.getTotalValue();
        if (total_value <= 0) return;

        for (const auto& ticker : getAllTickers()) {
            double current_shares = 0.0;
            double price = 0.0;

            if (portfolio_.hasPosition(ticker)) {
                const auto& pos = portfolio_.getPosition(ticker);
                current_shares = pos.shares;
                price = pos.current_price;
            }

            if (price <= 0) price = 100.0;

            // (shares + buy - sell) * price <= max_pct * total_value
            std::vector<double> coeffs(var_names_.size(), 0.0);

            auto buy_it = buy_var_index_.find(ticker);
            if (buy_it != buy_var_index_.end()) {
                coeffs[buy_it->second] = price;
            }

            auto sell_it = sell_var_index_.find(ticker);
            if (sell_it != sell_var_index_.end()) {
                coeffs[sell_it->second] = -price;
            }

            double rhs = max_pct * total_value - current_shares * price;

            addConstraint(coeffs, -std::numeric_limits<double>::infinity(), rhs,
                         "pos_limit_" + ticker);
        }
    }

    void setTaxConfig(const TaxConfig& config) {
        tax_config_ = config;
        tax_enabled_ = (config.lot_manager != nullptr);
    }

    void setLotMethod(data::TaxLotMethod method) {
        lot_method_ = method;
    }

    void setCurrentPrices(const std::map<std::string, double>& prices) {
        current_prices_ = prices;
    }

    void addTaxLotConstraint(const core::TaxLotConstraint* /*constraint*/) {
        // Tax lot constraints affect how we select lots for sells
        // In the MILP formulation, this influences the objective function
        // by penalizing or favoring certain lot selections

        // For now, this is handled in the objective function through tax_weight
        // More sophisticated handling would add per-lot variables
        tax_enabled_ = true;
    }

    void addWashSaleConstraint(const core::WashSaleConstraint* constraint) {
        if (!constraint || !constraint->blocksViolations()) {
            return;
        }

        // Wash sale constraint prevents selling a position at a loss and buying
        // a "substantially identical" security within 30 days before or after

        // This is a complex constraint that requires tracking equivalence groups
        // For the MILP formulation, we can add constraints that prevent
        // simultaneous sells of one security and buys of equivalent securities

        if (!tax_config_.optimizer) {
            return;
        }

        // For now, mark tax as enabled - full wash sale formulation would
        // require adding binary variables for each potential violation
        tax_enabled_ = true;
    }

    void addTaxObjectiveTerms() {
        if (!tax_enabled_ || !tax_config_.lot_manager || weights_.tax_weight <= 0) {
            return;
        }

        // Add tax impact terms to the objective function
        // For each potential sell, estimate the tax liability

        for (const auto& ticker : getAllTickers()) {
            auto sell_it = sell_var_index_.find(ticker);
            if (sell_it == sell_var_index_.end()) continue;

            // Get price for tax calculation
            double price = getTickerPrice(ticker);
            if (current_prices_.count(ticker)) {
                price = current_prices_.at(ticker);
            }

            // Get lots for this ticker to estimate tax impact
            auto lots = tax_config_.lot_manager->getLotsForTicker(ticker);
            if (lots.empty()) continue;

            // Calculate weighted average tax impact per share sold
            // Use the default lot selection method to estimate which lots would be sold
            double total_shares = 0.0;
            double total_tax_liability = 0.0;

            for (const auto& lot : lots) {
                if (lot.isSold()) continue;

                double gain = (price - lot.cost_basis_per_share) * lot.shares;
                double tax_rate = (lot.getGainType() == data::GainType::SHORT_TERM)
                    ? tax_config_.short_term_rate
                    : tax_config_.long_term_rate;

                // Only count positive gains (losses reduce taxes)
                if (gain > 0) {
                    total_tax_liability += gain * tax_rate;
                } else {
                    // Negative gain (loss) - tax savings
                    total_tax_liability += gain * tax_rate;  // Negative value = savings
                }
                total_shares += lot.shares;
            }

            if (total_shares > 0) {
                // Average tax impact per share
                double tax_per_share = total_tax_liability / total_shares;

                // Add to objective coefficient for sell variable
                obj_coeffs_[sell_it->second] += weights_.tax_weight * tax_per_share;
            }
        }
    }

    int addVariable(const std::string& name, double lb, double ub, bool is_integer) {
        int idx = static_cast<int>(var_names_.size());
        var_names_.push_back(name);
        var_lower_.push_back(lb);
        var_upper_.push_back(ub);
        var_integer_.push_back(is_integer);

        // Extend objective coefficients if needed
        if (obj_coeffs_.size() < var_names_.size()) {
            obj_coeffs_.push_back(0.0);
        }

        return idx;
    }

    int getVariableIndex(const std::string& name) const {
        for (size_t i = 0; i < var_names_.size(); ++i) {
            if (var_names_[i] == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool isReady() const {
        return !var_names_.empty() && !obj_coeffs_.empty();
    }

    int getNumVariables() const {
        return static_cast<int>(var_names_.size());
    }

    int getNumConstraints() const {
        return static_cast<int>(constraint_matrix_.size());
    }

    std::vector<core::Trade> extractTrades(const std::vector<double>& solution) const {
        std::vector<core::Trade> trades;
        constexpr double threshold = 1e-6;  // Ignore negligible trades

        for (const auto& ticker : getAllTickers()) {
            double buy_shares = 0.0;
            double sell_shares = 0.0;

            auto buy_it = buy_var_index_.find(ticker);
            if (buy_it != buy_var_index_.end() && buy_it->second < solution.size()) {
                buy_shares = solution[buy_it->second];
            }

            auto sell_it = sell_var_index_.find(ticker);
            if (sell_it != sell_var_index_.end() && sell_it->second < solution.size()) {
                sell_shares = solution[sell_it->second];
            }

            double price = getTickerPrice(ticker);

            // Create buy trade if significant
            if (buy_shares > threshold) {
                core::Trade trade;
                trade.ticker = ticker;
                trade.action = core::Trade::Action::BUY;
                trade.shares = buy_shares;
                trade.price = price;
                trade.amount = buy_shares * price;
                trade.transaction_cost = trade.amount * 0.001;  // 0.1%
                trade.rationale = "Rebalance: increase allocation to target";

                // Filter by minimum trade size
                if (min_trade_amount_ <= 0 || trade.amount >= min_trade_amount_) {
                    trades.push_back(trade);
                }
            }

            // Create sell trade if significant
            if (sell_shares > threshold) {
                core::Trade trade;
                trade.ticker = ticker;
                trade.action = core::Trade::Action::SELL;
                trade.shares = sell_shares;
                trade.price = price;
                trade.amount = sell_shares * price;
                trade.transaction_cost = trade.amount * 0.001;
                trade.rationale = "Rebalance: decrease allocation to target";

                // If tax optimization is enabled, select specific lots
                if (tax_enabled_ && tax_config_.lot_manager) {
                    auto selected_lots = tax_config_.lot_manager->selectLotsToSell(
                        ticker, sell_shares, lot_method_);

                    for (const auto& lot : selected_lots) {
                        trade.lot_ids.push_back(lot.id);
                    }

                    // Update rationale based on lot selection
                    if (!selected_lots.empty()) {
                        trade.rationale = "Rebalance: sell using " +
                            data::taxLotMethodToString(lot_method_) +
                            " lot selection";
                    }
                }

                if (min_trade_amount_ <= 0 || trade.amount >= min_trade_amount_) {
                    trades.push_back(trade);
                }
            }
        }

        return trades;
    }

    // Accessors for HiGHS model building
    const std::vector<std::string>& getVarNames() const { return var_names_; }
    const std::vector<double>& getVarLower() const { return var_lower_; }
    const std::vector<double>& getVarUpper() const { return var_upper_; }
    const std::vector<bool>& getVarInteger() const { return var_integer_; }
    const std::vector<double>& getObjCoeffs() const { return obj_coeffs_; }
    const std::vector<std::vector<double>>& getConstraintMatrix() const { return constraint_matrix_; }
    const std::vector<double>& getConstraintLower() const { return constraint_lower_; }
    const std::vector<double>& getConstraintUpper() const { return constraint_upper_; }
    const std::vector<std::string>& getConstraintNames() const { return constraint_names_; }

private:
    void buildDriftConstraints() {
        // For each ticker with a target, add drift linearization constraints:
        // drift_i >= w_final_i - w_target_i
        // drift_i >= -(w_final_i - w_target_i)

        double total_value = portfolio_.getTotalValue();
        if (total_value <= 0) return;

        for (const auto& alloc_target : target_.getAllTargets()) {
            if (alloc_target.is_asset_class) continue;  // Skip asset class targets for now

            const std::string& ticker = alloc_target.identifier;
            double target_pct = alloc_target.target_value;

            auto drift_it = drift_var_index_.find(ticker);
            if (drift_it == drift_var_index_.end()) continue;

            double current_shares = 0.0;
            double price = 0.0;

            if (portfolio_.hasPosition(ticker)) {
                const auto& pos = portfolio_.getPosition(ticker);
                current_shares = pos.shares;
                price = pos.current_price;
            }

            if (price <= 0) price = 100.0;

            auto buy_it = buy_var_index_.find(ticker);
            auto sell_it = sell_var_index_.find(ticker);

            // w_final = (current + buy - sell) * price / total_value
            // drift >= w_final - target
            // drift >= -w_final + target

            // Constraint 1: drift - (buy - sell) * price / V >= -current * price / V - target
            {
                std::vector<double> coeffs(var_names_.size(), 0.0);
                coeffs[drift_it->second] = 1.0;

                if (buy_it != buy_var_index_.end()) {
                    coeffs[buy_it->second] = -price / total_value;
                }
                if (sell_it != sell_var_index_.end()) {
                    coeffs[sell_it->second] = price / total_value;
                }

                double rhs = -current_shares * price / total_value - target_pct;
                addConstraint(coeffs, rhs, std::numeric_limits<double>::infinity(),
                             "drift_pos_" + ticker);
            }

            // Constraint 2: drift + (buy - sell) * price / V >= current * price / V + target
            {
                std::vector<double> coeffs(var_names_.size(), 0.0);
                coeffs[drift_it->second] = 1.0;

                if (buy_it != buy_var_index_.end()) {
                    coeffs[buy_it->second] = price / total_value;
                }
                if (sell_it != sell_var_index_.end()) {
                    coeffs[sell_it->second] = -price / total_value;
                }

                double rhs = current_shares * price / total_value - target_pct;
                addConstraint(coeffs, rhs, std::numeric_limits<double>::infinity(),
                             "drift_neg_" + ticker);
            }
        }
    }

    void addConstraint(const std::vector<double>& coeffs, double lb, double ub,
                      const std::string& name) {
        constraint_matrix_.push_back(coeffs);
        constraint_lower_.push_back(lb);
        constraint_upper_.push_back(ub);
        constraint_names_.push_back(name);
    }

    std::vector<std::string> getAllTickers() const {
        std::set<std::string> ticker_set;

        for (const auto& [ticker, idx] : buy_var_index_) {
            ticker_set.insert(ticker);
        }

        return std::vector<std::string>(ticker_set.begin(), ticker_set.end());
    }

    double getTickerPrice(const std::string& ticker) const {
        if (portfolio_.hasPosition(ticker)) {
            return portfolio_.getPosition(ticker).current_price;
        }
        return 100.0;  // Default price for new positions
    }

    const core::Portfolio& portfolio_;
    const core::TargetAllocation& target_;

    // Variable data
    std::vector<std::string> var_names_;
    std::vector<double> var_lower_;
    std::vector<double> var_upper_;
    std::vector<bool> var_integer_;
    std::vector<double> obj_coeffs_;

    // Variable index maps
    std::map<std::string, int> buy_var_index_;
    std::map<std::string, int> sell_var_index_;
    std::map<std::string, int> drift_var_index_;

    // Constraint data
    std::vector<std::vector<double>> constraint_matrix_;
    std::vector<double> constraint_lower_;
    std::vector<double> constraint_upper_;
    std::vector<std::string> constraint_names_;

    // Parameters
    ObjectiveType objective_type_ = ObjectiveType::MINIMIZE_DRIFT;
    ObjectiveWeights weights_;
    double budget_ = 0.0;
    double min_cash_ = 0.0;
    double min_trade_amount_ = 0.0;

    // Tax configuration
    TaxConfig tax_config_;
    data::TaxLotMethod lot_method_ = data::TaxLotMethod::FIFO;  // Default method
    std::map<std::string, double> current_prices_;
    bool tax_enabled_ = false;
};

// =============================================================================
// MILPBuilder
// =============================================================================

MILPBuilder::MILPBuilder(const core::Portfolio& portfolio, const core::TargetAllocation& target)
    : impl_(std::make_unique<Impl>(portfolio, target)) {}

MILPBuilder::~MILPBuilder() = default;

void MILPBuilder::setObjective(ObjectiveType type, const ObjectiveWeights& weights) {
    impl_->setObjective(type, weights);
}

void MILPBuilder::setCustomObjective(const std::vector<double>& coefficients) {
    impl_->setCustomObjective(coefficients);
}

void MILPBuilder::addConstraintSet(const core::ConstraintSet& constraints) {
    impl_->addConstraintSet(constraints);
}

void MILPBuilder::addBudgetConstraint(double budget, double min_cash) {
    impl_->addBudgetConstraint(budget, min_cash);
}

void MILPBuilder::addAllocationBounds(const std::map<std::string, std::pair<double, double>>& bounds) {
    impl_->addAllocationBounds(bounds);
}

void MILPBuilder::addIntegerConstraints(const std::set<std::string>& tickers) {
    impl_->addIntegerConstraints(tickers);
}

void MILPBuilder::addMinTradeConstraint(double min_amount) {
    impl_->addMinTradeConstraint(min_amount);
}

void MILPBuilder::setTaxConfig(const TaxConfig& config) {
    impl_->setTaxConfig(config);
}

void MILPBuilder::addTaxLotConstraint(const core::TaxLotConstraint* constraint) {
    impl_->addTaxLotConstraint(constraint);
}

void MILPBuilder::addWashSaleConstraint(const core::WashSaleConstraint* constraint) {
    impl_->addWashSaleConstraint(constraint);
}

void MILPBuilder::setCurrentPrices(const std::map<std::string, double>& prices) {
    impl_->setCurrentPrices(prices);
}

int MILPBuilder::addVariable(const std::string& name, double lb, double ub, bool is_integer) {
    return impl_->addVariable(name, lb, ub, is_integer);
}

int MILPBuilder::getVariableIndex(const std::string& name) const {
    return impl_->getVariableIndex(name);
}

bool MILPBuilder::isReady() const {
    return impl_->isReady();
}

int MILPBuilder::getNumVariables() const {
    return impl_->getNumVariables();
}

int MILPBuilder::getNumConstraints() const {
    return impl_->getNumConstraints();
}

std::vector<core::Trade> MILPBuilder::extractTrades(const std::vector<double>& solution) const {
    return impl_->extractTrades(solution);
}

const std::vector<std::string>& MILPBuilder::getVarNames() const {
    return impl_->getVarNames();
}

const std::vector<double>& MILPBuilder::getVarLower() const {
    return impl_->getVarLower();
}

const std::vector<double>& MILPBuilder::getVarUpper() const {
    return impl_->getVarUpper();
}

const std::vector<bool>& MILPBuilder::getVarInteger() const {
    return impl_->getVarInteger();
}

const std::vector<double>& MILPBuilder::getObjCoeffs() const {
    return impl_->getObjCoeffs();
}

const std::vector<std::vector<double>>& MILPBuilder::getConstraintMatrix() const {
    return impl_->getConstraintMatrix();
}

const std::vector<double>& MILPBuilder::getConstraintLower() const {
    return impl_->getConstraintLower();
}

const std::vector<double>& MILPBuilder::getConstraintUpper() const {
    return impl_->getConstraintUpper();
}

const std::vector<std::string>& MILPBuilder::getConstraintNames() const {
    return impl_->getConstraintNames();
}

// =============================================================================
// QPBuilder::Impl
// =============================================================================

class QPBuilder::Impl {
public:
    Impl(const core::Portfolio& portfolio, const core::TargetAllocation& target)
        : portfolio_(portfolio), target_(target) {}

    void setQuadraticObjective(const std::vector<std::vector<double>>& Q,
                               const std::vector<double>& c) {
        Q_ = Q;
        c_ = c;
    }

    void buildMeanVarianceObjective(const std::vector<std::vector<double>>& covariance,
                                    const std::vector<double>& expected_returns,
                                    double risk_aversion) {
        // Objective: maximize return - (risk_aversion/2) * variance
        // = max c'x - (lambda/2) * x'Qx
        // = min -c'x + (lambda/2) * x'Qx

        c_ = expected_returns;
        for (auto& val : c_) {
            val = -val;  // Negate for minimization
        }

        Q_ = covariance;
        for (auto& row : Q_) {
            for (auto& val : row) {
                val *= risk_aversion;
            }
        }
    }

    void addConstraintSet(const core::ConstraintSet& /*constraints*/) {
        // TODO: Implement QP constraint handling
    }

    bool isReady() const {
        return !Q_.empty() && !c_.empty();
    }

    const std::vector<std::vector<double>>& getQ() const { return Q_; }
    const std::vector<double>& getC() const { return c_; }

private:
    const core::Portfolio& portfolio_;
    const core::TargetAllocation& target_;
    std::vector<std::vector<double>> Q_;
    std::vector<double> c_;
};

// =============================================================================
// QPBuilder
// =============================================================================

QPBuilder::QPBuilder(const core::Portfolio& portfolio, const core::TargetAllocation& target)
    : impl_(std::make_unique<Impl>(portfolio, target)) {}

QPBuilder::~QPBuilder() = default;

void QPBuilder::setQuadraticObjective(const std::vector<std::vector<double>>& Q,
                                      const std::vector<double>& c) {
    impl_->setQuadraticObjective(Q, c);
}

void QPBuilder::buildMeanVarianceObjective(const std::vector<std::vector<double>>& covariance,
                                           const std::vector<double>& expected_returns,
                                           double risk_aversion) {
    impl_->buildMeanVarianceObjective(covariance, expected_returns, risk_aversion);
}

void QPBuilder::addConstraintSet(const core::ConstraintSet& constraints) {
    impl_->addConstraintSet(constraints);
}

bool QPBuilder::isReady() const {
    return impl_->isReady();
}

// =============================================================================
// HighsOptimizer::Impl
// =============================================================================

class HighsOptimizer::Impl {
public:
    Impl() = default;

#ifdef LUMEN_HAS_HIGHS
    Highs highs_;
#endif

    std::vector<double> primal_solution_;
    std::vector<double> dual_solution_;
    double objective_value_ = 0.0;
    bool presolve_enabled_ = true;
    bool parallel_enabled_ = true;
    double mip_gap_ = 1e-4;
};

// =============================================================================
// HighsOptimizer
// =============================================================================

HighsOptimizer::HighsOptimizer() : impl_(std::make_unique<Impl>()) {}

HighsOptimizer::~HighsOptimizer() = default;

bool HighsOptimizer::isAvailable() const {
#ifdef LUMEN_HAS_HIGHS
    return true;
#else
    return false;
#endif
}

core::SolverResult HighsOptimizer::solve(const core::Portfolio& portfolio,
                                         const core::TargetAllocation& target,
                                         const core::ConstraintSet& constraints) {
    core::SolverResult result;
    result.solver_used = "HiGHS";

    auto start_time = std::chrono::steady_clock::now();

#ifdef LUMEN_HAS_HIGHS
    // Build the MILP problem
    MILPBuilder builder(portfolio, target);

    // Check for tax constraints to configure tax-aware optimization
    auto tax_lot_constraints = constraints.getConstraintsByType(core::ConstraintType::TAX_LOT);
    auto wash_sale_constraints = constraints.getConstraintsByType(core::ConstraintType::WASH_SALE);
    bool has_tax_optimization = !tax_lot_constraints.empty() || !wash_sale_constraints.empty();

    // Configure tax settings if tax constraints are present
    if (has_tax_optimization) {
        TaxConfig tax_config;

        // Look for TaxLotConstraint to configure lot selection
        for (const auto* c : tax_lot_constraints) {
            const auto* tlc = dynamic_cast<const core::TaxLotConstraint*>(c);
            if (tlc) {
                tax_config.prefer_long_term_lots = tlc->prefersLongTerm();
                // Note: lot_manager and optimizer would need to be passed separately
                // For now, we rely on the constraint handling
                builder.setTaxConfig(tax_config);
                break;
            }
        }
    }

    // Set objective: minimize drift with transaction cost penalty (and tax if enabled)
    ObjectiveWeights weights;
    weights.drift_weight = 1.0;
    weights.cost_weight = 0.1;
    weights.tax_weight = has_tax_optimization ? 0.2 : 0.0;
    builder.setObjective(ObjectiveType::MULTI_OBJECTIVE, weights);

    // Add constraints (including tax constraints)
    builder.addConstraintSet(constraints);

    // Add self-funding constraint
    builder.addBudgetConstraint(portfolio.getTotalValue(), 0.0);

    if (!builder.isReady()) {
        result.success = false;
        result.status = "error";
        return result;
    }

    // Set up HiGHS model
    Highs& highs = impl_->highs_;
    highs.clear();

    // Configure solver
    highs.setOptionValue("time_limit", timeout_ms_ / 1000.0);
    highs.setOptionValue("mip_rel_gap", impl_->mip_gap_);
    highs.setOptionValue("presolve", impl_->presolve_enabled_ ? "on" : "off");
    highs.setOptionValue("parallel", impl_->parallel_enabled_ ? "on" : "off");

    if (verbosity_ == 0) {
        highs.setOptionValue("output_flag", false);
    }

    // Build model using MILPBuilder's public interface
    const int num_vars = builder.getNumVariables();
    const int num_constraints = builder.getNumConstraints();

    if (num_vars == 0) {
        result.success = false;
        result.status = "error";
        return result;
    }

    // Get model data from builder
    const auto& var_lower = builder.getVarLower();
    const auto& var_upper = builder.getVarUpper();
    const auto& var_integer = builder.getVarInteger();
    const auto& obj_coeffs = builder.getObjCoeffs();
    const auto& constraint_matrix = builder.getConstraintMatrix();
    const auto& constraint_lower = builder.getConstraintLower();
    const auto& constraint_upper = builder.getConstraintUpper();

    // Build HighsModel
    HighsModel model;
    model.lp_.sense_ = ObjSense::kMinimize;
    model.lp_.num_col_ = num_vars;
    model.lp_.num_row_ = num_constraints;
    model.lp_.offset_ = 0.0;

    // Set objective coefficients
    model.lp_.col_cost_ = obj_coeffs;

    // Set variable bounds
    model.lp_.col_lower_ = var_lower;
    model.lp_.col_upper_ = var_upper;

    // Build sparse constraint matrix in CSC format
    // HiGHS uses column-major sparse format
    model.lp_.a_matrix_.format_ = MatrixFormat::kColwise;

    if (num_constraints > 0 && !constraint_matrix.empty()) {
        // Convert dense matrix to sparse CSC format
        std::vector<int> col_start(num_vars + 1, 0);
        std::vector<int> row_index;
        std::vector<double> values;

        for (int col = 0; col < num_vars; ++col) {
            col_start[col] = static_cast<int>(row_index.size());
            for (int row = 0; row < num_constraints; ++row) {
                if (row < static_cast<int>(constraint_matrix.size()) &&
                    col < static_cast<int>(constraint_matrix[row].size())) {
                    double val = constraint_matrix[row][col];
                    if (std::abs(val) > 1e-12) {  // Only store non-zeros
                        row_index.push_back(row);
                        values.push_back(val);
                    }
                }
            }
        }
        col_start[num_vars] = static_cast<int>(row_index.size());

        model.lp_.a_matrix_.start_ = col_start;
        model.lp_.a_matrix_.index_ = row_index;
        model.lp_.a_matrix_.value_ = values;

        // Set row bounds
        model.lp_.row_lower_ = constraint_lower;
        model.lp_.row_upper_ = constraint_upper;
    }

    // Set integer constraints
    bool has_integers = false;
    for (bool is_int : var_integer) {
        if (is_int) {
            has_integers = true;
            break;
        }
    }

    if (has_integers) {
        model.lp_.integrality_.resize(num_vars, HighsVarType::kContinuous);
        for (int i = 0; i < num_vars; ++i) {
            if (var_integer[i]) {
                model.lp_.integrality_[i] = HighsVarType::kInteger;
            }
        }
    }

    HighsStatus status = highs.passModel(model);
    if (status != HighsStatus::kOk) {
        result.success = false;
        result.status = "error";
        return result;
    }

    status = highs.run();

    auto end_time = std::chrono::steady_clock::now();
    last_solve_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    last_iterations_ = highs.getInfo().simplex_iteration_count;

    const HighsInfo& info = highs.getInfo();
    const HighsModelStatus& model_status = highs.getModelStatus();

    if (model_status == HighsModelStatus::kOptimal) {
        result.success = true;
        result.status = "optimal";
        result.objective_value = info.objective_function_value;
        result.optimality_gap = 0.0;

        // Extract solution
        const HighsSolution& solution = highs.getSolution();
        impl_->primal_solution_ = std::vector<double>(
            solution.col_value.begin(), solution.col_value.end());
        impl_->dual_solution_ = std::vector<double>(
            solution.row_dual.begin(), solution.row_dual.end());
        impl_->objective_value_ = info.objective_function_value;

        // Extract trades
        result.trades = builder.extractTrades(impl_->primal_solution_);

        // Calculate total transaction cost
        result.total_transaction_cost = 0.0;
        for (const auto& trade : result.trades) {
            result.total_transaction_cost += trade.transaction_cost;
        }

    } else if (model_status == HighsModelStatus::kInfeasible) {
        result.success = false;
        result.status = "infeasible";
    } else if (model_status == HighsModelStatus::kUnbounded) {
        result.success = false;
        result.status = "unbounded";
    } else {
        result.success = false;
        result.status = "timeout";
    }

    result.solve_time_ms = last_solve_time_;
    result.iterations = last_iterations_;

#else
    // HiGHS not available - return error
    result.success = false;
    result.status = "error";

    auto end_time = std::chrono::steady_clock::now();
    last_solve_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
#endif

    return result;
}

void HighsOptimizer::setTimeout(long timeout_ms) {
    timeout_ms_ = timeout_ms;
}

void HighsOptimizer::setVerbosity(int level) {
    verbosity_ = level;
}

void HighsOptimizer::setPresolve(bool enable) {
    impl_->presolve_enabled_ = enable;
}

void HighsOptimizer::setParallel(bool enable) {
    impl_->parallel_enabled_ = enable;
}

void HighsOptimizer::setMIPGap(double gap) {
    impl_->mip_gap_ = gap;
}

std::vector<double> HighsOptimizer::getPrimalSolution() const {
    return impl_->primal_solution_;
}

std::vector<double> HighsOptimizer::getDualSolution() const {
    return impl_->dual_solution_;
}

double HighsOptimizer::getObjectiveValue() const {
    return impl_->objective_value_;
}

}  // namespace lumen::solvers
