/// @file main.cpp
/// @brief Lumen CLI application entry point
///
/// Main entry point for the Lumen command-line interface providing
/// portfolio optimization, import, explain, and configuration commands.

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/data/market_data.hpp"
#include "lumen/data/persistence.hpp"
#include "lumen/data/tax_lot.hpp"
#include "lumen/data/broker_import.hpp"
#include "lumen/explain/explainer.hpp"
#include "lumen/explain/provenance.hpp"
#include "lumen/solvers/highs_wrapper.hpp"
#include "lumen/utils/logging.hpp"
#include "lumen/utils/config.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace lumen;
using json = nlohmann::json;

// =============================================================================
// Output Format Enum
// =============================================================================

enum class OutputFormat {
    JSON,
    TEXT,
    MARKDOWN,
    HTML
};

OutputFormat parseOutputFormat(const std::string& format_str) {
    if (format_str == "json") return OutputFormat::JSON;
    if (format_str == "text") return OutputFormat::TEXT;
    if (format_str == "markdown" || format_str == "md") return OutputFormat::MARKDOWN;
    if (format_str == "html") return OutputFormat::HTML;
    return OutputFormat::TEXT;
}

// =============================================================================
// Helper Functions
// =============================================================================

/// @brief Print version information
void printVersion() {
    std::cout << "Lumen - Quantum-Enhanced Portfolio Optimizer\n";
    std::cout << "Version: 1.0.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
    std::cout << "Part of the Lumen Project by OA Quantum Labs\n";
}

/// @brief Initialize the application
bool initialize(const std::string& config_path = "") {
    auto& config = utils::Configuration::getInstance();

    // Load environment variables first
    config.loadFromEnvironment();

    // Ensure home directory exists
    if (!config.ensureDirectories()) {
        std::cerr << "Failed to create configuration directories\n";
        return false;
    }

    // Load config file if specified or exists
    if (!config_path.empty()) {
        if (!config.loadFromFile(config_path)) {
            std::cerr << "Failed to load config from: " << config_path << "\n";
            return false;
        }
    } else if (fs::exists(config.getConfigPath())) {
        config.loadFromFile(config.getConfigPath());
    }

    // Configure logging
    auto& logger = utils::Logger::getInstance();
    const auto& obs_config = config.getObservabilityConfig();

    if (obs_config.log_level == "TRACE") {
        logger.setLevel(utils::LogLevel::TRACE);
    } else if (obs_config.log_level == "DEBUG") {
        logger.setLevel(utils::LogLevel::DEBUG);
    } else if (obs_config.log_level == "WARN") {
        logger.setLevel(utils::LogLevel::WARN);
    } else if (obs_config.log_level == "ERROR") {
        logger.setLevel(utils::LogLevel::ERROR);
    } else {
        logger.setLevel(utils::LogLevel::INFO);
    }

    if (obs_config.enable_file_logging && !obs_config.log_file_path.empty()) {
        logger.enableFile(obs_config.log_file_path);
    }

    logger.setJsonFormat(obs_config.json_logs);

    // Initialize persistence
    if (!data::initializePersistence(config.getDatabasePath())) {
        utils::Logger::warn("Failed to initialize persistence layer");
    }

    return true;
}

/// @brief Generate a cryptographically secure unique session ID
/// @details Uses random_device for cryptographic randomness instead of timestamp
std::string generateSessionId() {
    // SECURITY FIX: Use cryptographically secure random generation
    // instead of predictable timestamp-based IDs
    std::random_device rd;
    std::array<uint8_t, 16> bytes;

    for (auto& byte : bytes) {
        byte = static_cast<uint8_t>(rd());
    }

    static const char hex_chars[] = "0123456789abcdef";
    std::stringstream ss;
    ss << "OPT_";

    for (uint8_t byte : bytes) {
        ss << hex_chars[byte >> 4] << hex_chars[byte & 0x0F];
    }

    return ss.str();
}

/// @brief Load portfolio from JSON file
std::optional<core::Portfolio> loadPortfolio(const std::string& filepath) {
    if (!fs::exists(filepath)) {
        std::cerr << "Portfolio file not found: " << filepath << "\n";
        return std::nullopt;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open portfolio file: " << filepath << "\n";
        return std::nullopt;
    }

    try {
        json j;
        file >> j;
        return core::Portfolio::fromJSON(j);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse portfolio JSON: " << e.what() << "\n";
        return std::nullopt;
    }
}

/// @brief Load target allocation from JSON file
std::optional<core::TargetAllocation> loadTargetAllocation(const std::string& filepath) {
    if (!fs::exists(filepath)) {
        std::cerr << "Target file not found: " << filepath << "\n";
        return std::nullopt;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open target file: " << filepath << "\n";
        return std::nullopt;
    }

    try {
        json j;
        file >> j;
        return core::TargetAllocation::fromJSON(j);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse target JSON: " << e.what() << "\n";
        return std::nullopt;
    }
}

/// @brief Load constraints from JSON file
core::ConstraintSet loadConstraints(const std::string& filepath) {
    core::ConstraintSet constraints;

    if (filepath.empty() || !fs::exists(filepath)) {
        // Return empty constraint set with defaults
        return constraints;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        return constraints;
    }

    try {
        json j;
        file >> j;
        constraints = core::ConstraintSet::fromJSON(j);
    } catch (const std::exception& e) {
        utils::Logger::warn("Failed to parse constraints: " + std::string(e.what()));
    }

    return constraints;
}

