/// @file persistence.cpp
/// @brief SQLite persistence layer implementation
///
/// Implementation of the SQLite-based persistence layer for portfolios,
/// tax lots, and optimization history.
///
/// Security features:
/// - Cryptographically secure ID generation using std::random_device
/// - Support for SQLite encryption via SQLCipher (when available)
/// - Secure file permissions on database files

#include "lumen/data/persistence.hpp"
#include "lumen/utils/logging.hpp"
#include "lumen/utils/config.hpp"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <random>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace lumen::data {

namespace fs = std::filesystem;

// =============================================================================
// Cryptographically Secure ID Generation
// =============================================================================

/// @brief Thread-safe cryptographically secure random number generator
class SecureRandom {
public:
    static SecureRandom& getInstance() {
        static SecureRandom instance;
        return instance;
    }

    /// @brief Generate cryptographically secure random bytes
    void getBytes(uint8_t* buffer, size_t length) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < length; ++i) {
            buffer[i] = static_cast<uint8_t>(rd_());
        }
    }

    /// @brief Generate a random 64-bit integer
    uint64_t getUint64() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::array<uint8_t, 8> bytes;
        for (auto& b : bytes) {
            b = static_cast<uint8_t>(rd_());
        }
        uint64_t result = 0;
        for (int i = 0; i < 8; ++i) {
            result = (result << 8) | bytes[i];
        }
        return result;
    }

private:
    SecureRandom() = default;
    std::random_device rd_;
    std::mutex mutex_;
};

std::string generateUniqueId() {
    auto& rng = SecureRandom::getInstance();

    // Generate 128 bits (16 bytes) of cryptographically random data
    std::array<uint8_t, 16> bytes;
    rng.getBytes(bytes.data(), bytes.size());

    // Format as UUID-like hex string
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(32);

    for (uint8_t byte : bytes) {
        result += hex_chars[byte >> 4];
        result += hex_chars[byte & 0x0F];
    }

    return result;
}

/// @brief Generate a cryptographically secure session ID
std::string generateSecureSessionId() {
    auto& rng = SecureRandom::getInstance();

    // Generate 256 bits for session IDs (extra security for session tokens)
    std::array<uint8_t, 32> bytes;
    rng.getBytes(bytes.data(), bytes.size());

    static const char hex_chars[] = "0123456789abcdef";
    std::string result = "SID_";
    result.reserve(68);  // "SID_" + 64 hex chars

    for (uint8_t byte : bytes) {
        result += hex_chars[byte >> 4];
        result += hex_chars[byte & 0x0F];
    }

    return result;
}

static int64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

static std::chrono::system_clock::time_point timestampToTimePoint(int64_t ts) {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(ts));
}

// =============================================================================
// Database Implementation
// =============================================================================

class Database::Impl {
public:
    sqlite3* db_ = nullptr;
    std::string last_error_;
    std::string db_path_;
    bool encrypted_ = false;
    mutable std::mutex mutex_;

    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    /// @brief Set secure file permissions (owner read/write only)
    static bool setSecurePermissions(const std::string& path) {
#ifndef _WIN32
        // Unix: Set file permissions to 0600 (owner read/write only)
        if (chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0) {
            return false;
        }
#else
        // Windows: File permissions are handled differently
        // For now, just return true - would need Windows ACL for proper implementation
        (void)path;
#endif
        return true;
    }

    /// @brief Apply encryption key if SQLCipher is available
    bool applyEncryption(const std::string& key) {
        if (key.empty() || !db_) {
            return false;
        }

#ifdef SQLITE_HAS_CODEC
        // SQLCipher is available - apply encryption key
        int rc = sqlite3_key(db_, key.c_str(), static_cast<int>(key.length()));
        if (rc != SQLITE_OK) {
            last_error_ = "Failed to apply encryption key";
            return false;
        }
        encrypted_ = true;

        // Verify the key works by running a simple query
        char* err_msg = nullptr;
        rc = sqlite3_exec(db_, "SELECT count(*) FROM sqlite_master;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            if (err_msg) {
                last_error_ = "Encryption key verification failed: " + std::string(err_msg);
                sqlite3_free(err_msg);
            }
            return false;
        }
        return true;
#else
        // SQLCipher not available - log warning
        LOG_WARN("Database encryption requested but SQLCipher is not available. "
                 "Data will be stored unencrypted. Install SQLCipher for encryption support.");
        (void)key;
        return false;
#endif
    }
};

