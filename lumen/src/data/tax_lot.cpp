/// @file tax_lot.cpp
/// @brief Tax lot management and tax optimization implementation
///
/// Complete implementation of tax lot tracking, lot selection strategies,
/// wash sale detection, and tax-loss harvesting functionality.

#include "lumen/data/tax_lot.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include <sqlite3.h>

namespace lumen::data {

// =============================================================================
// Utility Functions
// =============================================================================

std::string taxLotMethodToString(TaxLotMethod method) {
    switch (method) {
        case TaxLotMethod::FIFO: return "FIFO";
        case TaxLotMethod::LIFO: return "LIFO";
        case TaxLotMethod::HIFO: return "HIFO";
        case TaxLotMethod::LOFO: return "LOFO";
        case TaxLotMethod::SPEC_ID: return "SPEC_ID";
        default: return "UNKNOWN";
    }
}

std::string gainTypeToString(GainType type) {
    switch (type) {
        case GainType::SHORT_TERM: return "SHORT_TERM";
        case GainType::LONG_TERM: return "LONG_TERM";
        default: return "UNKNOWN";
    }
}

namespace {

// Parse ISO 8601 date string to time_point
std::chrono::system_clock::time_point parseDate(const std::string& date_str) {
    std::tm tm = {};
    std::istringstream ss(date_str);
    ss >> std::get_time(&tm, "%Y-%m-%d");
    if (ss.fail()) {
        // Try with time component
        ss.clear();
        ss.str(date_str);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    }
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

// Format time_point to ISO 8601 date string
std::string formatDate(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time_t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

// Calculate days between two time_points
int daysBetween(const std::chrono::system_clock::time_point& start,
                const std::chrono::system_clock::time_point& end) {
    auto duration = end - start;
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24);
}

}  // anonymous namespace

// =============================================================================
// TaxLot Implementation
// =============================================================================

double TaxLot::getTotalCostBasis() const {
    return cost_basis_per_share * shares;
}

double TaxLot::getCurrentValue(double current_price) const {
    return current_price * shares;
}

double TaxLot::getUnrealizedGain(double current_price) const {
    return (current_price - cost_basis_per_share) * shares;
}

double TaxLot::getRealizedGain() const {
    if (!isSold() || !sale_price.has_value()) {
        return 0.0;
    }
    return (sale_price.value() - cost_basis_per_share) * shares;
}

GainType TaxLot::getGainType() const {
    auto reference_date = isSold() && sale_date.has_value()
        ? sale_date.value()
        : std::chrono::system_clock::now();

    int days = daysBetween(purchase_date, reference_date);
    return (days > 365) ? GainType::LONG_TERM : GainType::SHORT_TERM;
}

int TaxLot::getDaysHeld() const {
    auto reference_date = isSold() && sale_date.has_value()
        ? sale_date.value()
        : std::chrono::system_clock::now();

    return daysBetween(purchase_date, reference_date);
}

nlohmann::json TaxLot::toJSON() const {
    nlohmann::json j;
    j["id"] = id;
    j["ticker"] = ticker;
    j["shares"] = shares;
    j["cost_basis_per_share"] = cost_basis_per_share;
    j["purchase_date"] = formatDate(purchase_date);
    j["gain_type"] = gainTypeToString(getGainType());
    j["days_held"] = getDaysHeld();
    j["total_cost_basis"] = getTotalCostBasis();

    if (sale_date.has_value()) {
        j["sale_date"] = formatDate(sale_date.value());
    }
    if (sale_price.has_value()) {
        j["sale_price"] = sale_price.value();
        j["realized_gain"] = getRealizedGain();
    }

    return j;
}

TaxLot TaxLot::fromJSON(const nlohmann::json& j) {
    TaxLot lot;
    lot.id = j.at("id").get<std::string>();
    lot.ticker = j.at("ticker").get<std::string>();
    lot.shares = j.at("shares").get<double>();
    lot.cost_basis_per_share = j.at("cost_basis_per_share").get<double>();
    lot.purchase_date = parseDate(j.at("purchase_date").get<std::string>());

    if (j.contains("sale_date") && !j["sale_date"].is_null()) {
        lot.sale_date = parseDate(j["sale_date"].get<std::string>());
    }
    if (j.contains("sale_price") && !j["sale_price"].is_null()) {
        lot.sale_price = j["sale_price"].get<double>();
    }

    return lot;
}

// =============================================================================
// WashSaleViolation Implementation
// =============================================================================

nlohmann::json WashSaleViolation::toJSON() const {
    return nlohmann::json{
        {"lot_id", lot_id},
        {"replacement_ticker", replacement_ticker},
        {"violation_date", formatDate(violation_date)},
        {"disallowed_loss", disallowed_loss},
        {"description", description}
    };
}

// =============================================================================
// TaxHarvestingOpportunity Implementation
// =============================================================================

nlohmann::json TaxHarvestingOpportunity::toJSON() const {
    return nlohmann::json{
        {"lot", lot.toJSON()},
        {"current_price", current_price},
        {"unrealized_loss", unrealized_loss},
        {"gain_type", gainTypeToString(gain_type)},
        {"replacement_candidates", replacement_candidates},
        {"would_trigger_wash_sale", would_trigger_wash_sale}
    };
}

// =============================================================================
// CapitalGainsReport Implementation
// =============================================================================

nlohmann::json CapitalGainsReport::toJSON() const {
    nlohmann::json j;
    j["short_term_gains"] = short_term_gains;
    j["long_term_gains"] = long_term_gains;
    j["short_term_losses"] = short_term_losses;
    j["long_term_losses"] = long_term_losses;
    j["net_short_term"] = net_short_term;
    j["net_long_term"] = net_long_term;
    j["harvested_losses"] = harvested_losses;
    j["estimated_tax_savings"] = estimated_tax_savings;

    nlohmann::json trades_json = nlohmann::json::array();
    for (const auto& trade : trades) {
        trades_json.push_back(trade.toJSON());
    }
    j["trades"] = trades_json;

    return j;
}

std::string CapitalGainsReport::toPlainText() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);

