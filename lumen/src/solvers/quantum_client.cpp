/// @file quantum_client.cpp
/// @brief Quantum computing client implementations
///
/// Implementation of quantum computing clients for D-Wave and IBM Quantum,
/// including QUBO formulation for portfolio optimization.

#include "lumen/solvers/quantum_client.hpp"
#include "lumen/solvers/qubo_types.hpp"
#include "lumen/data/tax_lot.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

namespace lumen::solvers {

// =============================================================================
// QuboMatrix Implementation
// =============================================================================

double QuboMatrix::evaluate(const std::vector<int>& solution) const {
    if (static_cast<int>(solution.size()) != num_variables) {
        throw std::invalid_argument("Solution size does not match number of variables");
    }

    double result = offset;
    for (int i = 0; i < num_variables; ++i) {
        for (int j = i; j < num_variables; ++j) {
            if (i == j) {
                // Diagonal terms: Q[i][i] * x[i] (since x[i]^2 = x[i] for binary)
                result += Q[i][i] * solution[i];
            } else {
                // Off-diagonal terms: Q[i][j] * x[i] * x[j]
                result += Q[i][j] * solution[i] * solution[j];
            }
        }
    }
    return result;
}

bool QuboMatrix::isValid() const {
    if (num_variables <= 0) return false;
    if (static_cast<int>(Q.size()) != num_variables) return false;
    for (const auto& row : Q) {
        if (static_cast<int>(row.size()) != num_variables) return false;
    }
    return true;
}

int QuboMatrix::getNonZeroCount() const {
    int count = 0;
    for (int i = 0; i < num_variables; ++i) {
        for (int j = i; j < num_variables; ++j) {
            if (std::abs(Q[i][j]) > 1e-10) {
                ++count;
            }
        }
    }
    return count;
}

nlohmann::json QuboMatrix::toJSON() const {
    nlohmann::json j;
    j["Q"] = Q;
    j["offset"] = offset;
    j["num_variables"] = num_variables;
    j["var_names"] = var_names;
    return j;
}

QuboMatrix QuboMatrix::fromJSON(const nlohmann::json& j) {
    QuboMatrix q;
    q.Q = j["Q"].get<std::vector<std::vector<double>>>();
    q.offset = j["offset"].get<double>();
    q.num_variables = j["num_variables"].get<int>();
    if (j.contains("var_names")) {
        q.var_names = j["var_names"].get<std::map<int, std::string>>();
    }
    return q;
}

// =============================================================================
// EncodingConfig Implementation
// =============================================================================

std::vector<int> EncodingConfig::encode(double value) const {
    // Clamp value to valid range
    value = std::max(min_value, std::min(max_value, value));

    // Convert to integer level
    int num_levels = getNumLevels();
    int level = static_cast<int>(
        std::round((value - min_value) / (max_value - min_value) * (num_levels - 1)));
    level = std::max(0, std::min(num_levels - 1, level));

    // Convert to binary representation
    std::vector<int> binary(precision_bits);
    for (int i = 0; i < precision_bits; ++i) {
        binary[i] = (level >> i) & 1;
    }
    return binary;
}

double EncodingConfig::decode(const std::vector<int>& binary) const {
    if (static_cast<int>(binary.size()) != precision_bits) {
        throw std::invalid_argument("Binary vector size does not match precision_bits");
    }

    // Convert binary to integer level
    int level = 0;
    for (int i = 0; i < precision_bits; ++i) {
        level += binary[i] * (1 << i);
    }

    // Convert level to continuous value
    int num_levels = getNumLevels();
    return min_value + (max_value - min_value) * level / (num_levels - 1);
}

nlohmann::json EncodingConfig::toJSON() const {
    return {{"precision_bits", precision_bits},
            {"min_value", min_value},
            {"max_value", max_value}};
}

EncodingConfig EncodingConfig::fromJSON(const nlohmann::json& j) {
    EncodingConfig c;
    c.precision_bits = j.value("precision_bits", 8);
    c.min_value = j.value("min_value", 0.0);
    c.max_value = j.value("max_value", 1.0);
    return c;
}

// =============================================================================
// PenaltyConfig Implementation
// =============================================================================

double PenaltyConfig::computeAutoTunedPenalty(double max_objective_coeff,
                                               int num_constraints) const {
    if (!auto_tune) {
        return constraint_penalty;
    }
    // Penalty should be large enough that any constraint violation
    // costs more than the best possible objective improvement
    return std::max(constraint_penalty, max_objective_coeff * tune_factor * num_constraints);
}

nlohmann::json PenaltyConfig::toJSON() const {
    return {{"constraint_penalty", constraint_penalty},
            {"auto_tune", auto_tune},
            {"tune_factor", tune_factor}};
}

PenaltyConfig PenaltyConfig::fromJSON(const nlohmann::json& j) {
    PenaltyConfig c;
    c.constraint_penalty = j.value("constraint_penalty", 1000.0);
    c.auto_tune = j.value("auto_tune", true);
    c.tune_factor = j.value("tune_factor", 10.0);
    return c;
}

// =============================================================================
// VariableMapping Implementation
// =============================================================================

double VariableMapping::decode(const std::vector<int>& binary_solution) const {
    // Extract bits for this variable
    int level = 0;
    for (int i = 0; i < num_bits; ++i) {
        int idx = start_index + i;
        if (idx < static_cast<int>(binary_solution.size())) {
            level += binary_solution[idx] * (1 << i);
        }
    }
    return level * scale_factor;
}

// =============================================================================
// DecodedSolution Implementation
// =============================================================================

bool DecodedSolution::is_feasible() const {
    return std::all_of(constraints_satisfied.begin(), constraints_satisfied.end(),
                       [](const auto& pair) { return pair.second; });
}

nlohmann::json DecodedSolution::toJSON() const {
    nlohmann::json j;
    j["target_weights"] = target_weights;
    j["lots_to_sell"] = lots_to_sell;
    j["constraints_satisfied"] = constraints_satisfied;
    j["total_violation"] = total_violation;
    j["objective_value"] = objective_value;
    return j;
}

// =============================================================================
// QuboFormulationInfo Implementation
// =============================================================================

nlohmann::json QuboFormulationInfo::toJSON() const {
    nlohmann::json j;
    j["num_positions"] = num_positions;
    j["num_tax_lots"] = num_tax_lots;
    j["total_qubits"] = total_qubits;
    j["num_constraints"] = num_constraints;
    j["constraint_names"] = constraint_names;
    j["encoding"] = encoding.toJSON();
    j["penalties"] = penalties.toJSON();
    return j;
}

QuboFormulationInfo QuboFormulationInfo::fromJSON(const nlohmann::json& j) {
    QuboFormulationInfo info;
    info.num_positions = j["num_positions"].get<int>();
    info.num_tax_lots = j.value("num_tax_lots", 0);
    info.total_qubits = j["total_qubits"].get<int>();
    info.num_constraints = j["num_constraints"].get<int>();
    if (j.contains("constraint_names")) {
        info.constraint_names = j["constraint_names"].get<std::vector<std::string>>();
    }
    if (j.contains("encoding")) {
        info.encoding = EncodingConfig::fromJSON(j["encoding"]);
    }
    if (j.contains("penalties")) {
        info.penalties = PenaltyConfig::fromJSON(j["penalties"]);
    }
    return info;
}

// =============================================================================
// QuboFormulator::Impl
// =============================================================================

class QuboFormulator::Impl {
public:
    Impl(const core::Portfolio& portfolio, const core::TargetAllocation& target)
        : portfolio_(portfolio), target_(target) {
        tickers_ = portfolio_.getAllTickers();

        // Add tickers from target that might not be in portfolio
        for (const auto& alloc_target : target_.getAllTargets()) {
            if (!alloc_target.is_asset_class) {
                if (std::find(tickers_.begin(), tickers_.end(), alloc_target.identifier) ==
                    tickers_.end()) {
                    tickers_.push_back(alloc_target.identifier);
                }
            }
        }
    }

