#pragma once

/// @file quantum_mock.hpp
/// @brief Mock quantum solver for testing without API costs
///
/// This module provides a mock quantum solver that simulates quantum
/// annealing behavior using classical simulated annealing. It's useful
/// for development, testing, and when API access is unavailable.

#include <random>
#include <vector>

#include "lumen/solvers/qubo_types.hpp"
#include "lumen/solvers/quantum_client.hpp"

namespace lumen::solvers {

/// Configuration for the mock quantum solver
struct MockSolverConfig {
    int num_reads = 1000;                ///< Number of samples to generate
    int num_sweeps = 100;                ///< Sweeps per sample (annealing steps)
    double initial_temperature = 10.0;   ///< Starting temperature for annealing
    double final_temperature = 0.01;     ///< Final temperature
    bool deterministic = false;          ///< Use fixed seed for reproducibility
    unsigned int seed = 42;              ///< Random seed (if deterministic)
};

/// Mock quantum solver using simulated annealing
class MockQuantumSolver {
public:
    MockQuantumSolver();
    explicit MockQuantumSolver(const MockSolverConfig& config);
    ~MockQuantumSolver();

    /// Configure the mock solver
    void configure(const MockSolverConfig& config);

    /// Solve a QUBO problem using simulated annealing
    /// @param Q Upper triangular QUBO matrix
    /// @param num_reads Number of samples to generate (overrides config if > 0)
    /// @return QuantumResult with samples and energies
    QuantumResult solve(const std::vector<std::vector<double>>& Q, int num_reads = 0);

    /// Solve using a QuboMatrix structure
    QuantumResult solve(const QuboMatrix& qubo, int num_reads = 0);

    /// Get the last solve time in milliseconds
    double getLastSolveTimeMs() const { return last_solve_time_ms_; }

    /// Set seed for deterministic testing
    void setSeed(unsigned int seed);

    /// Enable/disable deterministic mode
    void setDeterministic(bool deterministic);

private:
    /// Run simulated annealing for one sample
    std::vector<int> runAnneal(const std::vector<std::vector<double>>& Q);

    /// Calculate QUBO energy for a solution
    double calculateEnergy(const std::vector<std::vector<double>>& Q,
                          const std::vector<int>& solution) const;

    /// Calculate energy delta for flipping a single bit
    double calculateFlipDelta(const std::vector<std::vector<double>>& Q,
                              const std::vector<int>& solution,
                              int flip_index) const;

    MockSolverConfig config_;
    std::mt19937 rng_;
    double last_solve_time_ms_ = 0.0;
};

/// Factory function to create appropriate quantum solver based on configuration
/// Returns mock solver if use_mock is true, otherwise returns real client
std::unique_ptr<QuantumClient> createQuantumClient(const QuantumSolverConfig& config,
                                                    bool use_mock);

}  // namespace lumen::solvers
