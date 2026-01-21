/// @file broker_import.cpp
/// @brief Broker-specific tax lot import implementation
///
/// Implementation of parsers for Schwab, Fidelity, Vanguard, and generic CSV formats.

#include "lumen/data/broker_import.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace lumen::data {

// =============================================================================
// Utility Functions
// =============================================================================

std::string brokerFormatToString(BrokerFormat format) {
    switch (format) {
        case BrokerFormat::GENERIC_CSV: return "generic_csv";
        case BrokerFormat::SCHWAB: return "schwab";
        case BrokerFormat::FIDELITY: return "fidelity";
        case BrokerFormat::VANGUARD: return "vanguard";
        case BrokerFormat::AUTO_DETECT: return "auto_detect";
        default: return "unknown";
    }
}

BrokerFormat brokerFormatFromString(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "generic_csv" || lower == "csv" || lower == "generic") {
        return BrokerFormat::GENERIC_CSV;
    } else if (lower == "schwab" || lower == "charles_schwab") {
        return BrokerFormat::SCHWAB;
    } else if (lower == "fidelity") {
        return BrokerFormat::FIDELITY;
    } else if (lower == "vanguard") {
        return BrokerFormat::VANGUARD;
    } else if (lower == "auto" || lower == "auto_detect") {
        return BrokerFormat::AUTO_DETECT;
    }
    return BrokerFormat::GENERIC_CSV;
}

namespace {

// Read entire file contents
std::string readFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_path);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Write string to file
bool writeFile(const std::string& file_path, const std::string& content) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return true;
}

// Trim whitespace from string
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

/// @brief SECURITY: Sanitize CSV field to prevent formula injection
/// @details When CSV data is opened in spreadsheet applications (Excel, Google Sheets),
///          cells starting with =, +, -, @, or tab can be interpreted as formulas,
///          potentially leading to code execution or data exfiltration.
std::string sanitizeCSVField(const std::string& field) {
    if (field.empty()) {
        return field;
    }

    // Characters that trigger formula interpretation in spreadsheets
    static const std::string dangerous_prefixes = "=+-@\t";

    char first_char = field[0];
    if (dangerous_prefixes.find(first_char) != std::string::npos) {
        // Prefix with single quote to prevent formula interpretation
        return "'" + field;
    }

    return field;
}

/// @brief SECURITY: Check if a CSV field contains potentially dangerous content
bool isFieldSuspicious(const std::string& field) {
    if (field.empty()) return false;

    // Check for formula injection attempts
    static const std::string dangerous_prefixes = "=+-@\t";
    if (dangerous_prefixes.find(field[0]) != std::string::npos) {
        return true;
    }

    // Check for DDE (Dynamic Data Exchange) injection
    // Patterns like =cmd|' or @SUM(1+1)*cmd|'
    std::string lower_field = field;
    std::transform(lower_field.begin(), lower_field.end(), lower_field.begin(), ::tolower);
    if (lower_field.find("cmd|") != std::string::npos ||
        lower_field.find("powershell|") != std::string::npos ||
        lower_field.find("|'") != std::string::npos) {
        return true;
    }

    return false;
}

// Parse CSV line respecting quotes with security sanitization
std::vector<std::string> parseCSVLine(const std::string& line, char delimiter = ',',
                                       bool sanitize = true) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                // Escaped quote
                field += '"';
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == delimiter && !in_quotes) {
            std::string trimmed = trim(field);
            // SECURITY: Sanitize field if requested
            if (sanitize) {
                trimmed = sanitizeCSVField(trimmed);
            }
            fields.push_back(trimmed);
            field.clear();
        } else {
            field += c;
        }
    }

    std::string trimmed = trim(field);
    if (sanitize) {
        trimmed = sanitizeCSVField(trimmed);
    }
    fields.push_back(trimmed);

    return fields;
}

// Parse date string with multiple format attempts
std::chrono::system_clock::time_point parseDate(const std::string& date_str,
                                                 const std::string& format = "") {
    std::vector<std::string> formats = {
        "%Y-%m-%d",
        "%m/%d/%Y",
        "%m/%d/%y",
        "%d-%b-%Y",
        "%b %d, %Y",
        "%Y/%m/%d"
    };

    if (!format.empty()) {
        formats.insert(formats.begin(), format);
    }

    std::string cleaned = trim(date_str);

    for (const auto& fmt : formats) {
        std::tm tm = {};
        std::istringstream ss(cleaned);
        ss >> std::get_time(&tm, fmt.c_str());
        if (!ss.fail()) {
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        }
    }

    // Fallback to current date
    return std::chrono::system_clock::now();
}