/// @brief Write output to file or stdout
void writeOutput(const std::string& content, const std::string& filepath) {
    if (filepath.empty() || filepath == "-") {
        std::cout << content << std::endl;
    } else {
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << content;
            file.close();
            utils::Logger::info("Output written to: " + filepath);
        } else {
            std::cerr << "Failed to write to: " << filepath << "\n";
            std::cout << content << std::endl;
        }
    }
}

// =============================================================================
// Command Implementations
// =============================================================================

/// @brief Run optimization command
int runOptimize(const std::string& portfolio_file, const std::string& target_file,
                const std::string& constraints_file, const std::string& output_file,
                const std::string& format_str, bool fetch_prices, bool verbose) {
    utils::Logger::info("Starting optimization...");

    // Load portfolio
    auto portfolio_opt = loadPortfolio(portfolio_file);
    if (!portfolio_opt) {
        return 1;
    }
    core::Portfolio portfolio = *portfolio_opt;

    // Load target allocation
    auto target_opt = loadTargetAllocation(target_file);
    if (!target_opt) {
        return 1;
    }
    core::TargetAllocation target = *target_opt;

    // Load constraints (optional)
    core::ConstraintSet constraints = loadConstraints(constraints_file);

    // Fetch current prices if requested
    if (fetch_prices) {
        utils::Logger::info("Fetching current market prices...");
        auto& market_client = data::getMarketDataClient();

        std::map<std::string, double> prices;
        for (const auto& ticker : portfolio.getAllTickers()) {
            auto quote_opt = market_client.getQuote(ticker);
            if (quote_opt) {
                prices[ticker] = quote_opt->price;
                utils::Logger::debug(ticker + ": $" + std::to_string(quote_opt->price));
            } else {
                utils::Logger::warn("Failed to fetch price for: " + ticker);
            }
        }

        if (!prices.empty()) {
            portfolio.updateAllPrices(prices);
        }
    }

    // Generate session ID
    std::string session_id = generateSessionId();
    utils::Logger::info("Session ID: " + session_id);

    // Create solver dispatcher
    auto& config = utils::Configuration::getInstance();
    core::SolverDispatcher dispatcher(config);

    // Run optimization
    auto start_time = std::chrono::steady_clock::now();
    core::SolverResult result = dispatcher.dispatch(portfolio, target, constraints);
    auto end_time = std::chrono::steady_clock::now();

    auto solve_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // Build provenance
    explain::Provenance provenance;
    provenance.setSessionId(session_id);

    explain::SolverRecord solver_record;
    solver_record.solver_name = result.solver_used;
    solver_record.solve_time_ms = solve_time;
    solver_record.iterations = result.iterations;
    solver_record.objective_value = result.objective_value;
    solver_record.termination_status = result.status;
    solver_record.optimality_gap = result.optimality_gap;
    provenance.recordSolver(solver_record);

    // Record constraints
    for (const auto* c : constraints.getAllConstraints()) {
        explain::ConstraintRecord cr;
        cr.constraint_name = c->getName();
        cr.type = c->getType();
        cr.status = c->getStatus();
        cr.slack_value = c->getSlackValue();
        cr.description = c->getDescription();
        provenance.recordConstraint(cr);
    }

    // Generate explanation
    explain::Explainer explainer;
    explainer.setVerbosity(verbose ? 2 : 1);
    auto explanation = explainer.generateFullExplanation(
        portfolio, target, constraints, result, provenance);

    // Save to persistence
    auto& persistence = data::getPersistenceManager();
    if (persistence.isInitialized()) {
        persistence.saveOptimizationResult(
            portfolio.getName(),
            session_id,
            result.solver_used,
            result.status,
            result.objective_value,
            static_cast<double>(solve_time),
            result.toJSON(),
            provenance.toJSON()
        );
    }

    // Output result
    OutputFormat format = parseOutputFormat(format_str);
    std::string output;

    switch (format) {
        case OutputFormat::JSON:
            output = explanation.toJSON().dump(2);
            break;
        case OutputFormat::MARKDOWN:
            output = explanation.toMarkdown();
            break;
        case OutputFormat::HTML:
            output = explanation.toHTML();
            break;
        case OutputFormat::TEXT:
        default:
            output = explanation.toPlainText();
            break;
    }

    writeOutput(output, output_file);

    // Print summary to stderr if output goes to file
    if (!output_file.empty() && output_file != "-") {
        std::cerr << "\nOptimization " << (result.success ? "succeeded" : "failed") << "\n";
        std::cerr << "Status: " << result.status << "\n";
        std::cerr << "Trades: " << result.trades.size() << "\n";
        std::cerr << "Solve time: " << solve_time << " ms\n";
        std::cerr << "Session ID: " << session_id << "\n";
    }

    return result.success ? 0 : 1;
}

