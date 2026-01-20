/// @file portfolio.cpp
/// @brief Portfolio implementation
///
/// Implementation of portfolio data structures and management.

#include "lumen/core/portfolio.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace lumen::core {

namespace {

/// Safely parse a double from a string with validation
/// @param str The string to parse
/// @param field_name Name of the field for error messages
/// @param line_num Line number for error messages (0 = unknown)
/// @return Parsed double value
/// @throws std::runtime_error if parsing fails or value is invalid
double safeParseDouble(const std::string& str, const std::string& field_name, int line_num = 0) {
    if (str.empty()) {
        throw std::runtime_error("Empty value for " + field_name +
            (line_num > 0 ? " at line " + std::to_string(line_num) : ""));
    }

    // Check for obviously invalid characters that could indicate injection
    for (char c : str) {
        if (!std::isdigit(c) && c != '.' && c != '-' && c != '+' && c != 'e' && c != 'E') {
            throw std::runtime_error("Invalid character in " + field_name + ": '" + str + "'" +
                (line_num > 0 ? " at line " + std::to_string(line_num) : ""));
        }
    }

    try {
        size_t pos = 0;
        double value = std::stod(str, &pos);

        // Ensure entire string was consumed
        if (pos != str.length()) {
            throw std::runtime_error("Trailing characters in " + field_name + ": '" + str + "'" +
                (line_num > 0 ? " at line " + std::to_string(line_num) : ""));
        }

        // Check for infinity or NaN
        if (!std::isfinite(value)) {
            throw std::runtime_error("Non-finite value in " + field_name + ": '" + str + "'" +
                (line_num > 0 ? " at line " + std::to_string(line_num) : ""));
        }

        return value;
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Value out of range for " + field_name + ": '" + str + "'" +
            (line_num > 0 ? " at line " + std::to_string(line_num) : ""));
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Invalid number format for " + field_name + ": '" + str + "'" +
            (line_num > 0 ? " at line " + std::to_string(line_num) : ""));
    }
}

/// Safely parse a double with a default value if the string is empty
double safeParseDoubleOrDefault(const std::string& str, double default_value,
                                 const std::string& field_name, int line_num = 0) {
    if (str.empty()) {
        return default_value;
    }
    return safeParseDouble(str, field_name, line_num);
}

}  // anonymous namespace

// =============================================================================
// AssetClass utilities
// =============================================================================

std::string assetClassToString(AssetClass ac) {
    switch (ac) {
        case AssetClass::STOCKS: return "stocks";
        case AssetClass::BONDS: return "bonds";
        case AssetClass::CASH: return "cash";
        case AssetClass::COMMODITIES: return "commodities";
        case AssetClass::REAL_ESTATE: return "real_estate";
        case AssetClass::CRYPTO: return "crypto";
        case AssetClass::OTHER: return "other";
        default: return "unknown";
    }
}

AssetClass assetClassFromString(const std::string& str) {
    if (str == "stocks") return AssetClass::STOCKS;
    if (str == "bonds") return AssetClass::BONDS;
    if (str == "cash") return AssetClass::CASH;
    if (str == "commodities") return AssetClass::COMMODITIES;
    if (str == "real_estate") return AssetClass::REAL_ESTATE;
    if (str == "crypto") return AssetClass::CRYPTO;
    return AssetClass::OTHER;
}

// =============================================================================
// Position implementation
// =============================================================================

double Position::getCurrentValue() const {
    return shares * current_price;
}

double Position::getCostBasisTotal() const {
    return shares * cost_basis;
}

double Position::getUnrealizedGain() const {
    return getCurrentValue() - getCostBasisTotal();
}

double Position::getUnrealizedGainPercent() const {
    double basis = getCostBasisTotal();
    if (basis <= 0) return 0.0;
    return (getUnrealizedGain() / basis) * 100.0;
}

bool Position::isLongTerm() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now - purchase_date;
    auto days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
    return days > 365;
}