// Format date to ISO string
std::string formatDate(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time_t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d");
    return ss.str();
}

// Parse dollar amount (remove $, commas, etc.)
double parseDollarAmount(const std::string& str) {
    std::string cleaned;
    bool negative = false;

    for (char c : str) {
        if (c == '-' || c == '(') {
            negative = true;
        } else if (std::isdigit(c) || c == '.') {
            cleaned += c;
        }
    }

    if (cleaned.empty()) {
        return 0.0;
    }

    double value = std::stod(cleaned);
    return negative ? -value : value;
}

// Parse share quantity (may include commas)
double parseShares(const std::string& str) {
    std::string cleaned;
    for (char c : str) {
        if (std::isdigit(c) || c == '.' || c == '-') {
            cleaned += c;
        }
    }

    if (cleaned.empty()) {
        return 0.0;
    }

    return std::stod(cleaned);
}

// Generate unique lot ID
int g_lot_counter = 1;
std::string generateLotId(const std::string& ticker) {
    return "LOT_" + ticker + "_" + std::to_string(g_lot_counter++);
}

// Check if string contains (case-insensitive)
bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    std::string lower_haystack = haystack;
    std::string lower_needle = needle;
    std::transform(lower_haystack.begin(), lower_haystack.end(), lower_haystack.begin(), ::tolower);
    std::transform(lower_needle.begin(), lower_needle.end(), lower_needle.begin(), ::tolower);
    return lower_haystack.find(lower_needle) != std::string::npos;
}

}  // anonymous namespace

// =============================================================================
// CSVColumnMapping
// =============================================================================

nlohmann::json CSVColumnMapping::toJSON() const {
    return nlohmann::json{
        {"ticker_column", ticker_column},
        {"shares_column", shares_column},
        {"cost_basis_column", cost_basis_column},
        {"purchase_date_column", purchase_date_column},
        {"lot_id_column", lot_id_column},
        {"sale_date_column", sale_date_column},
        {"sale_price_column", sale_price_column},
        {"has_header", has_header},
        {"delimiter", std::string(1, delimiter)},
        {"date_format", date_format}
    };
}

CSVColumnMapping CSVColumnMapping::fromJSON(const nlohmann::json& j) {
    CSVColumnMapping mapping;
    if (j.contains("ticker_column")) mapping.ticker_column = j["ticker_column"];
    if (j.contains("shares_column")) mapping.shares_column = j["shares_column"];
    if (j.contains("cost_basis_column")) mapping.cost_basis_column = j["cost_basis_column"];
    if (j.contains("purchase_date_column")) mapping.purchase_date_column = j["purchase_date_column"];
    if (j.contains("lot_id_column")) mapping.lot_id_column = j["lot_id_column"];
    if (j.contains("sale_date_column")) mapping.sale_date_column = j["sale_date_column"];
    if (j.contains("sale_price_column")) mapping.sale_price_column = j["sale_price_column"];
    if (j.contains("has_header")) mapping.has_header = j["has_header"];
    if (j.contains("delimiter")) {
        std::string d = j["delimiter"];
        mapping.delimiter = d.empty() ? ',' : d[0];
    }
    if (j.contains("date_format")) mapping.date_format = j["date_format"];
    return mapping;
}

// =============================================================================
// ImportResult
// =============================================================================

nlohmann::json ImportResult::toJSON() const {
    nlohmann::json j;
    j["success"] = success;
    j["rows_processed"] = rows_processed;
    j["rows_imported"] = rows_imported;
    j["rows_skipped"] = rows_skipped;
    j["detected_format"] = brokerFormatToString(detected_format);
    j["warnings"] = warnings;
    j["errors"] = errors;

    nlohmann::json lots_json = nlohmann::json::array();
    for (const auto& lot : lots) {
        lots_json.push_back(lot.toJSON());
    }
    j["lots"] = lots_json;

    return j;
}

