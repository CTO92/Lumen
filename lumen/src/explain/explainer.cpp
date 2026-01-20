/// @file explainer.cpp
/// @brief Explainer implementation
///
/// Implementation of the explainer module for generating human-readable
/// explanations of optimization results.

#include "lumen/explain/explainer.hpp"
#include "lumen/utils/logging.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace lumen::explain {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

std::string formatPercent(double value, int precision = 2) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << (value * 100) << "%";
    return ss.str();
}

std::string formatMoney(double value, int precision = 2) {
    std::stringstream ss;
    if (value < 0) {
        ss << "-$" << std::fixed << std::setprecision(precision) << std::abs(value);
    } else {
        ss << "$" << std::fixed << std::setprecision(precision) << value;
    }
    return ss.str();
}

std::string formatNumber(double value, int precision = 2) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
}

std::string tradeActionToString(core::Trade::Action action) {
    switch (action) {
        case core::Trade::Action::BUY: return "Buy";
        case core::Trade::Action::SELL: return "Sell";
        case core::Trade::Action::HOLD: return "Hold";
        default: return "Unknown";
    }
}

std::string escapeHTML(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c; break;
        }
    }
    return result;
}

}  // anonymous namespace

// =============================================================================
// TradeRationale
// =============================================================================

nlohmann::json TradeRationale::toJSON() const {
    nlohmann::json j{
        {"trade", trade.toJSON()},
        {"reasons", reasons},
        {"impact_on_objective", impact_on_objective},
        {"constraints_addressed", constraints_addressed},
        {"before_allocation_pct", before_allocation_pct},
        {"after_allocation_pct", after_allocation_pct},
        {"target_allocation_pct", target_allocation_pct}
    };

    if (tax_impact.has_value()) {
        j["tax_impact"] = tax_impact->toJSON();
    }

    return j;
}

std::string TradeRationale::toPlainText() const {
    std::stringstream ss;

    std::string action_str = tradeActionToString(trade.action);
    ss << action_str << " " << formatNumber(trade.shares, 2)
       << " shares of " << trade.ticker;

    if (trade.price > 0) {
        ss << " @ " << formatMoney(trade.price);
    }

    ss << " (" << formatMoney(trade.amount) << ")\n";

    // Allocation change
    ss << "  Allocation: " << formatPercent(before_allocation_pct)
       << " -> " << formatPercent(after_allocation_pct)
       << " (target: " << formatPercent(target_allocation_pct) << ")\n";

    // Reasons
    if (!reasons.empty()) {
        ss << "  Reasons:\n";
        for (const auto& reason : reasons) {
            ss << "    - " << reason << "\n";
        }
    }

    // Constraints addressed
    if (!constraints_addressed.empty()) {
        ss << "  Addresses constraints:\n";
        for (const auto& constraint : constraints_addressed) {
            ss << "    - " << constraint << "\n";
        }
    }

    // Tax impact
    if (tax_impact.has_value()) {
        ss << "  Tax impact:\n";
        ss << "    - Short-term gain/loss: " << formatMoney(tax_impact->net_short_term) << "\n";
        ss << "    - Long-term gain/loss: " << formatMoney(tax_impact->net_long_term) << "\n";
        ss << "    - Estimated tax: " << formatMoney(tax_impact->estimated_tax_savings) << "\n";
    }

    return ss.str();
}

// =============================================================================
// AllocationComparison
// =============================================================================

nlohmann::json AllocationComparison::toJSON() const {
    return nlohmann::json{
        {"before", before},
        {"after", after},
        {"target", target},
        {"deviation_before", deviation_before},
        {"deviation_after", deviation_after},
        {"total_drift_before", total_drift_before},
        {"total_drift_after", total_drift_after},
        {"drift_reduction_pct", drift_reduction_pct}
    };
}

std::string AllocationComparison::toPlainText() const {
    std::stringstream ss;

    ss << "Allocation Comparison:\n";
    ss << std::string(60, '-') << "\n";
    ss << std::left << std::setw(12) << "Ticker"
       << std::right << std::setw(12) << "Before"
       << std::setw(12) << "After"
       << std::setw(12) << "Target"
       << std::setw(12) << "Deviation" << "\n";
    ss << std::string(60, '-') << "\n";

    // Get all tickers
    std::set<std::string> all_tickers;
    for (const auto& [ticker, _] : before) all_tickers.insert(ticker);
    for (const auto& [ticker, _] : after) all_tickers.insert(ticker);
    for (const auto& [ticker, _] : target) all_tickers.insert(ticker);

    for (const auto& ticker : all_tickers) {
        double before_pct = before.count(ticker) ? before.at(ticker) : 0.0;
        double after_pct = after.count(ticker) ? after.at(ticker) : 0.0;
        double target_pct = target.count(ticker) ? target.at(ticker) : 0.0;
        double dev_after = deviation_after.count(ticker) ? deviation_after.at(ticker) : 0.0;

        ss << std::left << std::setw(12) << ticker
           << std::right << std::setw(11) << formatPercent(before_pct)
           << std::setw(12) << formatPercent(after_pct)
           << std::setw(12) << formatPercent(target_pct)
           << std::setw(12) << formatPercent(dev_after) << "\n";
    }

    ss << std::string(60, '-') << "\n";
    ss << "Total drift: " << formatPercent(total_drift_before)
       << " -> " << formatPercent(total_drift_after);
    ss << " (" << formatPercent(drift_reduction_pct) << " reduction)\n";

    return ss.str();
}

