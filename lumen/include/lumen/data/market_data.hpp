#pragma once

/// @file market_data.hpp
/// @brief Market data retrieval and caching
///
/// This module provides interfaces and implementations for fetching
/// real-time and historical market data from various providers.

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lumen {

namespace utils {
class Configuration;
}

namespace data {

/// Real-time price quote
struct PriceQuote {
    std::string ticker;       ///< Security symbol
    double price;             ///< Last trade price
    double bid;               ///< Bid price
    double ask;               ///< Ask price
    double volume;            ///< Trading volume
    std::chrono::system_clock::time_point timestamp;  ///< Quote timestamp
    std::string source;       ///< Data provider name

    nlohmann::json toJSON() const;
    static PriceQuote fromJSON(const nlohmann::json& j);
};

/// OHLCV (Open, High, Low, Close, Volume) data point
struct OHLCV {
    std::chrono::system_clock::time_point date;
    double open;
    double high;
    double low;
    double close;
    long volume;

    nlohmann::json toJSON() const;
    static OHLCV fromJSON(const nlohmann::json& j);
};

/// Time series of historical price data
struct TimeSeries {
    std::string ticker;             ///< Security symbol
    std::vector<OHLCV> data;        ///< Historical data points
    std::string interval;           ///< Data interval: "daily", "weekly", "monthly"

    /// Get close prices as a vector
    std::vector<double> getClosePrices() const;

    /// Calculate daily returns
    std::vector<double> getReturns() const;

    /// Calculate mean return
    double getMean() const;

    /// Calculate standard deviation
    double getStdDev() const;

    /// Calculate annualized volatility
    double getAnnualizedVolatility() const;

    nlohmann::json toJSON() const;
    static TimeSeries fromJSON(const nlohmann::json& j);
};

/// Abstract interface for market data providers
class MarketDataProvider {
public:
    virtual ~MarketDataProvider() = default;

    /// Get provider name
    virtual std::string getName() const = 0;

    /// Check if authenticated
    virtual bool isAuthenticated() const = 0;

    /// Authenticate with provider
    virtual bool authenticate() = 0;

    /// Get current price for a single ticker
    virtual PriceQuote getCurrentPrice(const std::string& ticker) = 0;

    /// Get current prices for multiple tickers
    virtual std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers) = 0;

    /// Get historical data for a ticker
    virtual TimeSeries getHistoricalData(
        const std::string& ticker,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) = 0;

    /// Get remaining API rate limit
    virtual int getRateLimitRemaining() const = 0;

    /// Wait for rate limit to reset
    virtual void waitForRateLimit() = 0;
};

/// Alpha Vantage market data provider
class AlphaVantageProvider : public MarketDataProvider {
public:
    AlphaVantageProvider();
    explicit AlphaVantageProvider(const std::string& api_key);
    ~AlphaVantageProvider() override;

    std::string getName() const override { return "Alpha Vantage"; }
    bool isAuthenticated() const override;
    bool authenticate() override;

    PriceQuote getCurrentPrice(const std::string& ticker) override;
    std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers) override;
    TimeSeries getHistoricalData(
        const std::string& ticker,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) override;

    int getRateLimitRemaining() const override;
    void waitForRateLimit() override;

    // Alpha Vantage specific
    TimeSeries getDailyAdjusted(const std::string& ticker);
    TimeSeries getIntraday(const std::string& ticker, const std::string& interval);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Yahoo Finance market data provider (no API key required)
class YahooFinanceProvider : public MarketDataProvider {
public:
    YahooFinanceProvider();
    ~YahooFinanceProvider() override;

    std::string getName() const override { return "Yahoo Finance"; }
    bool isAuthenticated() const override { return true; }
    bool authenticate() override { return true; }

    PriceQuote getCurrentPrice(const std::string& ticker) override;
    std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers) override;
    TimeSeries getHistoricalData(
        const std::string& ticker,
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end) override;

    int getRateLimitRemaining() const override { return 100; }
    void waitForRateLimit() override {}

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Covariance matrix calculator
class CovarianceCalculator {
public:
    /// Calculate covariance matrix from historical time series
    static std::vector<std::vector<double>> calculate(
        const std::vector<TimeSeries>& series);

    /// Calculate correlation matrix
    static std::vector<std::vector<double>> calculateCorrelation(
        const std::vector<TimeSeries>& series);

    /// Calculate expected returns from historical data
    static std::vector<double> calculateExpectedReturns(
        const std::vector<TimeSeries>& series);

    /// Annualize daily covariance matrix
    static std::vector<std::vector<double>> annualizeCovariance(
        const std::vector<std::vector<double>>& daily_cov);

    /// Annualize daily returns
    static std::vector<double> annualizeReturns(const std::vector<double>& daily_returns);

    /// Calculate portfolio variance
    static double calculatePortfolioVariance(
        const std::vector<double>& weights,
        const std::vector<std::vector<double>>& cov);

    /// Calculate portfolio volatility
    static double calculatePortfolioVolatility(
        const std::vector<double>& weights,
        const std::vector<std::vector<double>>& cov);

    /// Calculate Sharpe ratio
    static double calculateSharpeRatio(
        const std::vector<double>& weights,
        const std::vector<double>& returns,
        const std::vector<std::vector<double>>& cov,
        double risk_free_rate = 0.02);
};

/// SQLite-backed cache for market data
class DataCache {
public:
    explicit DataCache(const std::string& db_path);
    ~DataCache();

    /// Get cached price quote
    std::optional<PriceQuote> getPrice(const std::string& ticker) const;

    /// Cache a price quote
    void putPrice(const std::string& ticker, const PriceQuote& quote, int ttl_minutes = 60);

    /// Check if cached price is stale
    bool isPriceStale(const std::string& ticker) const;

    /// Get cached time series
    std::optional<TimeSeries> getTimeSeries(const std::string& ticker,
                                            const std::string& interval) const;

    /// Cache time series data
    void putTimeSeries(const std::string& ticker, const TimeSeries& series,
                       int ttl_minutes = 1440);

    /// Remove expired entries
    void purgeExpired();

    /// Clear all cached data
    void clearAll();

    /// Get cache size (number of entries)
    size_t getCacheSize() const;

    /// Get cache hit rate
    double getHitRate() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// High-level market data client with caching
class MarketDataClient {
public:
    explicit MarketDataClient(const utils::Configuration& config);
    ~MarketDataClient();

    /// Get current price (uses cache + providers)
    PriceQuote getCurrentPrice(const std::string& ticker);

    /// Get batch prices
    std::map<std::string, PriceQuote> getBatchPrices(
        const std::vector<std::string>& tickers);

    /// Get historical data
    TimeSeries getHistoricalData(const std::string& ticker, int days_back = 252);

    /// Get covariance matrix for a set of tickers
    std::vector<std::vector<double>> getCovarianceMatrix(
        const std::vector<std::string>& tickers);

    /// Get expected returns for a set of tickers
    std::vector<double> getExpectedReturns(const std::vector<std::string>& tickers);

    /// Add a market data provider
    void addProvider(std::unique_ptr<MarketDataProvider> provider);

    /// Set primary provider
    void setPrimaryProvider(const std::string& name);

    /// Enable/disable caching
    void enableCache(bool enable);

    /// Set cache TTL
    void setCacheTTL(int minutes);

    /// Get the cache instance
    DataCache& getCache();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace data
}  // namespace lumen