Database::Database() : impl_(std::make_unique<Impl>()) {}

Database::~Database() = default;

bool Database::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (impl_->db_) {
        sqlite3_close(impl_->db_);
        impl_->db_ = nullptr;
    }

    impl_->db_path_ = path;

    // Ensure parent directory exists with secure permissions
    fs::path db_path(path);
    if (db_path.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(db_path.parent_path(), ec);
        if (ec) {
            impl_->last_error_ = "Failed to create directory: " + ec.message();
            return false;
        }

#ifndef _WIN32
        // Set directory permissions to 0700 (owner only)
        chmod(db_path.parent_path().c_str(), S_IRWXU);
#endif
    }

    int rc = sqlite3_open(path.c_str(), &impl_->db_);
    if (rc != SQLITE_OK) {
        impl_->last_error_ = sqlite3_errmsg(impl_->db_);
        sqlite3_close(impl_->db_);
        impl_->db_ = nullptr;
        return false;
    }

    // Set secure file permissions on the database file
    Impl::setSecurePermissions(path);

    // Check if encryption is enabled in configuration
    try {
        auto& config = utils::Configuration::getInstance();
        if (config.getPersistenceConfig().encryption_enabled) {
            // Get encryption key from environment variable
            const char* db_key = std::getenv("LUMEN_DB_KEY");
            if (db_key && std::strlen(db_key) > 0) {
                if (!impl_->applyEncryption(db_key)) {
                    LOG_WARN("Database encryption failed: " + impl_->last_error_);
                }
            } else {
                LOG_WARN("Database encryption enabled but LUMEN_DB_KEY not set. "
                         "Data will be stored unencrypted.");
            }
        }
    } catch (...) {
        // Configuration not available yet - proceed without encryption
    }

    // Enable foreign keys
    sqlite3_exec(impl_->db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Enable WAL mode for better concurrency
    sqlite3_exec(impl_->db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

    // Set synchronous mode
    sqlite3_exec(impl_->db_, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);

    // Security: Disable loading of extensions
    sqlite3_db_config(impl_->db_, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, 0, nullptr);

    return true;
}

bool Database::openEncrypted(const std::string& path, const std::string& key) {
    if (!open(path)) {
        return false;
    }

    if (!key.empty()) {
        return impl_->applyEncryption(key);
    }

    return true;
}

bool Database::isEncrypted() const {
    return impl_->encrypted_;
}

void Database::close() {
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->db_) {
        sqlite3_close(impl_->db_);
        impl_->db_ = nullptr;
    }
}

bool Database::isOpen() const {
    return impl_->db_ != nullptr;
}

bool Database::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (!impl_->db_) {
        impl_->last_error_ = "Database not open";
        return false;
    }

    char* error_msg = nullptr;
    int rc = sqlite3_exec(impl_->db_, sql.c_str(), nullptr, nullptr, &error_msg);

    if (rc != SQLITE_OK) {
        if (error_msg) {
            impl_->last_error_ = error_msg;
            sqlite3_free(error_msg);
        }
        return false;
    }

    return true;
}

std::string Database::getLastError() const {
    return impl_->last_error_;
}

void* Database::getHandle() {
    return impl_->db_;
}

bool Database::beginTransaction() {
    return execute("BEGIN TRANSACTION;");
}

bool Database::commitTransaction() {
    return execute("COMMIT;");
}

bool Database::rollbackTransaction() {
    return execute("ROLLBACK;");
}

// =============================================================================
// PersistenceManager Implementation
// =============================================================================

class PersistenceManager::Impl {
public:
    Database db_;
    std::string db_path_;
    bool initialized_ = false;