std::string ImportResult::toPlainText() const {
    std::ostringstream ss;

    ss << "Import Result" << "\n";
    ss << "=============" << "\n\n";

    ss << "Status: " << (success ? "Success" : "Failed") << "\n";
    ss << "Format: " << brokerFormatToString(detected_format) << "\n";
    ss << "Rows Processed: " << rows_processed << "\n";
    ss << "Rows Imported: " << rows_imported << "\n";
    ss << "Rows Skipped: " << rows_skipped << "\n\n";

    if (!errors.empty()) {
        ss << "Errors:" << "\n";
        for (const auto& error : errors) {
            ss << "  - " << error << "\n";
        }
        ss << "\n";
    }

    if (!warnings.empty()) {
        ss << "Warnings:" << "\n";
        for (const auto& warning : warnings) {
            ss << "  - " << warning << "\n";
        }
        ss << "\n";
    }

    if (!lots.empty()) {
        ss << "Imported Lots (" << lots.size() << "):" << "\n";
        for (const auto& lot : lots) {
            ss << "  " << lot.id << ": " << lot.ticker << " "
               << lot.shares << " shares @ $" << lot.cost_basis_per_share << "\n";
        }
    }

    return ss.str();
}

// =============================================================================
// GenericCSVImporter
// =============================================================================

class GenericCSVImporter::Impl {
public:
    Impl() = default;
    explicit Impl(const CSVColumnMapping& mapping) : mapping_(mapping) {}

    CSVColumnMapping mapping_;

    ImportResult importFromString(const std::string& content) {
        ImportResult result;
        result.detected_format = BrokerFormat::GENERIC_CSV;

        std::istringstream stream(content);
        std::string line;
        int line_num = 0;

        // Skip header if configured
        if (mapping_.has_header && std::getline(stream, line)) {
            ++line_num;
        }

        while (std::getline(stream, line)) {
            ++line_num;
            ++result.rows_processed;

            if (trim(line).empty()) {
                continue;
            }

            try {
                auto fields = parseCSVLine(line, mapping_.delimiter);

                // Check required fields exist
                int max_col = std::max({mapping_.ticker_column, mapping_.shares_column,
                                       mapping_.cost_basis_column, mapping_.purchase_date_column});
                if (static_cast<int>(fields.size()) <= max_col) {
                    result.warnings.push_back("Line " + std::to_string(line_num) +
                                             ": Not enough columns, skipping");
                    ++result.rows_skipped;
                    continue;
                }

                TaxLot lot;
                lot.ticker = trim(fields[mapping_.ticker_column]);
                lot.shares = parseShares(fields[mapping_.shares_column]);
                lot.cost_basis_per_share = parseDollarAmount(fields[mapping_.cost_basis_column]);
                lot.purchase_date = parseDate(fields[mapping_.purchase_date_column], mapping_.date_format);

                // Optional fields
                if (mapping_.lot_id_column >= 0 && mapping_.lot_id_column < static_cast<int>(fields.size())) {
                    lot.id = trim(fields[mapping_.lot_id_column]);
                }
                if (lot.id.empty()) {
                    lot.id = generateLotId(lot.ticker);
                }

                if (mapping_.sale_date_column >= 0 && mapping_.sale_date_column < static_cast<int>(fields.size())) {
                    std::string sale_date_str = trim(fields[mapping_.sale_date_column]);
                    if (!sale_date_str.empty()) {
                        lot.sale_date = parseDate(sale_date_str, mapping_.date_format);
                    }
                }

                if (mapping_.sale_price_column >= 0 && mapping_.sale_price_column < static_cast<int>(fields.size())) {
                    std::string sale_price_str = trim(fields[mapping_.sale_price_column]);
                    if (!sale_price_str.empty()) {
                        lot.sale_price = parseDollarAmount(sale_price_str);
                    }
                }

                // Validate
                if (lot.ticker.empty()) {
                    result.warnings.push_back("Line " + std::to_string(line_num) + ": Empty ticker, skipping");
                    ++result.rows_skipped;
                    continue;
                }
                if (lot.shares <= 0) {
                    result.warnings.push_back("Line " + std::to_string(line_num) + ": Invalid shares, skipping");
                    ++result.rows_skipped;
                    continue;
                }

                result.lots.push_back(lot);
                ++result.rows_imported;

            } catch (const std::exception& e) {
                result.warnings.push_back("Line " + std::to_string(line_num) + ": " + e.what());
                ++result.rows_skipped;
            }
        }

        result.success = !result.lots.empty();
        return result;
    }
};

