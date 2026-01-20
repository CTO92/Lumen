/// @file provenance.cpp
/// @brief Provenance tracking implementation
///
/// Implementation of provenance tracking for explainable optimization results.

#include "lumen/explain/provenance.hpp"
#include "lumen/utils/logging.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace lumen::explain {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

std::string timePointToISO8601(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point iso8601ToTimePoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string constraintTypeToString(core::ConstraintType type) {
    switch (type) {
        case core::ConstraintType::BUDGET: return "budget";
        case core::ConstraintType::ALLOCATION: return "allocation";
        case core::ConstraintType::MIN_TRADE: return "min_trade";
        case core::ConstraintType::INTEGER_SHARES: return "integer_shares";
        case core::ConstraintType::TAX_LOT: return "tax_lot";
        case core::ConstraintType::WASH_SALE: return "wash_sale";
        case core::ConstraintType::POSITION_LIMIT: return "position_limit";
        case core::ConstraintType::SECTOR_LIMIT: return "sector_limit";
        case core::ConstraintType::CUSTOM: return "custom";
        default: return "unknown";
    }
}

core::ConstraintType constraintTypeFromString(const std::string& str) {
    if (str == "budget") return core::ConstraintType::BUDGET;
    if (str == "allocation") return core::ConstraintType::ALLOCATION;
    if (str == "min_trade") return core::ConstraintType::MIN_TRADE;
    if (str == "integer_shares") return core::ConstraintType::INTEGER_SHARES;
    if (str == "tax_lot") return core::ConstraintType::TAX_LOT;
    if (str == "wash_sale") return core::ConstraintType::WASH_SALE;
    if (str == "position_limit") return core::ConstraintType::POSITION_LIMIT;
    if (str == "sector_limit") return core::ConstraintType::SECTOR_LIMIT;
    return core::ConstraintType::CUSTOM;
}

std::string constraintStatusToString(core::ConstraintStatus status) {
    switch (status) {
        case core::ConstraintStatus::ACTIVE: return "active";
        case core::ConstraintStatus::BINDING: return "binding";
        case core::ConstraintStatus::SLACK: return "slack";
        case core::ConstraintStatus::VIOLATED: return "violated";
        case core::ConstraintStatus::DISABLED: return "disabled";
        default: return "unknown";
    }
}

core::ConstraintStatus constraintStatusFromString(const std::string& str) {
    if (str == "active") return core::ConstraintStatus::ACTIVE;
    if (str == "binding") return core::ConstraintStatus::BINDING;
    if (str == "slack") return core::ConstraintStatus::SLACK;
    if (str == "violated") return core::ConstraintStatus::VIOLATED;
    if (str == "disabled") return core::ConstraintStatus::DISABLED;
    return core::ConstraintStatus::ACTIVE;
}

}  // anonymous namespace

// =============================================================================
// DataSourceRecord
// =============================================================================

nlohmann::json DataSourceRecord::toJSON() const {
    nlohmann::json j{
        {"provider_name", provider_name},
        {"endpoint", endpoint},
        {"timestamp", timePointToISO8601(timestamp)},
        {"from_cache", from_cache},
        {"http_status", http_status},
        {"latency_ms", latency_ms}
    };

    // Add parameters (excluding any sensitive data)
    nlohmann::json params = nlohmann::json::object();
    for (const auto& [key, value] : parameters) {
        // Skip API keys or tokens
        if (key.find("key") == std::string::npos &&
            key.find("token") == std::string::npos &&
            key.find("secret") == std::string::npos) {
            params[key] = value;
        }
    }
    j["parameters"] = params;

    return j;
}

