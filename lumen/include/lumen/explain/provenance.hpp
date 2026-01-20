#pragma once

/// @file provenance.hpp
/// @brief Solution provenance and data source tracking
///
/// This module tracks the origin and dependencies of optimization solutions,
/// including data sources, solver choices, and constraint status.

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/core/constraint.hpp"

namespace lumen::explain {

/// Record of a data source used in optimization
struct DataSourceRecord {
    std::string provider_name;  ///< Data provider (e.g., "Alpha Vantage")
    std::string endpoint;       ///< API endpoint called
    std::chrono::system_clock::time_point timestamp;  ///< When data was fetched
    std::map<std::string, std::string> parameters;    ///< Query parameters (no API keys)
    bool from_cache;            ///< Whether data came from cache
    int http_status;            ///< HTTP response status
    long latency_ms;            ///< Request latency in milliseconds

    nlohmann::json toJSON() const;
    static DataSourceRecord fromJSON(const nlohmann::json& j);
};

/// Record of a constraint's status in the solution
struct ConstraintRecord {
    std::string constraint_name;    ///< Constraint identifier
    core::ConstraintType type;      ///< Type of constraint
    core::ConstraintStatus status;  ///< Status after solving
    double slack_value;             ///< Distance from binding (0 = binding)
    std::string description;        ///< Human-readable description

    nlohmann::json toJSON() const;
    static ConstraintRecord fromJSON(const nlohmann::json& j);
};

/// Record of solver execution
struct SolverRecord {
    std::string solver_name;     ///< Solver used (e.g., "HiGHS", "D-Wave")
    std::string solver_version;  ///< Solver version
    long solve_time_ms;          ///< Solve time in milliseconds
    int iterations;              ///< Number of iterations
    std::string termination_status;  ///< How solver terminated
    double objective_value;      ///< Final objective value
    double optimality_gap;       ///< Gap from optimal (for MILP)
    std::map<std::string, std::string> solver_options;  ///< Solver settings used

    nlohmann::json toJSON() const;
    static SolverRecord fromJSON(const nlohmann::json& j);
};

/// Tracks the complete provenance of an optimization solution
class Provenance {
public:
    Provenance();
    ~Provenance();

    /// Record a data source that was used
    void recordDataSource(const DataSourceRecord& record);

    /// Record a constraint's status
    void recordConstraint(const ConstraintRecord& record);

    /// Record solver execution details
    void recordSolver(const SolverRecord& record);

    /// Record an assumption made during optimization
    void recordAssumption(const std::string& assumption);

    /// Record a warning that occurred
    void recordWarning(const std::string& warning);

    /// Set the run timestamp
    void setRunTimestamp(const std::chrono::system_clock::time_point& ts);

    /// Set the session ID
    void setSessionId(const std::string& id);

    /// Get all data source records
    std::vector<DataSourceRecord> getDataSources() const;

    /// Get all constraint records
    std::vector<ConstraintRecord> getConstraints() const;

    /// Get solver information
    SolverRecord getSolverInfo() const;

    /// Get assumptions made
    std::vector<std::string> getAssumptions() const;

    /// Get warnings generated
    std::vector<std::string> getWarnings() const;

    /// Get session ID
    std::string getSessionId() const;

    /// Get run timestamp
    std::chrono::system_clock::time_point getRunTimestamp() const;

    /// Get constraints that were active
    std::vector<ConstraintRecord> getActiveConstraints() const;

    /// Get constraints that were binding
    std::vector<ConstraintRecord> getBindingConstraints() const;

    /// Get constraints that were violated
    std::vector<ConstraintRecord> getViolatedConstraints() const;

    /// Serialize to JSON
    nlohmann::json toJSON() const;

    /// Deserialize from JSON
    static Provenance fromJSON(const nlohmann::json& j);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lumen::explain