    ss << "=== Capital Gains Report ===" << "\n\n";

    ss << "Short-Term (Ordinary Income Rates):" << "\n";
    ss << "  Gains:  $" << short_term_gains << "\n";
    ss << "  Losses: $" << short_term_losses << "\n";
    ss << "  Net:    $" << net_short_term << "\n\n";

    ss << "Long-Term (Preferential Rates):" << "\n";
    ss << "  Gains:  $" << long_term_gains << "\n";
    ss << "  Losses: $" << long_term_losses << "\n";
    ss << "  Net:    $" << net_long_term << "\n\n";

    ss << "Tax-Loss Harvesting:" << "\n";
    ss << "  Harvested Losses: $" << harvested_losses << "\n";
    ss << "  Estimated Tax Savings: $" << estimated_tax_savings << "\n\n";

    if (!trades.empty()) {
        ss << "Trades:" << "\n";
        for (const auto& trade : trades) {
            std::string action = (trade.action == core::Trade::Action::BUY) ? "BUY" :
                                 (trade.action == core::Trade::Action::SELL) ? "SELL" : "HOLD";
            ss << "  " << action << " " << trade.shares << " " << trade.ticker
               << " @ $" << trade.price << " = $" << trade.amount << "\n";
        }
    }

    return ss.str();
}

// =============================================================================
// TaxLotManager Implementation
// =============================================================================

class TaxLotManager::Impl {
public:
    Impl() : next_lot_id_(1) {}

    void addLot(const TaxLot& lot) {
        std::lock_guard<std::mutex> lock(mutex_);
        lots_[lot.id] = lot;
        lots_by_ticker_[lot.ticker].push_back(lot.id);
    }

    void removeLot(const std::string& lot_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = lots_.find(lot_id);
        if (it != lots_.end()) {
            const std::string& ticker = it->second.ticker;
            auto& ticker_lots = lots_by_ticker_[ticker];
            ticker_lots.erase(
                std::remove(ticker_lots.begin(), ticker_lots.end(), lot_id),
                ticker_lots.end());
            lots_.erase(it);
        }
    }

