/// @file config.cpp
/// @brief Configuration system implementation
///
/// Implementation of the configuration management system for Lumen.

#include "lumen/utils/config.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef LUMEN_HAS_YAML
#include <yaml-cpp/yaml.h>
#endif

namespace lumen::utils {

namespace fs = std::filesystem;

// =============================================================================
// Configuration::Impl
// =============================================================================

class Configuration::Impl {
public:
    SolverConfig solver_config_;
    MarketDataConfig market_data_config_;
    ObservabilityConfig observability_config_;
    PersistenceConfig persistence_config_;
    std::map<std::string, std::string> custom_settings_;
    std::string home_directory_;

    Impl() {
        // Determine home directory
        const char* home_env = std::getenv("LUMEN_HOME");
        if (home_env) {
            home_directory_ = home_env;
        } else {
            const char* user_home = std::getenv("HOME");
            if (!user_home) {
                user_home = std::getenv("USERPROFILE");  // Windows
            }
            if (user_home) {
                home_directory_ = std::string(user_home) + "/.lumen";
            } else {
                home_directory_ = ".lumen";
            }
        }
    }

    static std::string expandPath(const std::string& path) {
        if (path.empty()) return path;

        std::string result = path;

        // Expand ~ to home directory
        if (result[0] == '~') {
            const char* home = std::getenv("HOME");
            if (!home) {
                home = std::getenv("USERPROFILE");
            }
            if (home) {
                result = std::string(home) + result.substr(1);
            }
        }

        // Security: Validate and sanitize path to prevent traversal attacks
        // Convert to canonical form and check it's within allowed directories
        return sanitizePath(result);
    }