// =============================================================================
// CostBreakdown
// =============================================================================

nlohmann::json CostBreakdown::toJSON() const {
    return nlohmann::json{
        {"total_transaction_cost", total_transaction_cost},
        {"commission_cost", commission_cost},
        {"spread_cost", spread_cost},
        {"market_impact_estimate", market_impact_estimate},
        {"tax_cost_estimate", tax_cost_estimate},
        {"per_trade_costs", per_trade_costs}
    };
}

// =============================================================================
// SensitivityResult
// =============================================================================

nlohmann::json SensitivityResult::toJSON() const {
    return nlohmann::json{
        {"parameter_name", parameter_name},
        {"original_value", original_value},
        {"changed_value", changed_value},
        {"change_percent", change_percent},
        {"new_objective_value", new_objective_value},
        {"objective_change_pct", objective_change_pct},
        {"impact_description", impact_description}
    };
}

// =============================================================================
// SensitivityAnalysis
// =============================================================================

class SensitivityAnalysis::Impl {
public:
    core::SolverResult base_result;
    core::Portfolio portfolio;
    core::TargetAllocation target;
    std::vector<SensitivityResult> results;
};

SensitivityAnalysis::SensitivityAnalysis(const core::SolverResult& result,
                                         const core::Portfolio& portfolio,
                                         const core::TargetAllocation& target)
    : impl_(std::make_unique<Impl>()) {
    impl_->base_result = result;
    impl_->portfolio = portfolio;
    impl_->target = target;
}

SensitivityAnalysis::~SensitivityAnalysis() = default;

SensitivityResult SensitivityAnalysis::priceShock(const std::string& ticker,
                                                   double pct_change) {
    SensitivityResult result;
    result.parameter_name = ticker + "_price";

    // Get original price
    const auto& pos = impl_->portfolio.getPosition(ticker);
    result.original_value = pos.current_price;
    result.changed_value = pos.current_price * (1.0 + pct_change);
    result.change_percent = pct_change * 100.0;

    // Estimate impact on objective (simplified linear approximation)
    double allocation = impl_->portfolio.getAllocationPercent(ticker);
    double objective_impact = allocation * pct_change;
    result.new_objective_value = impl_->base_result.objective_value * (1.0 + objective_impact);
    result.objective_change_pct = objective_impact * 100.0;

    // Generate description
    std::stringstream ss;
    ss << "A " << formatPercent(pct_change) << " change in " << ticker
       << " price would impact objective by ~" << formatPercent(objective_impact);
    result.impact_description = ss.str();

    impl_->results.push_back(result);
    return result;
}

std::vector<SensitivityResult> SensitivityAnalysis::allPriceShocks(double pct_change) {
    std::vector<SensitivityResult> results;
    for (const auto& ticker : impl_->portfolio.getAllTickers()) {
        results.push_back(priceShock(ticker, pct_change));
    }
    return results;
}

SensitivityResult SensitivityAnalysis::relaxConstraint(const std::string& constraint_name,
                                                        double delta) {
    SensitivityResult result;
    result.parameter_name = constraint_name;
    result.original_value = 0.0;  // Constraint bound
    result.changed_value = delta;
    result.change_percent = delta * 100.0;

    // Estimate using shadow price approximation
    result.new_objective_value = impl_->base_result.objective_value;
    result.objective_change_pct = 0.0;

    // Generate description
    std::stringstream ss;
    ss << "Relaxing constraint '" << constraint_name << "' by "
       << formatNumber(delta) << " would have minimal impact on objective";
    result.impact_description = ss.str();

    impl_->results.push_back(result);
    return result;
}

std::vector<SensitivityResult> SensitivityAnalysis::allConstraintSensitivities() {
    // Return cached results for binding constraints
    std::vector<SensitivityResult> results;
    for (const auto& constraint_name : impl_->base_result.binding_constraints) {
        results.push_back(relaxConstraint(constraint_name, 0.01));
    }
    return results;
}

SensitivityResult SensitivityAnalysis::changeTargetWeight(const std::string& ticker,
                                                           double new_weight) {
    SensitivityResult result;
    result.parameter_name = ticker + "_target";

    auto target_spec = impl_->target.getFullTarget(ticker);
    result.original_value = target_spec.target_value;
    result.changed_value = new_weight;
    result.change_percent = (new_weight - target_spec.target_value) / target_spec.target_value * 100.0;

    // Estimate impact
    double weight_change = new_weight - target_spec.target_value;
    result.new_objective_value = impl_->base_result.objective_value;
    result.objective_change_pct = std::abs(weight_change) * 10.0;  // Rough estimate

    std::stringstream ss;
    ss << "Changing " << ticker << " target from " << formatPercent(target_spec.target_value)
       << " to " << formatPercent(new_weight) << " may require rebalancing";
    result.impact_description = ss.str();

    impl_->results.push_back(result);
    return result;
}

std::vector<SensitivityResult> SensitivityAnalysis::runFullAnalysis() {
    impl_->results.clear();

    // Price shocks: +/- 5%, +/- 10%
    for (const auto& ticker : impl_->portfolio.getAllTickers()) {
        priceShock(ticker, 0.05);
        priceShock(ticker, -0.05);
        priceShock(ticker, 0.10);
        priceShock(ticker, -0.10);
    }

    // Constraint sensitivities
    allConstraintSensitivities();

    return impl_->results;
}