/// @brief Run import command for portfolio data
int runImport(const std::string& input_file, const std::string& portfolio_name,
              const std::string& format_str, bool update_prices) {
    utils::Logger::info("Importing portfolio from: " + input_file);

    if (!fs::exists(input_file)) {
        std::cerr << "Input file not found: " << input_file << "\n";
        return 1;
    }

    core::Portfolio portfolio;

    // Determine format from extension if not specified
    std::string format = format_str;
    if (format.empty()) {
        std::string ext = fs::path(input_file).extension().string();
        if (ext == ".csv") format = "csv";
        else if (ext == ".json") format = "json";
        else format = "csv";  // Default
    }

    try {
        if (format == "csv") {
            portfolio = core::Portfolio::fromCSV(input_file);
        } else if (format == "json") {
            std::ifstream file(input_file);
            json j;
            file >> j;
            portfolio = core::Portfolio::fromJSON(j);
        } else {
            std::cerr << "Unknown format: " << format << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to import portfolio: " << e.what() << "\n";
        return 1;
    }

    // Set name if provided
    if (!portfolio_name.empty()) {
        portfolio.setName(portfolio_name);
    }

    // Update prices if requested
    if (update_prices) {
        utils::Logger::info("Fetching current market prices...");
        auto& market_client = data::getMarketDataClient();

        std::map<std::string, double> prices;
        for (const auto& ticker : portfolio.getAllTickers()) {
            auto quote_opt = market_client.getQuote(ticker);
            if (quote_opt) {
                prices[ticker] = quote_opt->price;
            }
        }

        if (!prices.empty()) {
            portfolio.updateAllPrices(prices);
        }
    }

    // Save to persistence
    auto& persistence = data::getPersistenceManager();
    std::string portfolio_id = data::generateUniqueId();

    if (persistence.isInitialized()) {
        bool saved = persistence.savePortfolio(
            portfolio_id,
            portfolio.getName(),
            portfolio.toJSON()
        );

        if (saved) {
            std::cout << "Portfolio imported successfully\n";
            std::cout << "  ID: " << portfolio_id << "\n";
            std::cout << "  Name: " << portfolio.getName() << "\n";
            std::cout << "  Positions: " << portfolio.getPositionCount() << "\n";
            std::cout << "  Total Value: $" << std::fixed << std::setprecision(2)
                      << portfolio.getTotalValue() << "\n";
        } else {
            std::cerr << "Failed to save portfolio to database\n";
            return 1;
        }
    } else {
        // Just output the portfolio as JSON
        std::cout << portfolio.toJSON().dump(2) << std::endl;
    }

    return 0;
}

/// @brief Run explain command for past optimization
int runExplain(const std::string& session_id, const std::string& output_file,
               const std::string& format_str) {
    auto& persistence = data::getPersistenceManager();

    if (!persistence.isInitialized()) {
        std::cerr << "Persistence layer not initialized\n";
        return 1;
    }

    // Fetch optimization record
    auto record_opt = persistence.getOptimizationBySession(session_id);
    if (!record_opt) {
        std::cerr << "No optimization found with session ID: " << session_id << "\n";
        return 1;
    }

    const auto& record = *record_opt;

    OutputFormat format = parseOutputFormat(format_str);
    std::string output;

    if (format == OutputFormat::JSON) {
        json j{
            {"session_id", record.session_id},
            {"portfolio_id", record.portfolio_id},
            {"solver", record.solver_used},
            {"status", record.status},
            {"objective_value", record.objective_value},
            {"solve_time_ms", record.solve_time_ms},
            {"trades", record.trades},
            {"provenance", record.provenance}
        };
        output = j.dump(2);
    } else {
        std::stringstream ss;
        ss << "# Optimization Report\n\n";
        ss << "**Session ID:** " << record.session_id << "\n";
        ss << "**Portfolio:** " << record.portfolio_id << "\n";
        ss << "**Solver:** " << record.solver_used << "\n";
        ss << "**Status:** " << record.status << "\n";
        ss << "**Objective Value:** " << std::fixed << std::setprecision(4)
           << record.objective_value << "\n";
        ss << "**Solve Time:** " << record.solve_time_ms << " ms\n\n";

        if (record.trades.is_array()) {
            ss << "## Trades\n\n";
            for (const auto& trade : record.trades) {
                std::string action = trade.value("action", "unknown");
                std::string ticker = trade.value("ticker", "???");
                double shares = trade.value("shares", 0.0);
                double amount = trade.value("amount", 0.0);
                ss << "- " << action << " " << std::fixed << std::setprecision(2)
                   << shares << " shares of " << ticker
                   << " ($" << amount << ")\n";
            }
        }

        output = ss.str();
    }

    writeOutput(output, output_file);
    return 0;
}

/// @brief Run list command to show portfolios or history
int runList(const std::string& what) {
    auto& persistence = data::getPersistenceManager();

    if (!persistence.isInitialized()) {
        std::cerr << "Persistence layer not initialized\n";
        return 1;
    }

    if (what == "portfolios") {
        auto portfolios = persistence.listPortfolios();
        if (portfolios.empty()) {
            std::cout << "No portfolios found.\n";
            return 0;
        }

        std::cout << "Portfolios:\n";
        std::cout << std::string(60, '-') << "\n";
        std::cout << std::left << std::setw(36) << "ID"
                  << std::setw(24) << "Name" << "\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& [id, name] : portfolios) {
            std::cout << std::left << std::setw(36) << id
                      << std::setw(24) << name << "\n";
        }
    } else if (what == "history") {
        // Get recent optimization history (all portfolios)
        auto history = persistence.getOptimizationHistory("", 20);
        if (history.empty()) {
            std::cout << "No optimization history found.\n";
            return 0;
        }

        std::cout << "Recent Optimizations:\n";
        std::cout << std::string(80, '-') << "\n";
        std::cout << std::left << std::setw(20) << "Session ID"
                  << std::setw(15) << "Status"
                  << std::setw(12) << "Solver"
                  << std::setw(12) << "Objective"
                  << std::setw(10) << "Time (ms)" << "\n";
        std::cout << std::string(80, '-') << "\n";

        for (const auto& record : history) {
            std::cout << std::left << std::setw(20) << record.session_id
                      << std::setw(15) << record.status
                      << std::setw(12) << record.solver_used
                      << std::setw(12) << std::fixed << std::setprecision(2)
                      << record.objective_value
                      << std::setw(10) << record.solve_time_ms << "\n";
        }
    } else {
        std::cerr << "Unknown list type: " << what << "\n";
        std::cerr << "Available: portfolios, history\n";
        return 1;
    }

    return 0;
}