    static std::string sanitizePath(const std::string& path) {
        // Reject paths with null bytes (could bypass checks)
        if (path.find('\0') != std::string::npos) {
            throw std::runtime_error("Path contains null byte");
        }

        // Reject paths with suspicious patterns
        if (path.find("..") != std::string::npos) {
            throw std::runtime_error("Path contains directory traversal sequence");
        }

        // Normalize the path using filesystem
        try {
            fs::path normalized = fs::weakly_canonical(fs::path(path));
            std::string normalized_str = normalized.string();

            // Get the Lumen home directory
            std::string lumen_home;
            const char* home_env = std::getenv("LUMEN_HOME");
            if (home_env) {
                lumen_home = home_env;
            } else {
                const char* user_home = std::getenv("HOME");
                if (!user_home) {
                    user_home = std::getenv("USERPROFILE");
                }
                if (user_home) {
                    lumen_home = std::string(user_home) + "/.lumen";
                } else {
                    lumen_home = ".lumen";
                }
            }

            fs::path lumen_home_path = fs::weakly_canonical(fs::path(lumen_home));

            // SECURITY: Only allow paths within specific trusted directories
            // Do NOT include current working directory - this was a security vulnerability
            std::vector<fs::path> allowed_dirs = {
                lumen_home_path,
                fs::path("/etc/lumen"),
                fs::path("/usr/local/etc/lumen"),
            };

            // SECURITY FIX: Removed current working directory from whitelist
            // CWD is not a safe default as it could be set to an attacker-controlled location

            // Check if normalized path is within any allowed directory
            bool is_allowed = false;
            for (const auto& allowed_dir : allowed_dirs) {
                std::string allowed_str = allowed_dir.string();
                // Check if normalized_str starts with allowed_str
                if (normalized_str.length() >= allowed_str.length() &&
                    normalized_str.compare(0, allowed_str.length(), allowed_str) == 0) {
                    // Ensure it's actually a subdirectory (not just prefix match)
                    if (normalized_str.length() == allowed_str.length() ||
                        normalized_str[allowed_str.length()] == '/') {
                        is_allowed = true;
                        break;
                    }
                }
            }

            if (!is_allowed) {
                throw std::runtime_error("Path is outside allowed directories: " + path);
            }

            return normalized_str;
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Invalid path: " + std::string(e.what()));
        }
    }
};

// =============================================================================
// Configuration
// =============================================================================

Configuration::Configuration() : impl_(std::make_unique<Impl>()) {}

Configuration::Configuration(const std::string& config_path)
    : impl_(std::make_unique<Impl>()) {
    loadFromFile(config_path);
}

Configuration::~Configuration() = default;

void Configuration::loadFromFile(const std::string& filepath) {
    std::string resolved_path = resolvePath(filepath);
    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + resolved_path);
    }

    try {
#ifdef LUMEN_HAS_YAML
        if (resolved_path.ends_with(".yaml") || resolved_path.ends_with(".yml")) {
            YAML::Node yaml = YAML::LoadFile(resolved_path);

            if (yaml["solver"]) {
                auto solver = yaml["solver"];
                if (solver["classical"]) {
                    auto classical = solver["classical"];
                    if (classical["default"]) {
                        std::string val = classical["default"].as<std::string>();
                        if (val.length() <= 64) {  // Limit solver name length
                            impl_->solver_config_.default_classical_solver = val;
                        }
                    }
                    if (classical["timeout_ms"]) {
                        long timeout = classical["timeout_ms"].as<long>();
                        // Validate timeout range (1ms to 1 hour)
                        if (timeout > 0 && timeout <= 3600000) {
                            impl_->solver_config_.classical_timeout_ms = timeout;
                        }
                    }
                }
                if (solver["quantum"]) {
                    auto quantum = solver["quantum"];
                    if (quantum["enabled"]) {
                        impl_->solver_config_.quantum_enabled = quantum["enabled"].as<bool>();
                    }
                    if (quantum["provider"]) {
                        std::string val = quantum["provider"].as<std::string>();
                        if (val.length() <= 64) {  // Limit provider name length
                            impl_->solver_config_.quantum_provider = val;
                        }
                    }
                    if (quantum["timeout_ms"]) {
                        long timeout = quantum["timeout_ms"].as<long>();
                        if (timeout > 0 && timeout <= 3600000) {
                            impl_->solver_config_.quantum_timeout_ms = timeout;
                        }
                    }
                }
            }

            if (yaml["market_data"]) {
                auto md = yaml["market_data"];
                if (md["primary_provider"]) {
                    std::string val = md["primary_provider"].as<std::string>();
                    if (val.length() <= 64) {
                        impl_->market_data_config_.primary_provider = val;
                    }
                }
                if (md["cache_ttl_minutes"]) {
                    int ttl = md["cache_ttl_minutes"].as<int>();
                    // Validate TTL range (0 to 1 week in minutes)
                    if (ttl >= 0 && ttl <= 10080) {
                        impl_->market_data_config_.cache_ttl_minutes = ttl;
                    }
                }
            }

            if (yaml["persistence"]) {
                auto p = yaml["persistence"];
                if (p["database_path"]) {
                    std::string db_path = p["database_path"].as<std::string>();
                    // Validate and sanitize the path
                    if (db_path.length() <= 1024) {
                        try {
                            impl_->persistence_config_.database_path = Impl::expandPath(db_path);
                        } catch (const std::exception&) {
                            // Invalid path - use default
                        }
                    }
                }
                if (p["encryption_enabled"]) {
                    impl_->persistence_config_.encryption_enabled = p["encryption_enabled"].as<bool>();
                }
            }

            return;
        }
#endif
        // JSON parsing fallback
        nlohmann::json j;
        file >> j;

        if (j.contains("solver")) {
            auto& solver = j["solver"];
            if (solver.contains("classical")) {
                auto& classical = solver["classical"];
                if (classical.contains("default") && classical["default"].is_string()) {
                    std::string val = classical["default"].get<std::string>();
                    if (val.length() <= 64) {
                        impl_->solver_config_.default_classical_solver = val;
                    }
                }
                if (classical.contains("timeout_ms") && classical["timeout_ms"].is_number()) {
                    long timeout = classical["timeout_ms"].get<long>();
                    if (timeout > 0 && timeout <= 3600000) {
                        impl_->solver_config_.classical_timeout_ms = timeout;
                    }
                }
            }
            if (solver.contains("quantum")) {
                auto& quantum = solver["quantum"];
                if (quantum.contains("enabled") && quantum["enabled"].is_boolean()) {
                    impl_->solver_config_.quantum_enabled = quantum["enabled"].get<bool>();
                }
                if (quantum.contains("provider") && quantum["provider"].is_string()) {
                    std::string val = quantum["provider"].get<std::string>();
                    if (val.length() <= 64) {
                        impl_->solver_config_.quantum_provider = val;
                    }
                }
                if (quantum.contains("timeout_ms") && quantum["timeout_ms"].is_number()) {
                    long timeout = quantum["timeout_ms"].get<long>();
                    if (timeout > 0 && timeout <= 3600000) {
                        impl_->solver_config_.quantum_timeout_ms = timeout;
                    }
                }
            }
        }

        if (j.contains("market_data")) {
            auto& md = j["market_data"];
            if (md.contains("primary_provider") && md["primary_provider"].is_string()) {
                std::string val = md["primary_provider"].get<std::string>();
                if (val.length() <= 64) {
                    impl_->market_data_config_.primary_provider = val;
                }
            }
            if (md.contains("cache_ttl_minutes") && md["cache_ttl_minutes"].is_number()) {
                int ttl = md["cache_ttl_minutes"].get<int>();
                if (ttl >= 0 && ttl <= 10080) {
                    impl_->market_data_config_.cache_ttl_minutes = ttl;
                }
            }
        }

        if (j.contains("persistence")) {
            auto& p = j["persistence"];
            if (p.contains("database_path") && p["database_path"].is_string()) {
                std::string db_path = p["database_path"].get<std::string>();
                if (db_path.length() <= 1024) {
                    try {
                        impl_->persistence_config_.database_path = Impl::expandPath(db_path);
                    } catch (const std::exception&) {
                        // Invalid path - use default
                    }
                }
            }
            if (p.contains("encryption_enabled") && p["encryption_enabled"].is_boolean()) {
                impl_->persistence_config_.encryption_enabled = p["encryption_enabled"].get<bool>();
            }
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse config: " + std::string(e.what()));
    }
}