nlohmann::json Position::toJSON() const {
    return nlohmann::json{
        {"ticker", ticker},
        {"shares", shares},
        {"current_price", current_price},
        {"cost_basis", cost_basis},
        {"purchase_date", std::chrono::system_clock::to_time_t(purchase_date)},
        {"asset_class", assetClassToString(asset_class)},
        {"exchange", exchange},
        {"supports_fractional", supports_fractional}
    };
}

Position Position::fromJSON(const nlohmann::json& j) {
    Position pos;

    // Validate and parse ticker
    if (!j.contains("ticker") || !j["ticker"].is_string()) {
        throw std::runtime_error("Position::fromJSON: missing or invalid 'ticker' field");
    }
    pos.ticker = j.at("ticker").get<std::string>();
    if (pos.ticker.empty() || pos.ticker.length() > 20) {
        throw std::runtime_error("Position::fromJSON: ticker must be 1-20 characters");
    }
    for (char c : pos.ticker) {
        if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') {
            throw std::runtime_error("Position::fromJSON: invalid character in ticker: " + pos.ticker);
        }
    }

    // Validate and parse shares
    if (!j.contains("shares") || !j["shares"].is_number()) {
        throw std::runtime_error("Position::fromJSON: missing or invalid 'shares' field");
    }
    pos.shares = j.at("shares").get<double>();
    if (!std::isfinite(pos.shares) || pos.shares < 0) {
        throw std::runtime_error("Position::fromJSON: shares must be a non-negative finite number");
    }
    if (pos.shares > 1e15) {
        throw std::runtime_error("Position::fromJSON: shares exceeds reasonable limit");
    }

    // Validate and parse current_price
    if (j.contains("current_price")) {
        if (!j["current_price"].is_number()) {
            throw std::runtime_error("Position::fromJSON: current_price must be a number");
        }
        pos.current_price = j["current_price"].get<double>();
        if (!std::isfinite(pos.current_price) || pos.current_price < 0) {
            throw std::runtime_error("Position::fromJSON: current_price must be a non-negative finite number");
        }
        if (pos.current_price > 1e12) {  // $1 trillion per share
            throw std::runtime_error("Position::fromJSON: current_price exceeds reasonable limit");
        }
    } else {
        pos.current_price = 0.0;
    }

    // Validate and parse cost_basis
    if (!j.contains("cost_basis") || !j["cost_basis"].is_number()) {
        throw std::runtime_error("Position::fromJSON: missing or invalid 'cost_basis' field");
    }
    pos.cost_basis = j.at("cost_basis").get<double>();
    if (!std::isfinite(pos.cost_basis) || pos.cost_basis < 0) {
        throw std::runtime_error("Position::fromJSON: cost_basis must be a non-negative finite number");
    }
    if (pos.cost_basis > 1e12) {
        throw std::runtime_error("Position::fromJSON: cost_basis exceeds reasonable limit");
    }

    // Parse purchase_date (optional)
    pos.purchase_date = std::chrono::system_clock::from_time_t(
        j.value("purchase_date", std::time_t{0}));

    // Parse asset_class (optional)
    if (j.contains("asset_class")) {
        if (!j["asset_class"].is_string()) {
            throw std::runtime_error("Position::fromJSON: asset_class must be a string");
        }
        std::string ac_str = j["asset_class"].get<std::string>();
        if (ac_str.length() > 50) {
            throw std::runtime_error("Position::fromJSON: asset_class too long");
        }
        pos.asset_class = assetClassFromString(ac_str);
    } else {
        pos.asset_class = AssetClass::OTHER;
    }

    // Parse exchange (optional)
    if (j.contains("exchange")) {
        if (!j["exchange"].is_string()) {
            throw std::runtime_error("Position::fromJSON: exchange must be a string");
        }
        pos.exchange = j["exchange"].get<std::string>();
        if (pos.exchange.length() > 50) {
            throw std::runtime_error("Position::fromJSON: exchange too long");
        }
    } else {
        pos.exchange = "";
    }

    // Parse supports_fractional (optional)
    if (j.contains("supports_fractional")) {
        if (!j["supports_fractional"].is_boolean()) {
            throw std::runtime_error("Position::fromJSON: supports_fractional must be a boolean");
        }
        pos.supports_fractional = j["supports_fractional"].get<bool>();
    } else {
        pos.supports_fractional = false;
    }

    return pos;
}