/// @brief Run config command
int runConfig(const std::string& action, const std::string& key,
              const std::string& value, const std::string& format_str) {
    auto& config = utils::Configuration::getInstance();

    if (action == "show") {
        OutputFormat format = parseOutputFormat(format_str);

        json config_json{
            {"home_directory", config.getHomeDirectory()},
            {"database_path", config.getDatabasePath()},
            {"config_path", config.getConfigPath()},
            {"solver", {
                {"classical_timeout_ms", config.getSolverConfig().classical_timeout_ms},
                {"quantum_timeout_ms", config.getSolverConfig().quantum_timeout_ms},
                {"enable_quantum", config.getSolverConfig().enable_quantum},
                {"fallback_to_classical", config.getSolverConfig().fallback_to_classical}
            }},
            {"market_data", {
                {"enable_alpha_vantage", config.getMarketDataConfig().enable_alpha_vantage},
                {"enable_yahoo_finance", config.getMarketDataConfig().enable_yahoo_finance},
                {"cache_ttl_seconds", config.getMarketDataConfig().cache_ttl_seconds}
            }},
            {"observability", {
                {"log_level", config.getObservabilityConfig().log_level},
                {"enable_file_logging", config.getObservabilityConfig().enable_file_logging},
                {"json_logs", config.getObservabilityConfig().json_logs}
            }}
        };

        if (format == OutputFormat::JSON) {
            std::cout << config_json.dump(2) << std::endl;
        } else {
            std::cout << "=== Lumen Configuration ===\n\n";
            std::cout << "Paths:\n";
            std::cout << "  Home Directory: " << config.getHomeDirectory() << "\n";
            std::cout << "  Database Path: " << config.getDatabasePath() << "\n";
            std::cout << "  Config Path: " << config.getConfigPath() << "\n\n";

            std::cout << "Solver:\n";
            std::cout << "  Classical Timeout: " << config.getSolverConfig().classical_timeout_ms << " ms\n";
            std::cout << "  Quantum Timeout: " << config.getSolverConfig().quantum_timeout_ms << " ms\n";
            std::cout << "  Quantum Enabled: " << (config.getSolverConfig().enable_quantum ? "Yes" : "No") << "\n";
            std::cout << "  Fallback: " << (config.getSolverConfig().fallback_to_classical ? "Yes" : "No") << "\n\n";

            std::cout << "Market Data:\n";
            std::cout << "  Alpha Vantage: " << (config.getMarketDataConfig().enable_alpha_vantage ? "Enabled" : "Disabled") << "\n";
            std::cout << "  Yahoo Finance: " << (config.getMarketDataConfig().enable_yahoo_finance ? "Enabled" : "Disabled") << "\n";
            std::cout << "  Cache TTL: " << config.getMarketDataConfig().cache_ttl_seconds << " seconds\n\n";

            std::cout << "Logging:\n";
            std::cout << "  Level: " << config.getObservabilityConfig().log_level << "\n";
            std::cout << "  File Logging: " << (config.getObservabilityConfig().enable_file_logging ? "Enabled" : "Disabled") << "\n";
            std::cout << "  JSON Format: " << (config.getObservabilityConfig().json_logs ? "Yes" : "No") << "\n";
        }
    } else if (action == "set") {
        if (key.empty() || value.empty()) {
            std::cerr << "Usage: lumen config set <key> <value>\n";
            return 1;
        }

        // Parse and set configuration value
        // Note: This would need to be persisted to the config file
        std::cout << "Setting " << key << " = " << value << "\n";
        std::cout << "Note: Run 'lumen config save' to persist changes\n";

        // TODO: Implement config modification and saving
        utils::Logger::warn("Config modification not yet fully implemented");
    } else if (action == "path") {
        std::cout << config.getConfigPath() << std::endl;
    } else {
        std::cerr << "Unknown config action: " << action << "\n";
        std::cerr << "Available: show, set, path\n";
        return 1;
    }

    return 0;
}

/// @brief Run status command
int runStatus() {
    auto& config = utils::Configuration::getInstance();

    std::cout << "=== Lumen Status ===\n\n";

    std::cout << "Configuration:\n";
    std::cout << "  Home Directory: " << config.getHomeDirectory() << "\n";
    std::cout << "  Database Path: " << config.getDatabasePath() << "\n";
    std::cout << "  Config Path: " << config.getConfigPath() << "\n\n";

    std::cout << "Solvers:\n";
#ifdef LUMEN_HAS_HIGHS
    solvers::HighsOptimizer highs;
    std::cout << "  HiGHS: " << (highs.isAvailable() ? "Available" : "Not Available") << "\n";
#else
    std::cout << "  HiGHS: Not Compiled\n";
#endif

    const auto& solver_config = config.getSolverConfig();
    std::cout << "  Quantum Enabled: " << (solver_config.enable_quantum ? "Yes" : "No") << "\n";

    if (solver_config.enable_quantum) {
        std::cout << "  D-Wave: " << (config.getApiKey("dwave").empty() ? "Not Configured" : "Configured") << "\n";
        std::cout << "  IBM Quantum: " << (config.getApiKey("ibm_quantum").empty() ? "Not Configured" : "Configured") << "\n";
    }

    std::cout << "\nMarket Data:\n";
    const auto& md_config = config.getMarketDataConfig();
    std::cout << "  Alpha Vantage: " << (md_config.enable_alpha_vantage ? "Enabled" : "Disabled");
    if (md_config.enable_alpha_vantage) {
        std::cout << " (" << (config.getApiKey("alpha_vantage").empty() ? "No API Key" : "Configured") << ")";
    }
    std::cout << "\n";
    std::cout << "  Yahoo Finance: " << (md_config.enable_yahoo_finance ? "Enabled" : "Disabled") << "\n";

    std::cout << "\nPersistence:\n";
    auto& persistence = data::getPersistenceManager();
    std::cout << "  Database: " << (persistence.isInitialized() ? "Connected" : "Not Connected") << "\n";
    if (persistence.isInitialized()) {
        std::cout << "  Size: " << (persistence.getDatabaseSize() / 1024) << " KB\n";
    }

    return 0;
}