void Configuration::saveToFile(const std::string& filepath) const {
    std::string resolved_path = resolvePath(filepath);

    // Ensure parent directory exists
    fs::path path(resolved_path);
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }

    std::ofstream file(resolved_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file for writing: " + resolved_path);
    }

    nlohmann::json j;
    j["solver"]["classical"]["default"] = impl_->solver_config_.default_classical_solver;
    j["solver"]["classical"]["timeout_ms"] = impl_->solver_config_.classical_timeout_ms;
    j["solver"]["quantum"]["enabled"] = impl_->solver_config_.quantum_enabled;
    j["solver"]["quantum"]["provider"] = impl_->solver_config_.quantum_provider;
    j["solver"]["quantum"]["timeout_ms"] = impl_->solver_config_.quantum_timeout_ms;
    j["solver"]["quantum"]["num_reads"] = impl_->solver_config_.quantum_num_reads;
    j["solver"]["quantum"]["fallback_to_classical"] = impl_->solver_config_.quantum_fallback_to_classical;

    j["market_data"]["primary_provider"] = impl_->market_data_config_.primary_provider;
    j["market_data"]["fallback_providers"] = impl_->market_data_config_.fallback_providers;
    j["market_data"]["cache_ttl_minutes"] = impl_->market_data_config_.cache_ttl_minutes;
    j["market_data"]["history_days_default"] = impl_->market_data_config_.history_days_default;

    j["observability"]["pyflare_enabled"] = impl_->observability_config_.pyflare_enabled;
    j["observability"]["local_logging_enabled"] = impl_->observability_config_.local_logging_enabled;
    j["observability"]["log_level"] = logLevelToString(impl_->observability_config_.log_level);

    j["persistence"]["database_path"] = impl_->persistence_config_.database_path;
    j["persistence"]["encryption_enabled"] = impl_->persistence_config_.encryption_enabled;

    file << j.dump(2);
}

void Configuration::loadFromEnvironment() {
    // Market data API keys
    const char* av_key = std::getenv("ALPHA_VANTAGE_API_KEY");
    if (av_key && std::string(av_key).find("your_") == std::string::npos) {
        impl_->market_data_config_.primary_provider = "alpha_vantage";
    }

    // Quantum provider API keys
    const char* dwave_key = std::getenv("DWAVE_API_KEY");
    if (dwave_key && std::string(dwave_key).find("your_") == std::string::npos) {
        impl_->solver_config_.quantum_enabled = true;
        impl_->solver_config_.quantum_provider = "dwave";
    }

    const char* ibm_key = std::getenv("IBM_QUANTUM_API_KEY");
    if (ibm_key && std::string(ibm_key).find("your_") == std::string::npos) {
        impl_->solver_config_.quantum_enabled = true;
        impl_->solver_config_.quantum_provider = "ibm";
    }

    // Log level - validate input
    const char* log_level = std::getenv("LUMEN_LOG_LEVEL");
    if (log_level) {
        std::string level_str(log_level);
        // Only accept known log levels
        if (level_str == "trace" || level_str == "debug" || level_str == "info" ||
            level_str == "warn" || level_str == "error" || level_str == "fatal" ||
            level_str == "TRACE" || level_str == "DEBUG" || level_str == "INFO" ||
            level_str == "WARN" || level_str == "ERROR" || level_str == "FATAL") {
            impl_->observability_config_.log_level = logLevelFromString(log_level);
        }
        // Invalid values are silently ignored (use default)
    }

    // PyFlare
    const char* pyflare = std::getenv("LUMEN_PYFLARE_ENABLED");
    if (pyflare) {
        std::string val(pyflare);
        // Only accept explicit true/false values
        impl_->observability_config_.pyflare_enabled = (val == "true" || val == "1" || val == "yes");
    }

    // Database path - validate and sanitize
    const char* db_path = std::getenv("LUMEN_DB_PATH");
    if (db_path) {
        std::string path_str(db_path);
        // Limit path length
        if (path_str.length() <= 1024) {
            try {
                // Use the path sanitization to ensure it's within allowed directories
                std::string sanitized = Impl::expandPath(path_str);
                impl_->persistence_config_.database_path = sanitized;
            } catch (const std::exception&) {
                // Invalid path - ignore and use default
            }
        }
    }
}

