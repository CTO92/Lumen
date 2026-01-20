#pragma once

/// @file explainer.hpp
/// @brief Explanation generation for optimization results
///
/// This module generates human-readable explanations for portfolio
/// optimization results, including trade rationales and sensitivity analysis.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumen/core/constraint.hpp"
#include "lumen/core/portfolio.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/data/tax_lot.hpp"
#include "lumen/explain/provenance.hpp"

namespace lumen::explain {

/// Rationale for a single trade recommendation
struct TradeRationale {
    core::Trade trade;                         ///< The recommended trade
    std::vector<std::string> reasons;          ///< List of reasons for this trade
    double impact_on_objective;                ///< How much this trade improves objective
    std::vector<std::string> constraints_addressed;  ///< Constraints this trade helps satisfy
    double before_allocation_pct;              ///< Allocation before trade
    double after_allocation_pct;               ///< Allocation after trade
    double target_allocation_pct;              ///< Target allocation
    std::optional<data::CapitalGainsReport> tax_impact;  ///< Tax impact if applicable

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Comparison of allocation before and after optimization
struct AllocationComparison {
    std::map<std::string, double> before;           ///< Allocation before trades
    std::map<std::string, double> after;            ///< Allocation after trades
    std::map<std::string, double> target;           ///< Target allocation
    std::map<std::string, double> deviation_before; ///< Deviation from target before
    std::map<std::string, double> deviation_after;  ///< Deviation from target after
    double total_drift_before;                      ///< Total drift before
    double total_drift_after;                       ///< Total drift after
    double drift_reduction_pct;                     ///< Percent reduction in drift

    nlohmann::json toJSON() const;
    std::string toPlainText() const;
};

/// Breakdown of transaction costs
struct CostBreakdown {
    double total_transaction_cost;         ///< Total cost
    double commission_cost;                ///< Commissions
    double spread_cost;                    ///< Bid-ask spread cost estimate
    double market_impact_estimate;         ///< Estimated market impact
    double tax_cost_estimate;              ///< Estimated tax cost
    std::map<std::string, double> per_trade_costs;  ///< Cost per trade

    nlohmann::json toJSON() const;
};

/// Result of sensitivity analysis
struct SensitivityResult {
    std::string parameter_name;     ///< Parameter that was varied
    double original_value;          ///< Original parameter value
    double changed_value;           ///< New parameter value
    double change_percent;          ///< Percent change
    double new_objective_value;     ///< Objective value with changed parameter
    double objective_change_pct;    ///< Percent change in objective
    std::string impact_description; ///< Human-readable description

    nlohmann::json toJSON() const;
};

/// Performs sensitivity analysis on optimization results
class SensitivityAnalysis {
public:
    SensitivityAnalysis(const core::SolverResult& result, const core::Portfolio& portfolio,
                        const core::TargetAllocation& target);
    ~SensitivityAnalysis();

    /// Analyze impact of price changes
    SensitivityResult priceShock(const std::string& ticker, double pct_change);

    /// Analyze all prices with same shock
    std::vector<SensitivityResult> allPriceShocks(double pct_change);

    /// Analyze impact of relaxing a constraint
    SensitivityResult relaxConstraint(const std::string& constraint_name, double delta);

    /// Analyze all constraints
    std::vector<SensitivityResult> allConstraintSensitivities();

    /// Analyze impact of changing target weight
    SensitivityResult changeTargetWeight(const std::string& ticker, double new_weight);

    /// Run full sensitivity analysis
    std::vector<SensitivityResult> runFullAnalysis();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Output format for explanation documents
enum class OutputFormat { JSON, PLAIN_TEXT, HTML, MARKDOWN };

/// Complete explanation document
class ExplanationDocument {
public:
    ExplanationDocument();
    ~ExplanationDocument();

    /// Set summary text
    void setSummary(const std::string& summary);

    /// Set allocation comparison
    void setAllocationComparison(const AllocationComparison& comparison);

    /// Add trade rationale
    void addTradeRationale(const TradeRationale& rationale);

    /// Set cost breakdown
    void setCostBreakdown(const CostBreakdown& costs);

    /// Set provenance information
    void setProvenance(const Provenance& provenance);

    /// Add sensitivity result
    void addSensitivityResult(const SensitivityResult& result);

    /// Set tax report
    void setTaxReport(const data::CapitalGainsReport& report);

    /// Render document in specified format
    std::string render(OutputFormat format) const;

    /// Convert to JSON
    nlohmann::json toJSON() const;

    /// Convert to plain text
    std::string toPlainText() const;

    /// Convert to HTML
    std::string toHTML() const;

    /// Convert to Markdown
    std::string toMarkdown() const;

    /// Save to file
    void saveToFile(const std::string& filepath, OutputFormat format) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Main class for generating explanations
class Explainer {
public:
    Explainer();
    ~Explainer();

    /// Generate rationale for a single trade
    TradeRationale explainTrade(const core::Trade& trade, const core::Portfolio& portfolio,
                                 const core::TargetAllocation& target,
                                 const core::ConstraintSet& constraints,
                                 const core::SolverResult& result);

    /// Compare allocations before and after trades
    AllocationComparison compareAllocations(const core::Portfolio& before,
                                            const std::vector<core::Trade>& trades,
                                            const core::TargetAllocation& target);

    /// Calculate cost breakdown
    CostBreakdown calculateCosts(const std::vector<core::Trade>& trades,
                                  double commission_per_trade = 0.0,
                                  double spread_estimate_bps = 5.0);

    /// Generate complete explanation document
    ExplanationDocument generateFullExplanation(const core::Portfolio& portfolio,
                                                 const core::TargetAllocation& target,
                                                 const core::ConstraintSet& constraints,
                                                 const core::SolverResult& result,
                                                 const Provenance& provenance);

    /// Set verbosity level (0-3)
    void setVerbosity(int level);

    /// Enable/disable sensitivity analysis
    void enableSensitivityAnalysis(bool enable);

    /// Set commission rate for cost calculation
    void setCommissionRate(double rate);

    /// Set tax optimizer for tax impact calculation
    void setTaxOptimizer(data::TaxOptimizer* optimizer);

    /// Set current prices for tax calculations
    void setCurrentPrices(const std::map<std::string, double>& prices);

    /// Calculate tax impact for a set of trades
    data::CapitalGainsReport calculateTaxImpact(const std::vector<core::Trade>& trades) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lumen::explain