    void updateLot(const std::string& lot_id, const TaxLot& updated) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = lots_.find(lot_id);
        if (it != lots_.end()) {
            // If ticker changed, update the index
            if (it->second.ticker != updated.ticker) {
                auto& old_ticker_lots = lots_by_ticker_[it->second.ticker];
                old_ticker_lots.erase(
                    std::remove(old_ticker_lots.begin(), old_ticker_lots.end(), lot_id),
                    old_ticker_lots.end());
                lots_by_ticker_[updated.ticker].push_back(lot_id);
            }
            it->second = updated;
        }
    }

    const TaxLot& getLot(const std::string& lot_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = lots_.find(lot_id);
        if (it == lots_.end()) {
            throw std::runtime_error("TaxLotManager: lot not found: " + lot_id);
        }
        return it->second;
    }

    std::vector<TaxLot> getLotsForTicker(const std::string& ticker) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TaxLot> result;
        auto it = lots_by_ticker_.find(ticker);
        if (it != lots_by_ticker_.end()) {
            for (const auto& lot_id : it->second) {
                auto lot_it = lots_.find(lot_id);
                if (lot_it != lots_.end()) {
                    result.push_back(lot_it->second);
                }
            }
        }
        return result;
    }

    std::vector<TaxLot> getUnsoldLots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TaxLot> result;
        for (const auto& [id, lot] : lots_) {
            if (!lot.isSold()) {
                result.push_back(lot);
            }
        }
        return result;
    }

    std::vector<TaxLot> getSoldLots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TaxLot> result;
        for (const auto& [id, lot] : lots_) {
            if (lot.isSold()) {
                result.push_back(lot);
            }
        }
        return result;
    }

    std::vector<TaxLot> getAllLots() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TaxLot> result;
        result.reserve(lots_.size());
        for (const auto& [id, lot] : lots_) {
            result.push_back(lot);
        }
        return result;
    }

    std::vector<TaxLot> selectLotsToSell(const std::string& ticker, double shares_to_sell,
                                          TaxLotMethod method) const {
        auto lots = getLotsForTicker(ticker);

        // Filter to unsold lots only
        lots.erase(
            std::remove_if(lots.begin(), lots.end(),
                          [](const TaxLot& lot) { return lot.isSold(); }),
            lots.end());

        if (lots.empty()) {
            return {};
        }

        // Sort based on method
        switch (method) {
            case TaxLotMethod::FIFO:
                // First In, First Out - oldest first
                std::sort(lots.begin(), lots.end(),
                         [](const TaxLot& a, const TaxLot& b) {
                             return a.purchase_date < b.purchase_date;
                         });
                break;

            case TaxLotMethod::LIFO:
                // Last In, First Out - newest first
                std::sort(lots.begin(), lots.end(),
                         [](const TaxLot& a, const TaxLot& b) {
                             return a.purchase_date > b.purchase_date;
                         });
                break;

            case TaxLotMethod::HIFO:
                // Highest In, First Out - highest cost basis first
                std::sort(lots.begin(), lots.end(),
                         [](const TaxLot& a, const TaxLot& b) {
                             return a.cost_basis_per_share > b.cost_basis_per_share;
                         });
                break;

            case TaxLotMethod::LOFO:
                // Lowest In, First Out - lowest cost basis first
                std::sort(lots.begin(), lots.end(),
                         [](const TaxLot& a, const TaxLot& b) {
                             return a.cost_basis_per_share < b.cost_basis_per_share;
                         });
                break;

            case TaxLotMethod::SPEC_ID:
                // For specific identification, return all lots unsorted
                // The caller must specify which lots to use
                break;
        }

        // Select lots up to the required shares
        std::vector<TaxLot> selected;
        double remaining = shares_to_sell;

        for (const auto& lot : lots) {
            if (remaining <= 0) break;

            TaxLot selected_lot = lot;
            if (lot.shares <= remaining) {
                // Use entire lot
                selected.push_back(selected_lot);
                remaining -= lot.shares;
            } else {
                // Partial lot - create a copy with reduced shares
                selected_lot.shares = remaining;
                selected.push_back(selected_lot);
                remaining = 0;
            }
        }

        return selected;
    }

    double getTotalCostBasis(const std::string& ticker) const {
        auto lots = getLotsForTicker(ticker);
        double total = 0.0;
        for (const auto& lot : lots) {
            if (!lot.isSold()) {
                total += lot.getTotalCostBasis();
            }
        }
        return total;
    }

    double getAverageCostBasis(const std::string& ticker) const {
        auto lots = getLotsForTicker(ticker);
        double total_cost = 0.0;
        double total_shares = 0.0;

        for (const auto& lot : lots) {
            if (!lot.isSold()) {
                total_cost += lot.getTotalCostBasis();
                total_shares += lot.shares;
            }
        }

        return (total_shares > 0) ? (total_cost / total_shares) : 0.0;
    }

    double getTotalUnrealizedGain(double current_price, const std::string& ticker) const {
        auto lots = getLotsForTicker(ticker);
        double total = 0.0;
        for (const auto& lot : lots) {
            if (!lot.isSold()) {
                total += lot.getUnrealizedGain(current_price);
            }
        }
        return total;
    }

    void saveToDatabase(const std::string& db_path) {
        sqlite3* db;
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
        }

        // Create table if not exists
        const char* create_sql = R"(
            CREATE TABLE IF NOT EXISTS tax_lots (
                id TEXT PRIMARY KEY,
                ticker TEXT NOT NULL,
                shares REAL NOT NULL,
                cost_basis_per_share REAL NOT NULL,
                purchase_date TEXT NOT NULL,
                sale_date TEXT,
                sale_price REAL,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            );
            CREATE INDEX IF NOT EXISTS idx_tax_lots_ticker ON tax_lots(ticker);
        )";

        char* err_msg = nullptr;
        rc = sqlite3_exec(db, create_sql, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg;
            sqlite3_free(err_msg);
            sqlite3_close(db);
            throw std::runtime_error("Failed to create table: " + error);
        }

        // Begin transaction
        sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

        // Clear existing lots
        sqlite3_exec(db, "DELETE FROM tax_lots", nullptr, nullptr, nullptr);

        // Insert all lots
        const char* insert_sql = R"(
            INSERT INTO tax_lots (id, ticker, shares, cost_basis_per_share, purchase_date, sale_date, sale_price)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        )";

        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare statement");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, lot] : lots_) {
            sqlite3_bind_text(stmt, 1, lot.id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, lot.ticker.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 3, lot.shares);
            sqlite3_bind_double(stmt, 4, lot.cost_basis_per_share);
            sqlite3_bind_text(stmt, 5, formatDate(lot.purchase_date).c_str(), -1, SQLITE_TRANSIENT);

            if (lot.sale_date.has_value()) {
                sqlite3_bind_text(stmt, 6, formatDate(lot.sale_date.value()).c_str(), -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_null(stmt, 6);
            }

            if (lot.sale_price.has_value()) {
                sqlite3_bind_double(stmt, 7, lot.sale_price.value());
            } else {
                sqlite3_bind_null(stmt, 7);
            }

            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);
        sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
        sqlite3_close(db);
    }

    void loadFromDatabase(const std::string& db_path) {
        sqlite3* db;
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
        }

        const char* select_sql = "SELECT id, ticker, shares, cost_basis_per_share, purchase_date, sale_date, sale_price FROM tax_lots";
        sqlite3_stmt* stmt;

        rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_close(db);
            throw std::runtime_error("Failed to prepare statement");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        lots_.clear();
        lots_by_ticker_.clear();

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TaxLot lot;
            lot.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            lot.ticker = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            lot.shares = sqlite3_column_double(stmt, 2);
            lot.cost_basis_per_share = sqlite3_column_double(stmt, 3);
            lot.purchase_date = parseDate(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));

            if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
                lot.sale_date = parseDate(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
            }
            if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
                lot.sale_price = sqlite3_column_double(stmt, 6);
            }

            lots_[lot.id] = lot;
            lots_by_ticker_[lot.ticker].push_back(lot.id);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    nlohmann::json toJSON() const {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json j = nlohmann::json::array();
        for (const auto& [id, lot] : lots_) {
            j.push_back(lot.toJSON());
        }
        return j;
    }

    std::string generateLotId() {
        return "LOT_" + std::to_string(next_lot_id_++);
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, TaxLot> lots_;  // id -> lot
    std::map<std::string, std::vector<std::string>> lots_by_ticker_;  // ticker -> lot ids
    int next_lot_id_;
};