GenericCSVImporter::GenericCSVImporter() : pImpl_(std::make_unique<Impl>()) {}
GenericCSVImporter::GenericCSVImporter(const CSVColumnMapping& mapping)
    : pImpl_(std::make_unique<Impl>(mapping)) {}
GenericCSVImporter::~GenericCSVImporter() = default;

bool GenericCSVImporter::canHandle(const std::string& file_path) const {
    return file_path.size() >= 4 &&
           file_path.substr(file_path.size() - 4) == ".csv";
}

ImportResult GenericCSVImporter::importFromFile(const std::string& file_path) {
    try {
        std::string content = readFile(file_path);
        return importFromString(content);
    } catch (const std::exception& e) {
        ImportResult result;
        result.errors.push_back(e.what());
        return result;
    }
}

ImportResult GenericCSVImporter::importFromString(const std::string& content) {
    return pImpl_->importFromString(content);
}

void GenericCSVImporter::setColumnMapping(const CSVColumnMapping& mapping) {
    pImpl_->mapping_ = mapping;
}

const CSVColumnMapping& GenericCSVImporter::getColumnMapping() const {
    return pImpl_->mapping_;
}

// =============================================================================
// SchwabImporter
// =============================================================================

class SchwabImporter::Impl {
public:
    ImportResult importFromString(const std::string& content) {
        ImportResult result;
        result.detected_format = BrokerFormat::SCHWAB;

        // Schwab format typically has:
        // Account Number, Symbol, Security Description, Quantity, Date Acquired,
        // Cost Basis, Market Value, Gain/Loss, etc.

        std::istringstream stream(content);
        std::string line;
        int line_num = 0;

        // Find header row
        int symbol_col = -1, quantity_col = -1, cost_basis_col = -1, date_col = -1;

        while (std::getline(stream, line)) {
            ++line_num;
            auto fields = parseCSVLine(line);

            // Look for header row
            for (size_t i = 0; i < fields.size(); ++i) {
                std::string lower = fields[i];
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower == "symbol" || lower == "ticker") {
                    symbol_col = static_cast<int>(i);
                } else if (lower == "quantity" || lower == "shares" || lower == "qty") {
                    quantity_col = static_cast<int>(i);
                } else if (containsIgnoreCase(fields[i], "cost basis") ||
                           containsIgnoreCase(fields[i], "cost per share")) {
                    cost_basis_col = static_cast<int>(i);
                } else if (containsIgnoreCase(fields[i], "date acquired") ||
                           containsIgnoreCase(fields[i], "acquisition date") ||
                           containsIgnoreCase(fields[i], "purchase date")) {
                    date_col = static_cast<int>(i);
                }
            }

            if (symbol_col >= 0 && quantity_col >= 0) {
                break;  // Found header
            }
        }

        if (symbol_col < 0 || quantity_col < 0) {
            result.errors.push_back("Could not find required columns (Symbol, Quantity)");
            return result;
        }

        // Default cost basis column if not found
        if (cost_basis_col < 0) {
            cost_basis_col = symbol_col + 2;  // Common position
        }
        if (date_col < 0) {
            date_col = symbol_col + 4;  // Common position
        }

        // Parse data rows
        while (std::getline(stream, line)) {
            ++line_num;
            ++result.rows_processed;

            if (trim(line).empty()) {
                continue;
            }

            try {
                auto fields = parseCSVLine(line);

                int max_col = std::max({symbol_col, quantity_col, cost_basis_col, date_col});
                if (static_cast<int>(fields.size()) <= max_col) {
                    ++result.rows_skipped;
                    continue;
                }

                std::string ticker = trim(fields[symbol_col]);
                if (ticker.empty() || ticker[0] == '-' || containsIgnoreCase(ticker, "total")) {
                    ++result.rows_skipped;
                    continue;  // Skip totals and empty rows
                }

                TaxLot lot;
                lot.ticker = ticker;
                lot.shares = parseShares(fields[quantity_col]);

                // Schwab may have total cost basis or per-share
                double cost = parseDollarAmount(fields[cost_basis_col]);
                if (cost > lot.shares * 10000) {
                    // Likely total cost basis, not per share
                    lot.cost_basis_per_share = cost / lot.shares;
                } else {
                    lot.cost_basis_per_share = cost;
                }

                lot.purchase_date = parseDate(fields[date_col]);
                lot.id = generateLotId(lot.ticker);

                if (lot.shares > 0 && lot.cost_basis_per_share > 0) {
                    result.lots.push_back(lot);
                    ++result.rows_imported;
                } else {
                    ++result.rows_skipped;
                }

            } catch (const std::exception& e) {
                result.warnings.push_back("Line " + std::to_string(line_num) + ": " + e.what());
                ++result.rows_skipped;
            }
        }