// =============================================================================
// TargetAllocation implementation
// =============================================================================

TargetAllocation::TargetAllocation(AllocationMode mode) : mode_(mode) {}

void TargetAllocation::setTarget(const std::string& identifier, double target,
                                  double tolerance, bool is_asset_class) {
    AllocationTarget t;
    t.identifier = identifier;
    t.target_value = target;
    t.lower_bound = target - tolerance;
    t.upper_bound = target + tolerance;
    t.is_asset_class = is_asset_class;
    targets_[identifier] = t;
}

void TargetAllocation::setTargetWithBounds(const std::string& identifier, double target,
                                            double lower, double upper, bool is_asset_class) {
    AllocationTarget t;
    t.identifier = identifier;
    t.target_value = target;
    t.lower_bound = lower;
    t.upper_bound = upper;
    t.is_asset_class = is_asset_class;
    targets_[identifier] = t;
}

void TargetAllocation::removeTarget(const std::string& identifier) {
    targets_.erase(identifier);
}

double TargetAllocation::getTarget(const std::string& identifier) const {
    auto it = targets_.find(identifier);
    if (it == targets_.end()) return 0.0;
    return it->second.target_value;
}

AllocationTarget TargetAllocation::getFullTarget(const std::string& identifier) const {
    auto it = targets_.find(identifier);
    if (it == targets_.end()) {
        throw std::out_of_range("Target not found: " + identifier);
    }
    return it->second;
}

std::vector<AllocationTarget> TargetAllocation::getAllTargets() const {
    std::vector<AllocationTarget> result;
    result.reserve(targets_.size());
    for (const auto& [key, target] : targets_) {
        result.push_back(target);
    }
    return result;
}

bool TargetAllocation::validate() const {
    return getValidationErrors().empty();
}

std::vector<std::string> TargetAllocation::getValidationErrors() const {
    std::vector<std::string> errors;

    if (mode_ == AllocationMode::PERCENTAGE) {
        // Check that ticker-level targets sum to approximately 100%
        double ticker_sum = 0.0;
        double asset_class_sum = 0.0;

        for (const auto& [key, target] : targets_) {
            if (target.is_asset_class) {
                asset_class_sum += target.target_value;
            } else {
                ticker_sum += target.target_value;
            }

            // Validate bounds are sensible
            if (target.lower_bound > target.target_value) {
                errors.push_back("Lower bound exceeds target for: " + target.identifier);
            }
            if (target.upper_bound < target.target_value) {
                errors.push_back("Upper bound is below target for: " + target.identifier);
            }
            if (target.lower_bound < 0.0) {
                errors.push_back("Negative lower bound for: " + target.identifier);
            }
            if (target.upper_bound > 1.0) {
                errors.push_back("Upper bound exceeds 100% for: " + target.identifier);
            }
        }

        // Allow for floating point tolerance (1e-6)
        constexpr double tolerance = 1e-6;

        // If we have ticker-level targets, they should sum close to 100%
        if (ticker_sum > 0 && std::abs(ticker_sum - 1.0) > tolerance) {
            errors.push_back("Ticker allocations sum to " +
                           std::to_string(ticker_sum * 100.0) + "%, expected 100%");
        }

        // If we have asset class targets, they should sum close to 100%
        if (asset_class_sum > 0 && std::abs(asset_class_sum - 1.0) > tolerance) {
            errors.push_back("Asset class allocations sum to " +
                           std::to_string(asset_class_sum * 100.0) + "%, expected 100%");
        }
    }

    return errors;
}

nlohmann::json TargetAllocation::toJSON() const {
    nlohmann::json j;
    j["mode"] = (mode_ == AllocationMode::PERCENTAGE) ? "percentage" : "dollar_amount";
    j["targets"] = nlohmann::json::array();
    for (const auto& [key, target] : targets_) {
        j["targets"].push_back({
            {"identifier", target.identifier},
            {"target_value", target.target_value},
            {"lower_bound", target.lower_bound},
            {"upper_bound", target.upper_bound},
            {"is_asset_class", target.is_asset_class}
        });
    }
    return j;
}