/// @brief Run quote command to fetch current prices
int runQuote(const std::vector<std::string>& tickers, const std::string& format_str) {
    if (tickers.empty()) {
        std::cerr << "No tickers specified\n";
        return 1;
    }

    auto& market_client = data::getMarketDataClient();
    OutputFormat format = parseOutputFormat(format_str);

    json quotes_json = json::array();

    for (const auto& ticker : tickers) {
        auto quote_opt = market_client.getQuote(ticker);
        if (quote_opt) {
            const auto& q = *quote_opt;
            quotes_json.push_back({
                {"ticker", q.ticker},
                {"price", q.price},
                {"change", q.change},
                {"change_percent", q.change_percent},
                {"volume", q.volume},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    q.timestamp.time_since_epoch()).count()}
            });
        } else {
            quotes_json.push_back({
                {"ticker", ticker},
                {"error", "Failed to fetch quote"}
            });
        }
    }

    if (format == OutputFormat::JSON) {
        std::cout << quotes_json.dump(2) << std::endl;
    } else {
        std::cout << std::left << std::setw(10) << "Ticker"
                  << std::right << std::setw(12) << "Price"
                  << std::setw(10) << "Change"
                  << std::setw(10) << "Change %" << "\n";
        std::cout << std::string(42, '-') << "\n";

        for (const auto& q : quotes_json) {
            if (q.contains("error")) {
                std::cout << std::left << std::setw(10) << q["ticker"].get<std::string>()
                          << "  " << q["error"].get<std::string>() << "\n";
            } else {
                std::cout << std::left << std::setw(10) << q["ticker"].get<std::string>()
                          << std::right << std::setw(11) << std::fixed << std::setprecision(2)
                          << "$" << q["price"].get<double>()
                          << std::setw(10) << q["change"].get<double>()
                          << std::setw(9) << q["change_percent"].get<double>() << "%\n";
            }
        }
    }

    return 0;
}

// =============================================================================
// Tax Command Implementations
// =============================================================================

/// @brief Run tax-harvest command to find tax-loss harvesting opportunities
int runTaxHarvest(const std::string& portfolio_file, const std::string& lots_db,
                  double min_loss, const std::string& output_file,
                  const std::string& format_str, bool fetch_prices) {
    utils::Logger::info("Finding tax-loss harvesting opportunities...");

    // Load portfolio
    auto portfolio_opt = loadPortfolio(portfolio_file);
    if (!portfolio_opt) {
        return 1;
    }
    core::Portfolio portfolio = *portfolio_opt;

    // Load tax lots
    data::TaxLotManager lot_manager;
    if (!lots_db.empty() && fs::exists(lots_db)) {
        try {
            lot_manager.loadFromDatabase(lots_db);
            utils::Logger::info("Loaded tax lots from: " + lots_db);
        } catch (const std::exception& e) {
            utils::Logger::warn("Failed to load tax lots: " + std::string(e.what()));
        }
    }

    // Get current prices
    std::map<std::string, double> prices;
    if (fetch_prices) {
        utils::Logger::info("Fetching current market prices...");
        auto& market_client = data::getMarketDataClient();

        for (const auto& ticker : portfolio.getAllTickers()) {
            auto quote_opt = market_client.getQuote(ticker);
            if (quote_opt) {
                prices[ticker] = quote_opt->price;
            }
        }
    } else {
        // Use prices from portfolio
        for (const auto& pos : portfolio.getAllPositions()) {
            prices[pos.ticker] = pos.current_price;
        }
    }

    // Create tax optimizer and find opportunities
    data::TaxOptimizer optimizer(lot_manager);
    auto opportunities = optimizer.findHarvestingOpportunities(prices, min_loss);

    // Format output
    OutputFormat format = parseOutputFormat(format_str);
    std::string output;

    if (format == OutputFormat::JSON) {
        json j = json::array();
        for (const auto& opp : opportunities) {
            j.push_back(opp.toJSON());
        }
        output = j.dump(2);
    } else {
        std::stringstream ss;
        ss << "=== Tax-Loss Harvesting Opportunities ===\n\n";

        if (opportunities.empty()) {
            ss << "No harvesting opportunities found above the $"
               << std::fixed << std::setprecision(2) << min_loss << " threshold.\n";
        } else {
            ss << "Found " << opportunities.size() << " opportunities:\n\n";
            ss << std::left << std::setw(10) << "Ticker"
               << std::right << std::setw(12) << "Shares"
               << std::setw(12) << "Cost Basis"
               << std::setw(12) << "Curr Price"
               << std::setw(14) << "Loss Amount"
               << std::setw(12) << "Type" << "\n";
            ss << std::string(72, '-') << "\n";

            for (const auto& opp : opportunities) {
                std::string gain_type = (opp.gain_type == data::GainType::SHORT_TERM)
                    ? "Short-term" : "Long-term";

                ss << std::left << std::setw(10) << opp.lot.ticker
                   << std::right << std::setw(12) << std::fixed << std::setprecision(2)
                   << opp.lot.shares
                   << std::setw(11) << "$" << opp.lot.cost_basis_per_share
                   << std::setw(11) << "$" << opp.current_price
                   << std::setw(13) << "$" << std::abs(opp.unrealized_loss)
                   << std::setw(12) << gain_type << "\n";

                if (!opp.replacement_candidates.empty()) {
                    ss << "    Replacement candidates: ";
                    for (size_t i = 0; i < opp.replacement_candidates.size() && i < 3; ++i) {
                        if (i > 0) ss << ", ";
                        ss << opp.replacement_candidates[i];
                    }
                    ss << "\n";
                }

                if (opp.would_trigger_wash_sale) {
                    ss << "    WARNING: Would trigger wash sale with recent purchases\n";
                }
            }

            // Summary
            double total_harvestable = 0.0;
            for (const auto& opp : opportunities) {
                total_harvestable += std::abs(opp.unrealized_loss);
            }
            ss << "\nTotal harvestable losses: $" << std::fixed << std::setprecision(2)
               << total_harvestable << "\n";
        }

        output = ss.str();
    }

    writeOutput(output, output_file);
    return 0;
}