        result.success = !result.lots.empty();
        return result;
    }
};

SchwabImporter::SchwabImporter() : pImpl_(std::make_unique<Impl>()) {}
SchwabImporter::~SchwabImporter() = default;

bool SchwabImporter::canHandle(const std::string& file_path) const {
    try {
        std::string content = readFile(file_path);
        return containsIgnoreCase(content, "schwab") ||
               (containsIgnoreCase(content, "symbol") &&
                containsIgnoreCase(content, "date acquired"));
    } catch (...) {
        return false;
    }
}

ImportResult SchwabImporter::importFromFile(const std::string& file_path) {
    try {
        std::string content = readFile(file_path);
        return importFromString(content);
    } catch (const std::exception& e) {
        ImportResult result;
        result.errors.push_back(e.what());
        return result;
    }
}

ImportResult SchwabImporter::importFromString(const std::string& content) {
    return pImpl_->importFromString(content);
}

// =============================================================================
// FidelityImporter
// =============================================================================

class FidelityImporter::Impl {
public:
    ImportResult importFromString(const std::string& content) {
        ImportResult result;
        result.detected_format = BrokerFormat::FIDELITY;

        // Fidelity format typically has:
        // Account Name/Number, Symbol, Description, Quantity, Cost Basis Total,
        // Cost Basis Per Share, Type, Acquired, Term, etc.

        std::istringstream stream(content);
        std::string line;
        int line_num = 0;

        int symbol_col = -1, quantity_col = -1, cost_per_share_col = -1, acquired_col = -1;

        while (std::getline(stream, line)) {
            ++line_num;
            auto fields = parseCSVLine(line);

            for (size_t i = 0; i < fields.size(); ++i) {
                std::string lower = fields[i];
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower == "symbol") {
                    symbol_col = static_cast<int>(i);
                } else if (lower == "quantity") {
                    quantity_col = static_cast<int>(i);
                } else if (containsIgnoreCase(fields[i], "cost basis per share") ||
                           lower == "cost per share") {
                    cost_per_share_col = static_cast<int>(i);
                } else if (lower == "acquired" || lower == "acquisition date") {
                    acquired_col = static_cast<int>(i);
                }
            }

            if (symbol_col >= 0 && quantity_col >= 0) {
                break;
            }
        }

        if (symbol_col < 0 || quantity_col < 0) {
            result.errors.push_back("Could not find required columns");
            return result;
        }

        if (cost_per_share_col < 0) {
            cost_per_share_col = quantity_col + 2;
        }
        if (acquired_col < 0) {
            acquired_col = quantity_col + 4;
        }

        while (std::getline(stream, line)) {
            ++line_num;
            ++result.rows_processed;

            if (trim(line).empty()) {
                continue;
            }

            try {
                auto fields = parseCSVLine(line);

                int max_col = std::max({symbol_col, quantity_col, cost_per_share_col, acquired_col});
                if (static_cast<int>(fields.size()) <= max_col) {
                    ++result.rows_skipped;
                    continue;
                }

                std::string ticker = trim(fields[symbol_col]);
                if (ticker.empty() || containsIgnoreCase(ticker, "total") ||
                    containsIgnoreCase(ticker, "pending")) {
                    ++result.rows_skipped;
                    continue;
                }

                TaxLot lot;
                lot.ticker = ticker;
                lot.shares = parseShares(fields[quantity_col]);
                lot.cost_basis_per_share = parseDollarAmount(fields[cost_per_share_col]);
                lot.purchase_date = parseDate(fields[acquired_col]);
                lot.id = generateLotId(lot.ticker);

                if (lot.shares > 0) {
                    result.lots.push_back(lot);
                    ++result.rows_imported;
                } else {
                    ++result.rows_skipped;
                }

            } catch (const std::exception& e) {
                result.warnings.push_back("Line " + std::to_string(line_num) + ": " + e.what());
                ++result.rows_skipped;
            }
        }