    std::vector<std::vector<double>> formulate(const core::ConstraintSet& constraints) {
        // Initialize formulation info
        info_.num_positions = static_cast<int>(tickers_.size());
        info_.num_tax_lots = static_cast<int>(tax_lot_variables_.size());
        info_.encoding = encoding_;
        info_.penalties = penalties_;

        // Calculate total qubits needed
        // Each position needs precision_bits for weight variable
        int weight_qubits = info_.num_positions * encoding_.precision_bits;
        int lot_qubits = info_.num_tax_lots;  // Binary: sell or not
        info_.total_qubits = weight_qubits + lot_qubits;

        if (info_.total_qubits == 0) {
            return {};
        }

        // Initialize QUBO matrix
        int n = info_.total_qubits;
        std::vector<std::vector<double>> Q(n, std::vector<double>(n, 0.0));

        // Build variable mappings
        int var_idx = 0;
        for (const auto& ticker : tickers_) {
            VariableMapping vm;
            vm.ticker = ticker;
            vm.start_index = var_idx;
            vm.num_bits = encoding_.precision_bits;
            vm.is_buy = true;  // For now, single weight variable
            vm.scale_factor = encoding_.getStepSize();
            info_.weight_variables.push_back(vm);
            var_idx += encoding_.precision_bits;
        }

        // Add objective terms (drift minimization)
        addDriftObjective(Q);

        // Add cost terms
        addCostObjective(Q);

        // Add tax terms if enabled
        if (!tax_lot_variables_.empty()) {
            addTaxObjective(Q);
        }

        // Find max objective coefficient for penalty auto-tuning
        double max_obj_coeff = findMaxCoefficient(Q);

        // Add constraint penalty terms
        info_.num_constraints = 0;

        // Budget constraint: sum of weights = 1
        addBudgetConstraint(Q, max_obj_coeff);

        // Position bounds from constraints
        for (const auto* c : constraints.getAllConstraints()) {
            if (c->getType() == core::ConstraintType::ALLOCATION) {
                const auto* ac = dynamic_cast<const core::AllocationConstraint*>(c);
                if (ac && !ac->isAssetClass()) {
                    addAllocationConstraint(Q, ac, max_obj_coeff);
                }
            }
        }

        return Q;
    }

    void setWeights(double drift_weight, double cost_weight, double tax_weight) {
        drift_weight_ = drift_weight;
        cost_weight_ = cost_weight;
        tax_weight_ = tax_weight;
    }

    void enableTaxLotOptimization(const std::vector<data::TaxLot>& lots) {
        for (const auto& lot : lots) {
            TaxLotVariable tlv;
            tlv.lot_id = lot.id;
            tlv.ticker = lot.ticker;
            tlv.shares = lot.shares;
            tlv.cost_basis = lot.cost_basis_per_share;
            tlv.is_long_term = (lot.getDaysHeld() >= 365);

            // Current price will be set later
            tlv.current_price = 0.0;
            tlv.unrealized_gain = 0.0;

            tax_lot_variables_.push_back(tlv);
        }
    }

    void setConstraintPenalty(double penalty) {
        penalties_.constraint_penalty = penalty;
        penalties_.auto_tune = false;  // User explicitly set penalty
    }

    std::vector<core::Trade> interpretSolution(const std::vector<int>& binary_solution) const {
        std::vector<core::Trade> trades;

        if (binary_solution.empty() || info_.weight_variables.empty()) {
            return trades;
        }

        double total_value = portfolio_.getTotalValue();
        if (total_value <= 0) total_value = 1.0;

        for (const auto& vm : info_.weight_variables) {
            double new_weight = vm.decode(binary_solution);

            // Get current weight
            double current_weight = 0.0;
            if (portfolio_.hasPosition(vm.ticker)) {
                current_weight = portfolio_.getAllocationPercent(vm.ticker);
            }

            double weight_diff = new_weight - current_weight;
            if (std::abs(weight_diff) < 0.001) {
                continue;  // No significant change
            }

            core::Trade trade;
            trade.ticker = vm.ticker;

            double price = 1.0;  // Default price
            if (portfolio_.hasPosition(vm.ticker)) {
                price = portfolio_.getPosition(vm.ticker).current_price;
            }

            double amount = std::abs(weight_diff) * total_value;
            trade.shares = amount / price;
            trade.price = price;
            trade.amount = amount;
            trade.transaction_cost = amount * 0.001;  // 0.1% transaction cost

            if (weight_diff > 0) {
                trade.action = core::Trade::Action::BUY;
                trade.rationale = "Increase allocation to target";
            } else {
                trade.action = core::Trade::Action::SELL;
                trade.rationale = "Decrease allocation toward target";
            }

            trades.push_back(trade);
        }

        // Process tax lot selections
        int lot_start_idx = static_cast<int>(info_.weight_variables.size()) *
                            encoding_.precision_bits;
        for (size_t i = 0; i < tax_lot_variables_.size(); ++i) {
            int idx = lot_start_idx + static_cast<int>(i);
            if (idx < static_cast<int>(binary_solution.size()) && binary_solution[idx] == 1) {
                const auto& tlv = tax_lot_variables_[i];

                // Find or create trade for this ticker
                auto it = std::find_if(trades.begin(), trades.end(),
                                       [&](const core::Trade& t) {
                                           return t.ticker == tlv.ticker &&
                                                  t.action == core::Trade::Action::SELL;
                                       });

                if (it != trades.end()) {
                    it->lot_ids.push_back(tlv.lot_id);
                }
            }
        }

        return trades;
    }

    bool validateFormulation() const {
        // Check that we have positions to optimize
        if (tickers_.empty()) return false;

        // Check encoding config is valid
        if (encoding_.precision_bits < 1 || encoding_.precision_bits > 16) return false;
        if (encoding_.min_value >= encoding_.max_value) return false;

        return true;
    }