TaxLotManager::~TaxLotManager() = default;

void TaxLotManager::addLot(const TaxLot& lot) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->addLot(lot);
}

void TaxLotManager::removeLot(const std::string& lot_id) {
    if (impl_) {
        impl_->removeLot(lot_id);
    }
}

void TaxLotManager::updateLot(const std::string& lot_id, const TaxLot& updated) {
    if (impl_) {
        impl_->updateLot(lot_id, updated);
    }
}

const TaxLot& TaxLotManager::getLot(const std::string& lot_id) const {
    if (!impl_) {
        throw std::runtime_error("TaxLotManager: no lots loaded");
    }
    return impl_->getLot(lot_id);
}

std::vector<TaxLot> TaxLotManager::getLotsForTicker(const std::string& ticker) const {
    if (!impl_) {
        return {};
    }
    return impl_->getLotsForTicker(ticker);
}

std::vector<TaxLot> TaxLotManager::getUnsoldLots() const {
    if (!impl_) {
        return {};
    }
    return impl_->getUnsoldLots();
}

std::vector<TaxLot> TaxLotManager::getSoldLots() const {
    if (!impl_) {
        return {};
    }
    return impl_->getSoldLots();
}

std::vector<TaxLot> TaxLotManager::getAllLots() const {
    if (!impl_) {
        return {};
    }
    return impl_->getAllLots();
}