        result.success = !result.lots.empty();
        return result;
    }
};

FidelityImporter::FidelityImporter() : pImpl_(std::make_unique<Impl>()) {}
FidelityImporter::~FidelityImporter() = default;

bool FidelityImporter::canHandle(const std::string& file_path) const {
    try {
        std::string content = readFile(file_path);
        return containsIgnoreCase(content, "fidelity") ||
               containsIgnoreCase(content, "cost basis per share");
    } catch (...) {
        return false;
    }
}

ImportResult FidelityImporter::importFromFile(const std::string& file_path) {
    try {
        std::string content = readFile(file_path);
        return importFromString(content);
    } catch (const std::exception& e) {
        ImportResult result;
        result.errors.push_back(e.what());
        return result;
    }
}

ImportResult FidelityImporter::importFromString(const std::string& content) {
    return pImpl_->importFromString(content);
}

// =============================================================================
// VanguardImporter
// =============================================================================

class VanguardImporter::Impl {
public:
    ImportResult importFromString(const std::string& content) {
        ImportResult result;
        result.detected_format = BrokerFormat::VANGUARD;

        // Vanguard format typically has:
        // Account Number, Investment Name, Symbol, Shares, Share Price,
        // Total Value, Total Cost, etc.

        std::istringstream stream(content);
        std::string line;
        int line_num = 0;

        int symbol_col = -1, shares_col = -1, cost_col = -1, date_col = -1;

        while (std::getline(stream, line)) {
            ++line_num;
            auto fields = parseCSVLine(line);

            for (size_t i = 0; i < fields.size(); ++i) {
                std::string lower = fields[i];
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

                if (lower == "symbol" || lower == "ticker") {
                    symbol_col = static_cast<int>(i);
                } else if (lower == "shares" || lower == "quantity") {
                    shares_col = static_cast<int>(i);
                } else if (containsIgnoreCase(fields[i], "cost basis") ||
                           containsIgnoreCase(fields[i], "total cost") ||
                           containsIgnoreCase(fields[i], "average cost")) {
                    cost_col = static_cast<int>(i);
                } else if (containsIgnoreCase(fields[i], "acquisition") ||
                           containsIgnoreCase(fields[i], "purchase date")) {
                    date_col = static_cast<int>(i);
                }
            }

            if (symbol_col >= 0 && shares_col >= 0) {
                break;
            }
        }

        if (symbol_col < 0 || shares_col < 0) {
            result.errors.push_back("Could not find required columns");
            return result;
        }

        if (cost_col < 0) {
            cost_col = shares_col + 2;
        }

        while (std::getline(stream, line)) {
            ++line_num;
            ++result.rows_processed;

            if (trim(line).empty()) {
                continue;
            }

            try {
                auto fields = parseCSVLine(line);

                int max_col = std::max({symbol_col, shares_col, cost_col});
                if (static_cast<int>(fields.size()) <= max_col) {
                    ++result.rows_skipped;
                    continue;
                }

                std::string ticker = trim(fields[symbol_col]);
                if (ticker.empty() || containsIgnoreCase(ticker, "total") ||
                    ticker == "--") {
                    ++result.rows_skipped;
                    continue;
                }

                TaxLot lot;
                lot.ticker = ticker;
                lot.shares = parseShares(fields[shares_col]);

                double cost = parseDollarAmount(fields[cost_col]);
                // Vanguard often provides total cost basis
                if (lot.shares > 0 && cost > lot.shares * 5000) {
                    lot.cost_basis_per_share = cost / lot.shares;
                } else {
                    lot.cost_basis_per_share = cost;
                }

                if (date_col >= 0 && date_col < static_cast<int>(fields.size())) {
                    lot.purchase_date = parseDate(fields[date_col]);
                } else {
                    lot.purchase_date = std::chrono::system_clock::now();
                    result.warnings.push_back("Line " + std::to_string(line_num) +
                                             ": No acquisition date, using today");
                }

                lot.id = generateLotId(lot.ticker);

                if (lot.shares > 0) {
                    result.lots.push_back(lot);
                    ++result.rows_imported;
                } else {
                    ++result.rows_skipped;
                }

            } catch (const std::exception& e) {
                result.warnings.push_back("Line " + std::to_string(line_num) + ": " + e.what());
                ++result.rows_skipped;
            }
        }

        result.success = !result.lots.empty();
        return result;
    }
};