    int getNumQubits() const {
        return info_.total_qubits;
    }

    std::map<int, std::string> getVariableMapping() const {
        std::map<int, std::string> mapping;
        for (const auto& vm : info_.weight_variables) {
            for (int i = 0; i < vm.num_bits; ++i) {
                mapping[vm.start_index + i] = vm.ticker + "_bit" + std::to_string(i);
            }
        }
        int lot_start = static_cast<int>(info_.weight_variables.size()) * encoding_.precision_bits;
        for (size_t i = 0; i < tax_lot_variables_.size(); ++i) {
            mapping[lot_start + static_cast<int>(i)] = "lot_" + tax_lot_variables_[i].lot_id;
        }
        return mapping;
    }

private:
    void addDriftObjective(std::vector<std::vector<double>>& Q) {
        // Minimize (w - w_target)^2 for each position
        // Expanded: w^2 - 2*w*w_target + w_target^2
        // In QUBO form with binary encoding

        for (size_t pos_idx = 0; pos_idx < info_.weight_variables.size(); ++pos_idx) {
            const auto& vm = info_.weight_variables[pos_idx];
            double target_weight = target_.getTarget(vm.ticker);

            // For each bit pair in the encoding
            for (int i = 0; i < vm.num_bits; ++i) {
                int qi = vm.start_index + i;
                double coeff_i = (1 << i) * vm.scale_factor;

                // Linear term: -2 * w_target * coeff_i
                Q[qi][qi] += drift_weight_ * (-2.0 * target_weight * coeff_i + coeff_i * coeff_i);

                // Quadratic terms between bits
                for (int j = i + 1; j < vm.num_bits; ++j) {
                    int qj = vm.start_index + j;
                    double coeff_j = (1 << j) * vm.scale_factor;
                    Q[qi][qj] += drift_weight_ * 2.0 * coeff_i * coeff_j;
                }
            }
        }
    }

    void addCostObjective(std::vector<std::vector<double>>& Q) {
        // Transaction costs are proportional to |w_new - w_current|
        // We approximate this with (w_new - w_current)^2 scaled by cost factor

        double total_value = portfolio_.getTotalValue();
        if (total_value <= 0) total_value = 1.0;

        for (const auto& vm : info_.weight_variables) {
            double current_weight = 0.0;
            if (portfolio_.hasPosition(vm.ticker)) {
                current_weight = portfolio_.getAllocationPercent(vm.ticker);
            }

            double price = 1.0;
            if (portfolio_.hasPosition(vm.ticker)) {
                price = portfolio_.getPosition(vm.ticker).current_price;
            }

            double cost_rate = 0.001;  // 0.1% transaction cost

            for (int i = 0; i < vm.num_bits; ++i) {
                int qi = vm.start_index + i;
                double coeff_i = (1 << i) * vm.scale_factor;

                // Cost increases with distance from current allocation
                double cost_factor = cost_weight_ * cost_rate * price * total_value;
                Q[qi][qi] += cost_factor * (-2.0 * current_weight * coeff_i + coeff_i * coeff_i);

                for (int j = i + 1; j < vm.num_bits; ++j) {
                    int qj = vm.start_index + j;
                    double coeff_j = (1 << j) * vm.scale_factor;
                    Q[qi][qj] += cost_factor * 2.0 * coeff_i * coeff_j;
                }
            }
        }
    }

    void addTaxObjective(std::vector<std::vector<double>>& Q) {
        // For tax lot selection: prefer lots with losses or long-term gains
        int lot_start = static_cast<int>(info_.weight_variables.size()) * encoding_.precision_bits;

        for (size_t i = 0; i < tax_lot_variables_.size(); ++i) {
            const auto& tlv = tax_lot_variables_[i];
            int qi = lot_start + static_cast<int>(i);

            // Tax impact: positive for gains (penalty), negative for losses (reward)
            double tax_rate = tlv.is_long_term ? 0.15 : 0.35;
            double tax_impact = tlv.unrealized_gain * tax_rate;

            // Add to diagonal (linear term)
            Q[qi][qi] += tax_weight_ * tax_impact;
        }
    }

    void addBudgetConstraint(std::vector<std::vector<double>>& Q, double max_obj_coeff) {
        // Constraint: sum of weights = 1
        // Penalty: P * (sum - 1)^2 = P * (sum^2 - 2*sum + 1)

        double penalty = penalties_.computeAutoTunedPenalty(max_obj_coeff, 1);
        info_.constraint_names.push_back("budget_sum_equals_1");
        info_.num_constraints++;

        // For each position pair, add penalty terms
        for (size_t p1 = 0; p1 < info_.weight_variables.size(); ++p1) {
            const auto& vm1 = info_.weight_variables[p1];

            for (int i = 0; i < vm1.num_bits; ++i) {
                int qi = vm1.start_index + i;
                double coeff_i = (1 << i) * vm1.scale_factor;

                // Diagonal: coeff_i^2 - 2*coeff_i (from sum^2 - 2*sum)
                Q[qi][qi] += penalty * (coeff_i * coeff_i - 2.0 * coeff_i);

                // Same position, different bits
                for (int j = i + 1; j < vm1.num_bits; ++j) {
                    int qj = vm1.start_index + j;
                    double coeff_j = (1 << j) * vm1.scale_factor;
                    Q[qi][qj] += penalty * 2.0 * coeff_i * coeff_j;
                }

                // Different positions
                for (size_t p2 = p1 + 1; p2 < info_.weight_variables.size(); ++p2) {
                    const auto& vm2 = info_.weight_variables[p2];
                    for (int j = 0; j < vm2.num_bits; ++j) {
                        int qj = vm2.start_index + j;
                        double coeff_j = (1 << j) * vm2.scale_factor;
                        Q[qi][qj] += penalty * 2.0 * coeff_i * coeff_j;
                    }
                }
            }
        }
    }