    bool createTables() {
        // Portfolios table
        const char* portfolios_sql = R"(
            CREATE TABLE IF NOT EXISTS portfolios (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                data TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                updated_at INTEGER NOT NULL
            );
            CREATE INDEX IF NOT EXISTS idx_portfolios_name ON portfolios(name);
        )";

        // Tax lots table
        const char* tax_lots_sql = R"(
            CREATE TABLE IF NOT EXISTS tax_lots (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                ticker TEXT NOT NULL,
                shares REAL NOT NULL,
                cost_basis REAL NOT NULL,
                acquisition_date TEXT NOT NULL,
                data TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_tax_lots_portfolio ON tax_lots(portfolio_id);
            CREATE INDEX IF NOT EXISTS idx_tax_lots_ticker ON tax_lots(ticker);
        )";

        // Optimization history table
        const char* opt_history_sql = R"(
            CREATE TABLE IF NOT EXISTS optimization_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                portfolio_id TEXT NOT NULL,
                session_id TEXT UNIQUE NOT NULL,
                solver_used TEXT NOT NULL,
                status TEXT NOT NULL,
                objective_value REAL,
                solve_time_ms REAL,
                trades TEXT,
                provenance TEXT,
                created_at INTEGER NOT NULL,
                FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_opt_history_portfolio ON optimization_history(portfolio_id);
            CREATE INDEX IF NOT EXISTS idx_opt_history_session ON optimization_history(session_id);
            CREATE INDEX IF NOT EXISTS idx_opt_history_created ON optimization_history(created_at);
        )";

        // Constraint sets table
        const char* constraints_sql = R"(
            CREATE TABLE IF NOT EXISTS constraint_sets (
                id TEXT PRIMARY KEY,
                portfolio_id TEXT NOT NULL,
                name TEXT NOT NULL,
                constraints TEXT NOT NULL,
                created_at INTEGER NOT NULL,
                FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) ON DELETE CASCADE
            );
            CREATE INDEX IF NOT EXISTS idx_constraint_sets_portfolio ON constraint_sets(portfolio_id);
        )";

        // Market data cache table
        const char* market_cache_sql = R"(
            CREATE TABLE IF NOT EXISTS market_data_cache (
                ticker TEXT NOT NULL,
                data_type TEXT NOT NULL,
                data TEXT NOT NULL,
                fetched_at INTEGER NOT NULL,
                PRIMARY KEY (ticker, data_type)
            );
            CREATE INDEX IF NOT EXISTS idx_market_cache_fetched ON market_data_cache(fetched_at);
        )";

        return db_.execute(portfolios_sql) &&
               db_.execute(tax_lots_sql) &&
               db_.execute(opt_history_sql) &&
               db_.execute(constraints_sql) &&
               db_.execute(market_cache_sql);
    }
};

PersistenceManager::PersistenceManager() : impl_(std::make_unique<Impl>()) {}

PersistenceManager::~PersistenceManager() = default;

bool PersistenceManager::initialize(const std::string& db_path) {
    impl_->db_path_ = db_path;

    if (!impl_->db_.open(db_path)) {
        LOG_ERROR("Failed to open database: " + impl_->db_.getLastError());
        return false;
    }

    if (!impl_->createTables()) {
        LOG_ERROR("Failed to create tables: " + impl_->db_.getLastError());
        return false;
    }

    impl_->initialized_ = true;
    LOG_INFO("Persistence layer initialized: " + db_path);
    return true;
}

bool PersistenceManager::isInitialized() const {
    return impl_->initialized_;
}

Database& PersistenceManager::getDatabase() {
    return impl_->db_;
}

// =============================================================================
// Portfolio Operations
// =============================================================================

