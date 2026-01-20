#pragma once

/// @file quantum_client.hpp
/// @brief Quantum solver clients for D-Wave and IBM Quantum
///
/// This module provides clients for quantum computing services,
/// including QUBO formulation and result interpretation.

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/solvers/highs_wrapper.hpp"

namespace lumen {

namespace data {
struct TaxLot;
}

namespace solvers {

/// Configuration for quantum solvers
struct QuantumSolverConfig {
    std::string provider;       ///< Provider name: "dwave", "ibm", "ionq"
    std::string api_key_env;    ///< Environment variable name for API key
    long timeout_ms = 60000;    ///< Timeout in milliseconds
    int num_reads = 1000;       ///< Number of samples for annealing
    bool use_hybrid = true;     ///< Use hybrid solver (recommended)
};

/// Result from a quantum solver
struct QuantumResult {
    bool success;                 ///< Whether solving succeeded
    std::string status;           ///< Status string
    std::vector<int> best_sample; ///< Best binary solution found
    double best_energy;           ///< Energy (objective value) of best solution
    std::vector<std::vector<int>> all_samples;  ///< All samples returned
    std::vector<double> all_energies;           ///< Energies of all samples

    // Quantum-specific metrics
    double qpu_access_time_us;      ///< QPU access time in microseconds
    double total_time_ms;           ///< Total time including queue
    int num_occurrences;            ///< How many times best solution found
    double chain_break_fraction;    ///< Chain break fraction (D-Wave)

    nlohmann::json toJSON() const;
    static QuantumResult fromJSON(const nlohmann::json& j);
};

/// Formulates portfolio optimization as QUBO (Quadratic Unconstrained Binary Optimization)
class QuboFormulator {
public:
    QuboFormulator(const core::Portfolio& portfolio, const core::TargetAllocation& target);
    ~QuboFormulator();

    /// Formulate QUBO matrix from portfolio optimization problem
    std::vector<std::vector<double>> formulate(const core::ConstraintSet& constraints);

    /// Set objective weights
    void setWeights(double drift_weight, double cost_weight, double tax_weight);

    /// Enable tax lot optimization with specific lots
    void enableTaxLotOptimization(const std::vector<data::TaxLot>& lots);

    /// Set penalty parameter for constraint enforcement
    void setConstraintPenalty(double penalty);

    /// Interpret binary solution as trades
    std::vector<core::Trade> interpretSolution(const std::vector<int>& binary_solution) const;

    /// Validate the QUBO formulation
    bool validateFormulation() const;

    /// Get number of qubits (variables) needed
    int getNumQubits() const;

    /// Get variable to qubit mapping
    std::map<int, std::string> getVariableMapping() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Abstract base class for quantum clients
class QuantumClient : public BaseSolver {
public:
    ~QuantumClient() override = default;

    /// Authenticate with the quantum service
    virtual bool authenticate() = 0;

    /// Check connection to the service
    virtual bool checkConnection() = 0;

    /// Estimate cost for a problem of given size
    virtual double estimateCost(int num_qubits) const = 0;

    /// Submit a QUBO problem
    virtual QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q,
                                     int num_reads = 1000) = 0;
};

/// D-Wave quantum annealing client
class DWaveClient : public QuantumClient {
public:
    explicit DWaveClient(const QuantumSolverConfig& config);
    ~DWaveClient() override;

    std::string getName() const override { return "D-Wave"; }
    bool isAvailable() const override;

    bool authenticate() override;
    bool checkConnection() override;
    double estimateCost(int num_qubits) const override;

    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints) override;

    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q,
                             int num_reads = 1000) override;

    void setTimeout(long timeout_ms) override;
    void setVerbosity(int level) override;

    long getLastSolveTime() const override { return last_solve_time_; }
    int getLastIterations() const override { return last_iterations_; }

    // D-Wave specific
    void setNumReads(int reads);
    void useHybridSolver(bool use);
    std::vector<std::string> getAvailableSolvers() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    QuantumSolverConfig config_;
    long last_solve_time_ = 0;
    int last_iterations_ = 0;
};

/// IBM Quantum gate-based client (QAOA)
class IBMQuantumClient : public QuantumClient {
public:
    explicit IBMQuantumClient(const QuantumSolverConfig& config);
    ~IBMQuantumClient() override;

    std::string getName() const override { return "IBM Quantum"; }
    bool isAvailable() const override;

    bool authenticate() override;
    bool checkConnection() override;
    double estimateCost(int num_qubits) const override;

    core::SolverResult solve(const core::Portfolio& portfolio,
                             const core::TargetAllocation& target,
                             const core::ConstraintSet& constraints) override;

    QuantumResult submitQUBO(const std::vector<std::vector<double>>& Q,
                             int num_reads = 1000) override;

    void setTimeout(long timeout_ms) override;
    void setVerbosity(int level) override;

    long getLastSolveTime() const override { return last_solve_time_; }
    int getLastIterations() const override { return last_iterations_; }

    // IBM specific - QAOA parameters
    void setQAOADepth(int p);
    void setOptimizer(const std::string& optimizer);
    void setNumShots(int shots);
    void setBackend(const std::string& backend);
    std::vector<std::string> getAvailableBackends() const;

    /// Get the QAOA circuit as OpenQASM 3.0 string
    std::string getQAOACircuit(const std::vector<std::vector<double>>& Q,
                               const std::vector<double>& gamma,
                               const std::vector<double>& beta) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    QuantumSolverConfig config_;
    long last_solve_time_ = 0;
    int last_iterations_ = 0;
};

}  // namespace solvers
}  // namespace lumen