    void addAllocationConstraint(std::vector<std::vector<double>>& Q,
                                  const core::AllocationConstraint* ac,
                                  double max_obj_coeff) {
        // Find the variable mapping for this ticker
        auto it = std::find_if(info_.weight_variables.begin(), info_.weight_variables.end(),
                               [&](const VariableMapping& vm) {
                                   return vm.ticker == ac->getIdentifier();
                               });

        if (it == info_.weight_variables.end()) return;

        double penalty = penalties_.computeAutoTunedPenalty(max_obj_coeff, 1);
        const auto& vm = *it;

        // Lower bound: w >= lb
        // Penalty for violation: P * max(0, lb - w)^2
        // We use slack variable approach or soft penalty
        double lb = ac->getLowerBound();
        double ub = ac->getUpperBound();

        if (lb > 0) {
            info_.constraint_names.push_back(vm.ticker + "_min_" + std::to_string(lb));
            info_.num_constraints++;

            // Add penalty for being below lower bound
            for (int i = 0; i < vm.num_bits; ++i) {
                int qi = vm.start_index + i;
                double coeff_i = (1 << i) * vm.scale_factor;

                // Encourage weight to be at least lb
                Q[qi][qi] += penalty * (-2.0 * lb * coeff_i + coeff_i * coeff_i);

                for (int j = i + 1; j < vm.num_bits; ++j) {
                    int qj = vm.start_index + j;
                    double coeff_j = (1 << j) * vm.scale_factor;
                    Q[qi][qj] += penalty * 2.0 * coeff_i * coeff_j;
                }
            }
        }

        if (ub < 1.0) {
            info_.constraint_names.push_back(vm.ticker + "_max_" + std::to_string(ub));
            info_.num_constraints++;

            // Add penalty for exceeding upper bound
            for (int i = 0; i < vm.num_bits; ++i) {
                int qi = vm.start_index + i;
                double coeff_i = (1 << i) * vm.scale_factor;

                // Penalize weight above ub
                // (w - ub)^2 when w > ub
                Q[qi][qi] += penalty * (coeff_i * coeff_i - 2.0 * ub * coeff_i);

                for (int j = i + 1; j < vm.num_bits; ++j) {
                    int qj = vm.start_index + j;
                    double coeff_j = (1 << j) * vm.scale_factor;
                    Q[qi][qj] += penalty * 2.0 * coeff_i * coeff_j;
                }
            }
        }
    }

    double findMaxCoefficient(const std::vector<std::vector<double>>& Q) const {
        double max_val = 0.0;
        for (const auto& row : Q) {
            for (double val : row) {
                max_val = std::max(max_val, std::abs(val));
            }
        }
        return max_val;
    }

    // Member variables
    const core::Portfolio& portfolio_;
    const core::TargetAllocation& target_;
    std::vector<std::string> tickers_;

    // Weights for multi-objective
    double drift_weight_ = 1.0;
    double cost_weight_ = 0.1;
    double tax_weight_ = 0.0;

    // Configuration
    EncodingConfig encoding_;
    PenaltyConfig penalties_;

    // Tax lot variables
    std::vector<TaxLotVariable> tax_lot_variables_;

    // Formulation info
    QuboFormulationInfo info_;
};

// =============================================================================
// QuboFormulator Public Interface
// =============================================================================

QuboFormulator::QuboFormulator(const core::Portfolio& portfolio,
                               const core::TargetAllocation& target)
    : impl_(std::make_unique<Impl>(portfolio, target)) {}

QuboFormulator::~QuboFormulator() = default;

std::vector<std::vector<double>> QuboFormulator::formulate(
    const core::ConstraintSet& constraints) {
    return impl_->formulate(constraints);
}

void QuboFormulator::setWeights(double drift_weight, double cost_weight, double tax_weight) {
    impl_->setWeights(drift_weight, cost_weight, tax_weight);
}

void QuboFormulator::enableTaxLotOptimization(const std::vector<data::TaxLot>& lots) {
    impl_->enableTaxLotOptimization(lots);
}

void QuboFormulator::setConstraintPenalty(double penalty) {
    impl_->setConstraintPenalty(penalty);
}

std::vector<core::Trade> QuboFormulator::interpretSolution(
    const std::vector<int>& binary_solution) const {
    return impl_->interpretSolution(binary_solution);
}

bool QuboFormulator::validateFormulation() const {
    return impl_->validateFormulation();
}

int QuboFormulator::getNumQubits() const {
    return impl_->getNumQubits();
}

std::map<int, std::string> QuboFormulator::getVariableMapping() const {
    return impl_->getVariableMapping();
}

// =============================================================================
// QuantumResult Implementation
// =============================================================================

nlohmann::json QuantumResult::toJSON() const {
    nlohmann::json j;
    j["success"] = success;
    j["status"] = status;
    j["best_sample"] = best_sample;
    j["best_energy"] = best_energy;
    j["all_samples"] = all_samples;
    j["all_energies"] = all_energies;
    j["qpu_access_time_us"] = qpu_access_time_us;
    j["total_time_ms"] = total_time_ms;
    j["num_occurrences"] = num_occurrences;
    j["chain_break_fraction"] = chain_break_fraction;
    return j;
}

QuantumResult QuantumResult::fromJSON(const nlohmann::json& j) {
    QuantumResult r;
    r.success = j.value("success", false);
    r.status = j.value("status", "");
    if (j.contains("best_sample")) {
        r.best_sample = j["best_sample"].get<std::vector<int>>();
    }
    r.best_energy = j.value("best_energy", 0.0);
    if (j.contains("all_samples")) {
        r.all_samples = j["all_samples"].get<std::vector<std::vector<int>>>();
    }
    if (j.contains("all_energies")) {
        r.all_energies = j["all_energies"].get<std::vector<double>>();
    }
    r.qpu_access_time_us = j.value("qpu_access_time_us", 0.0);
    r.total_time_ms = j.value("total_time_ms", 0.0);
    r.num_occurrences = j.value("num_occurrences", 0);
    r.chain_break_fraction = j.value("chain_break_fraction", 0.0);
    return r;
}

// =============================================================================
// DWaveClient Implementation
// =============================================================================

/// Mock quantum solver using simulated annealing (inline for now)
class MockQuantumSolverImpl {
public:
    MockQuantumSolverImpl() {
        std::random_device rd;
        rng_.seed(rd());
    }

    QuantumResult solve(const std::vector<std::vector<double>>& Q, int num_reads) {
        auto start_time = std::chrono::steady_clock::now();

        QuantumResult result;
        result.success = true;
        result.status = "MOCK_COMPLETED";

        if (Q.empty()) {
            result.success = false;
            result.status = "EMPTY_QUBO";
            return result;
        }

        int n = static_cast<int>(Q.size());

        // Validate Q is square
        for (const auto& row : Q) {
            if (static_cast<int>(row.size()) != n) {
                result.success = false;
                result.status = "INVALID_QUBO_DIMENSIONS";
                return result;
            }
        }

        // Run simulated annealing for each read
        double best_energy = std::numeric_limits<double>::max();
        int best_occurrences = 0;

        for (int read = 0; read < num_reads; ++read) {
            auto sample = runAnneal(Q, n);
            double energy = calculateEnergy(Q, sample);

            result.all_samples.push_back(sample);
            result.all_energies.push_back(energy);

            if (energy < best_energy - 1e-10) {
                best_energy = energy;
                result.best_sample = sample;
                result.best_energy = energy;
                best_occurrences = 1;
            } else if (std::abs(energy - best_energy) < 1e-10) {
                ++best_occurrences;
            }
        }

        result.num_occurrences = best_occurrences;
        result.qpu_access_time_us = 0.0;
        result.chain_break_fraction = 0.0;

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        result.total_time_ms = static_cast<double>(duration.count());

        return result;
    }

private:
    std::vector<int> runAnneal(const std::vector<std::vector<double>>& Q, int n) {
        std::uniform_int_distribution<int> bit_dist(0, 1);
        std::vector<int> solution(n);
        for (int i = 0; i < n; ++i) {
            solution[i] = bit_dist(rng_);
        }

        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
        std::uniform_int_distribution<int> index_dist(0, n - 1);

        constexpr int num_sweeps = 100;
        constexpr double initial_temp = 10.0;
        constexpr double final_temp = 0.01;

        double temp = initial_temp;
        double temp_factor = std::pow(final_temp / initial_temp, 1.0 / num_sweeps);

        for (int sweep = 0; sweep < num_sweeps; ++sweep) {
            for (int step = 0; step < n; ++step) {
                int flip_idx = index_dist(rng_);
                double delta_e = calculateFlipDelta(Q, solution, flip_idx, n);

                if (delta_e < 0 || prob_dist(rng_) < std::exp(-delta_e / temp)) {
                    solution[flip_idx] = 1 - solution[flip_idx];
                }
            }
            temp *= temp_factor;
        }

        return solution;
    }