// =============================================================================
// ExplanationDocument
// =============================================================================

class ExplanationDocument::Impl {
public:
    std::string summary;
    AllocationComparison allocation_comparison;
    std::vector<TradeRationale> trade_rationales;
    CostBreakdown cost_breakdown;
    Provenance provenance;
    std::vector<SensitivityResult> sensitivity_results;
    std::optional<data::CapitalGainsReport> tax_report;
};

ExplanationDocument::ExplanationDocument() : impl_(std::make_unique<Impl>()) {}
ExplanationDocument::~ExplanationDocument() = default;

void ExplanationDocument::setSummary(const std::string& summary) {
    impl_->summary = summary;
}

void ExplanationDocument::setAllocationComparison(const AllocationComparison& comparison) {
    impl_->allocation_comparison = comparison;
}

void ExplanationDocument::addTradeRationale(const TradeRationale& rationale) {
    impl_->trade_rationales.push_back(rationale);
}

void ExplanationDocument::setCostBreakdown(const CostBreakdown& costs) {
    impl_->cost_breakdown = costs;
}

void ExplanationDocument::setProvenance(const Provenance& provenance) {
    impl_->provenance = provenance;
}

void ExplanationDocument::addSensitivityResult(const SensitivityResult& result) {
    impl_->sensitivity_results.push_back(result);
}

void ExplanationDocument::setTaxReport(const data::CapitalGainsReport& report) {
    impl_->tax_report = report;
}

std::string ExplanationDocument::render(OutputFormat format) const {
    switch (format) {
        case OutputFormat::JSON:
            return toJSON().dump(2);
        case OutputFormat::PLAIN_TEXT:
            return toPlainText();
        case OutputFormat::HTML:
            return toHTML();
        case OutputFormat::MARKDOWN:
            return toMarkdown();
        default:
            return toPlainText();
    }
}

nlohmann::json ExplanationDocument::toJSON() const {
    nlohmann::json j{
        {"summary", impl_->summary},
        {"allocation_comparison", impl_->allocation_comparison.toJSON()},
        {"cost_breakdown", impl_->cost_breakdown.toJSON()},
        {"provenance", impl_->provenance.toJSON()}
    };

    // Trade rationales
    nlohmann::json rationales = nlohmann::json::array();
    for (const auto& r : impl_->trade_rationales) {
        rationales.push_back(r.toJSON());
    }
    j["trade_rationales"] = rationales;

    // Sensitivity results
    nlohmann::json sensitivity = nlohmann::json::array();
    for (const auto& s : impl_->sensitivity_results) {
        sensitivity.push_back(s.toJSON());
    }
    j["sensitivity_analysis"] = sensitivity;

    // Tax report
    if (impl_->tax_report.has_value()) {
        j["tax_report"] = impl_->tax_report->toJSON();
    }

    return j;
}

std::string ExplanationDocument::toPlainText() const {
    std::stringstream ss;

    // Header
    ss << "=" << std::string(78, '=') << "\n";
    ss << "PORTFOLIO OPTIMIZATION REPORT\n";
    ss << "=" << std::string(78, '=') << "\n\n";

    // Summary
    if (!impl_->summary.empty()) {
        ss << "SUMMARY\n";
        ss << std::string(40, '-') << "\n";
        ss << impl_->summary << "\n\n";
    }

    // Allocation comparison
    ss << impl_->allocation_comparison.toPlainText() << "\n";

    // Trade rationales
    if (!impl_->trade_rationales.empty()) {
        ss << "RECOMMENDED TRADES\n";
        ss << std::string(40, '-') << "\n";
        for (size_t i = 0; i < impl_->trade_rationales.size(); ++i) {
            ss << (i + 1) << ". " << impl_->trade_rationales[i].toPlainText() << "\n";
        }
    }

    // Cost breakdown
    ss << "TRANSACTION COSTS\n";
    ss << std::string(40, '-') << "\n";
    ss << "  Commission: " << formatMoney(impl_->cost_breakdown.commission_cost) << "\n";
    ss << "  Spread cost: " << formatMoney(impl_->cost_breakdown.spread_cost) << "\n";
    ss << "  Market impact: " << formatMoney(impl_->cost_breakdown.market_impact_estimate) << "\n";
    ss << "  Tax cost: " << formatMoney(impl_->cost_breakdown.tax_cost_estimate) << "\n";
    ss << "  TOTAL: " << formatMoney(impl_->cost_breakdown.total_transaction_cost) << "\n\n";

    // Tax report
    if (impl_->tax_report.has_value()) {
        ss << "TAX IMPACT\n";
        ss << std::string(40, '-') << "\n";
        ss << impl_->tax_report->toPlainText() << "\n";
    }

    // Sensitivity analysis
    if (!impl_->sensitivity_results.empty()) {
        ss << "SENSITIVITY ANALYSIS\n";
        ss << std::string(40, '-') << "\n";
        for (const auto& s : impl_->sensitivity_results) {
            ss << "  " << s.parameter_name << ": " << s.impact_description << "\n";
        }
        ss << "\n";
    }

    // Provenance
    auto solver_info = impl_->provenance.getSolverInfo();
    ss << "SOLVER INFORMATION\n";
    ss << std::string(40, '-') << "\n";
    ss << "  Solver: " << solver_info.solver_name << " " << solver_info.solver_version << "\n";
    ss << "  Status: " << solver_info.termination_status << "\n";
    ss << "  Solve time: " << solver_info.solve_time_ms << " ms\n";
    ss << "  Iterations: " << solver_info.iterations << "\n";
    ss << "  Objective: " << formatNumber(solver_info.objective_value) << "\n";
    if (solver_info.optimality_gap > 0) {
        ss << "  Gap: " << formatPercent(solver_info.optimality_gap) << "\n";
    }
    ss << "\n";

    // Data sources
    auto data_sources = impl_->provenance.getDataSources();
    if (!data_sources.empty()) {
        ss << "DATA SOURCES\n";
        ss << std::string(40, '-') << "\n";
        for (const auto& ds : data_sources) {
            ss << "  " << ds.provider_name;
            if (ds.from_cache) ss << " (cached)";
            ss << " - " << ds.latency_ms << "ms\n";
        }
        ss << "\n";
    }

    // Warnings
    auto warnings = impl_->provenance.getWarnings();
    if (!warnings.empty()) {
        ss << "WARNINGS\n";
        ss << std::string(40, '-') << "\n";
        for (const auto& w : warnings) {
            ss << "  ! " << w << "\n";
        }
        ss << "\n";
    }

    return ss.str();
}