TargetAllocation TargetAllocation::fromJSON(const nlohmann::json& j) {
    // Validate mode field
    if (j.contains("mode") && !j["mode"].is_string()) {
        throw std::runtime_error("TargetAllocation::fromJSON: mode must be a string");
    }
    std::string mode_str = j.value("mode", "percentage");
    if (mode_str != "percentage" && mode_str != "dollar_amount") {
        throw std::runtime_error("TargetAllocation::fromJSON: invalid mode (must be 'percentage' or 'dollar_amount')");
    }

    AllocationMode mode = (mode_str == "percentage")
        ? AllocationMode::PERCENTAGE : AllocationMode::DOLLAR_AMOUNT;
    TargetAllocation alloc(mode);

    // Validate targets array exists and is an array
    if (!j.contains("targets") || !j["targets"].is_array()) {
        throw std::runtime_error("TargetAllocation::fromJSON: missing or invalid 'targets' array");
    }

    // Limit number of targets to prevent DoS
    constexpr size_t MAX_TARGETS = 1000;
    if (j["targets"].size() > MAX_TARGETS) {
        throw std::runtime_error("TargetAllocation::fromJSON: too many targets (max " +
            std::to_string(MAX_TARGETS) + ")");
    }

    for (const auto& t : j["targets"]) {
        // Validate required fields
        if (!t.contains("identifier") || !t["identifier"].is_string()) {
            throw std::runtime_error("TargetAllocation::fromJSON: target missing 'identifier' string");
        }
        if (!t.contains("target_value") || !t["target_value"].is_number()) {
            throw std::runtime_error("TargetAllocation::fromJSON: target missing 'target_value' number");
        }
        if (!t.contains("lower_bound") || !t["lower_bound"].is_number()) {
            throw std::runtime_error("TargetAllocation::fromJSON: target missing 'lower_bound' number");
        }
        if (!t.contains("upper_bound") || !t["upper_bound"].is_number()) {
            throw std::runtime_error("TargetAllocation::fromJSON: target missing 'upper_bound' number");
        }

        std::string identifier = t.at("identifier").get<std::string>();
        double target_value = t.at("target_value").get<double>();
        double lower_bound = t.at("lower_bound").get<double>();
        double upper_bound = t.at("upper_bound").get<double>();
        bool is_asset_class = t.value("is_asset_class", false);

        // Validate identifier
        if (identifier.empty() || identifier.length() > 100) {
            throw std::runtime_error("TargetAllocation::fromJSON: identifier must be 1-100 characters");
        }

        // Validate numeric values are finite
        if (!std::isfinite(target_value) || !std::isfinite(lower_bound) || !std::isfinite(upper_bound)) {
            throw std::runtime_error("TargetAllocation::fromJSON: allocation values must be finite");
        }

        // Validate bounds make sense
        if (lower_bound < 0.0) {
            throw std::runtime_error("TargetAllocation::fromJSON: lower_bound cannot be negative");
        }
        if (mode == AllocationMode::PERCENTAGE) {
            if (upper_bound > 1.0) {
                throw std::runtime_error("TargetAllocation::fromJSON: upper_bound cannot exceed 1.0 (100%)");
            }
            if (target_value < 0.0 || target_value > 1.0) {
                throw std::runtime_error("TargetAllocation::fromJSON: target_value must be between 0 and 1");
            }
        } else {
            // Dollar amount mode - reasonable limits
            if (upper_bound > 1e15 || target_value > 1e15) {
                throw std::runtime_error("TargetAllocation::fromJSON: values exceed reasonable limits");
            }
        }
        if (lower_bound > upper_bound) {
            throw std::runtime_error("TargetAllocation::fromJSON: lower_bound cannot exceed upper_bound");
        }

        alloc.setTargetWithBounds(identifier, target_value, lower_bound, upper_bound, is_asset_class);
    }
    return alloc;
}

// =============================================================================
// Portfolio implementation
// =============================================================================