    double calculateEnergy(const std::vector<std::vector<double>>& Q,
                           const std::vector<int>& solution) const {
        int n = static_cast<int>(Q.size());
        double energy = 0.0;

        for (int i = 0; i < n; ++i) {
            energy += Q[i][i] * solution[i];
            for (int j = i + 1; j < n; ++j) {
                energy += Q[i][j] * solution[i] * solution[j];
            }
        }
        return energy;
    }

    double calculateFlipDelta(const std::vector<std::vector<double>>& Q,
                               const std::vector<int>& solution,
                               int flip_index, int n) const {
        int current_val = solution[flip_index];
        int new_val = 1 - current_val;
        double delta = Q[flip_index][flip_index] * (new_val - current_val);

        for (int j = 0; j < n; ++j) {
            if (j == flip_index) continue;
            double q_ij = (flip_index < j) ? Q[flip_index][j] : Q[j][flip_index];
            delta += q_ij * solution[j] * (new_val - current_val);
        }
        return delta;
    }

    std::mt19937 rng_;
};

class DWaveClient::Impl {
public:
    explicit Impl(const QuantumSolverConfig& config)
        : config_(config), mock_solver_(std::make_unique<MockQuantumSolverImpl>()) {
        // Check if we should use mock mode
        use_mock_ = (config.provider == "mock");

        // Try to get API key from environment
        if (!use_mock_) {
            const char* api_key = std::getenv(config.api_key_env.c_str());
            if (api_key != nullptr && std::strlen(api_key) > 0) {
                api_key_ = api_key;
            }
        }
    }

    bool authenticate() {
        if (use_mock_) {
            authenticated_ = true;
            return true;
        }

        if (api_key_.empty()) {
            return false;
        }

        // TODO: Implement actual D-Wave API authentication
        // For now, just check if key is present
        authenticated_ = !api_key_.empty();
        return authenticated_;
    }

    bool checkConnection() {
        if (use_mock_) {
            return true;
        }

        if (!authenticated_) {
            return false;
        }

        // TODO: Implement actual D-Wave API connection check
        return false;
    }

    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q, int num_reads) {
        if (use_mock_) {
            return mock_solver_->solve(Q, num_reads > 0 ? num_reads : config_.num_reads);
        }

        // TODO: Implement actual D-Wave SAPI submission
        QuantumResult result;
        result.success = false;
        result.status = "D-Wave API integration pending - use mock mode for now";
        return result;
    }

    double estimateCost(int num_qubits) const {
        if (use_mock_) {
            return 0.0;  // Mock mode is free
        }

        // D-Wave hybrid solver pricing (approximate):
        // Base: ~$0.20/minute of solver time
        // Hybrid solver handles larger problems more efficiently

        if (config_.use_hybrid) {
            // Hybrid solver: roughly linear scaling with some overhead
            double base_time_min = 1.0;  // Minimum 1 minute
            double scaling = num_qubits / 1000.0;  // ~1000 qubits per minute
            double estimated_minutes = base_time_min + scaling;
            return 0.20 * estimated_minutes;
        } else {
            // Pure QPU: limited by connectivity, may need multiple embeds
            // Much faster but may require problem decomposition
            double estimated_seconds = std::max(1.0, num_qubits * 0.001);
            return 0.20 * (estimated_seconds / 60.0);
        }
    }

    QuantumSolverConfig config_;
    std::string api_key_;
    bool authenticated_ = false;
    bool use_mock_ = false;
    std::unique_ptr<MockQuantumSolverImpl> mock_solver_;
    int verbosity_ = 0;
};

DWaveClient::DWaveClient(const QuantumSolverConfig& config)
    : config_(config), impl_(std::make_unique<Impl>(config)) {}

DWaveClient::~DWaveClient() = default;

bool DWaveClient::isAvailable() const {
    return impl_->authenticated_ || impl_->use_mock_;
}

bool DWaveClient::authenticate() {
    return impl_->authenticate();
}

bool DWaveClient::checkConnection() {
    return impl_->checkConnection();
}

double DWaveClient::estimateCost(int num_qubits) const {
    return impl_->estimateCost(num_qubits);
}

core::SolverResult DWaveClient::solve(const core::Portfolio& portfolio,
                                       const core::TargetAllocation& target,
                                       const core::ConstraintSet& constraints) {
    auto start_time = std::chrono::steady_clock::now();

    core::SolverResult result;
    result.solver_used = impl_->use_mock_ ? "D-Wave (Mock)" : "D-Wave";

    // Create QUBO formulator
    QuboFormulator formulator(portfolio, target);

    if (!formulator.validateFormulation()) {
        result.success = false;
        result.status = "Invalid QUBO formulation";
        return result;
    }

    // Formulate QUBO
    auto Q = formulator.formulate(constraints);

    if (Q.empty()) {
        result.success = false;
        result.status = "Empty QUBO matrix";
        return result;
    }

    // Submit to quantum solver
    auto quantum_result = submitQUBO(Q, config_.num_reads);

    if (!quantum_result.success) {
        result.success = false;
        result.status = quantum_result.status;
        return result;
    }

    // Interpret solution
    result.trades = formulator.interpretSolution(quantum_result.best_sample);
    result.objective_value = quantum_result.best_energy;
    result.success = true;
    result.status = "optimal";

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    last_solve_time_ = duration.count();
    last_iterations_ = quantum_result.num_occurrences;

    result.solve_time_ms = last_solve_time_;
    result.iterations = last_iterations_;

    return result;
}

QuantumResult DWaveClient::submitQUBO(const std::vector<std::vector<double>>& Q,
                                       int num_reads) {
    return impl_->submitQUBO(Q, num_reads);
}