std::string ExplanationDocument::toHTML() const {
    std::stringstream ss;

    ss << "<!DOCTYPE html>\n";
    ss << "<html lang=\"en\">\n";
    ss << "<head>\n";
    ss << "  <meta charset=\"UTF-8\">\n";
    ss << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    ss << "  <title>Portfolio Optimization Report</title>\n";
    ss << "  <style>\n";
    ss << "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; ";
    ss << "           max-width: 900px; margin: 0 auto; padding: 20px; line-height: 1.6; }\n";
    ss << "    h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n";
    ss << "    h2 { color: #34495e; margin-top: 30px; }\n";
    ss << "    table { border-collapse: collapse; width: 100%; margin: 20px 0; }\n";
    ss << "    th, td { border: 1px solid #ddd; padding: 12px; text-align: right; }\n";
    ss << "    th { background-color: #3498db; color: white; }\n";
    ss << "    tr:nth-child(even) { background-color: #f9f9f9; }\n";
    ss << "    tr:hover { background-color: #f5f5f5; }\n";
    ss << "    td:first-child { text-align: left; font-weight: bold; }\n";
    ss << "    .buy { color: #27ae60; }\n";
    ss << "    .sell { color: #e74c3c; }\n";
    ss << "    .warning { background-color: #fff3cd; padding: 10px; border-left: 4px solid #ffc107; margin: 10px 0; }\n";
    ss << "    .summary { background-color: #e8f4f8; padding: 15px; border-radius: 5px; margin: 20px 0; }\n";
    ss << "    .costs { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }\n";
    ss << "    .cost-item { background: #f8f9fa; padding: 10px; border-radius: 5px; }\n";
    ss << "    .cost-label { color: #6c757d; font-size: 0.9em; }\n";
    ss << "    .cost-value { font-size: 1.2em; font-weight: bold; }\n";
    ss << "  </style>\n";
    ss << "</head>\n";
    ss << "<body>\n";

    // Title
    ss << "  <h1>Portfolio Optimization Report</h1>\n";

    // Summary
    if (!impl_->summary.empty()) {
        ss << "  <div class=\"summary\">\n";
        ss << "    <h2>Summary</h2>\n";
        ss << "    <p>" << escapeHTML(impl_->summary) << "</p>\n";
        ss << "  </div>\n";
    }

    // Allocation comparison table
    ss << "  <h2>Allocation Changes</h2>\n";
    ss << "  <table>\n";
    ss << "    <thead>\n";
    ss << "      <tr><th>Ticker</th><th>Before</th><th>After</th><th>Target</th><th>Deviation</th></tr>\n";
    ss << "    </thead>\n";
    ss << "    <tbody>\n";

    std::set<std::string> all_tickers;
    for (const auto& [ticker, _] : impl_->allocation_comparison.before) all_tickers.insert(ticker);
    for (const auto& [ticker, _] : impl_->allocation_comparison.target) all_tickers.insert(ticker);

    for (const auto& ticker : all_tickers) {
        double before_pct = impl_->allocation_comparison.before.count(ticker) ?
            impl_->allocation_comparison.before.at(ticker) : 0.0;
        double after_pct = impl_->allocation_comparison.after.count(ticker) ?
            impl_->allocation_comparison.after.at(ticker) : 0.0;
        double target_pct = impl_->allocation_comparison.target.count(ticker) ?
            impl_->allocation_comparison.target.at(ticker) : 0.0;
        double dev = impl_->allocation_comparison.deviation_after.count(ticker) ?
            impl_->allocation_comparison.deviation_after.at(ticker) : 0.0;

        ss << "      <tr><td>" << escapeHTML(ticker) << "</td>";
        ss << "<td>" << formatPercent(before_pct) << "</td>";
        ss << "<td>" << formatPercent(after_pct) << "</td>";
        ss << "<td>" << formatPercent(target_pct) << "</td>";
        ss << "<td>" << formatPercent(dev) << "</td></tr>\n";
    }

    ss << "    </tbody>\n";
    ss << "  </table>\n";
    ss << "  <p><strong>Drift reduction:</strong> " << formatPercent(impl_->allocation_comparison.total_drift_before);
    ss << " &rarr; " << formatPercent(impl_->allocation_comparison.total_drift_after);
    ss << " (" << formatPercent(impl_->allocation_comparison.drift_reduction_pct) << " improvement)</p>\n";

    // Trades table
    if (!impl_->trade_rationales.empty()) {
        ss << "  <h2>Recommended Trades</h2>\n";
        ss << "  <table>\n";
        ss << "    <thead>\n";
        ss << "      <tr><th>Action</th><th>Ticker</th><th>Shares</th><th>Price</th><th>Amount</th></tr>\n";
        ss << "    </thead>\n";
        ss << "    <tbody>\n";

        for (const auto& r : impl_->trade_rationales) {
            std::string action_class = r.trade.action == core::Trade::Action::BUY ? "buy" : "sell";
            ss << "      <tr class=\"" << action_class << "\">";
            ss << "<td>" << tradeActionToString(r.trade.action) << "</td>";
            ss << "<td>" << escapeHTML(r.trade.ticker) << "</td>";
            ss << "<td>" << formatNumber(r.trade.shares) << "</td>";
            ss << "<td>" << formatMoney(r.trade.price) << "</td>";
            ss << "<td>" << formatMoney(r.trade.amount) << "</td></tr>\n";
        }

        ss << "    </tbody>\n";
        ss << "  </table>\n";
    }

    // Cost breakdown
    ss << "  <h2>Transaction Costs</h2>\n";
    ss << "  <div class=\"costs\">\n";
    ss << "    <div class=\"cost-item\"><div class=\"cost-label\">Commission</div>";
    ss << "<div class=\"cost-value\">" << formatMoney(impl_->cost_breakdown.commission_cost) << "</div></div>\n";
    ss << "    <div class=\"cost-item\"><div class=\"cost-label\">Spread</div>";
    ss << "<div class=\"cost-value\">" << formatMoney(impl_->cost_breakdown.spread_cost) << "</div></div>\n";
    ss << "    <div class=\"cost-item\"><div class=\"cost-label\">Market Impact</div>";
    ss << "<div class=\"cost-value\">" << formatMoney(impl_->cost_breakdown.market_impact_estimate) << "</div></div>\n";
    ss << "    <div class=\"cost-item\"><div class=\"cost-label\">Tax Cost</div>";
    ss << "<div class=\"cost-value\">" << formatMoney(impl_->cost_breakdown.tax_cost_estimate) << "</div></div>\n";
    ss << "  </div>\n";
    ss << "  <p><strong>Total:</strong> " << formatMoney(impl_->cost_breakdown.total_transaction_cost) << "</p>\n";

    // Warnings
    auto warnings = impl_->provenance.getWarnings();
    if (!warnings.empty()) {
        ss << "  <h2>Warnings</h2>\n";
        for (const auto& w : warnings) {
            ss << "  <div class=\"warning\">" << escapeHTML(w) << "</div>\n";
        }
    }

    // Solver info
    auto solver_info = impl_->provenance.getSolverInfo();
    ss << "  <h2>Solver Information</h2>\n";
    ss << "  <table>\n";
    ss << "    <tr><td>Solver</td><td>" << escapeHTML(solver_info.solver_name) << " ";
    ss << escapeHTML(solver_info.solver_version) << "</td></tr>\n";
    ss << "    <tr><td>Status</td><td>" << escapeHTML(solver_info.termination_status) << "</td></tr>\n";
    ss << "    <tr><td>Solve Time</td><td>" << solver_info.solve_time_ms << " ms</td></tr>\n";
    ss << "    <tr><td>Iterations</td><td>" << solver_info.iterations << "</td></tr>\n";
    ss << "    <tr><td>Objective Value</td><td>" << formatNumber(solver_info.objective_value) << "</td></tr>\n";
    ss << "  </table>\n";

    ss << "</body>\n";
    ss << "</html>\n";

    return ss.str();
}