bool PersistenceManager::savePortfolio(const std::string& id, const std::string& name,
                                        const nlohmann::json& data) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        INSERT OR REPLACE INTO portfolios (id, name, data, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    int64_t now = getCurrentTimestamp();
    std::string json_str = data.dump();

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, json_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, now);
    sqlite3_bind_int64(stmt, 5, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::optional<PortfolioRecord> PersistenceManager::loadPortfolio(const std::string& id) {
    if (!impl_->initialized_) return std::nullopt;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "SELECT id, name, data, created_at, updated_at FROM portfolios WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    std::optional<PortfolioRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        PortfolioRecord record;
        record.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

        const char* data_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        try {
            record.data = nlohmann::json::parse(data_str);
        } catch (...) {
            record.data = nlohmann::json::object();
        }

        record.created_at = timestampToTimePoint(sqlite3_column_int64(stmt, 3));
        record.updated_at = timestampToTimePoint(sqlite3_column_int64(stmt, 4));

        result = record;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::pair<std::string, std::string>> PersistenceManager::listPortfolios() {
    std::vector<std::pair<std::string, std::string>> portfolios;

    if (!impl_->initialized_) return portfolios;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "SELECT id, name FROM portfolios ORDER BY updated_at DESC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return portfolios;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        portfolios.emplace_back(id, name);
    }

    sqlite3_finalize(stmt);
    return portfolios;
}

bool PersistenceManager::deletePortfolio(const std::string& id) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "DELETE FROM portfolios WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

bool PersistenceManager::portfolioExists(const std::string& id) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "SELECT 1 FROM portfolios WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    return exists;
}

bool PersistenceManager::updatePortfolio(const std::string& id, const nlohmann::json& data) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "UPDATE portfolios SET data = ?, updated_at = ? WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string json_str = data.dump();
    int64_t now = getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, json_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, now);
    sqlite3_bind_text(stmt, 3, id.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

// =============================================================================
// Tax Lot Operations
// =============================================================================

bool PersistenceManager::saveTaxLot(const std::string& portfolio_id, const nlohmann::json& lot) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        INSERT OR REPLACE INTO tax_lots (id, portfolio_id, ticker, shares, cost_basis, acquisition_date, data, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string lot_id = lot.value("id", generateUniqueId());
    std::string ticker = lot.value("ticker", "");
    double shares = lot.value("shares", 0.0);
    double cost_basis = lot.value("cost_basis", 0.0);
    std::string acq_date = lot.value("acquisition_date", "");
    std::string data_str = lot.dump();
    int64_t now = getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, lot_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, portfolio_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 4, shares);
    sqlite3_bind_double(stmt, 5, cost_basis);
    sqlite3_bind_text(stmt, 6, acq_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, data_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 8, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::vector<nlohmann::json> PersistenceManager::loadTaxLots(const std::string& portfolio_id) {
    std::vector<nlohmann::json> lots;

    if (!impl_->initialized_) return lots;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "SELECT data FROM tax_lots WHERE portfolio_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return lots;
    }

    sqlite3_bind_text(stmt, 1, portfolio_id.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        try {
            lots.push_back(nlohmann::json::parse(data_str));
        } catch (...) {}
    }

    sqlite3_finalize(stmt);
    return lots;
}

bool PersistenceManager::deleteTaxLot(const std::string& lot_id) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "DELETE FROM tax_lots WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, lot_id.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

bool PersistenceManager::deleteAllTaxLots(const std::string& portfolio_id) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "DELETE FROM tax_lots WHERE portfolio_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, portfolio_id.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

// =============================================================================
// Optimization History Operations
// =============================================================================

bool PersistenceManager::saveOptimizationResult(const std::string& portfolio_id,
                                                 const std::string& session_id,
                                                 const std::string& solver_used,
                                                 const std::string& status,
                                                 double objective_value,
                                                 double solve_time_ms,
                                                 const nlohmann::json& trades,
                                                 const nlohmann::json& provenance) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        INSERT INTO optimization_history
            (portfolio_id, session_id, solver_used, status, objective_value, solve_time_ms, trades, provenance, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string trades_str = trades.dump();
    std::string provenance_str = provenance.dump();
    int64_t now = getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, portfolio_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, session_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, solver_used.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 5, objective_value);
    sqlite3_bind_double(stmt, 6, solve_time_ms);
    sqlite3_bind_text(stmt, 7, trades_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, provenance_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 9, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::vector<OptimizationRecord> PersistenceManager::getOptimizationHistory(
    const std::string& portfolio_id, int limit) {

    std::vector<OptimizationRecord> history;

    if (!impl_->initialized_) return history;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        SELECT id, portfolio_id, session_id, solver_used, status, objective_value,
               solve_time_ms, trades, provenance, created_at
        FROM optimization_history
        WHERE portfolio_id = ?
        ORDER BY created_at DESC
        LIMIT ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return history;
    }

    sqlite3_bind_text(stmt, 1, portfolio_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OptimizationRecord record;
        record.id = sqlite3_column_int64(stmt, 0);
        record.portfolio_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.solver_used = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        record.objective_value = sqlite3_column_double(stmt, 5);
        record.solve_time_ms = sqlite3_column_double(stmt, 6);

        const char* trades_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* prov_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));

        try {
            record.trades = nlohmann::json::parse(trades_str);
        } catch (...) {
            record.trades = nlohmann::json::array();
        }

        try {
            record.provenance = nlohmann::json::parse(prov_str);
        } catch (...) {
            record.provenance = nlohmann::json::object();
        }

        record.created_at = timestampToTimePoint(sqlite3_column_int64(stmt, 9));

        history.push_back(record);
    }

    sqlite3_finalize(stmt);
    return history;
}

