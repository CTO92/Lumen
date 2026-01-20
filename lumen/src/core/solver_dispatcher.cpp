/// @file solver_dispatcher.cpp
/// @brief Solver dispatcher implementation
///
/// Implementation of the solver dispatcher that routes optimization problems
/// to appropriate solvers based on problem characteristics.

#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/solvers/highs_wrapper.hpp"
#include "lumen/solvers/quantum_client.hpp"
#include "lumen/solvers/hybrid_orchestrator.hpp"
#include "lumen/utils/config.hpp"

#include <chrono>
#include <cmath>

namespace lumen::core {

// =============================================================================
// Utility functions
// =============================================================================

std::string problemTypeToString(ProblemType type) {
    switch (type) {
        case ProblemType::LP: return "LP";
        case ProblemType::MILP: return "MILP";
        case ProblemType::QP: return "QP";
        case ProblemType::QUBO: return "QUBO";
        default: return "unknown";
    }
}

std::string solverTierToString(SolverTier tier) {
    switch (tier) {
        case SolverTier::TIER_1: return "Tier 1 (Classical)";
        case SolverTier::TIER_2: return "Tier 2 (Hybrid)";
        case SolverTier::TIER_3: return "Tier 3 (Quantum)";
        default: return "unknown";
    }
}

std::string tradeActionToString(Trade::Action action) {
    switch (action) {
        case Trade::Action::BUY: return "buy";
        case Trade::Action::SELL: return "sell";
        case Trade::Action::HOLD: return "hold";
        default: return "unknown";
    }
}

// =============================================================================
// ProblemCharacteristics
// =============================================================================

nlohmann::json ProblemCharacteristics::toJSON() const {
    return nlohmann::json{
        {"num_positions", num_positions},
        {"num_constraints", num_constraints},
        {"num_variables", num_variables},
        {"num_integer_variables", num_integer_variables},
        {"problem_type", problemTypeToString(problem_type)},
        {"has_tax_optimization", has_tax_optimization},
        {"has_integer_constraints", has_integer_constraints},
        {"estimated_complexity", estimated_complexity},
        {"recommended_tier", solverTierToString(recommended_tier)}
    };
}

// =============================================================================
// Trade
// =============================================================================

nlohmann::json Trade::toJSON() const {
    nlohmann::json j = {
        {"ticker", ticker},
        {"action", tradeActionToString(action)},
        {"shares", shares},
        {"price", price},
        {"amount", amount},
        {"transaction_cost", transaction_cost},
        {"rationale", rationale}
    };

    if (!lot_ids.empty()) {
        j["lot_ids"] = lot_ids;
    }

    return j;
}

Trade Trade::fromJSON(const nlohmann::json& j) {
    Trade trade;

    // Validate and parse ticker
    if (!j.contains("ticker") || !j["ticker"].is_string()) {
        throw std::runtime_error("Trade::fromJSON: missing or invalid 'ticker' field");
    }
    trade.ticker = j.at("ticker").get<std::string>();
    if (trade.ticker.empty() || trade.ticker.length() > 20) {
        throw std::runtime_error("Trade::fromJSON: ticker must be 1-20 characters");
    }

    // Validate and parse action
    if (!j.contains("action") || !j["action"].is_string()) {
        throw std::runtime_error("Trade::fromJSON: missing or invalid 'action' field");
    }
    std::string action_str = j.at("action").get<std::string>();
    if (action_str == "buy") {
        trade.action = Trade::Action::BUY;
    } else if (action_str == "sell") {
        trade.action = Trade::Action::SELL;
    } else if (action_str == "hold") {
        trade.action = Trade::Action::HOLD;
    } else {
        throw std::runtime_error("Trade::fromJSON: invalid action (must be 'buy', 'sell', or 'hold')");
    }

    // Validate and parse shares
    if (!j.contains("shares") || !j["shares"].is_number()) {
        throw std::runtime_error("Trade::fromJSON: missing or invalid 'shares' field");
    }
    trade.shares = j.at("shares").get<double>();
    if (!std::isfinite(trade.shares) || trade.shares < 0) {
        throw std::runtime_error("Trade::fromJSON: shares must be a non-negative finite number");
    }
    if (trade.shares > 1e15) {
        throw std::runtime_error("Trade::fromJSON: shares exceeds reasonable limit");
    }

    // Parse optional price with validation
    if (j.contains("price")) {
        if (!j["price"].is_number()) {
            throw std::runtime_error("Trade::fromJSON: price must be a number");
        }
        trade.price = j["price"].get<double>();
        if (!std::isfinite(trade.price) || trade.price < 0) {
            throw std::runtime_error("Trade::fromJSON: price must be a non-negative finite number");
        }
        if (trade.price > 1e12) {
            throw std::runtime_error("Trade::fromJSON: price exceeds reasonable limit");
        }
    } else {
        trade.price = 0.0;
    }

    // Parse optional amount with validation
    if (j.contains("amount")) {
        if (!j["amount"].is_number()) {
            throw std::runtime_error("Trade::fromJSON: amount must be a number");
        }
        trade.amount = j["amount"].get<double>();
        if (!std::isfinite(trade.amount)) {
            throw std::runtime_error("Trade::fromJSON: amount must be finite");
        }
    } else {
        trade.amount = trade.shares * trade.price;
    }

    // Parse optional transaction_cost with validation
    if (j.contains("transaction_cost")) {
        if (!j["transaction_cost"].is_number()) {
            throw std::runtime_error("Trade::fromJSON: transaction_cost must be a number");
        }
        trade.transaction_cost = j["transaction_cost"].get<double>();
        if (!std::isfinite(trade.transaction_cost) || trade.transaction_cost < 0) {
            throw std::runtime_error("Trade::fromJSON: transaction_cost must be non-negative");
        }
    } else {
        trade.transaction_cost = 0.0;
    }

    // Parse optional rationale with length limit
    if (j.contains("rationale")) {
        if (!j["rationale"].is_string()) {
            throw std::runtime_error("Trade::fromJSON: rationale must be a string");
        }
        trade.rationale = j["rationale"].get<std::string>();
        if (trade.rationale.length() > 1000) {
            throw std::runtime_error("Trade::fromJSON: rationale too long (max 1000 chars)");
        }
    } else {
        trade.rationale = "";
    }

    // Parse optional lot_ids with validation
    if (j.contains("lot_ids")) {
        if (!j["lot_ids"].is_array()) {
            throw std::runtime_error("Trade::fromJSON: lot_ids must be an array");
        }
        if (j["lot_ids"].size() > 1000) {
            throw std::runtime_error("Trade::fromJSON: too many lot_ids (max 1000)");
        }
        for (const auto& id : j["lot_ids"]) {
            if (!id.is_string()) {
                throw std::runtime_error("Trade::fromJSON: lot_ids must contain strings");
            }
            std::string lot_id = id.get<std::string>();
            if (lot_id.length() > 100) {
                throw std::runtime_error("Trade::fromJSON: lot_id too long (max 100 chars)");
            }
            trade.lot_ids.push_back(lot_id);
        }
    }

    return trade;
}

// =============================================================================
// SolverResult
// =============================================================================

nlohmann::json SolverResult::toJSON() const {
    nlohmann::json trades_json = nlohmann::json::array();
    for (const auto& trade : trades) {
        trades_json.push_back(trade.toJSON());
    }

    return nlohmann::json{
        {"success", success},
        {"status", status},
        {"objective_value", objective_value},
        {"optimality_gap", optimality_gap},
        {"trades", trades_json},
        {"final_allocation", final_allocation},
        {"total_transaction_cost", total_transaction_cost},
        {"solver_used", solver_used},
        {"solve_time_ms", solve_time_ms},
        {"iterations", iterations},
        {"active_constraints", active_constraints},
        {"binding_constraints", binding_constraints}
    };
}

SolverResult SolverResult::fromJSON(const nlohmann::json& j) {
    SolverResult result;

    // Parse success (optional, defaults to false)
    if (j.contains("success")) {
        if (!j["success"].is_boolean()) {
            throw std::runtime_error("SolverResult::fromJSON: success must be a boolean");
        }
        result.success = j["success"].get<bool>();
    } else {
        result.success = false;
    }

    // Parse status with length limit
    if (j.contains("status")) {
        if (!j["status"].is_string()) {
            throw std::runtime_error("SolverResult::fromJSON: status must be a string");
        }
        result.status = j["status"].get<std::string>();
        if (result.status.length() > 100) {
            throw std::runtime_error("SolverResult::fromJSON: status too long");
        }
    } else {
        result.status = "unknown";
    }

    // Parse objective_value with validation
    if (j.contains("objective_value")) {
        if (!j["objective_value"].is_number()) {
            throw std::runtime_error("SolverResult::fromJSON: objective_value must be a number");
        }
        result.objective_value = j["objective_value"].get<double>();
        if (!std::isfinite(result.objective_value)) {
            throw std::runtime_error("SolverResult::fromJSON: objective_value must be finite");
        }
    } else {
        result.objective_value = 0.0;
    }

    // Parse optimality_gap with validation
    if (j.contains("optimality_gap")) {
        if (!j["optimality_gap"].is_number()) {
            throw std::runtime_error("SolverResult::fromJSON: optimality_gap must be a number");
        }
        result.optimality_gap = j["optimality_gap"].get<double>();
        if (!std::isfinite(result.optimality_gap) || result.optimality_gap < 0) {
            throw std::runtime_error("SolverResult::fromJSON: optimality_gap must be non-negative");
        }
    } else {
        result.optimality_gap = 0.0;
    }

    // Parse trades with size limit
    if (j.contains("trades")) {
        if (!j["trades"].is_array()) {
            throw std::runtime_error("SolverResult::fromJSON: trades must be an array");
        }
        constexpr size_t MAX_TRADES = 10000;
        if (j["trades"].size() > MAX_TRADES) {
            throw std::runtime_error("SolverResult::fromJSON: too many trades (max " +
                std::to_string(MAX_TRADES) + ")");
        }
        for (const auto& trade_json : j["trades"]) {
            result.trades.push_back(Trade::fromJSON(trade_json));
        }
    }

    // Parse final_allocation with size limit
    if (j.contains("final_allocation")) {
        if (!j["final_allocation"].is_object()) {
            throw std::runtime_error("SolverResult::fromJSON: final_allocation must be an object");
        }
        constexpr size_t MAX_ALLOCATIONS = 10000;
        if (j["final_allocation"].size() > MAX_ALLOCATIONS) {
            throw std::runtime_error("SolverResult::fromJSON: too many allocations");
        }
        for (auto& [key, value] : j["final_allocation"].items()) {
            if (key.length() > 100) {
                throw std::runtime_error("SolverResult::fromJSON: allocation key too long");
            }
            if (!value.is_number()) {
                throw std::runtime_error("SolverResult::fromJSON: allocation values must be numbers");
            }
            double alloc = value.get<double>();
            if (!std::isfinite(alloc)) {
                throw std::runtime_error("SolverResult::fromJSON: allocation values must be finite");
            }
            result.final_allocation[key] = alloc;
        }
    }

    // Parse total_transaction_cost with validation
    if (j.contains("total_transaction_cost")) {
        if (!j["total_transaction_cost"].is_number()) {
            throw std::runtime_error("SolverResult::fromJSON: total_transaction_cost must be a number");
        }
        result.total_transaction_cost = j["total_transaction_cost"].get<double>();
        if (!std::isfinite(result.total_transaction_cost) || result.total_transaction_cost < 0) {
            throw std::runtime_error("SolverResult::fromJSON: total_transaction_cost must be non-negative");
        }
    } else {
        result.total_transaction_cost = 0.0;
    }

    // Parse solver_used with length limit
    if (j.contains("solver_used")) {
        if (!j["solver_used"].is_string()) {
            throw std::runtime_error("SolverResult::fromJSON: solver_used must be a string");
        }
        result.solver_used = j["solver_used"].get<std::string>();
        if (result.solver_used.length() > 100) {
            throw std::runtime_error("SolverResult::fromJSON: solver_used too long");
        }
    } else {
        result.solver_used = "";
    }

    // Parse solve_time_ms with validation
    if (j.contains("solve_time_ms")) {
        if (!j["solve_time_ms"].is_number()) {
            throw std::runtime_error("SolverResult::fromJSON: solve_time_ms must be a number");
        }
        result.solve_time_ms = j["solve_time_ms"].get<long>();
        if (result.solve_time_ms < 0) {
            throw std::runtime_error("SolverResult::fromJSON: solve_time_ms cannot be negative");
        }
    } else {
        result.solve_time_ms = 0;
    }

    // Parse iterations with validation
    if (j.contains("iterations")) {
        if (!j["iterations"].is_number()) {
            throw std::runtime_error("SolverResult::fromJSON: iterations must be a number");
        }
        result.iterations = j["iterations"].get<int>();
        if (result.iterations < 0) {
            throw std::runtime_error("SolverResult::fromJSON: iterations cannot be negative");
        }
    } else {
        result.iterations = 0;
    }

    // Parse active_constraints with size limit
    if (j.contains("active_constraints")) {
        if (!j["active_constraints"].is_array()) {
            throw std::runtime_error("SolverResult::fromJSON: active_constraints must be an array");
        }
        if (j["active_constraints"].size() > 10000) {
            throw std::runtime_error("SolverResult::fromJSON: too many active_constraints");
        }
        for (const auto& c : j["active_constraints"]) {
            if (!c.is_string()) {
                throw std::runtime_error("SolverResult::fromJSON: active_constraints must contain strings");
            }
            std::string constraint_name = c.get<std::string>();
            if (constraint_name.length() > 200) {
                throw std::runtime_error("SolverResult::fromJSON: constraint name too long");
            }
            result.active_constraints.push_back(constraint_name);
        }
    }

    // Parse binding_constraints with size limit
    if (j.contains("binding_constraints")) {
        if (!j["binding_constraints"].is_array()) {
            throw std::runtime_error("SolverResult::fromJSON: binding_constraints must be an array");
        }
        if (j["binding_constraints"].size() > 10000) {
            throw std::runtime_error("SolverResult::fromJSON: too many binding_constraints");
        }
        for (const auto& c : j["binding_constraints"]) {
            if (!c.is_string()) {
                throw std::runtime_error("SolverResult::fromJSON: binding_constraints must contain strings");
            }
            std::string constraint_name = c.get<std::string>();
            if (constraint_name.length() > 200) {
                throw std::runtime_error("SolverResult::fromJSON: constraint name too long");
            }
            result.binding_constraints.push_back(constraint_name);
        }
    }

    return result;
}

// =============================================================================
// SolverDispatcher
// =============================================================================

SolverDispatcher::SolverDispatcher(const utils::Configuration& config)
    : classical_solver_(std::make_unique<solvers::HighsOptimizer>()) {
    // Configure from settings
    const auto& solver_config = config.getSolverConfig();
    classical_timeout_ms_ = solver_config.classical_timeout_ms;
    quantum_timeout_ms_ = solver_config.quantum_timeout_ms;
    quantum_enabled_ = solver_config.quantum_enabled;
    fallback_to_classical_ = solver_config.quantum_fallback_to_classical;
}

SolverDispatcher::~SolverDispatcher() = default;

ProblemCharacteristics SolverDispatcher::classifyProblem(
    const Portfolio& portfolio, const ConstraintSet& constraints) const {

    ProblemCharacteristics chars;

    chars.num_positions = static_cast<int>(portfolio.getPositionCount());

    // Count constraints
    chars.num_constraints = static_cast<int>(constraints.getConstraintCount());

    // Variables: 3 per position (buy, sell, drift)
    chars.num_variables = chars.num_positions * 3;

    // Check for integer constraints
    auto int_constraints = constraints.getConstraintsByType(ConstraintType::INTEGER_SHARES);
    chars.has_integer_constraints = !int_constraints.empty();

    if (chars.has_integer_constraints) {
        // Integer variables: 2 per position (buy, sell)
        chars.num_integer_variables = chars.num_positions * 2;
        chars.problem_type = ProblemType::MILP;
    } else {
        chars.num_integer_variables = 0;
        chars.problem_type = ProblemType::LP;
    }

    // Check for tax optimization
    auto tax_constraints = constraints.getConstraintsByType(ConstraintType::TAX_LOT);
    auto wash_constraints = constraints.getConstraintsByType(ConstraintType::WASH_SALE);
    chars.has_tax_optimization = !tax_constraints.empty() || !wash_constraints.empty();

    // Estimate complexity (heuristic)
    // Base complexity is O(n * m) for LP, much higher for MILP
    double base_complexity = static_cast<double>(chars.num_variables) *
                            static_cast<double>(chars.num_constraints);

    if (chars.has_integer_constraints) {
        // MILP can be exponential in worst case
        base_complexity *= std::pow(2.0, std::min(chars.num_integer_variables, 20));
    }

    if (chars.has_tax_optimization) {
        // Tax lot selection adds significant complexity
        base_complexity *= 10.0;
    }

    chars.estimated_complexity = base_complexity;

    // Determine recommended tier
    chars.recommended_tier = selectTier(chars);

    return chars;
}

SolverTier SolverDispatcher::selectTier(const ProblemCharacteristics& chars) const {
    // Tier selection based on problem characteristics
    // Tier 1: <20 positions, no tax optimization, classical solver
    // Tier 2: 20-50 positions, may benefit from hybrid approach
    // Tier 3: 50+ positions with tax optimization, quantum advantage

    if (chars.num_positions < 20 && !chars.has_tax_optimization) {
        return SolverTier::TIER_1;
    }

    if (chars.num_positions >= 50 && chars.has_tax_optimization) {
        return SolverTier::TIER_3;
    }

    if (chars.num_positions >= 20 || chars.has_tax_optimization) {
        return SolverTier::TIER_2;
    }

    return SolverTier::TIER_1;
}

std::string SolverDispatcher::selectSolver(SolverTier tier, bool quantum_available) const {
    switch (tier) {
        case SolverTier::TIER_1:
            return "HiGHS";

        case SolverTier::TIER_2:
            if (quantum_available && quantum_enabled_) {
                return "HiGHS+DWave";  // Hybrid approach
            }
            return "HiGHS";

        case SolverTier::TIER_3:
            if (quantum_available && quantum_enabled_) {
                return "DWave";
            }
            if (fallback_to_classical_) {
                return "HiGHS";  // Fallback
            }
            return "none";

        default:
            return "HiGHS";
    }
}

bool SolverDispatcher::isQuantumAvailable() const {
    return quantum_solver_ != nullptr && quantum_enabled_;
}

bool SolverDispatcher::isUserAuthorizedForQuantum() const {
    // In Phase 1, this is a placeholder
    // In later phases, this would check subscription status
    return quantum_enabled_;
}

SolverResult SolverDispatcher::dispatch(const Portfolio& portfolio,
                                        const TargetAllocation& target,
                                        const ConstraintSet& constraints) {
    // Classify the problem
    auto characteristics = classifyProblem(portfolio, constraints);

    // Select tier and solver
    SolverTier tier = characteristics.recommended_tier;
    std::string solver = selectSolver(tier, isQuantumAvailable());

    // Route to appropriate solving method
    SolverResult result;

    switch (tier) {
        case SolverTier::TIER_1:
            result = solveClassical(portfolio, target, constraints);
            break;

        case SolverTier::TIER_2:
            if (isQuantumAvailable()) {
                result = solveHybrid(portfolio, target, constraints);
            } else {
                result = solveClassical(portfolio, target, constraints);
            }
            break;

        case SolverTier::TIER_3:
            if (isQuantumAvailable()) {
                result = solveQuantum(portfolio, target, constraints);
            } else if (fallback_to_classical_) {
                result = solveClassical(portfolio, target, constraints);
            } else {
                result.success = false;
                result.status = "error";
                result.solver_used = "none";
            }
            break;
    }

    return result;
}

SolverResult SolverDispatcher::solveClassical(const Portfolio& portfolio,
                                              const TargetAllocation& target,
                                              const ConstraintSet& constraints) {
    if (!classical_solver_) {
        SolverResult result;
        result.success = false;
        result.status = "error";
        return result;
    }

    classical_solver_->setTimeout(classical_timeout_ms_);
    return classical_solver_->solve(portfolio, target, constraints);
}

SolverResult SolverDispatcher::solveQuantum(const Portfolio& portfolio,
                                            const TargetAllocation& target,
                                            const ConstraintSet& constraints) {
    // Use D-Wave quantum solver directly
    if (quantum_solver_) {
        quantum_solver_->setTimeout(quantum_timeout_ms_);
        auto result = quantum_solver_->solve(portfolio, target, constraints);

        if (!result.success && fallback_to_classical_) {
            // Quantum failed, fall back to classical
            return solveClassical(portfolio, target, constraints);
        }

        return result;
    }

    // No quantum solver configured
    if (fallback_to_classical_) {
        return solveClassical(portfolio, target, constraints);
    }

    SolverResult result;
    result.success = false;
    result.status = "error";
    result.solver_used = "quantum";
    return result;
}

SolverResult SolverDispatcher::solveHybrid(const Portfolio& portfolio,
                                           const TargetAllocation& target,
                                           const ConstraintSet& constraints) {
    // Use the hybrid orchestrator for classical-quantum-classical workflow
    solvers::HybridOrchestratorConfig hybrid_config;
    hybrid_config.lot_threshold = 50;
    hybrid_config.preprocess_enabled = true;
    hybrid_config.postprocess_enabled = true;
    hybrid_config.max_iterations = 3;

    // Configure quantum settings (use mock mode if no real quantum available)
    hybrid_config.quantum_config.provider = quantum_solver_ ? "dwave" : "mock";
    hybrid_config.quantum_config.num_reads = 100;
    hybrid_config.quantum_config.timeout_ms = quantum_timeout_ms_;

    solvers::HybridOrchestrator orchestrator(hybrid_config);

    auto result = orchestrator.solve(portfolio, target, constraints);

    if (!result.success && fallback_to_classical_) {
        // Hybrid failed, fall back to pure classical
        return solveClassical(portfolio, target, constraints);
    }

    return result;
}

void SolverDispatcher::setClassicalTimeout(long timeout_ms) {
    classical_timeout_ms_ = timeout_ms;
}

void SolverDispatcher::setQuantumTimeout(long timeout_ms) {
    quantum_timeout_ms_ = timeout_ms;
}

void SolverDispatcher::enableQuantum(bool enable) {
    quantum_enabled_ = enable;
}

void SolverDispatcher::setFallbackBehavior(bool fallback_to_classical) {
    fallback_to_classical_ = fallback_to_classical;
}

}  // namespace lumen::core