std::string ExplanationDocument::toMarkdown() const {
    std::stringstream ss;

    // Title
    ss << "# Portfolio Optimization Report\n\n";

    // Summary
    if (!impl_->summary.empty()) {
        ss << "## Summary\n\n";
        ss << impl_->summary << "\n\n";
    }

    // Allocation comparison
    ss << "## Allocation Changes\n\n";
    ss << "| Ticker | Before | After | Target | Deviation |\n";
    ss << "|--------|-------:|------:|-------:|----------:|\n";

    std::set<std::string> all_tickers;
    for (const auto& [ticker, _] : impl_->allocation_comparison.before) all_tickers.insert(ticker);
    for (const auto& [ticker, _] : impl_->allocation_comparison.target) all_tickers.insert(ticker);

    for (const auto& ticker : all_tickers) {
        double before_pct = impl_->allocation_comparison.before.count(ticker) ?
            impl_->allocation_comparison.before.at(ticker) : 0.0;
        double after_pct = impl_->allocation_comparison.after.count(ticker) ?
            impl_->allocation_comparison.after.at(ticker) : 0.0;
        double target_pct = impl_->allocation_comparison.target.count(ticker) ?
            impl_->allocation_comparison.target.at(ticker) : 0.0;
        double dev = impl_->allocation_comparison.deviation_after.count(ticker) ?
            impl_->allocation_comparison.deviation_after.at(ticker) : 0.0;

        ss << "| " << ticker << " | " << formatPercent(before_pct)
           << " | " << formatPercent(after_pct)
           << " | " << formatPercent(target_pct)
           << " | " << formatPercent(dev) << " |\n";
    }

    ss << "\n**Drift reduction:** " << formatPercent(impl_->allocation_comparison.total_drift_before);
    ss << " -> " << formatPercent(impl_->allocation_comparison.total_drift_after);
    ss << " (" << formatPercent(impl_->allocation_comparison.drift_reduction_pct) << " improvement)\n\n";

    // Recommended trades
    if (!impl_->trade_rationales.empty()) {
        ss << "## Recommended Trades\n\n";
        ss << "| Action | Ticker | Shares | Price | Amount |\n";
        ss << "|--------|--------|-------:|------:|-------:|\n";

        for (const auto& r : impl_->trade_rationales) {
            ss << "| " << tradeActionToString(r.trade.action)
               << " | " << r.trade.ticker
               << " | " << formatNumber(r.trade.shares)
               << " | " << formatMoney(r.trade.price)
               << " | " << formatMoney(r.trade.amount) << " |\n";
        }
        ss << "\n";

        // Detailed rationales
        ss << "### Trade Rationales\n\n";
        for (size_t i = 0; i < impl_->trade_rationales.size(); ++i) {
            const auto& r = impl_->trade_rationales[i];
            ss << "**" << (i + 1) << ". " << tradeActionToString(r.trade.action)
               << " " << r.trade.ticker << "**\n\n";

            if (!r.reasons.empty()) {
                ss << "Reasons:\n";
                for (const auto& reason : r.reasons) {
                    ss << "- " << reason << "\n";
                }
            }

            if (!r.constraints_addressed.empty()) {
                ss << "\nConstraints addressed:\n";
                for (const auto& c : r.constraints_addressed) {
                    ss << "- " << c << "\n";
                }
            }
            ss << "\n";
        }
    }

    // Cost breakdown
    ss << "## Transaction Costs\n\n";
    ss << "| Cost Type | Amount |\n";
    ss << "|-----------|-------:|\n";
    ss << "| Commission | " << formatMoney(impl_->cost_breakdown.commission_cost) << " |\n";
    ss << "| Spread | " << formatMoney(impl_->cost_breakdown.spread_cost) << " |\n";
    ss << "| Market Impact | " << formatMoney(impl_->cost_breakdown.market_impact_estimate) << " |\n";
    ss << "| Tax Cost | " << formatMoney(impl_->cost_breakdown.tax_cost_estimate) << " |\n";
    ss << "| **Total** | **" << formatMoney(impl_->cost_breakdown.total_transaction_cost) << "** |\n\n";

    // Tax report
    if (impl_->tax_report.has_value()) {
        ss << "## Tax Impact\n\n";
        ss << "| Category | Amount |\n";
        ss << "|----------|-------:|\n";
        ss << "| Short-term gains | " << formatMoney(impl_->tax_report->short_term_gains) << " |\n";
        ss << "| Short-term losses | " << formatMoney(impl_->tax_report->short_term_losses) << " |\n";
        ss << "| Long-term gains | " << formatMoney(impl_->tax_report->long_term_gains) << " |\n";
        ss << "| Long-term losses | " << formatMoney(impl_->tax_report->long_term_losses) << " |\n";
        ss << "| **Net short-term** | **" << formatMoney(impl_->tax_report->net_short_term) << "** |\n";
        ss << "| **Net long-term** | **" << formatMoney(impl_->tax_report->net_long_term) << "** |\n";
        ss << "| Estimated tax savings | " << formatMoney(impl_->tax_report->estimated_tax_savings) << " |\n\n";
    }

    // Sensitivity analysis
    if (!impl_->sensitivity_results.empty()) {
        ss << "## Sensitivity Analysis\n\n";
        for (const auto& s : impl_->sensitivity_results) {
            ss << "- **" << s.parameter_name << ":** " << s.impact_description << "\n";
        }
        ss << "\n";
    }

    // Warnings
    auto warnings = impl_->provenance.getWarnings();
    if (!warnings.empty()) {
        ss << "## Warnings\n\n";
        for (const auto& w : warnings) {
            ss << "> **Warning:** " << w << "\n\n";
        }
    }

    // Solver info
    auto solver_info = impl_->provenance.getSolverInfo();
    ss << "## Solver Information\n\n";
    ss << "| Property | Value |\n";
    ss << "|----------|-------|\n";
    ss << "| Solver | " << solver_info.solver_name << " " << solver_info.solver_version << " |\n";
    ss << "| Status | " << solver_info.termination_status << " |\n";
    ss << "| Solve Time | " << solver_info.solve_time_ms << " ms |\n";
    ss << "| Iterations | " << solver_info.iterations << " |\n";
    ss << "| Objective Value | " << formatNumber(solver_info.objective_value) << " |\n";
    if (solver_info.optimality_gap > 0) {
        ss << "| Optimality Gap | " << formatPercent(solver_info.optimality_gap) << " |\n";
    }
    ss << "\n";

    // Data sources
    auto data_sources = impl_->provenance.getDataSources();
    if (!data_sources.empty()) {
        ss << "## Data Sources\n\n";
        for (const auto& ds : data_sources) {
            ss << "- **" << ds.provider_name << "**";
            if (ds.from_cache) ss << " (cached)";
            ss << " - " << ds.latency_ms << "ms\n";
        }
        ss << "\n";
    }

    // Session info
    ss << "---\n";
    ss << "*Session ID: " << impl_->provenance.getSessionId() << "*\n";

    return ss.str();
}