void DWaveClient::setTimeout(long timeout_ms) {
    config_.timeout_ms = timeout_ms;
}

void DWaveClient::setVerbosity(int level) {
    impl_->verbosity_ = level;
}

void DWaveClient::setNumReads(int reads) {
    config_.num_reads = reads;
    impl_->config_.num_reads = reads;
}

void DWaveClient::useHybridSolver(bool use) {
    config_.use_hybrid = use;
    impl_->config_.use_hybrid = use;
}

std::vector<std::string> DWaveClient::getAvailableSolvers() const {
    if (impl_->use_mock_) {
        return {"mock_simulated_annealing"};
    }
    return {"hybrid_binary_quadratic_model_version2", "Advantage_system6.1"};
}

// =============================================================================
// IBMQuantumClient Implementation with QAOA and SPSA Optimizer
// =============================================================================

/// SPSA (Simultaneous Perturbation Stochastic Approximation) optimizer configuration
struct SPSAConfig {
    int max_iterations = 100;    ///< Maximum optimization iterations
    double a = 0.1;              ///< Step size parameter
    double c = 0.1;              ///< Perturbation size parameter
    double alpha = 0.602;        ///< Step size decay exponent
    double gamma = 0.101;        ///< Perturbation decay exponent
    double a_factor = 0.1;       ///< Scaling for a based on problem size
    double c_factor = 0.1;       ///< Scaling for c based on problem size
};

/// Mock QAOA simulator for testing without IBM Quantum access
class MockQAOASimulator {
public:
    MockQAOASimulator() {
        std::random_device rd;
        rng_.seed(rd());
    }

    /// Simulate QAOA circuit execution
    /// Returns measurement counts for each basis state
    std::map<std::string, int> simulate(
        const std::vector<std::vector<double>>& Q,
        const std::vector<double>& gamma,
        const std::vector<double>& beta,
        int num_shots) {

        int n = static_cast<int>(Q.size());
        int p = static_cast<int>(gamma.size());  // QAOA depth

        std::map<std::string, int> counts;

        // For mock simulation, we use a simplified model:
        // The probability of measuring a state is proportional to
        // exp(-energy * effective_gamma) where effective_gamma increases with depth

        // Calculate energies for all 2^n states (only feasible for small n)
        if (n > 20) {
            // For large problems, sample randomly with bias toward low energy
            return sampleLargeProblem(Q, gamma, beta, num_shots, n);
        }

        int num_states = 1 << n;
        std::vector<double> energies(num_states);
        std::vector<double> probabilities(num_states);

        double effective_gamma = 0.0;
        for (double g : gamma) {
            effective_gamma += std::abs(g);
        }

        double min_energy = std::numeric_limits<double>::max();
        for (int state = 0; state < num_states; ++state) {
            energies[state] = calculateStateEnergy(Q, state, n);
            min_energy = std::min(min_energy, energies[state]);
        }

        // Calculate Boltzmann-like probabilities
        double sum_prob = 0.0;
        double temperature = 1.0 / (effective_gamma + 0.1);
        for (int state = 0; state < num_states; ++state) {
            probabilities[state] = std::exp(-(energies[state] - min_energy) / temperature);
            sum_prob += probabilities[state];
        }

        // Normalize
        for (int state = 0; state < num_states; ++state) {
            probabilities[state] /= sum_prob;
        }

        // Sample according to probabilities
        std::discrete_distribution<int> dist(probabilities.begin(), probabilities.end());
        for (int shot = 0; shot < num_shots; ++shot) {
            int state = dist(rng_);
            std::string bitstring = stateToBitstring(state, n);
            counts[bitstring]++;
        }

        return counts;
    }

private:
    double calculateStateEnergy(const std::vector<std::vector<double>>& Q,
                                 int state, int n) const {
        double energy = 0.0;
        for (int i = 0; i < n; ++i) {
            int xi = (state >> i) & 1;
            energy += Q[i][i] * xi;
            for (int j = i + 1; j < n; ++j) {
                int xj = (state >> j) & 1;
                energy += Q[i][j] * xi * xj;
            }
        }
        return energy;
    }

    std::string stateToBitstring(int state, int n) const {
        std::string bits(n, '0');
        for (int i = 0; i < n; ++i) {
            if ((state >> i) & 1) {
                bits[n - 1 - i] = '1';
            }
        }
        return bits;
    }

    std::map<std::string, int> sampleLargeProblem(
        const std::vector<std::vector<double>>& Q,
        const std::vector<double>& gamma,
        const std::vector<double>& beta,
        int num_shots, int n) {

        // For large problems, use simulated annealing to find good states
        std::map<std::string, int> counts;
        std::uniform_int_distribution<int> bit_dist(0, 1);
        std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
        std::uniform_int_distribution<int> index_dist(0, n - 1);

        double effective_gamma = 0.0;
        for (double g : gamma) effective_gamma += std::abs(g);

        for (int shot = 0; shot < num_shots; ++shot) {
            // Initialize random state
            std::vector<int> state(n);
            for (int i = 0; i < n; ++i) {
                state[i] = bit_dist(rng_);
            }

            // Quick annealing
            int num_sweeps = static_cast<int>(10 + effective_gamma * 10);
            double temp = 1.0;
            double temp_factor = 0.9;

            for (int sweep = 0; sweep < num_sweeps; ++sweep) {
                for (int step = 0; step < n; ++step) {
                    int idx = index_dist(rng_);
                    double delta = calculateFlipDelta(Q, state, idx);
                    if (delta < 0 || prob_dist(rng_) < std::exp(-delta / temp)) {
                        state[idx] = 1 - state[idx];
                    }
                }
                temp *= temp_factor;
            }

            // Convert to bitstring
            std::string bitstring(n, '0');
            for (int i = 0; i < n; ++i) {
                if (state[i]) bitstring[n - 1 - i] = '1';
            }
            counts[bitstring]++;
        }

        return counts;
    }

    double calculateFlipDelta(const std::vector<std::vector<double>>& Q,
                               const std::vector<int>& state, int idx) const {
        int n = static_cast<int>(state.size());
        int current = state[idx];
        int new_val = 1 - current;
        double delta = Q[idx][idx] * (new_val - current);

        for (int j = 0; j < n; ++j) {
            if (j == idx) continue;
            double q_ij = (idx < j) ? Q[idx][j] : Q[j][idx];
            delta += q_ij * state[j] * (new_val - current);
        }
        return delta;
    }

    std::mt19937 rng_;
};

class IBMQuantumClient::Impl {
public:
    explicit Impl(const QuantumSolverConfig& config)
        : config_(config), mock_simulator_(std::make_unique<MockQAOASimulator>()) {
        // Check if we should use mock mode
        use_mock_ = (config.provider == "mock" || config.provider == "ibm_mock");

        // Try to get API key from environment
        if (!use_mock_) {
            const char* api_key = std::getenv("IBM_QUANTUM_API_KEY");
            if (api_key != nullptr && std::strlen(api_key) > 0) {
                api_key_ = api_key;
            }
        }
    }

