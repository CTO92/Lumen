#pragma once

/// @file config.hpp
/// @brief Configuration management
///
/// This module handles loading and managing application configuration
/// from YAML files and environment variables.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lumen/utils/logging.hpp"

namespace lumen::utils {

/// D-Wave specific configuration
struct DWaveConfig {
    std::string api_key_env = "DWAVE_API_KEY";       ///< Environment variable for API key
    std::string solver = "hybrid_binary_quadratic_model_version2";  ///< Solver name
    int num_reads = 1000;                            ///< Number of samples
    bool use_hybrid = true;                          ///< Use hybrid solver
    long timeout_ms = 60000;                         ///< Solver timeout
    int annealing_time_us = 20;                      ///< Annealing time in microseconds
};

/// IBM Quantum specific configuration
struct IBMQuantumConfig {
    std::string api_key_env = "IBM_QUANTUM_API_KEY"; ///< Environment variable for API key
    std::string backend = "ibm_brisbane";            ///< Default backend
    int num_shots = 1024;                            ///< Number of shots
    int qaoa_depth = 2;                              ///< QAOA circuit depth (p parameter)
    std::string optimizer = "SPSA";                  ///< Classical optimizer for QAOA
    long timeout_ms = 300000;                        ///< Solver timeout (5 minutes)
};

/// QUBO formulation configuration
struct QuboConfig {
    int precision_bits = 8;                          ///< Bits for continuous variable encoding
    bool auto_tune_penalty = true;                   ///< Auto-tune constraint penalties
    double penalty_factor = 10.0;                    ///< Multiplier for penalty tuning
    double base_penalty = 1000.0;                    ///< Base constraint penalty
};

/// Hybrid solver orchestration configuration
struct HybridConfig {
    int lot_threshold = 50;                          ///< Use hybrid above this many lots
    bool preprocess_enabled = true;                  ///< Enable classical preprocessing
    bool postprocess_enabled = true;                 ///< Enable classical postprocessing
    int max_iterations = 3;                          ///< Max classical-quantum iterations
    double improvement_threshold = 0.001;            ///< Stop if improvement below this
};

/// Complete quantum configuration
struct QuantumConfig {
    bool enabled = false;                            ///< Enable quantum solvers
    std::string provider = "dwave";                  ///< Default provider: dwave, ibm
    bool use_mock = false;                           ///< Use mock/simulator mode
    bool show_cost_estimate = true;                  ///< Show cost before solving
    double max_cost_per_solve = 0.0;                 ///< Max cost per solve (0 = no limit)

    DWaveConfig dwave;                               ///< D-Wave specific settings
    IBMQuantumConfig ibm;                            ///< IBM Quantum specific settings
    QuboConfig qubo;                                 ///< QUBO formulation settings
    HybridConfig hybrid;                             ///< Hybrid orchestration settings
};

/// Solver configuration settings
struct SolverConfig {
    std::string default_classical_solver = "highs";  ///< Default classical solver
    long classical_timeout_ms = 30000;               ///< Classical solver timeout
    bool quantum_fallback_to_classical = true;       ///< Fall back on quantum failure

    QuantumConfig quantum;                           ///< Quantum solver configuration
};

/// Market data configuration
struct MarketDataConfig {
    std::string primary_provider = "alpha_vantage";  ///< Primary data provider
    std::vector<std::string> fallback_providers = {"yahoo_finance"};  ///< Fallbacks
    int cache_ttl_minutes = 60;                      ///< Cache time-to-live
    int history_days_default = 252;                  ///< Default history period
};

/// Observability configuration
struct ObservabilityConfig {
    bool pyflare_enabled = true;                     ///< Enable PyFlare telemetry
    std::string pyflare_log_directory = "~/.lumen/logs/pyflare/";  ///< PyFlare log dir
    bool local_logging_enabled = true;               ///< Enable local logging
    std::string local_log_file = "~/.lumen/logs/lumen.log";  ///< Log file path
    size_t log_rotation_size_mb = 100;               ///< Log rotation threshold
    LogLevel log_level = LogLevel::INFO;             ///< Minimum log level
};

/// Persistence configuration
struct PersistenceConfig {
    std::string database_path = "~/.lumen/data/lumen.db";  ///< SQLite DB path
    bool encryption_enabled = true;                  ///< Encrypt sensitive data
};

/// Main configuration class
class Configuration {
public:
    /// Create with default settings
    Configuration();

    /// Load from file
    explicit Configuration(const std::string& config_path);

    ~Configuration();

    /// Load configuration from YAML file
    void loadFromFile(const std::string& filepath);

    /// Save current configuration to file
    void saveToFile(const std::string& filepath) const;

    /// Load settings from environment variables
    void loadFromEnvironment();

    /// Get solver configuration
    const SolverConfig& getSolverConfig() const;

    /// Get mutable solver configuration
    SolverConfig& getSolverConfig();

    /// Get market data configuration
    const MarketDataConfig& getMarketDataConfig() const;

    /// Get mutable market data configuration
    MarketDataConfig& getMarketDataConfig();

    /// Get observability configuration
    const ObservabilityConfig& getObservabilityConfig() const;

    /// Get mutable observability configuration
    ObservabilityConfig& getObservabilityConfig();

    /// Get persistence configuration
    const PersistenceConfig& getPersistenceConfig() const;

    /// Get mutable persistence configuration
    PersistenceConfig& getPersistenceConfig();

    /// Get API key from environment variable
    std::string getAPIKey(const std::string& env_var) const;

    /// Check if API key is set
    bool hasAPIKey(const std::string& env_var) const;

    /// Resolve path (expand ~ to home directory)
    static std::string resolvePath(const std::string& path);

    /// Get home directory for lumen (~/.lumen)
    static std::string getHomeDirectory();

    /// Validate configuration
    bool validate() const;

    /// Get validation errors
    std::vector<std::string> getValidationErrors() const;

    /// Get a custom string setting
    std::optional<std::string> getString(const std::string& key) const;

    /// Get a custom integer setting
    std::optional<int> getInt(const std::string& key) const;

    /// Get a custom double setting
    std::optional<double> getDouble(const std::string& key) const;

    /// Get a custom boolean setting
    std::optional<bool> getBool(const std::string& key) const;

    /// Set a custom setting
    void set(const std::string& key, const std::string& value);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lumen::utils