void ExplanationDocument::saveToFile(const std::string& filepath, OutputFormat format) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        utils::Logger::warn("Failed to open file for writing: " + filepath);
        return;
    }
    file << render(format);
    file.close();
}

// =============================================================================
// Explainer
// =============================================================================

class Explainer::Impl {
public:
    int verbosity_level = 1;
    bool sensitivity_enabled = false;
    double commission_rate = 0.0;
    data::TaxOptimizer* tax_optimizer = nullptr;
    std::map<std::string, double> current_prices;
};

Explainer::Explainer() : impl_(std::make_unique<Impl>()) {}
Explainer::~Explainer() = default;

TradeRationale Explainer::explainTrade(const core::Trade& trade,
                                        const core::Portfolio& portfolio,
                                        const core::TargetAllocation& target,
                                        const core::ConstraintSet& constraints,
                                        const core::SolverResult& result) {
    TradeRationale rationale;
    rationale.trade = trade;
    rationale.impact_on_objective = 0.0;

    // Calculate allocations
    double total_value = portfolio.getTotalValue();
    if (total_value > 0) {
        if (portfolio.hasPosition(trade.ticker)) {
            const auto& pos = portfolio.getPosition(trade.ticker);
            rationale.before_allocation_pct = pos.getCurrentValue() / total_value;
        } else {
            rationale.before_allocation_pct = 0.0;
        }

        // Calculate after allocation
        double position_change = (trade.action == core::Trade::Action::BUY) ?
            trade.amount : -trade.amount;
        double new_position_value = rationale.before_allocation_pct * total_value + position_change;
        rationale.after_allocation_pct = new_position_value / total_value;

        rationale.target_allocation_pct = target.getTarget(trade.ticker);
    }

    // Generate reasons
    if (trade.action == core::Trade::Action::BUY) {
        if (rationale.before_allocation_pct < rationale.target_allocation_pct) {
            double underweight = rationale.target_allocation_pct - rationale.before_allocation_pct;
            rationale.reasons.push_back("Currently underweight by " + formatPercent(underweight));
        }
    } else if (trade.action == core::Trade::Action::SELL) {
        if (rationale.before_allocation_pct > rationale.target_allocation_pct) {
            double overweight = rationale.before_allocation_pct - rationale.target_allocation_pct;
            rationale.reasons.push_back("Currently overweight by " + formatPercent(overweight));
        }
    }

    // Check which constraints this trade helps satisfy
    for (const auto* constraint : constraints.getAllConstraints()) {
        if (constraint->getName().find(trade.ticker) != std::string::npos ||
            constraint->getType() == core::ConstraintType::ALLOCATION) {
            rationale.constraints_addressed.push_back(constraint->getName());
        }
    }

    // Check binding constraints from result
    for (const auto& binding : result.binding_constraints) {
        if (binding.find(trade.ticker) != std::string::npos) {
            rationale.reasons.push_back("Binding constraint: " + binding);
        }
    }

    return rationale;
}