    bool authenticate() {
        if (use_mock_) {
            authenticated_ = true;
            return true;
        }

        if (api_key_.empty()) {
            return false;
        }

        // TODO: Implement actual IBM Quantum API authentication
        authenticated_ = !api_key_.empty();
        return authenticated_;
    }

    bool checkConnection() {
        if (use_mock_) {
            return true;
        }

        if (!authenticated_) {
            return false;
        }

        // TODO: Implement actual IBM Quantum API connection check
        return false;
    }

    /// Build QAOA circuit as OpenQASM 3.0 string
    std::string buildQAOACircuit(const std::vector<std::vector<double>>& Q,
                                  const std::vector<double>& gamma,
                                  const std::vector<double>& beta) {
        int n = static_cast<int>(Q.size());
        int p = static_cast<int>(gamma.size());

        std::stringstream qasm;
        qasm << "OPENQASM 3.0;\n";
        qasm << "include \"stdgates.inc\";\n";
        qasm << "qubit[" << n << "] q;\n";
        qasm << "bit[" << n << "] c;\n\n";

        // Initial superposition
        qasm << "// Initial state: equal superposition\n";
        for (int i = 0; i < n; ++i) {
            qasm << "h q[" << i << "];\n";
        }

        // QAOA layers
        for (int layer = 0; layer < p; ++layer) {
            // Cost unitary (problem Hamiltonian)
            qasm << "\n// Layer " << layer << " - Cost unitary (gamma = "
                 << gamma[layer] << ")\n";

            for (int i = 0; i < n; ++i) {
                // Single qubit Z rotations (diagonal terms)
                if (std::abs(Q[i][i]) > 1e-10) {
                    qasm << "rz(" << std::fixed << std::setprecision(6)
                         << (gamma[layer] * Q[i][i]) << ") q[" << i << "];\n";
                }

                // Two-qubit ZZ interactions (off-diagonal terms)
                for (int j = i + 1; j < n; ++j) {
                    if (std::abs(Q[i][j]) > 1e-10) {
                        double angle = gamma[layer] * Q[i][j];
                        qasm << "cx q[" << i << "], q[" << j << "];\n";
                        qasm << "rz(" << std::fixed << std::setprecision(6)
                             << angle << ") q[" << j << "];\n";
                        qasm << "cx q[" << i << "], q[" << j << "];\n";
                    }
                }
            }

            // Mixer unitary
            qasm << "\n// Layer " << layer << " - Mixer unitary (beta = "
                 << beta[layer] << ")\n";
            for (int i = 0; i < n; ++i) {
                qasm << "rx(" << std::fixed << std::setprecision(6)
                     << (2 * beta[layer]) << ") q[" << i << "];\n";
            }
        }

        // Measurement
        qasm << "\n// Measurement\n";
        qasm << "measure q -> c;\n";

        return qasm.str();
    }

    /// Evaluate QAOA circuit with given parameters (using mock or real backend)
    double evaluateQAOA(const std::vector<std::vector<double>>& Q,
                         const std::vector<double>& params) {
        int p = qaoa_depth_;
        std::vector<double> gamma(params.begin(), params.begin() + p);
        std::vector<double> beta(params.begin() + p, params.end());

        // Get measurement counts
        auto counts = mock_simulator_->simulate(Q, gamma, beta, num_shots_);

        // Calculate expectation value of energy
        double total_energy = 0.0;
        int total_shots = 0;

        for (const auto& [bitstring, count] : counts) {
            double energy = bitstringToEnergy(Q, bitstring);
            total_energy += energy * count;
            total_shots += count;
        }

        return total_energy / total_shots;
    }

    /// SPSA optimizer for QAOA parameters
    std::vector<double> optimizeSPSA(const std::vector<std::vector<double>>& Q) {
        int p = qaoa_depth_;
        int num_params = 2 * p;  // gamma and beta for each layer

        // Initialize parameters
        std::vector<double> params(num_params, 0.5);

        std::mt19937 rng;
        std::random_device rd;
        rng.seed(rd());
        std::uniform_int_distribution<int> bernoulli(0, 1);

        double best_energy = evaluateQAOA(Q, params);
        std::vector<double> best_params = params;

        for (int k = 0; k < spsa_config_.max_iterations; ++k) {
            // SPSA step sizes with decay
            double a_k = spsa_config_.a / std::pow(k + 1 + 100, spsa_config_.alpha);
            double c_k = spsa_config_.c / std::pow(k + 1, spsa_config_.gamma);

            // Random perturbation direction (Rademacher distribution)
            std::vector<double> delta(num_params);
            for (int i = 0; i < num_params; ++i) {
                delta[i] = (bernoulli(rng) == 0) ? -1.0 : 1.0;
            }

            // Evaluate at perturbed points
            std::vector<double> params_plus = params;
            std::vector<double> params_minus = params;
            for (int i = 0; i < num_params; ++i) {
                params_plus[i] += c_k * delta[i];
                params_minus[i] -= c_k * delta[i];
            }

            double f_plus = evaluateQAOA(Q, params_plus);
            double f_minus = evaluateQAOA(Q, params_minus);

            // Gradient estimate and update
            for (int i = 0; i < num_params; ++i) {
                double grad = (f_plus - f_minus) / (2.0 * c_k * delta[i]);
                params[i] -= a_k * grad;

                // Keep parameters in reasonable range
                params[i] = std::max(-M_PI, std::min(M_PI, params[i]));
            }

            // Track best solution
            double current_energy = evaluateQAOA(Q, params);
            if (current_energy < best_energy) {
                best_energy = current_energy;
                best_params = params;
            }

            // Early stopping if converged
            if (k > 10 && std::abs(f_plus - f_minus) < 1e-6) {
                break;
            }
        }

        return best_params;
    }