DataSourceRecord DataSourceRecord::fromJSON(const nlohmann::json& j) {
    DataSourceRecord record;
    record.provider_name = j.value("provider_name", "");
    record.endpoint = j.value("endpoint", "");
    record.timestamp = j.contains("timestamp") ?
        iso8601ToTimePoint(j["timestamp"].get<std::string>()) :
        std::chrono::system_clock::now();
    record.from_cache = j.value("from_cache", false);
    record.http_status = j.value("http_status", 0);
    record.latency_ms = j.value("latency_ms", 0L);

    if (j.contains("parameters") && j["parameters"].is_object()) {
        for (const auto& [key, value] : j["parameters"].items()) {
            if (value.is_string()) {
                record.parameters[key] = value.get<std::string>();
            }
        }
    }

    return record;
}

// =============================================================================
// ConstraintRecord
// =============================================================================

nlohmann::json ConstraintRecord::toJSON() const {
    return nlohmann::json{
        {"constraint_name", constraint_name},
        {"type", constraintTypeToString(type)},
        {"status", constraintStatusToString(status)},
        {"slack_value", slack_value},
        {"description", description}
    };
}

ConstraintRecord ConstraintRecord::fromJSON(const nlohmann::json& j) {
    ConstraintRecord record;
    record.constraint_name = j.value("constraint_name", "");
    record.type = constraintTypeFromString(j.value("type", "custom"));
    record.status = constraintStatusFromString(j.value("status", "active"));
    record.slack_value = j.value("slack_value", 0.0);
    record.description = j.value("description", "");
    return record;
}

// =============================================================================
// SolverRecord
// =============================================================================

nlohmann::json SolverRecord::toJSON() const {
    nlohmann::json j{
        {"solver_name", solver_name},
        {"solver_version", solver_version},
        {"solve_time_ms", solve_time_ms},
        {"iterations", iterations},
        {"termination_status", termination_status},
        {"objective_value", objective_value},
        {"optimality_gap", optimality_gap}
    };

    // Add solver options (excluding sensitive configuration)
    nlohmann::json opts = nlohmann::json::object();
    for (const auto& [key, value] : solver_options) {
        opts[key] = value;
    }
    j["solver_options"] = opts;

    return j;
}

SolverRecord SolverRecord::fromJSON(const nlohmann::json& j) {
    SolverRecord record;
    record.solver_name = j.value("solver_name", "");
    record.solver_version = j.value("solver_version", "");
    record.solve_time_ms = j.value("solve_time_ms", 0L);
    record.iterations = j.value("iterations", 0);
    record.termination_status = j.value("termination_status", "");
    record.objective_value = j.value("objective_value", 0.0);
    record.optimality_gap = j.value("optimality_gap", 0.0);

    if (j.contains("solver_options") && j["solver_options"].is_object()) {
        for (const auto& [key, value] : j["solver_options"].items()) {
            if (value.is_string()) {
                record.solver_options[key] = value.get<std::string>();
            }
        }
    }

    return record;
}

// =============================================================================
// Provenance Implementation
// =============================================================================

class Provenance::Impl {
public:
    std::vector<DataSourceRecord> data_sources;
    std::vector<ConstraintRecord> constraints;
    SolverRecord solver_record;
    std::vector<std::string> assumptions;
    std::vector<std::string> warnings;
    std::string session_id;
    std::chrono::system_clock::time_point run_timestamp;
};

Provenance::Provenance() : impl_(std::make_unique<Impl>()) {
    impl_->run_timestamp = std::chrono::system_clock::now();
}

Provenance::~Provenance() = default;

void Provenance::recordDataSource(const DataSourceRecord& record) {
    impl_->data_sources.push_back(record);
}

void Provenance::recordConstraint(const ConstraintRecord& record) {
    impl_->constraints.push_back(record);
}

void Provenance::recordSolver(const SolverRecord& record) {
    impl_->solver_record = record;
}

void Provenance::recordAssumption(const std::string& assumption) {
    impl_->assumptions.push_back(assumption);
}

void Provenance::recordWarning(const std::string& warning) {
    impl_->warnings.push_back(warning);
}

void Provenance::setRunTimestamp(const std::chrono::system_clock::time_point& ts) {
    impl_->run_timestamp = ts;
}

void Provenance::setSessionId(const std::string& id) {
    impl_->session_id = id;
}