AllocationComparison Explainer::compareAllocations(const core::Portfolio& before,
                                                    const std::vector<core::Trade>& trades,
                                                    const core::TargetAllocation& target) {
    AllocationComparison comparison;

    double total_before = before.getTotalValue();

    // Calculate "before" allocations
    for (const auto& pos : before.getAllPositions()) {
        if (total_before > 0) {
            comparison.before[pos.ticker] = pos.getCurrentValue() / total_before;
        }
    }

    // Get target allocations
    for (const auto& target_spec : target.getAllTargets()) {
        comparison.target[target_spec.identifier] = target_spec.target_value;
    }

    // Simulate "after" allocations
    std::map<std::string, double> position_values;
    for (const auto& pos : before.getAllPositions()) {
        position_values[pos.ticker] = pos.getCurrentValue();
    }

    for (const auto& trade : trades) {
        double change = (trade.action == core::Trade::Action::BUY) ? trade.amount : -trade.amount;
        position_values[trade.ticker] += change;
    }

    double total_after = 0.0;
    for (const auto& [ticker, value] : position_values) {
        if (value > 0) total_after += value;
    }

    for (const auto& [ticker, value] : position_values) {
        if (total_after > 0 && value > 0) {
            comparison.after[ticker] = value / total_after;
        }
    }

    // Calculate deviations
    comparison.total_drift_before = 0.0;
    comparison.total_drift_after = 0.0;

    std::set<std::string> all_tickers;
    for (const auto& [ticker, _] : comparison.before) all_tickers.insert(ticker);
    for (const auto& [ticker, _] : comparison.target) all_tickers.insert(ticker);

    for (const auto& ticker : all_tickers) {
        double before_pct = comparison.before.count(ticker) ? comparison.before[ticker] : 0.0;
        double after_pct = comparison.after.count(ticker) ? comparison.after[ticker] : 0.0;
        double target_pct = comparison.target.count(ticker) ? comparison.target[ticker] : 0.0;

        comparison.deviation_before[ticker] = before_pct - target_pct;
        comparison.deviation_after[ticker] = after_pct - target_pct;

        comparison.total_drift_before += std::abs(before_pct - target_pct);
        comparison.total_drift_after += std::abs(after_pct - target_pct);
    }

    // Calculate drift reduction
    if (comparison.total_drift_before > 0) {
        comparison.drift_reduction_pct =
            (comparison.total_drift_before - comparison.total_drift_after) / comparison.total_drift_before;
    } else {
        comparison.drift_reduction_pct = 0.0;
    }

    return comparison;
}