std::optional<OptimizationRecord> PersistenceManager::getOptimizationBySession(
    const std::string& session_id) {

    if (!impl_->initialized_) return std::nullopt;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        SELECT id, portfolio_id, session_id, solver_used, status, objective_value,
               solve_time_ms, trades, provenance, created_at
        FROM optimization_history
        WHERE session_id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_STATIC);

    std::optional<OptimizationRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        OptimizationRecord record;
        record.id = sqlite3_column_int64(stmt, 0);
        record.portfolio_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        record.solver_used = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        record.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        record.objective_value = sqlite3_column_double(stmt, 5);
        record.solve_time_ms = sqlite3_column_double(stmt, 6);

        const char* trades_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        const char* prov_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));

        try {
            record.trades = nlohmann::json::parse(trades_str);
        } catch (...) {
            record.trades = nlohmann::json::array();
        }

        try {
            record.provenance = nlohmann::json::parse(prov_str);
        } catch (...) {
            record.provenance = nlohmann::json::object();
        }

        record.created_at = timestampToTimePoint(sqlite3_column_int64(stmt, 9));

        result = record;
    }

    sqlite3_finalize(stmt);
    return result;
}

int PersistenceManager::deleteOldHistory(int days) {
    if (!impl_->initialized_) return 0;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());

    int64_t cutoff = getCurrentTimestamp() - (days * 24 * 60 * 60);

    const char* sql = "DELETE FROM optimization_history WHERE created_at < ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, cutoff);

    int deleted = 0;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        deleted = sqlite3_changes(db);
    }

    sqlite3_finalize(stmt);
    return deleted;
}

// =============================================================================
// Constraint Set Operations
// =============================================================================

bool PersistenceManager::saveConstraintSet(const std::string& id, const std::string& portfolio_id,
                                            const std::string& name, const nlohmann::json& constraints) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        INSERT OR REPLACE INTO constraint_sets (id, portfolio_id, name, constraints, created_at)
        VALUES (?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string constraints_str = constraints.dump();
    int64_t now = getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, portfolio_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, constraints_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::optional<ConstraintSetRecord> PersistenceManager::loadConstraintSet(const std::string& id) {
    if (!impl_->initialized_) return std::nullopt;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        SELECT id, portfolio_id, name, constraints, created_at
        FROM constraint_sets WHERE id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    std::optional<ConstraintSetRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ConstraintSetRecord record;
        record.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        record.portfolio_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        record.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        const char* constraints_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        try {
            record.constraints = nlohmann::json::parse(constraints_str);
        } catch (...) {
            record.constraints = nlohmann::json::array();
        }

        record.created_at = timestampToTimePoint(sqlite3_column_int64(stmt, 4));

        result = record;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::pair<std::string, std::string>> PersistenceManager::listConstraintSets(
    const std::string& portfolio_id) {

    std::vector<std::pair<std::string, std::string>> sets;

    if (!impl_->initialized_) return sets;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "SELECT id, name FROM constraint_sets WHERE portfolio_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return sets;
    }

    sqlite3_bind_text(stmt, 1, portfolio_id.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        sets.emplace_back(id, name);
    }

    sqlite3_finalize(stmt);
    return sets;
}

bool PersistenceManager::deleteConstraintSet(const std::string& id) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = "DELETE FROM constraint_sets WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

// =============================================================================
// Market Data Cache Operations
// =============================================================================

bool PersistenceManager::cacheMarketData(const std::string& ticker, const std::string& data_type,
                                          const nlohmann::json& data) {
    if (!impl_->initialized_) return false;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        INSERT OR REPLACE INTO market_data_cache (ticker, data_type, data, fetched_at)
        VALUES (?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    std::string data_str = data.dump();
    int64_t now = getCurrentTimestamp();

    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, data_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, data_str.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, now);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);

    return success;
}