/// @brief Run tax-report command to generate capital gains report
int runTaxReport(const std::string& lots_db, const std::string& year_str,
                 const std::string& output_file, const std::string& format_str) {
    utils::Logger::info("Generating tax report...");

    // Load tax lots
    data::TaxLotManager lot_manager;
    if (!lots_db.empty() && fs::exists(lots_db)) {
        try {
            lot_manager.loadFromDatabase(lots_db);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load tax lots: " << e.what() << "\n";
            return 1;
        }
    } else {
        std::cerr << "Tax lots database not found: " << lots_db << "\n";
        return 1;
    }

    // Filter to requested year
    int year = 0;
    if (!year_str.empty()) {
        try {
            year = std::stoi(year_str);
        } catch (...) {
            std::cerr << "Invalid year: " << year_str << "\n";
            return 1;
        }
    } else {
        // Default to current year
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm* tm_now = std::localtime(&time_t_now);
        year = tm_now->tm_year + 1900;
    }

    // Get sold lots and calculate gains/losses
    auto sold_lots = lot_manager.getSoldLots();

    double short_term_gains = 0.0, short_term_losses = 0.0;
    double long_term_gains = 0.0, long_term_losses = 0.0;
    std::vector<data::TaxLot> year_lots;

    for (const auto& lot : sold_lots) {
        if (!lot.sale_date) continue;

        // Check if sale was in the requested year
        auto sale_time_t = std::chrono::system_clock::to_time_t(*lot.sale_date);
        std::tm* sale_tm = std::localtime(&sale_time_t);
        int sale_year = sale_tm->tm_year + 1900;

        if (sale_year != year) continue;

        year_lots.push_back(lot);

        double gain = lot.getRealizedGain();
        if (lot.getGainType() == data::GainType::SHORT_TERM) {
            if (gain >= 0) short_term_gains += gain;
            else short_term_losses += std::abs(gain);
        } else {
            if (gain >= 0) long_term_gains += gain;
            else long_term_losses += std::abs(gain);
        }
    }

    // Create report
    data::CapitalGainsReport report;
    report.short_term_gains = short_term_gains;
    report.short_term_losses = short_term_losses;
    report.long_term_gains = long_term_gains;
    report.long_term_losses = long_term_losses;
    report.net_short_term = short_term_gains - short_term_losses;
    report.net_long_term = long_term_gains - long_term_losses;

    // Format output
    OutputFormat format = parseOutputFormat(format_str);
    std::string output;

    if (format == OutputFormat::JSON) {
        json j = report.toJSON();
        j["year"] = year;
        j["lot_count"] = year_lots.size();

        json lots_json = json::array();
        for (const auto& lot : year_lots) {
            lots_json.push_back(lot.toJSON());
        }
        j["lots"] = lots_json;
        output = j.dump(2);
    } else {
        output = "=== Capital Gains Report for " + std::to_string(year) + " ===\n\n";
        output += report.toPlainText();

        // Add lot details
        if (!year_lots.empty()) {
            std::stringstream ss;
            ss << "\nDetailed Transactions:\n";
            ss << std::string(80, '-') << "\n";
            ss << std::left << std::setw(10) << "Ticker"
               << std::setw(12) << "Shares"
               << std::setw(12) << "Cost Basis"
               << std::setw(12) << "Proceeds"
               << std::setw(14) << "Gain/Loss"
               << std::setw(12) << "Type" << "\n";
            ss << std::string(80, '-') << "\n";

            for (const auto& lot : year_lots) {
                double gain = lot.getRealizedGain();
                std::string gain_type = (lot.getGainType() == data::GainType::SHORT_TERM)
                    ? "Short-term" : "Long-term";

                ss << std::left << std::setw(10) << lot.ticker
                   << std::right << std::setw(11) << std::fixed << std::setprecision(2)
                   << lot.shares
                   << std::setw(11) << "$" << lot.getTotalCostBasis()
                   << std::setw(11) << "$" << (lot.sale_price.value_or(0.0) * lot.shares)
                   << std::setw(13) << "$" << gain
                   << std::setw(12) << gain_type << "\n";
            }

            output += ss.str();
        }
    }

    writeOutput(output, output_file);
    return 0;
}