std::vector<DataSourceRecord> Provenance::getDataSources() const {
    return impl_->data_sources;
}

std::vector<ConstraintRecord> Provenance::getConstraints() const {
    return impl_->constraints;
}

SolverRecord Provenance::getSolverInfo() const {
    return impl_->solver_record;
}

std::vector<std::string> Provenance::getAssumptions() const {
    return impl_->assumptions;
}

std::vector<std::string> Provenance::getWarnings() const {
    return impl_->warnings;
}

std::string Provenance::getSessionId() const {
    return impl_->session_id;
}

std::chrono::system_clock::time_point Provenance::getRunTimestamp() const {
    return impl_->run_timestamp;
}

std::vector<ConstraintRecord> Provenance::getActiveConstraints() const {
    std::vector<ConstraintRecord> active;
    for (const auto& c : impl_->constraints) {
        if (c.status == core::ConstraintStatus::ACTIVE ||
            c.status == core::ConstraintStatus::BINDING ||
            c.status == core::ConstraintStatus::SLACK) {
            active.push_back(c);
        }
    }
    return active;
}

std::vector<ConstraintRecord> Provenance::getBindingConstraints() const {
    std::vector<ConstraintRecord> binding;
    for (const auto& c : impl_->constraints) {
        if (c.status == core::ConstraintStatus::BINDING) {
            binding.push_back(c);
        }
    }
    return binding;
}

std::vector<ConstraintRecord> Provenance::getViolatedConstraints() const {
    std::vector<ConstraintRecord> violated;
    for (const auto& c : impl_->constraints) {
        if (c.status == core::ConstraintStatus::VIOLATED) {
            violated.push_back(c);
        }
    }
    return violated;
}

nlohmann::json Provenance::toJSON() const {
    // Data sources
    nlohmann::json data_sources_json = nlohmann::json::array();
    for (const auto& ds : impl_->data_sources) {
        data_sources_json.push_back(ds.toJSON());
    }

    // Constraints
    nlohmann::json constraints_json = nlohmann::json::array();
    for (const auto& c : impl_->constraints) {
        constraints_json.push_back(c.toJSON());
    }

    return nlohmann::json{
        {"session_id", impl_->session_id},
        {"run_timestamp", timePointToISO8601(impl_->run_timestamp)},
        {"data_sources", data_sources_json},
        {"constraints", constraints_json},
        {"solver", impl_->solver_record.toJSON()},
        {"assumptions", impl_->assumptions},
        {"warnings", impl_->warnings}
    };
}

Provenance Provenance::fromJSON(const nlohmann::json& j) {
    Provenance prov;

    prov.impl_->session_id = j.value("session_id", "");

    if (j.contains("run_timestamp")) {
        prov.impl_->run_timestamp =
            iso8601ToTimePoint(j["run_timestamp"].get<std::string>());
    }

    if (j.contains("data_sources") && j["data_sources"].is_array()) {
        for (const auto& ds : j["data_sources"]) {
            prov.impl_->data_sources.push_back(DataSourceRecord::fromJSON(ds));
        }
    }

    if (j.contains("constraints") && j["constraints"].is_array()) {
        for (const auto& c : j["constraints"]) {
            prov.impl_->constraints.push_back(ConstraintRecord::fromJSON(c));
        }
    }

    if (j.contains("solver")) {
        prov.impl_->solver_record = SolverRecord::fromJSON(j["solver"]);
    }

    if (j.contains("assumptions") && j["assumptions"].is_array()) {
        for (const auto& a : j["assumptions"]) {
            if (a.is_string()) {
                prov.impl_->assumptions.push_back(a.get<std::string>());
            }
        }
    }

    if (j.contains("warnings") && j["warnings"].is_array()) {
        for (const auto& w : j["warnings"]) {
            if (w.is_string()) {
                prov.impl_->warnings.push_back(w.get<std::string>());
            }
        }
    }

    return prov;
}

}  // namespace lumen::explain
