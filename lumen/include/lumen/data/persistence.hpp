#pragma once

/// @file persistence.hpp
/// @brief SQLite persistence layer for Lumen data
///
/// This module provides persistent storage for portfolios, tax lots,
/// optimization history, and constraint sets using SQLite.

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lumen::data {

/// @brief Portfolio snapshot for persistence
struct PortfolioRecord {
    std::string id;                    ///< Unique portfolio ID
    std::string name;                  ///< Portfolio name
    nlohmann::json data;               ///< Full portfolio JSON data
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

/// @brief Optimization run record for history
struct OptimizationRecord {
    int64_t id;                        ///< Auto-increment ID
    std::string portfolio_id;          ///< Associated portfolio
    std::string session_id;            ///< Session identifier
    std::string solver_used;           ///< Solver name
    std::string status;                ///< Result status
    double objective_value;            ///< Objective value
    double solve_time_ms;              ///< Solve time in milliseconds
    nlohmann::json trades;             ///< Trade recommendations
    nlohmann::json provenance;         ///< Provenance data
    std::chrono::system_clock::time_point created_at;
};

/// @brief Constraint set record for persistence
struct ConstraintSetRecord {
    std::string id;                    ///< Unique constraint set ID
    std::string portfolio_id;          ///< Associated portfolio
    std::string name;                  ///< Constraint set name
    nlohmann::json constraints;        ///< Constraints JSON
    std::chrono::system_clock::time_point created_at;
};

/// @brief RAII wrapper for SQLite database connection with encryption support
class Database {
public:
    Database();
    ~Database();

    // Prevent copying
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /// Open database at path (uses encryption if configured and LUMEN_DB_KEY set)
    bool open(const std::string& path);

    /// Open database with explicit encryption key (requires SQLCipher)
    /// @param path Database file path
    /// @param key Encryption key (empty string disables encryption)
    bool openEncrypted(const std::string& path, const std::string& key);

    /// Close database
    void close();

    /// Check if database is open
    bool isOpen() const;

    /// Check if database is encrypted
    bool isEncrypted() const;

    /// Execute SQL statement
    bool execute(const std::string& sql);

    /// Get last error message
    std::string getLastError() const;

    /// Get native handle for advanced operations
    void* getHandle();

    /// Begin transaction
    bool beginTransaction();

    /// Commit transaction
    bool commitTransaction();

    /// Rollback transaction
    bool rollbackTransaction();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief Persistence manager for Lumen data
class PersistenceManager {
public:
    PersistenceManager();
    ~PersistenceManager();

    // Prevent copying
    PersistenceManager(const PersistenceManager&) = delete;
    PersistenceManager& operator=(const PersistenceManager&) = delete;

    /// Initialize with database path
    bool initialize(const std::string& db_path);

    /// Check if initialized
    bool isInitialized() const;

    /// Get the database instance
    Database& getDatabase();

    // =========================================================================
    // Portfolio Operations
    // =========================================================================

    /// Save a portfolio
    bool savePortfolio(const std::string& id, const std::string& name,
                       const nlohmann::json& data);

    /// Load a portfolio by ID
    std::optional<PortfolioRecord> loadPortfolio(const std::string& id);

    /// List all portfolios (returns id, name pairs)
    std::vector<std::pair<std::string, std::string>> listPortfolios();

    /// Delete a portfolio
    bool deletePortfolio(const std::string& id);

    /// Check if portfolio exists
    bool portfolioExists(const std::string& id);

    /// Update portfolio data
    bool updatePortfolio(const std::string& id, const nlohmann::json& data);

    // =========================================================================
    // Tax Lot Operations
    // =========================================================================

    /// Save a tax lot
    bool saveTaxLot(const std::string& portfolio_id, const nlohmann::json& lot);

    /// Load all tax lots for a portfolio
    std::vector<nlohmann::json> loadTaxLots(const std::string& portfolio_id);

    /// Delete a tax lot
    bool deleteTaxLot(const std::string& lot_id);

    /// Delete all tax lots for a portfolio
    bool deleteAllTaxLots(const std::string& portfolio_id);

    // =========================================================================
    // Optimization History Operations
    // =========================================================================

    /// Save an optimization result
    bool saveOptimizationResult(const std::string& portfolio_id,
                                 const std::string& session_id,
                                 const std::string& solver_used,
                                 const std::string& status,
                                 double objective_value,
                                 double solve_time_ms,
                                 const nlohmann::json& trades,
                                 const nlohmann::json& provenance);

    /// Get optimization history for a portfolio
    std::vector<OptimizationRecord> getOptimizationHistory(
        const std::string& portfolio_id, int limit = 10);

    /// Get a specific optimization result by session ID
    std::optional<OptimizationRecord> getOptimizationBySession(
        const std::string& session_id);

    /// Delete old optimization history (older than days)
    int deleteOldHistory(int days);

    // =========================================================================
    // Constraint Set Operations
    // =========================================================================

    /// Save a constraint set
    bool saveConstraintSet(const std::string& id, const std::string& portfolio_id,
                           const std::string& name, const nlohmann::json& constraints);

    /// Load a constraint set by ID
    std::optional<ConstraintSetRecord> loadConstraintSet(const std::string& id);

    /// List constraint sets for a portfolio
    std::vector<std::pair<std::string, std::string>> listConstraintSets(
        const std::string& portfolio_id);

    /// Delete a constraint set
    bool deleteConstraintSet(const std::string& id);

    // =========================================================================
    // Market Data Cache Operations
    // =========================================================================

    /// Cache market data
    bool cacheMarketData(const std::string& ticker, const std::string& data_type,
                         const nlohmann::json& data);

    /// Get cached market data
    std::optional<nlohmann::json> getCachedMarketData(
        const std::string& ticker, const std::string& data_type,
        int max_age_seconds = 300);

    /// Clear market data cache
    void clearMarketDataCache();

    /// Purge expired cache entries
    void purgeExpiredCache();

    // =========================================================================
    // Utility Operations
    // =========================================================================

    /// Vacuum the database to reclaim space
    void vacuum();

    /// Get database file size in bytes
    size_t getDatabaseSize() const;

    /// Export all data to JSON
    nlohmann::json exportAllData();

    /// Import data from JSON
    bool importData(const nlohmann::json& data);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Global persistence manager access
PersistenceManager& getPersistenceManager();

/// Initialize global persistence manager
bool initializePersistence(const std::string& db_path);

/// Generate a cryptographically secure unique ID (128-bit random)
std::string generateUniqueId();

/// Generate a cryptographically secure session ID (256-bit random)
std::string generateSecureSessionId();

}  // namespace lumen::data