Portfolio::Portfolio(const std::string& name) : name_(name) {
    last_updated_ = std::chrono::system_clock::now();
}

void Portfolio::addPosition(const Position& pos) {
    positions_[pos.ticker] = pos;
    last_updated_ = std::chrono::system_clock::now();
}

void Portfolio::addPosition(const std::string& ticker, double shares, double cost_basis) {
    Position pos;
    pos.ticker = ticker;
    pos.shares = shares;
    pos.cost_basis = cost_basis;
    pos.current_price = cost_basis;  // Default to cost basis
    pos.asset_class = AssetClass::STOCKS;  // Default
    pos.purchase_date = std::chrono::system_clock::now();
    pos.supports_fractional = false;
    addPosition(pos);
}

void Portfolio::removePosition(const std::string& ticker) {
    positions_.erase(ticker);
    last_updated_ = std::chrono::system_clock::now();
}

void Portfolio::updatePosition(const std::string& ticker, double new_shares) {
    auto it = positions_.find(ticker);
    if (it != positions_.end()) {
        it->second.shares = new_shares;
        last_updated_ = std::chrono::system_clock::now();
    }
}

void Portfolio::updatePrice(const std::string& ticker, double new_price) {
    auto it = positions_.find(ticker);
    if (it != positions_.end()) {
        it->second.current_price = new_price;
        last_updated_ = std::chrono::system_clock::now();
    }
}

void Portfolio::updateAllPrices(const std::map<std::string, double>& prices) {
    for (const auto& [ticker, price] : prices) {
        auto it = positions_.find(ticker);
        if (it != positions_.end()) {
            it->second.current_price = price;
        }
    }
    last_updated_ = std::chrono::system_clock::now();
}

const Position& Portfolio::getPosition(const std::string& ticker) const {
    auto it = positions_.find(ticker);
    if (it == positions_.end()) {
        throw std::out_of_range("Position not found: " + ticker);
    }
    return it->second;
}

std::vector<Position> Portfolio::getAllPositions() const {
    std::vector<Position> result;
    result.reserve(positions_.size());
    for (const auto& [key, pos] : positions_) {
        result.push_back(pos);
    }
    return result;
}

std::vector<std::string> Portfolio::getAllTickers() const {
    std::vector<std::string> result;
    result.reserve(positions_.size());
    for (const auto& [key, pos] : positions_) {
        result.push_back(key);
    }
    return result;
}

bool Portfolio::hasPosition(const std::string& ticker) const {
    return positions_.find(ticker) != positions_.end();
}

double Portfolio::getTotalValue() const {
    double total = cash_balance_;
    for (const auto& [key, pos] : positions_) {
        total += pos.getCurrentValue();
    }
    return total;
}

double Portfolio::getTotalCostBasis() const {
    double total = 0.0;
    for (const auto& [key, pos] : positions_) {
        total += pos.getCostBasisTotal();
    }
    return total;
}

double Portfolio::getTotalUnrealizedGain() const {
    double total = 0.0;
    for (const auto& [key, pos] : positions_) {
        total += pos.getUnrealizedGain();
    }
    return total;
}

double Portfolio::getAllocationPercent(const std::string& ticker) const {
    double total = getTotalValue();
    if (total <= 0) return 0.0;

    auto it = positions_.find(ticker);
    if (it == positions_.end()) return 0.0;

    return it->second.getCurrentValue() / total;
}

std::map<std::string, double> Portfolio::getAllocationMap() const {
    std::map<std::string, double> result;
    double total = getTotalValue();
    if (total <= 0) return result;

    for (const auto& [key, pos] : positions_) {
        result[key] = pos.getCurrentValue() / total;
    }
    return result;
}

std::map<AssetClass, double> Portfolio::getAssetClassExposure() const {
    std::map<AssetClass, double> result;
    double total = getTotalValue();
    if (total <= 0) return result;

    for (const auto& [key, pos] : positions_) {
        result[pos.asset_class] += pos.getCurrentValue() / total;
    }
    return result;
}