std::vector<TaxLot> TaxLotManager::selectLotsToSell(const std::string& ticker, double shares_to_sell,
                                                     TaxLotMethod method) const {
    if (!impl_) {
        return {};
    }
    return impl_->selectLotsToSell(ticker, shares_to_sell, method);
}

double TaxLotManager::getTotalCostBasis(const std::string& ticker) const {
    if (!impl_) {
        return 0.0;
    }
    return impl_->getTotalCostBasis(ticker);
}

double TaxLotManager::getAverageCostBasis(const std::string& ticker) const {
    if (!impl_) {
        return 0.0;
    }
    return impl_->getAverageCostBasis(ticker);
}

double TaxLotManager::getTotalUnrealizedGain(double current_price, const std::string& ticker) const {
    if (!impl_) {
        return 0.0;
    }
    return impl_->getTotalUnrealizedGain(current_price, ticker);
}

void TaxLotManager::saveToDatabase(const std::string& db_path) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->saveToDatabase(db_path);
}

void TaxLotManager::loadFromDatabase(const std::string& db_path) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    impl_->loadFromDatabase(db_path);
}

nlohmann::json TaxLotManager::toJSON() const {
    if (!impl_) {
        return nlohmann::json::array();
    }
    return impl_->toJSON();
}

TaxLotManager TaxLotManager::fromJSON(const nlohmann::json& j) {
    TaxLotManager manager;
    manager.impl_ = std::make_unique<Impl>();
    for (const auto& lot_json : j) {
        manager.addLot(TaxLot::fromJSON(lot_json));
    }
    return manager;
}

// =============================================================================
// TaxOptimizer Implementation
// =============================================================================

class TaxOptimizer::Impl {
public:
    Impl(TaxLotManager& lot_manager, double tax_rate_short, double tax_rate_long)
        : lot_manager_(lot_manager),
          tax_rate_short_(tax_rate_short),
          tax_rate_long_(tax_rate_long),
          wash_sale_window_days_(30) {
        initializeDefaultEquivalenceGroups();
    }

    std::vector<TaxHarvestingOpportunity> findHarvestingOpportunities(
        const std::map<std::string, double>& current_prices,
        double min_loss_threshold) const {

        std::vector<TaxHarvestingOpportunity> opportunities;

        auto unsold_lots = lot_manager_.getUnsoldLots();

        for (const auto& lot : unsold_lots) {
            auto price_it = current_prices.find(lot.ticker);
            if (price_it == current_prices.end()) {
                continue;  // No price available
            }

            double current_price = price_it->second;
            double unrealized_gain = lot.getUnrealizedGain(current_price);

            // Only interested in losses
            if (unrealized_gain >= 0) {
                continue;
            }

            double unrealized_loss = std::abs(unrealized_gain);
            if (unrealized_loss < min_loss_threshold) {
                continue;
            }

            TaxHarvestingOpportunity opp;
            opp.lot = lot;
            opp.current_price = current_price;
            opp.unrealized_loss = unrealized_loss;
            opp.gain_type = lot.getGainType();
            opp.replacement_candidates = getReplacementCandidates(lot.ticker);
            opp.would_trigger_wash_sale = false;  // Will be evaluated per replacement

            opportunities.push_back(opp);
        }

        // Sort by loss amount descending
        std::sort(opportunities.begin(), opportunities.end(),
                 [](const TaxHarvestingOpportunity& a, const TaxHarvestingOpportunity& b) {
                     return a.unrealized_loss > b.unrealized_loss;
                 });

        return opportunities;
    }

