#pragma once

/// @file qubo_types.hpp
/// @brief QUBO (Quadratic Unconstrained Binary Optimization) data structures
///
/// This module defines data structures for QUBO formulation used in quantum
/// optimization. QUBO problems are solved by quantum annealers (D-Wave) and
/// QAOA algorithms (IBM Quantum).

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lumen::solvers {

/// QUBO matrix representation
/// The objective is: min x^T Q x, where x is a binary vector
struct QuboMatrix {
    std::vector<std::vector<double>> Q;  ///< Upper triangular QUBO matrix
    double offset = 0.0;                  ///< Constant offset in objective
    int num_variables = 0;                ///< Number of binary variables

    /// Variable index to human-readable name mapping
    std::map<int, std::string> var_names;

    /// Evaluate the QUBO objective for a given binary solution
    double evaluate(const std::vector<int>& solution) const;

    /// Check if matrix is valid (correct dimensions, symmetric-ish for diagonal)
    bool isValid() const;

    /// Get the number of non-zero elements
    int getNonZeroCount() const;

    /// Convert to JSON for API submission
    nlohmann::json toJSON() const;

    /// Create from JSON response
    static QuboMatrix fromJSON(const nlohmann::json& j);
};

/// Configuration for encoding continuous variables as binary
struct EncodingConfig {
    int precision_bits = 8;      ///< Bits for continuous variable encoding
    double min_value = 0.0;      ///< Minimum portfolio weight (0%)
    double max_value = 1.0;      ///< Maximum portfolio weight (100%)

    /// Get the number of discrete levels (2^precision_bits)
    int getNumLevels() const { return 1 << precision_bits; }

    /// Get the step size between levels
    double getStepSize() const {
        return (max_value - min_value) / (getNumLevels() - 1);
    }

    /// Encode a continuous value to binary representation
    std::vector<int> encode(double value) const;

    /// Decode binary representation to continuous value
    double decode(const std::vector<int>& binary) const;

    nlohmann::json toJSON() const;
    static EncodingConfig fromJSON(const nlohmann::json& j);
};

/// Configuration for constraint penalty weights
struct PenaltyConfig {
    double constraint_penalty = 1000.0;  ///< Base penalty for constraint violations
    bool auto_tune = true;               ///< Enable auto-tuning of penalties
    double tune_factor = 10.0;           ///< Multiplier for auto-tuned penalties

    /// Auto-tune penalty based on objective coefficient magnitudes
    double computeAutoTunedPenalty(double max_objective_coeff, int num_constraints) const;

    nlohmann::json toJSON() const;
    static PenaltyConfig fromJSON(const nlohmann::json& j);
};

/// Variable mapping information for interpreting QUBO solutions
struct VariableMapping {
    std::string ticker;          ///< Security symbol
    int start_index;             ///< Starting index in binary vector
    int num_bits;                ///< Number of bits for this variable
    bool is_buy;                 ///< true = buy variable, false = sell variable
    double scale_factor;         ///< Multiplier for decoding

    /// Decode the portion of binary solution for this variable
    double decode(const std::vector<int>& binary_solution) const;
};

/// Tax lot selection variable for QUBO
struct TaxLotVariable {
    std::string lot_id;          ///< Tax lot identifier
    std::string ticker;          ///< Security symbol
    int qubit_index;             ///< Index in binary vector (single bit: sell or not)
    double shares;               ///< Shares in this lot
    double cost_basis;           ///< Cost basis per share
    double current_price;        ///< Current market price
    double unrealized_gain;      ///< Current unrealized gain/loss
    bool is_long_term;           ///< Long-term vs short-term for tax purposes
};

/// QUBO formulation metadata
struct QuboFormulationInfo {
    int num_positions;            ///< Number of portfolio positions
    int num_tax_lots;             ///< Number of tax lots (if enabled)
    int total_qubits;             ///< Total binary variables needed
    int num_constraints;          ///< Number of constraints encoded

    /// Variable mappings for decoding solutions
    std::vector<VariableMapping> weight_variables;

    /// Tax lot variables (if tax optimization enabled)
    std::vector<TaxLotVariable> tax_lot_variables;

    /// Constraint names that were encoded
    std::vector<std::string> constraint_names;

    /// Encoding configuration used
    EncodingConfig encoding;

    /// Penalty configuration used
    PenaltyConfig penalties;

    nlohmann::json toJSON() const;
    static QuboFormulationInfo fromJSON(const nlohmann::json& j);
};

/// Result of decoding a QUBO solution
struct DecodedSolution {
    /// Target weights for each position (ticker -> weight as percentage)
    std::map<std::string, double> target_weights;

    /// Tax lots selected for sale (lot_id -> shares to sell)
    std::map<std::string, double> lots_to_sell;

    /// Constraint satisfaction status
    std::map<std::string, bool> constraints_satisfied;

    /// Total constraint violation (lower is better)
    double total_violation = 0.0;

    /// Objective value (without penalties)
    double objective_value = 0.0;

    /// Whether solution is feasible (all constraints satisfied)
    bool is_feasible() const;

    nlohmann::json toJSON() const;
};

}  // namespace lumen::solvers