VanguardImporter::VanguardImporter() : pImpl_(std::make_unique<Impl>()) {}
VanguardImporter::~VanguardImporter() = default;

bool VanguardImporter::canHandle(const std::string& file_path) const {
    try {
        std::string content = readFile(file_path);
        return containsIgnoreCase(content, "vanguard") ||
               containsIgnoreCase(content, "investment name");
    } catch (...) {
        return false;
    }
}

ImportResult VanguardImporter::importFromFile(const std::string& file_path) {
    try {
        std::string content = readFile(file_path);
        return importFromString(content);
    } catch (const std::exception& e) {
        ImportResult result;
        result.errors.push_back(e.what());
        return result;
    }
}

ImportResult VanguardImporter::importFromString(const std::string& content) {
    return pImpl_->importFromString(content);
}

// =============================================================================
// BrokerImportFactory
// =============================================================================

std::unique_ptr<BrokerImporter> BrokerImportFactory::create(BrokerFormat format) {
    switch (format) {
        case BrokerFormat::SCHWAB:
            return std::make_unique<SchwabImporter>();
        case BrokerFormat::FIDELITY:
            return std::make_unique<FidelityImporter>();
        case BrokerFormat::VANGUARD:
            return std::make_unique<VanguardImporter>();
        case BrokerFormat::GENERIC_CSV:
        case BrokerFormat::AUTO_DETECT:
        default:
            return std::make_unique<GenericCSVImporter>();
    }
}

std::unique_ptr<BrokerImporter> BrokerImportFactory::createForFile(const std::string& file_path) {
    // Try each broker-specific importer
    auto schwab = std::make_unique<SchwabImporter>();
    if (schwab->canHandle(file_path)) {
        return schwab;
    }

    auto fidelity = std::make_unique<FidelityImporter>();
    if (fidelity->canHandle(file_path)) {
        return fidelity;
    }

    auto vanguard = std::make_unique<VanguardImporter>();
    if (vanguard->canHandle(file_path)) {
        return vanguard;
    }

    // Fall back to generic CSV
    return std::make_unique<GenericCSVImporter>();
}

std::vector<BrokerFormat> BrokerImportFactory::getSupportedFormats() {
    return {
        BrokerFormat::GENERIC_CSV,
        BrokerFormat::SCHWAB,
        BrokerFormat::FIDELITY,
        BrokerFormat::VANGUARD
    };
}

ImportResult BrokerImportFactory::importFile(const std::string& file_path, BrokerFormat format) {
    std::unique_ptr<BrokerImporter> importer;

    if (format == BrokerFormat::AUTO_DETECT) {
        importer = createForFile(file_path);
    } else {
        importer = create(format);
    }

    return importer->importFromFile(file_path);
}

ImportResult BrokerImportFactory::importToManager(const std::string& file_path,
                                                   TaxLotManager& manager,
                                                   BrokerFormat format) {
    auto result = importFile(file_path, format);

    if (result.success) {
        for (const auto& lot : result.lots) {
            manager.addLot(lot);
        }
    }

    return result;
}

// =============================================================================
// TaxLotExporter
// =============================================================================

std::string TaxLotExporter::toCSV(const std::vector<TaxLot>& lots) {
    std::ostringstream ss;

    // Header
    ss << "id,ticker,shares,cost_basis_per_share,purchase_date,sale_date,sale_price\n";

    // Data rows
    for (const auto& lot : lots) {
        ss << "\"" << lot.id << "\","
           << "\"" << lot.ticker << "\","
           << lot.shares << ","
           << lot.cost_basis_per_share << ","
           << "\"" << formatDate(lot.purchase_date) << "\",";

        if (lot.sale_date.has_value()) {
            ss << "\"" << formatDate(lot.sale_date.value()) << "\",";
        } else {
            ss << ",";
        }

        if (lot.sale_price.has_value()) {
            ss << lot.sale_price.value();
        }

        ss << "\n";
    }

    return ss.str();
}

bool TaxLotExporter::toFile(const std::vector<TaxLot>& lots, const std::string& file_path) {
    return writeFile(file_path, toCSV(lots));
}

bool TaxLotExporter::exportManager(const TaxLotManager& manager, const std::string& file_path) {
    return toFile(manager.getAllLots(), file_path);
}

}  // namespace lumen::data