/// @brief Run import-lots command to import tax lots from broker files
int runImportLots(const std::string& input_file, const std::string& lots_db,
                  const std::string& broker_str, bool auto_detect) {
    utils::Logger::info("Importing tax lots from: " + input_file);

    if (!fs::exists(input_file)) {
        std::cerr << "Input file not found: " << input_file << "\n";
        return 1;
    }

    // Determine broker format
    data::BrokerFormat broker_format = data::BrokerFormat::GENERIC_CSV;
    if (!broker_str.empty()) {
        if (broker_str == "schwab") broker_format = data::BrokerFormat::SCHWAB;
        else if (broker_str == "fidelity") broker_format = data::BrokerFormat::FIDELITY;
        else if (broker_str == "vanguard") broker_format = data::BrokerFormat::VANGUARD;
        else if (broker_str == "csv") broker_format = data::BrokerFormat::GENERIC_CSV;
        else {
            std::cerr << "Unknown broker: " << broker_str << "\n";
            std::cerr << "Supported: schwab, fidelity, vanguard, csv\n";
            return 1;
        }
    }

    // Create importer
    std::unique_ptr<data::BrokerImporter> importer;
    if (auto_detect || broker_str.empty()) {
        importer = data::BrokerImportFactory::autoDetect(input_file);
        if (!importer) {
            std::cerr << "Could not auto-detect broker format. Please specify --broker.\n";
            return 1;
        }
        utils::Logger::info("Auto-detected broker format");
    } else {
        importer = data::BrokerImportFactory::create(broker_format);
    }

    // Import lots
    data::ImportResult result = importer->importFromFile(input_file);

    if (!result.success) {
        std::cerr << "Import failed: " << result.error_message << "\n";
        if (!result.warnings.empty()) {
            std::cerr << "Warnings:\n";
            for (const auto& w : result.warnings) {
                std::cerr << "  - " << w << "\n";
            }
        }
        return 1;
    }

    // Load existing lots and merge
    data::TaxLotManager lot_manager;
    if (!lots_db.empty() && fs::exists(lots_db)) {
        try {
            lot_manager.loadFromDatabase(lots_db);
            utils::Logger::info("Loaded existing tax lots from: " + lots_db);
        } catch (const std::exception& e) {
            utils::Logger::warn("Could not load existing lots: " + std::string(e.what()));
        }
    }

    // Add imported lots
    int added = 0;
    for (const auto& lot : result.lots) {
        try {
            lot_manager.addLot(lot);
            added++;
        } catch (const std::exception& e) {
            utils::Logger::warn("Failed to add lot: " + std::string(e.what()));
        }
    }

    // Save to database
    std::string db_path = lots_db.empty() ?
        utils::Configuration::getInstance().getDatabasePath() + "_lots.db" : lots_db;

    try {
        lot_manager.saveToDatabase(db_path);
        std::cout << "Tax lots imported successfully\n";
        std::cout << "  Imported: " << added << " lots\n";
        std::cout << "  Total lots: " << lot_manager.getAllLots().size() << "\n";
        std::cout << "  Database: " << db_path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Failed to save lots: " << e.what() << "\n";
        return 1;
    }

    // Show warnings
    if (!result.warnings.empty()) {
        std::cout << "\nWarnings:\n";
        for (const auto& w : result.warnings) {
            std::cout << "  - " << w << "\n";
        }
    }

    return 0;
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    CLI::App app{"Lumen - Quantum-Enhanced Portfolio Optimizer"};
    app.set_version_flag("-v,--version", "1.0.0");

    // Global options
    std::string config_path;
    bool verbose = false;

    app.add_option("-c,--config", config_path, "Path to configuration file");
    app.add_flag("--verbose", verbose, "Enable verbose output");

    // -------------------------------------------------------------------------
    // Optimize subcommand
    // -------------------------------------------------------------------------
    auto* optimize_cmd = app.add_subcommand("optimize", "Run portfolio optimization");
    std::string portfolio_file, target_file, constraints_file;
    std::string output_file, format_str = "text";
    bool fetch_prices = false;

    optimize_cmd->add_option("-p,--portfolio", portfolio_file, "Portfolio JSON file")
        ->required();
    optimize_cmd->add_option("-t,--target", target_file, "Target allocation JSON file")
        ->required();
    optimize_cmd->add_option("--constraints", constraints_file, "Constraints JSON file");
    optimize_cmd->add_option("-o,--output", output_file, "Output file (default: stdout)");
    optimize_cmd->add_option("-f,--format", format_str, "Output format: json, text, markdown, html")
        ->default_val("text");
    optimize_cmd->add_flag("--fetch-prices", fetch_prices, "Fetch current market prices");

    // -------------------------------------------------------------------------
    // Import subcommand
    // -------------------------------------------------------------------------
    auto* import_cmd = app.add_subcommand("import", "Import portfolio data");
    std::string import_file, import_name, import_format;
    bool update_prices = false;

    import_cmd->add_option("file", import_file, "Input file (CSV or JSON)")
        ->required();
    import_cmd->add_option("-n,--name", import_name, "Portfolio name");
    import_cmd->add_option("-f,--format", import_format, "File format: csv, json");
    import_cmd->add_flag("--update-prices", update_prices, "Fetch current prices after import");

    // -------------------------------------------------------------------------
    // Explain subcommand
    // -------------------------------------------------------------------------
    auto* explain_cmd = app.add_subcommand("explain", "Explain past optimization results");
    std::string explain_session, explain_output, explain_format = "text";

    explain_cmd->add_option("session", explain_session, "Session ID to explain")
        ->required();
    explain_cmd->add_option("-o,--output", explain_output, "Output file");
    explain_cmd->add_option("-f,--format", explain_format, "Output format: json, text, markdown")
        ->default_val("text");

    // -------------------------------------------------------------------------
    // List subcommand
    // -------------------------------------------------------------------------
    auto* list_cmd = app.add_subcommand("list", "List portfolios or optimization history");
    std::string list_what = "portfolios";

    list_cmd->add_option("what", list_what, "What to list: portfolios, history")
        ->default_val("portfolios");

    // -------------------------------------------------------------------------
    // Config subcommand
    // -------------------------------------------------------------------------
    auto* config_cmd = app.add_subcommand("config", "Manage configuration");
    std::string config_action = "show", config_key, config_value, config_format = "text";

    config_cmd->add_option("action", config_action, "Action: show, set, path")
        ->default_val("show");
    config_cmd->add_option("key", config_key, "Configuration key (for set)");
    config_cmd->add_option("value", config_value, "Configuration value (for set)");
    config_cmd->add_option("-f,--format", config_format, "Output format: json, text")
        ->default_val("text");

    // -------------------------------------------------------------------------
    // Status subcommand
    // -------------------------------------------------------------------------
    auto* status_cmd = app.add_subcommand("status", "Show system status");

    // -------------------------------------------------------------------------
    // Quote subcommand
    // -------------------------------------------------------------------------
    auto* quote_cmd = app.add_subcommand("quote", "Get current stock quotes");
    std::vector<std::string> quote_tickers;
    std::string quote_format = "text";

    quote_cmd->add_option("tickers", quote_tickers, "Stock ticker symbols")
        ->required();
    quote_cmd->add_option("-f,--format", quote_format, "Output format: json, text")
        ->default_val("text");

    // -------------------------------------------------------------------------
    // Version subcommand
    // -------------------------------------------------------------------------
    auto* version_cmd = app.add_subcommand("version", "Show version information");

    // -------------------------------------------------------------------------
    // Tax-harvest subcommand
    // -------------------------------------------------------------------------
    auto* tax_harvest_cmd = app.add_subcommand("tax-harvest", "Find tax-loss harvesting opportunities");
    std::string harvest_portfolio, harvest_lots_db, harvest_output;
    std::string harvest_format = "text";
    double harvest_min_loss = 100.0;
    bool harvest_fetch_prices = false;

    tax_harvest_cmd->add_option("-p,--portfolio", harvest_portfolio, "Portfolio JSON file")
        ->required();
    tax_harvest_cmd->add_option("--lots-db", harvest_lots_db, "Tax lots database file");
    tax_harvest_cmd->add_option("--min-loss", harvest_min_loss, "Minimum loss threshold")
        ->default_val(100.0);
    tax_harvest_cmd->add_option("-o,--output", harvest_output, "Output file");
    tax_harvest_cmd->add_option("-f,--format", harvest_format, "Output format: json, text")
        ->default_val("text");
    tax_harvest_cmd->add_flag("--fetch-prices", harvest_fetch_prices, "Fetch current market prices");

    // -------------------------------------------------------------------------
    // Tax-report subcommand
    // -------------------------------------------------------------------------
    auto* tax_report_cmd = app.add_subcommand("tax-report", "Generate capital gains tax report");
    std::string report_lots_db, report_year, report_output;
    std::string report_format = "text";

    tax_report_cmd->add_option("--lots-db", report_lots_db, "Tax lots database file")
        ->required();
    tax_report_cmd->add_option("--year", report_year, "Tax year (default: current year)");
    tax_report_cmd->add_option("-o,--output", report_output, "Output file");
    tax_report_cmd->add_option("-f,--format", report_format, "Output format: json, text")
        ->default_val("text");

    // -------------------------------------------------------------------------
    // Import-lots subcommand
    // -------------------------------------------------------------------------
    auto* import_lots_cmd = app.add_subcommand("import-lots", "Import tax lots from broker files");
    std::string import_lots_file, import_lots_db, import_lots_broker;
    bool import_lots_auto = true;

    import_lots_cmd->add_option("file", import_lots_file, "Broker CSV file")
        ->required();
    import_lots_cmd->add_option("--lots-db", import_lots_db, "Tax lots database file");
    import_lots_cmd->add_option("--broker", import_lots_broker,
        "Broker format: schwab, fidelity, vanguard, csv");
    import_lots_cmd->add_flag("--auto-detect,!--no-auto-detect", import_lots_auto,
        "Auto-detect broker format")
        ->default_val(true);

    // Parse arguments
    CLI11_PARSE(app, argc, argv);

    // Initialize
    if (!initialize(config_path)) {
        return 1;
    }

    if (verbose) {
        utils::Logger::getInstance().setLevel(utils::LogLevel::DEBUG);
    }

    // Execute subcommand
    if (*version_cmd) {
        printVersion();
        return 0;
    }

    if (*status_cmd) {
        return runStatus();
    }

    if (*optimize_cmd) {
        return runOptimize(portfolio_file, target_file, constraints_file,
                          output_file, format_str, fetch_prices, verbose);
    }

    if (*import_cmd) {
        return runImport(import_file, import_name, import_format, update_prices);
    }

    if (*explain_cmd) {
        return runExplain(explain_session, explain_output, explain_format);
    }

    if (*list_cmd) {
        return runList(list_what);
    }

    if (*config_cmd) {
        return runConfig(config_action, config_key, config_value, config_format);
    }

    if (*quote_cmd) {
        return runQuote(quote_tickers, quote_format);
    }

    if (*tax_harvest_cmd) {
        return runTaxHarvest(harvest_portfolio, harvest_lots_db, harvest_min_loss,
                            harvest_output, harvest_format, harvest_fetch_prices);
    }

    if (*tax_report_cmd) {
        return runTaxReport(report_lots_db, report_year, report_output, report_format);
    }

    if (*import_lots_cmd) {
        return runImportLots(import_lots_file, import_lots_db, import_lots_broker,
                            import_lots_auto);
    }

    // No subcommand - show help
    std::cout << app.help() << std::endl;
    return 0;
}