double Portfolio::getAssetClassPercent(AssetClass ac) const {
    auto exposure = getAssetClassExposure();
    auto it = exposure.find(ac);
    return (it != exposure.end()) ? it->second : 0.0;
}

double Portfolio::calculateDrift(const TargetAllocation& target) const {
    // Calculate total absolute deviation from target allocations
    // Drift = Σᵢ |w_current_i - w_target_i|
    double total_drift = 0.0;
    auto deviations = getDeviationMap(target);

    for (const auto& [identifier, deviation] : deviations) {
        total_drift += std::abs(deviation);
    }

    return total_drift;
}

std::map<std::string, double> Portfolio::getDeviationMap(const TargetAllocation& target) const {
    std::map<std::string, double> deviations;
    double total_value = getTotalValue();

    if (total_value <= 0) {
        return deviations;
    }

    for (const auto& alloc_target : target.getAllTargets()) {
        double current_allocation = 0.0;

        if (alloc_target.is_asset_class) {
            // Calculate allocation by asset class
            AssetClass ac = assetClassFromString(alloc_target.identifier);
            current_allocation = getAssetClassPercent(ac);
        } else {
            // Calculate allocation by ticker
            current_allocation = getAllocationPercent(alloc_target.identifier);
        }

        // Deviation = current - target (positive means overweight)
        deviations[alloc_target.identifier] = current_allocation - alloc_target.target_value;
    }

    return deviations;
}

bool Portfolio::needsRebalancing(const TargetAllocation& target) const {
    // Check if any position is outside its tolerance band
    for (const auto& alloc_target : target.getAllTargets()) {
        double current_allocation = 0.0;

        if (alloc_target.is_asset_class) {
            AssetClass ac = assetClassFromString(alloc_target.identifier);
            current_allocation = getAssetClassPercent(ac);
        } else {
            current_allocation = getAllocationPercent(alloc_target.identifier);
        }

        // Check if outside bounds
        if (current_allocation < alloc_target.lower_bound ||
            current_allocation > alloc_target.upper_bound) {
            return true;
        }
    }

    return false;
}

nlohmann::json Portfolio::toJSON() const {
    nlohmann::json j;
    j["name"] = name_;
    j["currency"] = currency_;
    j["cash_balance"] = cash_balance_;
    j["last_updated"] = std::chrono::system_clock::to_time_t(last_updated_);
    j["positions"] = nlohmann::json::array();
    for (const auto& [key, pos] : positions_) {
        j["positions"].push_back(pos.toJSON());
    }
    return j;
}

Portfolio Portfolio::fromJSON(const nlohmann::json& j) {
    // Validate name field
    if (j.contains("name") && !j["name"].is_string()) {
        throw std::runtime_error("Portfolio::fromJSON: name must be a string");
    }
    std::string name = j.value("name", "");
    if (name.length() > 200) {
        throw std::runtime_error("Portfolio::fromJSON: name too long (max 200 chars)");
    }

    Portfolio portfolio(name);

    // Validate and parse currency
    if (j.contains("currency")) {
        if (!j["currency"].is_string()) {
            throw std::runtime_error("Portfolio::fromJSON: currency must be a string");
        }
        portfolio.currency_ = j["currency"].get<std::string>();
        if (portfolio.currency_.length() > 10) {
            throw std::runtime_error("Portfolio::fromJSON: currency too long (max 10 chars)");
        }
    } else {
        portfolio.currency_ = "USD";
    }

    // Validate and parse cash_balance
    if (j.contains("cash_balance")) {
        if (!j["cash_balance"].is_number()) {
            throw std::runtime_error("Portfolio::fromJSON: cash_balance must be a number");
        }
        portfolio.cash_balance_ = j["cash_balance"].get<double>();
        if (!std::isfinite(portfolio.cash_balance_)) {
            throw std::runtime_error("Portfolio::fromJSON: cash_balance must be finite");
        }
        if (portfolio.cash_balance_ < -1e15 || portfolio.cash_balance_ > 1e15) {
            throw std::runtime_error("Portfolio::fromJSON: cash_balance exceeds reasonable limits");
        }
    } else {
        portfolio.cash_balance_ = 0.0;
    }

    // Parse last_updated
    portfolio.last_updated_ = std::chrono::system_clock::from_time_t(
        j.value("last_updated", std::time_t{0}));

    // Validate positions array
    if (!j.contains("positions") || !j["positions"].is_array()) {
        throw std::runtime_error("Portfolio::fromJSON: missing or invalid 'positions' array");
    }

    // Limit number of positions to prevent DoS
    constexpr size_t MAX_POSITIONS = 10000;
    if (j["positions"].size() > MAX_POSITIONS) {
        throw std::runtime_error("Portfolio::fromJSON: too many positions (max " +
            std::to_string(MAX_POSITIONS) + ")");
    }

    for (const auto& pos_json : j["positions"]) {
        portfolio.addPosition(Position::fromJSON(pos_json));
    }
    return portfolio;
}