    bool wouldTriggerWashSale(const TaxLot& lot_to_sell,
                              const std::string& replacement_ticker,
                              const std::chrono::system_clock::time_point& trade_date) const {
        // Only applies to sales at a loss
        // We need to check if replacement_ticker is "substantially identical"
        // and if there are purchases within the 30-day window

        if (!areSubstantiallyIdentical(lot_to_sell.ticker, replacement_ticker)) {
            return false;  // Different security, no wash sale
        }

        // Check for purchases of the replacement within 30 days before or after
        auto window_start = trade_date - std::chrono::hours(30 * 24);
        auto window_end = trade_date + std::chrono::hours(30 * 24);

        auto replacement_lots = lot_manager_.getLotsForTicker(replacement_ticker);
        for (const auto& lot : replacement_lots) {
            if (lot.purchase_date >= window_start && lot.purchase_date <= window_end) {
                return true;  // Wash sale triggered
            }
        }

        return false;
    }

    std::vector<WashSaleViolation> checkWashSaleViolations(
        const std::vector<core::Trade>& proposed_trades) const {

        std::vector<WashSaleViolation> violations;
        auto now = std::chrono::system_clock::now();

        // Find all sells at a loss
        std::vector<const core::Trade*> sell_trades;
        std::vector<const core::Trade*> buy_trades;

        for (const auto& trade : proposed_trades) {
            if (trade.action == core::Trade::Action::SELL) {
                sell_trades.push_back(&trade);
            } else if (trade.action == core::Trade::Action::BUY) {
                buy_trades.push_back(&trade);
            }
        }

        for (const auto* sell : sell_trades) {
            // Check if this is a sale at a loss
            auto lots = lot_manager_.getLotsForTicker(sell->ticker);
            double avg_cost = lot_manager_.getAverageCostBasis(sell->ticker);

            if (sell->price >= avg_cost) {
                continue;  // Not a loss, no wash sale possible
            }

            // Check against buys of substantially identical securities
            for (const auto* buy : buy_trades) {
                if (areSubstantiallyIdentical(sell->ticker, buy->ticker)) {
                    WashSaleViolation violation;
                    violation.lot_id = "";  // Would need lot-level tracking
                    violation.replacement_ticker = buy->ticker;
                    violation.violation_date = now;
                    violation.disallowed_loss = (avg_cost - sell->price) * sell->shares;
                    violation.description = "Selling " + sell->ticker + " at a loss and buying " +
                                           buy->ticker + " (substantially identical) triggers wash sale rule";
                    violations.push_back(violation);
                }
            }
        }

        return violations;
    }

    std::vector<TaxLot> selectOptimalLots(const std::string& ticker, double shares_to_sell,
                                           double current_price, bool prefer_losses) const {
        auto lots = lot_manager_.getLotsForTicker(ticker);

        // Filter unsold lots
        lots.erase(
            std::remove_if(lots.begin(), lots.end(),
                          [](const TaxLot& lot) { return lot.isSold(); }),
            lots.end());

        if (lots.empty()) {
            return {};
        }

        // Sort by tax efficiency
        if (prefer_losses) {
            // For tax-loss harvesting: prefer lots with largest losses (lowest prices)
            std::sort(lots.begin(), lots.end(),
                     [current_price](const TaxLot& a, const TaxLot& b) {
                         double gain_a = a.getUnrealizedGain(current_price);
                         double gain_b = b.getUnrealizedGain(current_price);
                         return gain_a < gain_b;  // Most negative (biggest loss) first
                     });
        } else {
            // To minimize gains: prefer lots with smallest gains (highest cost basis)
            std::sort(lots.begin(), lots.end(),
                     [](const TaxLot& a, const TaxLot& b) {
                         return a.cost_basis_per_share > b.cost_basis_per_share;
                     });
        }

        // Also consider long-term vs short-term for gains
        // Long-term gains have lower tax rates, so prefer selling long-term lots when realizing gains

        std::vector<TaxLot> selected;
        double remaining = shares_to_sell;

        for (const auto& lot : lots) {
            if (remaining <= 0) break;

            TaxLot selected_lot = lot;
            if (lot.shares <= remaining) {
                selected.push_back(selected_lot);
                remaining -= lot.shares;
            } else {
                selected_lot.shares = remaining;
                selected.push_back(selected_lot);
                remaining = 0;
            }
        }

        return selected;
    }