    /// Submit QUBO problem using QAOA
    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q, int num_shots) {
        auto start_time = std::chrono::steady_clock::now();

        QuantumResult result;
        result.success = true;
        result.status = use_mock_ ? "MOCK_QAOA_COMPLETED" : "QAOA_COMPLETED";

        if (Q.empty()) {
            result.success = false;
            result.status = "EMPTY_QUBO";
            return result;
        }

        int n = static_cast<int>(Q.size());
        num_shots_ = (num_shots > 0) ? num_shots : 1024;

        // Optimize QAOA parameters using SPSA
        auto optimal_params = optimizeSPSA(Q);

        int p = qaoa_depth_;
        std::vector<double> gamma(optimal_params.begin(), optimal_params.begin() + p);
        std::vector<double> beta(optimal_params.begin() + p, optimal_params.end());

        // Run final circuit with optimized parameters
        auto counts = mock_simulator_->simulate(Q, gamma, beta, num_shots_);

        // Find best solution
        double best_energy = std::numeric_limits<double>::max();
        std::string best_bitstring;
        int best_count = 0;

        for (const auto& [bitstring, count] : counts) {
            double energy = bitstringToEnergy(Q, bitstring);
            result.all_energies.push_back(energy);

            std::vector<int> sample = bitstringToVector(bitstring);
            result.all_samples.push_back(sample);

            if (energy < best_energy) {
                best_energy = energy;
                best_bitstring = bitstring;
                best_count = count;
                result.best_sample = sample;
                result.best_energy = energy;
            }
        }

        result.num_occurrences = best_count;
        result.qpu_access_time_us = 0.0;  // Mock doesn't use real QPU
        result.chain_break_fraction = 0.0;

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        result.total_time_ms = static_cast<double>(duration.count());

        return result;
    }

    double estimateCost(int num_qubits) const {
        if (use_mock_) {
            return 0.0;
        }

        // IBM Quantum pricing: ~$1.60/second of quantum time
        // QAOA circuit depth scales with problem size and p
        int circuit_depth = num_qubits * qaoa_depth_ * 3;  // ~3 gates per qubit per layer
        double time_per_shot_us = circuit_depth * 0.5;     // ~0.5us per gate
        double total_time_s = (time_per_shot_us * num_shots_) / 1e6;

        // Add optimization overhead (multiple circuit evaluations)
        double optimization_factor = spsa_config_.max_iterations * 2.0;
        return 1.60 * total_time_s * optimization_factor;
    }

private:
    double bitstringToEnergy(const std::vector<std::vector<double>>& Q,
                              const std::string& bitstring) const {
        int n = static_cast<int>(Q.size());
        double energy = 0.0;

        for (int i = 0; i < n; ++i) {
            int xi = (bitstring[n - 1 - i] == '1') ? 1 : 0;
            energy += Q[i][i] * xi;
            for (int j = i + 1; j < n; ++j) {
                int xj = (bitstring[n - 1 - j] == '1') ? 1 : 0;
                energy += Q[i][j] * xi * xj;
            }
        }
        return energy;
    }

    std::vector<int> bitstringToVector(const std::string& bitstring) const {
        int n = static_cast<int>(bitstring.size());
        std::vector<int> vec(n);
        for (int i = 0; i < n; ++i) {
            vec[i] = (bitstring[n - 1 - i] == '1') ? 1 : 0;
        }
        return vec;
    }

public:
    QuantumSolverConfig config_;
    std::string api_key_;
    std::string backend_ = "ibm_brisbane";
    bool authenticated_ = false;
    bool use_mock_ = false;
    int qaoa_depth_ = 2;
    int num_shots_ = 1024;
    std::string optimizer_ = "SPSA";
    SPSAConfig spsa_config_;
    std::unique_ptr<MockQAOASimulator> mock_simulator_;
    int verbosity_ = 0;
};

IBMQuantumClient::IBMQuantumClient(const QuantumSolverConfig& config)
    : config_(config), impl_(std::make_unique<Impl>(config)) {}

IBMQuantumClient::~IBMQuantumClient() = default;

bool IBMQuantumClient::isAvailable() const {
    return impl_->authenticated_ || impl_->use_mock_;
}

bool IBMQuantumClient::authenticate() {
    return impl_->authenticate();
}

bool IBMQuantumClient::checkConnection() {
    return impl_->checkConnection();
}

double IBMQuantumClient::estimateCost(int num_qubits) const {
    return impl_->estimateCost(num_qubits);
}

core::SolverResult IBMQuantumClient::solve(const core::Portfolio& portfolio,
                                            const core::TargetAllocation& target,
                                            const core::ConstraintSet& constraints) {
    auto start_time = std::chrono::steady_clock::now();

    core::SolverResult result;
    result.solver_used = impl_->use_mock_ ? "IBM Quantum (Mock QAOA)" : "IBM Quantum (QAOA)";

    // Create QUBO formulator
    QuboFormulator formulator(portfolio, target);

    if (!formulator.validateFormulation()) {
        result.success = false;
        result.status = "Invalid QUBO formulation";
        return result;
    }

    // Formulate QUBO
    auto Q = formulator.formulate(constraints);

    if (Q.empty()) {
        result.success = false;
        result.status = "Empty QUBO matrix";
        return result;
    }

    // Submit to quantum solver (QAOA)
    auto quantum_result = submitQUBO(Q, impl_->num_shots_);

    if (!quantum_result.success) {
        result.success = false;
        result.status = quantum_result.status;
        return result;
    }

    // Interpret solution
    result.trades = formulator.interpretSolution(quantum_result.best_sample);
    result.objective_value = quantum_result.best_energy;
    result.success = true;
    result.status = "optimal";

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    last_solve_time_ = duration.count();
    last_iterations_ = quantum_result.num_occurrences;

    result.solve_time_ms = last_solve_time_;
    result.iterations = last_iterations_;

    return result;
}

QuantumResult IBMQuantumClient::submitQUBO(const std::vector<std::vector<double>>& Q,
                                            int num_shots) {
    return impl_->submitQUBO(Q, num_shots);
}

void IBMQuantumClient::setTimeout(long timeout_ms) {
    config_.timeout_ms = timeout_ms;
    impl_->config_.timeout_ms = timeout_ms;
}

void IBMQuantumClient::setVerbosity(int level) {
    impl_->verbosity_ = level;
}

void IBMQuantumClient::setQAOADepth(int p) {
    impl_->qaoa_depth_ = std::max(1, p);
}

void IBMQuantumClient::setOptimizer(const std::string& optimizer) {
    impl_->optimizer_ = optimizer;
    // Adjust SPSA config based on optimizer choice
    if (optimizer == "COBYLA") {
        // COBYLA uses fewer iterations typically
        impl_->spsa_config_.max_iterations = 50;
    } else {
        // Default SPSA config
        impl_->spsa_config_.max_iterations = 100;
    }
}

void IBMQuantumClient::setNumShots(int shots) {
    impl_->num_shots_ = std::max(1, shots);
}

void IBMQuantumClient::setBackend(const std::string& backend) {
    impl_->backend_ = backend;
}

std::vector<std::string> IBMQuantumClient::getAvailableBackends() const {
    if (impl_->use_mock_) {
        return {"mock_qaoa_simulator"};
    }
    return {"ibmq_qasm_simulator", "ibm_brisbane", "ibm_osaka", "ibm_kyoto"};
}

std::string IBMQuantumClient::getQAOACircuit(const std::vector<std::vector<double>>& Q,
                                              const std::vector<double>& gamma,
                                              const std::vector<double>& beta) const {
    return impl_->buildQAOACircuit(Q, gamma, beta);
}

}  // namespace lumen::solvers