const SolverConfig& Configuration::getSolverConfig() const {
    return impl_->solver_config_;
}

SolverConfig& Configuration::getSolverConfig() {
    return impl_->solver_config_;
}

const MarketDataConfig& Configuration::getMarketDataConfig() const {
    return impl_->market_data_config_;
}

MarketDataConfig& Configuration::getMarketDataConfig() {
    return impl_->market_data_config_;
}

const ObservabilityConfig& Configuration::getObservabilityConfig() const {
    return impl_->observability_config_;
}

ObservabilityConfig& Configuration::getObservabilityConfig() {
    return impl_->observability_config_;
}

const PersistenceConfig& Configuration::getPersistenceConfig() const {
    return impl_->persistence_config_;
}

PersistenceConfig& Configuration::getPersistenceConfig() {
    return impl_->persistence_config_;
}

std::string Configuration::getAPIKey(const std::string& env_var) const {
    const char* key = std::getenv(env_var.c_str());
    return key ? std::string(key) : "";
}

bool Configuration::hasAPIKey(const std::string& env_var) const {
    const char* key = std::getenv(env_var.c_str());
    if (!key) return false;
    std::string key_str(key);
    return !key_str.empty() && key_str.find("your_") == std::string::npos;
}

std::string Configuration::resolvePath(const std::string& path) {
    return Impl::expandPath(path);
}

std::string Configuration::getHomeDirectory() {
    const char* home_env = std::getenv("LUMEN_HOME");
    if (home_env) {
        return home_env;
    }
    const char* user_home = std::getenv("HOME");
    if (!user_home) {
        user_home = std::getenv("USERPROFILE");
    }
    if (user_home) {
        return std::string(user_home) + "/.lumen";
    }
    return ".lumen";
}

bool Configuration::validate() const {
    return getValidationErrors().empty();
}

std::vector<std::string> Configuration::getValidationErrors() const {
    std::vector<std::string> errors;

    if (impl_->solver_config_.classical_timeout_ms <= 0) {
        errors.push_back("Solver timeout must be positive");
    }

    if (impl_->solver_config_.quantum_enabled &&
        impl_->solver_config_.quantum_num_reads <= 0) {
        errors.push_back("Quantum num_reads must be positive when quantum is enabled");
    }

    if (impl_->market_data_config_.cache_ttl_minutes < 0) {
        errors.push_back("Cache TTL cannot be negative");
    }

    return errors;
}

std::optional<std::string> Configuration::getString(const std::string& key) const {
    auto it = impl_->custom_settings_.find(key);
    if (it != impl_->custom_settings_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<int> Configuration::getInt(const std::string& key) const {
    auto str = getString(key);
    if (!str) return std::nullopt;
    try {
        // Validate that string contains only valid integer characters
        const std::string& s = *str;
        if (s.empty()) return std::nullopt;
        size_t start = 0;
        if (s[0] == '-' || s[0] == '+') start = 1;
        for (size_t i = start; i < s.length(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
                return std::nullopt;
            }
        }
        size_t pos = 0;
        int value = std::stoi(s, &pos);
        if (pos != s.length()) return std::nullopt;  // Trailing characters
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> Configuration::getDouble(const std::string& key) const {
    auto str = getString(key);
    if (!str) return std::nullopt;
    try {
        // Validate that string contains only valid numeric characters
        const std::string& s = *str;
        if (s.empty()) return std::nullopt;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' &&
                c != '-' && c != '+' && c != 'e' && c != 'E') {
                return std::nullopt;
            }
        }
        size_t pos = 0;
        double value = std::stod(s, &pos);
        if (pos != s.length()) return std::nullopt;  // Trailing characters
        if (!std::isfinite(value)) return std::nullopt;  // Reject NaN/Infinity
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> Configuration::getBool(const std::string& key) const {
    auto str = getString(key);
    if (!str) return std::nullopt;
    return (*str == "true" || *str == "1" || *str == "yes");
}

void Configuration::set(const std::string& key, const std::string& value) {
    impl_->custom_settings_[key] = value;
}

}  // namespace lumen::utils