    CapitalGainsReport calculateCapitalGains(
        const std::vector<core::Trade>& trades,
        const std::map<std::string, double>& prices) const {

        CapitalGainsReport report;
        report.short_term_gains = 0.0;
        report.long_term_gains = 0.0;
        report.short_term_losses = 0.0;
        report.long_term_losses = 0.0;
        report.harvested_losses = 0.0;
        report.trades = trades;

        for (const auto& trade : trades) {
            if (trade.action != core::Trade::Action::SELL) {
                continue;  // Only sells generate taxable events
            }

            // Get lots that would be sold
            auto lots = lot_manager_.selectLotsToSell(
                trade.ticker, trade.shares, TaxLotMethod::FIFO);

            for (const auto& lot : lots) {
                double gain = (trade.price - lot.cost_basis_per_share) * lot.shares;
                GainType gain_type = lot.getGainType();

                if (gain >= 0) {
                    if (gain_type == GainType::SHORT_TERM) {
                        report.short_term_gains += gain;
                    } else {
                        report.long_term_gains += gain;
                    }
                } else {
                    double loss = std::abs(gain);
                    if (gain_type == GainType::SHORT_TERM) {
                        report.short_term_losses += loss;
                    } else {
                        report.long_term_losses += loss;
                    }
                    report.harvested_losses += loss;
                }
            }
        }

        report.net_short_term = report.short_term_gains - report.short_term_losses;
        report.net_long_term = report.long_term_gains - report.long_term_losses;

        // Calculate estimated tax savings from harvesting
        // Losses offset gains, and up to $3000 can offset ordinary income
        double loss_used_against_gains = std::min(
            report.harvested_losses,
            report.short_term_gains + report.long_term_gains);
        double loss_used_against_income = std::min(
            report.harvested_losses - loss_used_against_gains,
            3000.0);

        report.estimated_tax_savings =
            (loss_used_against_gains * tax_rate_short_) +  // Simplified
            (loss_used_against_income * tax_rate_short_);

        return report;
    }

    double estimateTaxImpact(const std::vector<core::Trade>& trades,
                             const std::map<std::string, double>& prices) const {
        double total_tax = 0.0;

        for (const auto& trade : trades) {
            if (trade.action != core::Trade::Action::SELL) {
                continue;
            }

            auto lots = lot_manager_.selectLotsToSell(
                trade.ticker, trade.shares, TaxLotMethod::FIFO);

            for (const auto& lot : lots) {
                double gain = (trade.price - lot.cost_basis_per_share) * lot.shares;
                if (gain > 0) {
                    double rate = (lot.getGainType() == GainType::SHORT_TERM)
                        ? tax_rate_short_ : tax_rate_long_;
                    total_tax += gain * rate;
                }
            }
        }

        return total_tax;
    }

    void setTaxRates(double short_term, double long_term) {
        tax_rate_short_ = short_term;
        tax_rate_long_ = long_term;
    }

    void setReplacementCandidates(const std::string& ticker,
                                  const std::vector<std::string>& candidates) {
        replacement_candidates_[ticker] = candidates;

        // Also update equivalence groups
        std::set<std::string> group;
        group.insert(ticker);
        for (const auto& candidate : candidates) {
            group.insert(candidate);
        }

        // Add all combinations to equivalence
        for (const auto& t1 : group) {
            for (const auto& t2 : group) {
                if (t1 != t2) {
                    equivalence_groups_[t1].insert(t2);
                }
            }
        }
    }

private:
    void initializeDefaultEquivalenceGroups() {
        // Total US Stock Market equivalents
        std::vector<std::string> total_market = {"VTI", "VTSAX", "SPTM", "ITOT", "SCHB"};
        for (const auto& ticker : total_market) {
            for (const auto& other : total_market) {
                if (ticker != other) {
                    equivalence_groups_[ticker].insert(other);
                }
            }
            replacement_candidates_[ticker] = total_market;
        }

        // S&P 500 equivalents
        std::vector<std::string> sp500 = {"VOO", "SPY", "IVV", "VFIAX", "SWPPX"};
        for (const auto& ticker : sp500) {
            for (const auto& other : sp500) {
                if (ticker != other) {
                    equivalence_groups_[ticker].insert(other);
                }
            }
            replacement_candidates_[ticker] = sp500;
        }

        // Total Bond Market equivalents
        std::vector<std::string> total_bond = {"BND", "AGG", "VBTLX", "SCHZ"};
        for (const auto& ticker : total_bond) {
            for (const auto& other : total_bond) {
                if (ticker != other) {
                    equivalence_groups_[ticker].insert(other);
                }
            }
            replacement_candidates_[ticker] = total_bond;
        }

        // International Developed equivalents
        std::vector<std::string> intl_developed = {"VXUS", "VEA", "IEFA", "SCHF", "VTIAX"};
        for (const auto& ticker : intl_developed) {
            for (const auto& other : intl_developed) {
                if (ticker != other) {
                    equivalence_groups_[ticker].insert(other);
                }
            }
            replacement_candidates_[ticker] = intl_developed;
        }
    }