Portfolio Portfolio::fromCSV(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open CSV file: " + filepath);
    }

    Portfolio portfolio;
    std::string line;
    bool header_parsed = false;
    int line_num = 0;

    // Column indices (will be set from header)
    int ticker_col = -1;
    int shares_col = -1;
    int price_col = -1;
    int cost_basis_col = -1;
    int asset_class_col = -1;
    int exchange_col = -1;
    int fractional_col = -1;

    // Limit maximum file size to prevent DoS (10MB)
    constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;
    file.seekg(0, std::ios::end);
    size_t file_size = static_cast<size_t>(file.tellg());
    if (file_size > MAX_FILE_SIZE) {
        throw std::runtime_error("CSV file exceeds maximum allowed size (10MB)");
    }
    file.seekg(0, std::ios::beg);

    // Limit maximum number of positions to prevent memory exhaustion
    constexpr size_t MAX_POSITIONS = 10000;

    while (std::getline(file, line)) {
        ++line_num;

        // Skip empty lines
        if (line.empty()) continue;

        // Limit line length to prevent memory issues
        constexpr size_t MAX_LINE_LENGTH = 4096;
        if (line.length() > MAX_LINE_LENGTH) {
            throw std::runtime_error("CSV line " + std::to_string(line_num) +
                " exceeds maximum length (" + std::to_string(MAX_LINE_LENGTH) + " chars)");
        }

        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;

        while (std::getline(ss, field, ',')) {
            // Trim whitespace
            size_t start = field.find_first_not_of(" \t\r\n");
            size_t end = field.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                field = field.substr(start, end - start + 1);
            } else {
                field = "";
            }
            fields.push_back(field);
        }

        if (!header_parsed) {
            // Parse header to find column indices
            for (size_t i = 0; i < fields.size(); ++i) {
                std::string lower_field = fields[i];
                std::transform(lower_field.begin(), lower_field.end(),
                             lower_field.begin(), ::tolower);

                if (lower_field == "ticker" || lower_field == "symbol") {
                    ticker_col = static_cast<int>(i);
                } else if (lower_field == "shares" || lower_field == "quantity") {
                    shares_col = static_cast<int>(i);
                } else if (lower_field == "price" || lower_field == "current_price") {
                    price_col = static_cast<int>(i);
                } else if (lower_field == "cost_basis" || lower_field == "cost" ||
                          lower_field == "avg_cost") {
                    cost_basis_col = static_cast<int>(i);
                } else if (lower_field == "asset_class" || lower_field == "type") {
                    asset_class_col = static_cast<int>(i);
                } else if (lower_field == "exchange") {
                    exchange_col = static_cast<int>(i);
                } else if (lower_field == "fractional" || lower_field == "supports_fractional") {
                    fractional_col = static_cast<int>(i);
                }
            }

            // Validate required columns
            if (ticker_col < 0 || shares_col < 0) {
                throw std::runtime_error("CSV must have 'ticker' and 'shares' columns");
            }

            header_parsed = true;
            continue;
        }

        // Check position limit
        if (portfolio.getPositionCount() >= MAX_POSITIONS) {
            throw std::runtime_error("CSV exceeds maximum number of positions (" +
                std::to_string(MAX_POSITIONS) + ")");
        }

        // Parse data row
        if (static_cast<int>(fields.size()) <= ticker_col ||
            static_cast<int>(fields.size()) <= shares_col) {
            continue;  // Skip malformed rows
        }

        Position pos;

        // Validate ticker (alphanumeric with limited special chars)
        pos.ticker = fields[ticker_col];
        if (pos.ticker.empty() || pos.ticker.length() > 20) {
            throw std::runtime_error("Invalid ticker at line " + std::to_string(line_num) +
                ": must be 1-20 characters");
        }
        for (char c : pos.ticker) {
            if (!std::isalnum(c) && c != '.' && c != '-' && c != '_') {
                throw std::runtime_error("Invalid character in ticker at line " +
                    std::to_string(line_num) + ": " + pos.ticker);
            }
        }

        // Parse shares with validation
        pos.shares = safeParseDouble(fields[shares_col], "shares", line_num);
        if (pos.shares < 0) {
            throw std::runtime_error("Negative shares at line " + std::to_string(line_num));
        }

        // Parse optional price
        if (price_col >= 0 && static_cast<int>(fields.size()) > price_col) {
            pos.current_price = safeParseDoubleOrDefault(fields[price_col], 0.0, "price", line_num);
            if (pos.current_price < 0) {
                throw std::runtime_error("Negative price at line " + std::to_string(line_num));
            }
        } else {
            pos.current_price = 0.0;
        }

        // Parse optional cost basis
        if (cost_basis_col >= 0 && static_cast<int>(fields.size()) > cost_basis_col) {
            pos.cost_basis = safeParseDoubleOrDefault(fields[cost_basis_col],
                pos.current_price, "cost_basis", line_num);
            if (pos.cost_basis < 0) {
                throw std::runtime_error("Negative cost basis at line " + std::to_string(line_num));
            }
        } else {
            pos.cost_basis = pos.current_price;  // Default to current price
        }

        if (asset_class_col >= 0 && static_cast<int>(fields.size()) > asset_class_col &&
            !fields[asset_class_col].empty()) {
            pos.asset_class = assetClassFromString(fields[asset_class_col]);
        } else {
            pos.asset_class = AssetClass::STOCKS;
        }

        if (exchange_col >= 0 && static_cast<int>(fields.size()) > exchange_col) {
            pos.exchange = fields[exchange_col];
            // Limit exchange field length
            if (pos.exchange.length() > 50) {
                pos.exchange = pos.exchange.substr(0, 50);
            }
        }

        if (fractional_col >= 0 && static_cast<int>(fields.size()) > fractional_col &&
            !fields[fractional_col].empty()) {
            std::string val = fields[fractional_col];
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            pos.supports_fractional = (val == "true" || val == "yes" || val == "1");
        } else {
            pos.supports_fractional = false;
        }

        pos.purchase_date = std::chrono::system_clock::now();

        portfolio.addPosition(pos);
    }

    return portfolio;
}

void Portfolio::saveToFile(const std::string& filepath) const {
    // Validate filepath to prevent path traversal attacks
    if (filepath.empty()) {
        throw std::runtime_error("Portfolio::saveToFile: filepath cannot be empty");
    }

    // Reject paths with null bytes (could bypass checks)
    if (filepath.find('\0') != std::string::npos) {
        throw std::runtime_error("Portfolio::saveToFile: filepath contains null byte");
    }

    // Limit filepath length
    if (filepath.length() > 4096) {
        throw std::runtime_error("Portfolio::saveToFile: filepath too long (max 4096 chars)");
    }

    // Normalize and check for path traversal
    try {
        std::filesystem::path normalized = std::filesystem::weakly_canonical(
            std::filesystem::path(filepath));

        // Check that the parent directory exists or can be created
        std::filesystem::path parent = normalized.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            throw std::runtime_error("Portfolio::saveToFile: parent directory does not exist: " +
                parent.string());
        }

        std::ofstream file(normalized);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filepath);
        }
        file << toJSON().dump(2);
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error("Portfolio::saveToFile: filesystem error: " + std::string(e.what()));
    }
}

}  // namespace lumen::core
