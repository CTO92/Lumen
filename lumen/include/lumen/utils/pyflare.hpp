#pragma once

/// @file pyflare.hpp
/// @brief PyFlare telemetry integration
///
/// This module exports telemetry events to PyFlare-compatible JSON format
/// for monitoring and analysis.

#include <chrono>
#include <map>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

namespace lumen::utils {

/// Types of telemetry events
enum class TelemetryEventType {
    SOLVER_RUN,         ///< Optimization solver execution
    QUANTUM_API_CALL,   ///< Quantum API call
    MARKET_DATA_FETCH,  ///< Market data retrieval
    CONSTRAINT_CHECK,   ///< Constraint validation
    ERROR               ///< Error event
};

/// Convert TelemetryEventType to string
std::string telemetryEventTypeToString(TelemetryEventType type);

/// A telemetry event for PyFlare
struct TelemetryEvent {
    TelemetryEventType type;              ///< Event type
    std::string session_id;               ///< Session identifier
    std::chrono::system_clock::time_point timestamp;  ///< Event timestamp
    std::map<std::string, std::string> string_fields;  ///< String fields
    std::map<std::string, double> numeric_fields;      ///< Numeric fields
    std::map<std::string, bool> bool_fields;           ///< Boolean fields

    /// Convert to JSON
    nlohmann::json toJSON() const;

    /// Create from JSON
    static TelemetryEvent fromJSON(const nlohmann::json& j);
};

/// Exports telemetry events to PyFlare format
class PyFlareExporter {
public:
    /// Create exporter with output directory
    explicit PyFlareExporter(const std::string& log_directory);
    ~PyFlareExporter();

    /// Record a generic event
    void recordEvent(const TelemetryEvent& event);

    /// Record a solver run event
    void recordSolverRun(const std::string& session_id, const std::string& solver_name,
                         int num_positions, int num_constraints,
                         const std::string& problem_type, long solve_time_ms,
                         const std::string& status, double objective_value);

    /// Record a quantum API call event
    void recordQuantumAPICall(const std::string& session_id, const std::string& provider,
                              int num_qubits, long latency_ms, double qpu_time_us,
                              double estimated_cost, const std::string& status);

    /// Record a market data fetch event
    void recordMarketDataFetch(const std::string& session_id, const std::string& provider,
                               int num_tickers, long latency_ms, bool cache_hit);

    /// Record a constraint violation event
    void recordConstraintViolation(const std::string& session_id,
                                   const std::string& constraint_name,
                                   double violation_amount);

    /// Record an error event
    void recordError(const std::string& session_id, const std::string& error_type,
                     const std::string& message);

    /// Flush buffered events to disk
    void flush();

    /// Rotate log files
    void rotate();

    /// Get current log file path
    std::string getCurrentLogFile() const;

    /// Set maximum file size before rotation
    void setMaxFileSize(size_t bytes);

    /// Set buffer size (events before auto-flush)
    void setBufferSize(int events);

    /// Enable/disable exporting
    void setEnabled(bool enabled);

    /// Check if exporting is enabled
    bool isEnabled() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lumen::utils