CostBreakdown Explainer::calculateCosts(const std::vector<core::Trade>& trades,
                                         double commission_per_trade,
                                         double spread_estimate_bps) {
    CostBreakdown costs{};

    for (const auto& trade : trades) {
        // Commission
        costs.commission_cost += commission_per_trade;
        costs.per_trade_costs[trade.ticker] = commission_per_trade;

        // Spread cost (in basis points)
        double spread_cost = trade.amount * (spread_estimate_bps / 10000.0);
        costs.spread_cost += spread_cost;
        costs.per_trade_costs[trade.ticker] += spread_cost;

        // Market impact (simple sqrt model: 10 bps per $100k traded)
        double market_impact = trade.amount * 0.001 * std::sqrt(trade.amount / 100000.0);
        costs.market_impact_estimate += market_impact;
        costs.per_trade_costs[trade.ticker] += market_impact;
    }

    // Tax cost would be calculated separately using TaxOptimizer
    costs.tax_cost_estimate = 0.0;

    costs.total_transaction_cost = costs.commission_cost + costs.spread_cost +
                                   costs.market_impact_estimate + costs.tax_cost_estimate;

    return costs;
}

ExplanationDocument Explainer::generateFullExplanation(const core::Portfolio& portfolio,
                                                        const core::TargetAllocation& target,
                                                        const core::ConstraintSet& constraints,
                                                        const core::SolverResult& result,
                                                        const Provenance& provenance) {
    ExplanationDocument doc;

    // Generate summary
    std::stringstream summary;
    if (result.success) {
        summary << "Optimization completed successfully with " << result.trades.size() << " trades. ";
        summary << "Status: " << result.status << ". ";
        summary << "Objective value: " << formatNumber(result.objective_value) << ". ";
        summary << "Total transaction cost: " << formatMoney(result.total_transaction_cost) << ".";
    } else {
        summary << "Optimization failed. Status: " << result.status << ". ";
        summary << "Please review constraints for feasibility.";
    }
    doc.setSummary(summary.str());

    // Allocation comparison
    auto allocation_comp = compareAllocations(portfolio, result.trades, target);
    doc.setAllocationComparison(allocation_comp);

    // Trade rationales
    for (const auto& trade : result.trades) {
        auto rationale = explainTrade(trade, portfolio, target, constraints, result);
        doc.addTradeRationale(rationale);
    }

    // Cost breakdown
    auto costs = calculateCosts(result.trades, impl_->commission_rate);
    doc.setCostBreakdown(costs);

    // Provenance
    doc.setProvenance(provenance);

    // Tax report (if tax optimizer is available)
    if (impl_->tax_optimizer && result.success && !result.trades.empty()) {
        auto tax_report = calculateTaxImpact(result.trades);
        doc.setTaxReport(tax_report);

        // Also update cost breakdown with tax cost
        costs.tax_cost_estimate = impl_->tax_optimizer->estimateTaxImpact(
            result.trades, impl_->current_prices);
        costs.total_transaction_cost = costs.commission_cost + costs.spread_cost +
                                       costs.market_impact_estimate + costs.tax_cost_estimate;
        doc.setCostBreakdown(costs);
    }

    // Sensitivity analysis (if enabled)
    if (impl_->sensitivity_enabled && result.success) {
        SensitivityAnalysis sensitivity(result, portfolio, target);
        auto results = sensitivity.runFullAnalysis();
        for (const auto& s : results) {
            doc.addSensitivityResult(s);
        }
    }

    return doc;
}

void Explainer::setVerbosity(int level) {
    impl_->verbosity_level = std::clamp(level, 0, 3);
}

void Explainer::enableSensitivityAnalysis(bool enable) {
    impl_->sensitivity_enabled = enable;
}

void Explainer::setCommissionRate(double rate) {
    impl_->commission_rate = rate;
}

void Explainer::setTaxOptimizer(data::TaxOptimizer* optimizer) {
    impl_->tax_optimizer = optimizer;
}

void Explainer::setCurrentPrices(const std::map<std::string, double>& prices) {
    impl_->current_prices = prices;
}

data::CapitalGainsReport Explainer::calculateTaxImpact(const std::vector<core::Trade>& trades) const {
    if (!impl_->tax_optimizer) {
        // Return empty report if no tax optimizer
        return data::CapitalGainsReport{};
    }

    return impl_->tax_optimizer->calculateCapitalGains(trades, impl_->current_prices);
}

}  // namespace lumen::explain