std::optional<nlohmann::json> PersistenceManager::getCachedMarketData(
    const std::string& ticker, const std::string& data_type, int max_age_seconds) {

    if (!impl_->initialized_) return std::nullopt;

    sqlite3* db = static_cast<sqlite3*>(impl_->db_.getHandle());
    const char* sql = R"(
        SELECT data FROM market_data_cache
        WHERE ticker = ? AND data_type = ? AND fetched_at > ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    int64_t cutoff = getCurrentTimestamp() - max_age_seconds;

    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, data_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, cutoff);

    std::optional<nlohmann::json> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        try {
            result = nlohmann::json::parse(data_str);
        } catch (...) {}
    }

    sqlite3_finalize(stmt);
    return result;
}

void PersistenceManager::clearMarketDataCache() {
    if (!impl_->initialized_) return;
    impl_->db_.execute("DELETE FROM market_data_cache;");
}

void PersistenceManager::purgeExpiredCache() {
    if (!impl_->initialized_) return;

    // Default: remove entries older than 24 hours
    int64_t cutoff = getCurrentTimestamp() - (24 * 60 * 60);
    std::string sql = "DELETE FROM market_data_cache WHERE fetched_at < " +
                      std::to_string(cutoff) + ";";
    impl_->db_.execute(sql);
}

// =============================================================================
// Utility Operations
// =============================================================================

void PersistenceManager::vacuum() {
    if (impl_->initialized_) {
        impl_->db_.execute("VACUUM;");
    }
}

size_t PersistenceManager::getDatabaseSize() const {
    if (impl_->db_path_.empty()) return 0;

    std::error_code ec;
    auto size = fs::file_size(impl_->db_path_, ec);
    return ec ? 0 : size;
}

nlohmann::json PersistenceManager::exportAllData() {
    nlohmann::json data;

    if (!impl_->initialized_) return data;

    // Export portfolios
    data["portfolios"] = nlohmann::json::array();
    for (const auto& [id, name] : listPortfolios()) {
        auto record = loadPortfolio(id);
        if (record) {
            data["portfolios"].push_back({
                {"id", record->id},
                {"name", record->name},
                {"data", record->data}
            });
        }
    }

    return data;
}

bool PersistenceManager::importData(const nlohmann::json& data) {
    if (!impl_->initialized_) return false;

    impl_->db_.beginTransaction();

    try {
        if (data.contains("portfolios") && data["portfolios"].is_array()) {
            for (const auto& p : data["portfolios"]) {
                savePortfolio(
                    p.value("id", generateUniqueId()),
                    p.value("name", "Imported Portfolio"),
                    p.value("data", nlohmann::json::object())
                );
            }
        }

        impl_->db_.commitTransaction();
        return true;
    } catch (...) {
        impl_->db_.rollbackTransaction();
        return false;
    }
}

// =============================================================================
// Global Instance
// =============================================================================

static std::unique_ptr<PersistenceManager> g_persistence_manager;

PersistenceManager& getPersistenceManager() {
    if (!g_persistence_manager) {
        g_persistence_manager = std::make_unique<PersistenceManager>();
    }
    return *g_persistence_manager;
}

bool initializePersistence(const std::string& db_path) {
    return getPersistenceManager().initialize(db_path);
}

}  // namespace lumen::data
