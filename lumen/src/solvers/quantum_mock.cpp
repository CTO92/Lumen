/// @file quantum_mock.cpp
/// @brief Mock quantum solver implementation
///
/// Implementation of the mock quantum solver using simulated annealing
/// for testing and development without requiring quantum API access.

#include "lumen/solvers/quantum_mock.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace lumen::solvers {

// =============================================================================
// MockQuantumSolver Implementation
// =============================================================================

MockQuantumSolver::MockQuantumSolver() {
    // Use random seed by default
    std::random_device rd;
    rng_.seed(rd());
}

MockQuantumSolver::MockQuantumSolver(const MockSolverConfig& config)
    : config_(config) {
    if (config_.deterministic) {
        rng_.seed(config_.seed);
    } else {
        std::random_device rd;
        rng_.seed(rd());
    }
}

MockQuantumSolver::~MockQuantumSolver() = default;

void MockQuantumSolver::configure(const MockSolverConfig& config) {
    config_ = config;
    if (config_.deterministic) {
        rng_.seed(config_.seed);
    }
}

QuantumResult MockQuantumSolver::solve(const std::vector<std::vector<double>>& Q,
                                        int num_reads) {
    auto start_time = std::chrono::high_resolution_clock::now();

    int actual_reads = (num_reads > 0) ? num_reads : config_.num_reads;

    QuantumResult result;
    result.success = true;
    result.status = "MOCK_COMPLETED";

    if (Q.empty()) {
        result.success = false;
        result.status = "EMPTY_QUBO";
        return result;
    }

    // Validate Q is square
    int n = static_cast<int>(Q.size());
    for (const auto& row : Q) {
        if (static_cast<int>(row.size()) != n) {
            result.success = false;
            result.status = "INVALID_QUBO_DIMENSIONS";
            return result;
        }
    }

    // Run simulated annealing for each read
    result.all_samples.reserve(actual_reads);
    result.all_energies.reserve(actual_reads);

    double best_energy = std::numeric_limits<double>::max();
    int best_occurrences = 0;

    for (int read = 0; read < actual_reads; ++read) {
        auto sample = runAnneal(Q);
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

    // Mock quantum metrics
    result.qpu_access_time_us = 0.0;  // No actual QPU
    result.chain_break_fraction = 0.0;  // No embedding needed

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    last_solve_time_ms_ = duration.count() / 1000.0;
    result.total_time_ms = last_solve_time_ms_;

    return result;
}

QuantumResult MockQuantumSolver::solve(const QuboMatrix& qubo, int num_reads) {
    auto result = solve(qubo.Q, num_reads);

    // Adjust energy by offset
    if (result.success) {
        result.best_energy += qubo.offset;
        for (auto& e : result.all_energies) {
            e += qubo.offset;
        }
    }

    return result;
}

std::vector<int> MockQuantumSolver::runAnneal(
    const std::vector<std::vector<double>>& Q) {
    int n = static_cast<int>(Q.size());

    // Initialize with random binary solution
    std::uniform_int_distribution<int> bit_dist(0, 1);
    std::vector<int> solution(n);
    for (int i = 0; i < n; ++i) {
        solution[i] = bit_dist(rng_);
    }

    double current_energy = calculateEnergy(Q, solution);

    // Simulated annealing
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::uniform_int_distribution<int> index_dist(0, n - 1);

    double temp = config_.initial_temperature;
    double temp_factor = std::pow(config_.final_temperature / config_.initial_temperature,
                                   1.0 / config_.num_sweeps);

    for (int sweep = 0; sweep < config_.num_sweeps; ++sweep) {
        // Each sweep tries to flip each bit once (on average)
        for (int step = 0; step < n; ++step) {
            int flip_idx = index_dist(rng_);

            // Calculate energy change if we flip this bit
            double delta_e = calculateFlipDelta(Q, solution, flip_idx);

            // Accept with Metropolis criterion
            if (delta_e < 0 || prob_dist(rng_) < std::exp(-delta_e / temp)) {
                solution[flip_idx] = 1 - solution[flip_idx];
                current_energy += delta_e;
            }
        }

        // Cool down
        temp *= temp_factor;
    }

    return solution;
}

double MockQuantumSolver::calculateEnergy(const std::vector<std::vector<double>>& Q,
                                           const std::vector<int>& solution) const {
    int n = static_cast<int>(Q.size());
    double energy = 0.0;

    for (int i = 0; i < n; ++i) {
        // Diagonal terms
        energy += Q[i][i] * solution[i];

        // Off-diagonal terms (upper triangle only, since Q is upper triangular)
        for (int j = i + 1; j < n; ++j) {
            energy += Q[i][j] * solution[i] * solution[j];
        }
    }

    return energy;
}

double MockQuantumSolver::calculateFlipDelta(const std::vector<std::vector<double>>& Q,
                                              const std::vector<int>& solution,
                                              int flip_index) const {
    int n = static_cast<int>(Q.size());
    int current_val = solution[flip_index];
    int new_val = 1 - current_val;

    // Change in diagonal term
    double delta = Q[flip_index][flip_index] * (new_val - current_val);

    // Change in off-diagonal terms
    for (int j = 0; j < n; ++j) {
        if (j == flip_index) continue;

        double q_ij;
        if (flip_index < j) {
            q_ij = Q[flip_index][j];
        } else {
            q_ij = Q[j][flip_index];
        }

        delta += q_ij * solution[j] * (new_val - current_val);
    }

    return delta;
}

void MockQuantumSolver::setSeed(unsigned int seed) {
    config_.seed = seed;
    rng_.seed(seed);
}

void MockQuantumSolver::setDeterministic(bool deterministic) {
    config_.deterministic = deterministic;
    if (deterministic) {
        rng_.seed(config_.seed);
    }
}

// =============================================================================
// Factory Function
// =============================================================================

std::unique_ptr<QuantumClient> createQuantumClient(const QuantumSolverConfig& config,
                                                    bool use_mock) {
    if (use_mock || config.provider == "mock") {
        // For mock mode, we'll use the D-Wave client in mock mode
        // The actual mock implementation is in the DWaveClient
        QuantumSolverConfig mock_config = config;
        mock_config.provider = "mock";
        return std::make_unique<DWaveClient>(mock_config);
    }

    if (config.provider == "dwave") {
        return std::make_unique<DWaveClient>(config);
    } else if (config.provider == "ibm") {
        return std::make_unique<IBMQuantumClient>(config);
    }

    // Default to D-Wave
    return std::make_unique<DWaveClient>(config);
}

}  // namespace lumen::solvers