    bool areSubstantiallyIdentical(const std::string& ticker1, const std::string& ticker2) const {
        if (ticker1 == ticker2) {
            return true;
        }

        auto it = equivalence_groups_.find(ticker1);
        if (it != equivalence_groups_.end()) {
            return it->second.count(ticker2) > 0;
        }

        return false;
    }

    std::vector<std::string> getReplacementCandidates(const std::string& ticker) const {
        auto it = replacement_candidates_.find(ticker);
        if (it != replacement_candidates_.end()) {
            // Return candidates that are NOT substantially identical (to avoid wash sale)
            std::vector<std::string> safe_candidates;
            for (const auto& candidate : it->second) {
                if (!areSubstantiallyIdentical(ticker, candidate)) {
                    safe_candidates.push_back(candidate);
                }
            }

            // If no safe candidates, return all (user should be warned about wash sale)
            if (safe_candidates.empty()) {
                return it->second;
            }
            return safe_candidates;
        }
        return {};
    }

    TaxLotManager& lot_manager_;
    double tax_rate_short_;
    double tax_rate_long_;
    int wash_sale_window_days_;

    // Ticker -> set of substantially identical tickers
    std::map<std::string, std::set<std::string>> equivalence_groups_;

    // Ticker -> replacement candidates (may include identical)
    std::map<std::string, std::vector<std::string>> replacement_candidates_;
};

TaxOptimizer::TaxOptimizer(TaxLotManager& lot_manager, double tax_rate_short, double tax_rate_long)
    : impl_(std::make_unique<Impl>(lot_manager, tax_rate_short, tax_rate_long)) {}

TaxOptimizer::~TaxOptimizer() = default;

std::vector<TaxHarvestingOpportunity> TaxOptimizer::findHarvestingOpportunities(
    const std::map<std::string, double>& current_prices,
    double min_loss_threshold) const {
    return impl_->findHarvestingOpportunities(current_prices, min_loss_threshold);
}

bool TaxOptimizer::wouldTriggerWashSale(const TaxLot& lot_to_sell,
                                        const std::string& replacement_ticker,
                                        const std::chrono::system_clock::time_point& trade_date) const {
    return impl_->wouldTriggerWashSale(lot_to_sell, replacement_ticker, trade_date);
}

std::vector<WashSaleViolation> TaxOptimizer::checkWashSaleViolations(
    const std::vector<core::Trade>& proposed_trades) const {
    return impl_->checkWashSaleViolations(proposed_trades);
}

std::vector<TaxLot> TaxOptimizer::selectOptimalLots(const std::string& ticker, double shares_to_sell,
                                                     double current_price, bool prefer_losses) const {
    return impl_->selectOptimalLots(ticker, shares_to_sell, current_price, prefer_losses);
}

CapitalGainsReport TaxOptimizer::calculateCapitalGains(
    const std::vector<core::Trade>& trades,
    const std::map<std::string, double>& prices) const {
    return impl_->calculateCapitalGains(trades, prices);
}

double TaxOptimizer::estimateTaxImpact(const std::vector<core::Trade>& trades,
                                        const std::map<std::string, double>& prices) const {
    return impl_->estimateTaxImpact(trades, prices);
}

void TaxOptimizer::setTaxRates(double short_term, double long_term) {
    impl_->setTaxRates(short_term, long_term);
}

void TaxOptimizer::setReplacementCandidates(const std::string& ticker,
                                            const std::vector<std::string>& candidates) {
    impl_->setReplacementCandidates(ticker, candidates);
}

}  // namespace lumen::data
