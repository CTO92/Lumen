/// @file market_data.cpp
/// @brief Market data provider implementations
///
/// Implementation of market data providers for Alpha Vantage and Yahoo Finance,
/// including SQLite-backed caching and rate limiting.

#include "lumen/data/market_data.hpp"
#include "lumen/utils/logging.hpp"
#include "lumen/utils/config.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include <sqlite3.h>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>

namespace lumen::data {

// =============================================================================
// PriceQuote
// =============================================================================

nlohmann::json PriceQuote::toJSON() const {
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return nlohmann::json{
        {"ticker", ticker},
        {"price", price},
        {"bid", bid},
        {"ask", ask},
        {"volume", volume},
        {"timestamp", ts},
        {"source", source}
    };
}

PriceQuote PriceQuote::fromJSON(const nlohmann::json& j) {
    PriceQuote q;
    q.ticker = j.value("ticker", "");
    q.price = j.value("price", 0.0);
    q.bid = j.value("bid", 0.0);
    q.ask = j.value("ask", 0.0);
    q.volume = j.value("volume", 0.0);
    if (j.contains("timestamp")) {
        auto ts = j["timestamp"].get<int64_t>();
        q.timestamp = std::chrono::system_clock::from_time_t(ts);
    } else {
        q.timestamp = std::chrono::system_clock::now();
    }
    q.source = j.value("source", "");
    return q;
}

// =============================================================================
// OHLCV
// =============================================================================

nlohmann::json OHLCV::toJSON() const {
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(
        date.time_since_epoch()).count();
    return nlohmann::json{
        {"date", ts},
        {"open", open},
        {"high", high},
        {"low", low},
        {"close", close},
        {"volume", volume}
    };
}

OHLCV OHLCV::fromJSON(const nlohmann::json& j) {
    OHLCV bar;
    if (j.contains("date")) {
        auto ts = j["date"].get<int64_t>();
        bar.date = std::chrono::system_clock::from_time_t(ts);
    }
    bar.open = j.value("open", 0.0);
    bar.high = j.value("high", 0.0);
    bar.low = j.value("low", 0.0);
    bar.close = j.value("close", 0.0);
    bar.volume = j.value("volume", 0L);
    return bar;
}

// =============================================================================
// TimeSeries
// =============================================================================

std::vector<double> TimeSeries::getClosePrices() const {
    std::vector<double> prices;
    prices.reserve(data.size());
    for (const auto& bar : data) {
        prices.push_back(bar.close);
    }
    return prices;
}

std::vector<double> TimeSeries::getReturns() const {
    std::vector<double> returns;
    if (data.size() < 2) return returns;

    returns.reserve(data.size() - 1);
    for (size_t i = 1; i < data.size(); ++i) {
        if (data[i-1].close > 0) {
            double ret = (data[i].close - data[i-1].close) / data[i-1].close;
            returns.push_back(ret);
        }
    }
    return returns;
}

double TimeSeries::getMean() const {
    auto returns = getReturns();
    if (returns.empty()) return 0.0;

    double sum = 0.0;
    for (double r : returns) {
        sum += r;
    }
    return sum / static_cast<double>(returns.size());
}

double TimeSeries::getStdDev() const {
    auto returns = getReturns();
    if (returns.size() < 2) return 0.0;

    double mean = getMean();
    double sum_sq = 0.0;
    for (double r : returns) {
        sum_sq += (r - mean) * (r - mean);
    }
    return std::sqrt(sum_sq / static_cast<double>(returns.size() - 1));
}

double TimeSeries::getAnnualizedVolatility() const {
    // Assuming daily data, annualize with sqrt(252)
    return getStdDev() * std::sqrt(252.0);
}

nlohmann::json TimeSeries::toJSON() const {
    nlohmann::json j;
    j["ticker"] = ticker;
    j["interval"] = interval;
    j["data"] = nlohmann::json::array();
    for (const auto& bar : data) {
        j["data"].push_back(bar.toJSON());
    }
    return j;
}

TimeSeries TimeSeries::fromJSON(const nlohmann::json& j) {
    TimeSeries ts;
    ts.ticker = j.value("ticker", "");
    ts.interval = j.value("interval", "daily");
    if (j.contains("data") && j["data"].is_array()) {
        for (const auto& bar_json : j["data"]) {
            ts.data.push_back(OHLCV::fromJSON(bar_json));
        }
    }
    return ts;
}

// =============================================================================
// AlphaVantageProvider Implementation
// =============================================================================

class AlphaVantageProvider::Impl {
public:
    std::string api_key_;
    std::unique_ptr<httplib::Client> client_;
    int rate_limit_remaining_ = 5;
    std::chrono::steady_clock::time_point last_call_time_;
    std::mutex mutex_;

    static constexpr int CALLS_PER_MINUTE = 5;  // Free tier
    static constexpr int MIN_CALL_INTERVAL_MS = 12000;  // 60000ms / 5 calls

    Impl() : last_call_time_(std::chrono::steady_clock::now() - std::chrono::seconds(60)) {
        client_ = std::make_unique<httplib::Client>("https://www.alphavantage.co");
        client_->set_connection_timeout(10);
        client_->set_read_timeout(30);
    }

    void waitForRateLimit() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_call_time_).count();

        if (elapsed < MIN_CALL_INTERVAL_MS) {
            auto wait_time = MIN_CALL_INTERVAL_MS - elapsed;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
        }

        last_call_time_ = std::chrono::steady_clock::now();
    }

    std::optional<nlohmann::json> makeRequest(const std::string& function,
                                               const std::map<std::string, std::string>& params) {
        if (api_key_.empty()) {
            LOG_ERROR("Alpha Vantage API key not configured");
            return std::nullopt;
        }

        waitForRateLimit();

        std::string path = "/query?function=" + function + "&apikey=" + api_key_;
        for (const auto& [key, value] : params) {
            path += "&" + key + "=" + value;
        }

        LOG_DEBUG("Alpha Vantage request: " + function);

        auto res = client_->Get(path);
        if (!res) {
            LOG_ERROR("Alpha Vantage request failed: connection error");
            return std::nullopt;
        }

        if (res->status != 200) {
            LOG_ERROR("Alpha Vantage request failed: HTTP " + std::to_string(res->status));
            return std::nullopt;
        }

        try {
            auto json = nlohmann::json::parse(res->body);

            // Check for API error messages
            if (json.contains("Error Message")) {
                LOG_ERROR("Alpha Vantage error: " + json["Error Message"].get<std::string>());
                return std::nullopt;
            }
            if (json.contains("Note")) {
                LOG_WARN("Alpha Vantage rate limit: " + json["Note"].get<std::string>());
                return std::nullopt;
            }

            return json;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse Alpha Vantage response: " + std::string(e.what()));
            return std::nullopt;
        }
    }
};

AlphaVantageProvider::AlphaVantageProvider() : impl_(std::make_unique<Impl>()) {}

AlphaVantageProvider::AlphaVantageProvider(const std::string& api_key)
    : impl_(std::make_unique<Impl>()) {
    impl_->api_key_ = api_key;
}

AlphaVantageProvider::~AlphaVantageProvider() = default;

bool AlphaVantageProvider::isAuthenticated() const {
    return !impl_->api_key_.empty();
}

bool AlphaVantageProvider::authenticate() {
    // Try to get API key from environment if not already set
    if (impl_->api_key_.empty()) {
        const char* key = std::getenv("ALPHA_VANTAGE_API_KEY");
        if (key) {
            impl_->api_key_ = key;
        }
    }
    return isAuthenticated();
}

PriceQuote AlphaVantageProvider::getCurrentPrice(const std::string& ticker) {
    PriceQuote quote;
    quote.ticker = ticker;
    quote.source = "Alpha Vantage";
    quote.timestamp = std::chrono::system_clock::now();

    auto json = impl_->makeRequest("GLOBAL_QUOTE", {{"symbol", ticker}});
    if (!json) {
        return quote;
    }

    try {
        if (json->contains("Global Quote")) {
            const auto& gq = (*json)["Global Quote"];
            quote.price = std::stod(gq.value("05. price", "0"));
            quote.volume = std::stod(gq.value("06. volume", "0"));
            // Alpha Vantage doesn't provide bid/ask in free tier
            quote.bid = quote.price;
            quote.ask = quote.price;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse quote for " + ticker + ": " + e.what());
    }

    return quote;
}

std::map<std::string, PriceQuote> AlphaVantageProvider::getBatchPrices(
    const std::vector<std::string>& tickers) {
    std::map<std::string, PriceQuote> results;

    // Alpha Vantage free tier doesn't support batch quotes, so we fetch one by one
    for (const auto& ticker : tickers) {
        results[ticker] = getCurrentPrice(ticker);
    }

    return results;
}

TimeSeries AlphaVantageProvider::getHistoricalData(
    const std::string& ticker,
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {

    // Use TIME_SERIES_DAILY_ADJUSTED for historical data
    return getDailyAdjusted(ticker);
}

int AlphaVantageProvider::getRateLimitRemaining() const {
    return impl_->rate_limit_remaining_;
}

void AlphaVantageProvider::waitForRateLimit() {
    impl_->waitForRateLimit();
}

TimeSeries AlphaVantageProvider::getDailyAdjusted(const std::string& ticker) {
    TimeSeries ts;
    ts.ticker = ticker;
    ts.interval = "daily";

    auto json = impl_->makeRequest("TIME_SERIES_DAILY_ADJUSTED",
                                   {{"symbol", ticker}, {"outputsize", "full"}});
    if (!json) {
        return ts;
    }

    try {
        if (json->contains("Time Series (Daily)")) {
            const auto& daily = (*json)["Time Series (Daily)"];

            for (auto it = daily.begin(); it != daily.end(); ++it) {
                OHLCV bar;

                // Parse date string "YYYY-MM-DD"
                std::tm tm = {};
                std::istringstream ss(it.key());
                ss >> std::get_time(&tm, "%Y-%m-%d");
                bar.date = std::chrono::system_clock::from_time_t(std::mktime(&tm));

                bar.open = std::stod(it.value().value("1. open", "0"));
                bar.high = std::stod(it.value().value("2. high", "0"));
                bar.low = std::stod(it.value().value("3. low", "0"));
                bar.close = std::stod(it.value().value("5. adjusted close", "0"));
                bar.volume = std::stol(it.value().value("6. volume", "0"));

                ts.data.push_back(bar);
            }

            // Sort by date ascending
            std::sort(ts.data.begin(), ts.data.end(),
                     [](const OHLCV& a, const OHLCV& b) { return a.date < b.date; });
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse daily data for " + ticker + ": " + e.what());
    }

    return ts;
}

TimeSeries AlphaVantageProvider::getIntraday(const std::string& ticker,
                                              const std::string& interval) {
    TimeSeries ts;
    ts.ticker = ticker;
    ts.interval = interval;

    auto json = impl_->makeRequest("TIME_SERIES_INTRADAY",
                                   {{"symbol", ticker}, {"interval", interval}});
    if (!json) {
        return ts;
    }

    std::string key = "Time Series (" + interval + ")";
    try {
        if (json->contains(key)) {
            const auto& series = (*json)[key];

            for (auto it = series.begin(); it != series.end(); ++it) {
                OHLCV bar;

                // Parse datetime string
                std::tm tm = {};
                std::istringstream ss(it.key());
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                bar.date = std::chrono::system_clock::from_time_t(std::mktime(&tm));

                bar.open = std::stod(it.value().value("1. open", "0"));
                bar.high = std::stod(it.value().value("2. high", "0"));
                bar.low = std::stod(it.value().value("3. low", "0"));
                bar.close = std::stod(it.value().value("4. close", "0"));
                bar.volume = std::stol(it.value().value("5. volume", "0"));

                ts.data.push_back(bar);
            }

            std::sort(ts.data.begin(), ts.data.end(),
                     [](const OHLCV& a, const OHLCV& b) { return a.date < b.date; });
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse intraday data for " + ticker + ": " + e.what());
    }

    return ts;
}

// =============================================================================
// YahooFinanceProvider Implementation
// =============================================================================

class YahooFinanceProvider::Impl {
public:
    std::unique_ptr<httplib::Client> client_;
    std::mutex mutex_;

    Impl() {
        client_ = std::make_unique<httplib::Client>("https://query1.finance.yahoo.com");
        client_->set_connection_timeout(10);
        client_->set_read_timeout(30);
        // Yahoo Finance requires a user agent
        client_->set_default_headers({
            {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}
        });
    }

    std::string buildHistoryURL(const std::string& ticker, long period1, long period2) {
        std::ostringstream oss;
        oss << "/v8/finance/chart/" << ticker
            << "?period1=" << period1
            << "&period2=" << period2
            << "&interval=1d"
            << "&includePrePost=false";
        return oss.str();
    }

    std::string buildQuoteURL(const std::string& ticker) {
        return "/v8/finance/chart/" + ticker + "?interval=1d&range=1d";
    }
};

YahooFinanceProvider::YahooFinanceProvider() : impl_(std::make_unique<Impl>()) {}

YahooFinanceProvider::~YahooFinanceProvider() = default;

PriceQuote YahooFinanceProvider::getCurrentPrice(const std::string& ticker) {
    PriceQuote quote;
    quote.ticker = ticker;
    quote.source = "Yahoo Finance";
    quote.timestamp = std::chrono::system_clock::now();

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    auto res = impl_->client_->Get(impl_->buildQuoteURL(ticker));
    if (!res || res->status != 200) {
        LOG_ERROR("Yahoo Finance request failed for " + ticker);
        return quote;
    }

    try {
        auto json = nlohmann::json::parse(res->body);

        if (json.contains("chart") && json["chart"].contains("result") &&
            !json["chart"]["result"].empty()) {

            const auto& result = json["chart"]["result"][0];

            if (result.contains("meta")) {
                const auto& meta = result["meta"];
                quote.price = meta.value("regularMarketPrice", 0.0);
                quote.volume = meta.value("regularMarketVolume", 0.0);
                quote.bid = quote.price;  // Yahoo doesn't always provide bid/ask
                quote.ask = quote.price;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Yahoo quote for " + ticker + ": " + e.what());
    }

    return quote;
}

std::map<std::string, PriceQuote> YahooFinanceProvider::getBatchPrices(
    const std::vector<std::string>& tickers) {
    std::map<std::string, PriceQuote> results;

    for (const auto& ticker : tickers) {
        results[ticker] = getCurrentPrice(ticker);
    }

    return results;
}

TimeSeries YahooFinanceProvider::getHistoricalData(
    const std::string& ticker,
    const std::chrono::system_clock::time_point& start,
    const std::chrono::system_clock::time_point& end) {

    TimeSeries ts;
    ts.ticker = ticker;
    ts.interval = "daily";

    long period1 = std::chrono::duration_cast<std::chrono::seconds>(
        start.time_since_epoch()).count();
    long period2 = std::chrono::duration_cast<std::chrono::seconds>(
        end.time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    auto res = impl_->client_->Get(impl_->buildHistoryURL(ticker, period1, period2));
    if (!res || res->status != 200) {
        LOG_ERROR("Yahoo Finance history request failed for " + ticker);
        return ts;
    }

    try {
        auto json = nlohmann::json::parse(res->body);

        if (json.contains("chart") && json["chart"].contains("result") &&
            !json["chart"]["result"].empty()) {

            const auto& result = json["chart"]["result"][0];

            if (result.contains("timestamp") && result.contains("indicators")) {
                const auto& timestamps = result["timestamp"];
                const auto& quote = result["indicators"]["quote"][0];

                const auto& opens = quote["open"];
                const auto& highs = quote["high"];
                const auto& lows = quote["low"];
                const auto& closes = quote["close"];
                const auto& volumes = quote["volume"];

                for (size_t i = 0; i < timestamps.size(); ++i) {
                    OHLCV bar;
                    bar.date = std::chrono::system_clock::from_time_t(
                        timestamps[i].get<long>());

                    // Handle null values
                    bar.open = opens[i].is_null() ? 0.0 : opens[i].get<double>();
                    bar.high = highs[i].is_null() ? 0.0 : highs[i].get<double>();
                    bar.low = lows[i].is_null() ? 0.0 : lows[i].get<double>();
                    bar.close = closes[i].is_null() ? 0.0 : closes[i].get<double>();
                    bar.volume = volumes[i].is_null() ? 0L : volumes[i].get<long>();

                    // Skip invalid bars
                    if (bar.close > 0) {
                        ts.data.push_back(bar);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse Yahoo history for " + ticker + ": " + e.what());
    }

    return ts;
}

// =============================================================================
// CovarianceCalculator
// =============================================================================

std::vector<std::vector<double>> CovarianceCalculator::calculate(
    const std::vector<TimeSeries>& series) {

    size_t n = series.size();
    std::vector<std::vector<double>> cov(n, std::vector<double>(n, 0.0));

    if (n == 0) return cov;

    // Get returns for each series
    std::vector<std::vector<double>> returns;
    returns.reserve(n);
    for (const auto& ts : series) {
        returns.push_back(ts.getReturns());
    }

    // Find minimum length
    size_t min_len = SIZE_MAX;
    for (const auto& r : returns) {
        min_len = std::min(min_len, r.size());
    }

    if (min_len < 2) return cov;

    // Calculate means
    std::vector<double> means(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t t = 0; t < min_len; ++t) {
            means[i] += returns[i][t];
        }
        means[i] /= static_cast<double>(min_len);
    }

    // Calculate covariance matrix
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i; j < n; ++j) {
            double sum = 0.0;
            for (size_t t = 0; t < min_len; ++t) {
                sum += (returns[i][t] - means[i]) * (returns[j][t] - means[j]);
            }
            cov[i][j] = sum / static_cast<double>(min_len - 1);
            cov[j][i] = cov[i][j];  // Symmetric
        }
    }

    return cov;
}

std::vector<std::vector<double>> CovarianceCalculator::calculateCorrelation(
    const std::vector<TimeSeries>& series) {

    auto cov = calculate(series);
    size_t n = cov.size();

    // Extract standard deviations (diagonal elements)
    std::vector<double> stddevs(n);
    for (size_t i = 0; i < n; ++i) {
        stddevs[i] = std::sqrt(cov[i][i]);
    }

    // Convert to correlation
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            if (stddevs[i] > 0 && stddevs[j] > 0) {
                cov[i][j] /= (stddevs[i] * stddevs[j]);
            } else {
                cov[i][j] = (i == j) ? 1.0 : 0.0;
            }
        }
    }

    return cov;
}

std::vector<double> CovarianceCalculator::calculateExpectedReturns(
    const std::vector<TimeSeries>& series) {

    std::vector<double> expected;
    expected.reserve(series.size());

    for (const auto& ts : series) {
        expected.push_back(ts.getMean());
    }

    return expected;
}

std::vector<std::vector<double>> CovarianceCalculator::annualizeCovariance(
    const std::vector<std::vector<double>>& daily_cov) {

    std::vector<std::vector<double>> annual = daily_cov;
    for (auto& row : annual) {
        for (auto& val : row) {
            val *= 252.0;  // Trading days per year
        }
    }
    return annual;
}

std::vector<double> CovarianceCalculator::annualizeReturns(
    const std::vector<double>& daily_returns) {

    std::vector<double> annual = daily_returns;
    for (auto& r : annual) {
        r *= 252.0;
    }
    return annual;
}

double CovarianceCalculator::calculatePortfolioVariance(
    const std::vector<double>& weights,
    const std::vector<std::vector<double>>& cov) {

    size_t n = weights.size();
    if (n != cov.size()) return 0.0;

    double variance = 0.0;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            variance += weights[i] * weights[j] * cov[i][j];
        }
    }

    return variance;
}

double CovarianceCalculator::calculatePortfolioVolatility(
    const std::vector<double>& weights,
    const std::vector<std::vector<double>>& cov) {

    return std::sqrt(calculatePortfolioVariance(weights, cov));
}

double CovarianceCalculator::calculateSharpeRatio(
    const std::vector<double>& weights,
    const std::vector<double>& returns,
    const std::vector<std::vector<double>>& cov,
    double risk_free_rate) {

    // Calculate portfolio return
    double portfolio_return = 0.0;
    for (size_t i = 0; i < weights.size(); ++i) {
        portfolio_return += weights[i] * returns[i];
    }

    double volatility = calculatePortfolioVolatility(weights, cov);
    if (volatility <= 0) return 0.0;

    return (portfolio_return - risk_free_rate) / volatility;
}

// =============================================================================
// DataCache Implementation (SQLite-backed)
// =============================================================================

class DataCache::Impl {
public:
    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;
    mutable size_t hits_ = 0;
    mutable size_t misses_ = 0;

    ~Impl() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    bool open(const std::string& db_path) {
        std::lock_guard<std::mutex> lock(mutex_);

        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to open cache database: " + std::string(sqlite3_errmsg(db_)));
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }

        // Enable WAL mode for better concurrent access
        sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

        return createTables();
    }

    bool createTables() {
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS price_cache (
                ticker TEXT PRIMARY KEY,
                data TEXT NOT NULL,
                expires_at INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS timeseries_cache (
                ticker TEXT NOT NULL,
                interval TEXT NOT NULL,
                data TEXT NOT NULL,
                expires_at INTEGER NOT NULL,
                PRIMARY KEY (ticker, interval)
            );

            CREATE INDEX IF NOT EXISTS idx_price_expires ON price_cache(expires_at);
            CREATE INDEX IF NOT EXISTS idx_ts_expires ON timeseries_cache(expires_at);
        )";

        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to create cache tables: " + std::string(error_msg));
            sqlite3_free(error_msg);
            return false;
        }

        return true;
    }

    int64_t getCurrentTime() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

DataCache::DataCache(const std::string& db_path) : impl_(std::make_unique<Impl>()) {
    if (!impl_->open(db_path)) {
        LOG_WARN("Cache database failed to open, caching disabled");
    }
}

DataCache::~DataCache() = default;

std::optional<PriceQuote> DataCache::getPrice(const std::string& ticker) const {
    if (!impl_->db_) return std::nullopt;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    const char* sql = "SELECT data FROM price_cache WHERE ticker = ? AND expires_at > ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(impl_->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, impl_->getCurrentTime());

    std::optional<PriceQuote> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        try {
            auto json = nlohmann::json::parse(data);
            result = PriceQuote::fromJSON(json);
            impl_->hits_++;
        } catch (...) {}
    } else {
        impl_->misses_++;
    }

    sqlite3_finalize(stmt);
    return result;
}

void DataCache::putPrice(const std::string& ticker, const PriceQuote& quote, int ttl_minutes) {
    if (!impl_->db_) return;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO price_cache (ticker, data, expires_at)
        VALUES (?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    std::string data = quote.toJSON().dump();
    int64_t expires = impl_->getCurrentTime() + (ttl_minutes * 60);

    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, data.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, expires);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

bool DataCache::isPriceStale(const std::string& ticker) const {
    return !getPrice(ticker).has_value();
}

std::optional<TimeSeries> DataCache::getTimeSeries(const std::string& ticker,
                                                    const std::string& interval) const {
    if (!impl_->db_) return std::nullopt;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    const char* sql = R"(
        SELECT data FROM timeseries_cache
        WHERE ticker = ? AND interval = ? AND expires_at > ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, interval.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, impl_->getCurrentTime());

    std::optional<TimeSeries> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        try {
            auto json = nlohmann::json::parse(data);
            result = TimeSeries::fromJSON(json);
            impl_->hits_++;
        } catch (...) {}
    } else {
        impl_->misses_++;
    }

    sqlite3_finalize(stmt);
    return result;
}

void DataCache::putTimeSeries(const std::string& ticker, const TimeSeries& series,
                               int ttl_minutes) {
    if (!impl_->db_) return;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO timeseries_cache (ticker, interval, data, expires_at)
        VALUES (?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return;
    }

    std::string data = series.toJSON().dump();
    int64_t expires = impl_->getCurrentTime() + (ttl_minutes * 60);

    sqlite3_bind_text(stmt, 1, ticker.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, series.interval.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, data.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, expires);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void DataCache::purgeExpired() {
    if (!impl_->db_) return;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    int64_t now = impl_->getCurrentTime();

    std::string sql1 = "DELETE FROM price_cache WHERE expires_at <= " + std::to_string(now) + ";";
    std::string sql2 = "DELETE FROM timeseries_cache WHERE expires_at <= " + std::to_string(now) + ";";

    sqlite3_exec(impl_->db_, sql1.c_str(), nullptr, nullptr, nullptr);
    sqlite3_exec(impl_->db_, sql2.c_str(), nullptr, nullptr, nullptr);
}

void DataCache::clearAll() {
    if (!impl_->db_) return;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    sqlite3_exec(impl_->db_, "DELETE FROM price_cache;", nullptr, nullptr, nullptr);
    sqlite3_exec(impl_->db_, "DELETE FROM timeseries_cache;", nullptr, nullptr, nullptr);

    impl_->hits_ = 0;
    impl_->misses_ = 0;
}

size_t DataCache::getCacheSize() const {
    if (!impl_->db_) return 0;

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    size_t count = 0;
    const char* sql1 = "SELECT COUNT(*) FROM price_cache;";
    const char* sql2 = "SELECT COUNT(*) FROM timeseries_cache;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(impl_->db_, sql1, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count += sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(impl_->db_, sql2, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count += sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return count;
}

double DataCache::getHitRate() const {
    size_t total = impl_->hits_ + impl_->misses_;
    if (total == 0) return 0.0;
    return static_cast<double>(impl_->hits_) / static_cast<double>(total);
}

// =============================================================================
// MarketDataClient Implementation
// =============================================================================

class MarketDataClient::Impl {
public:
    std::vector<std::unique_ptr<MarketDataProvider>> providers_;
    std::string primary_provider_name_;
    std::unique_ptr<DataCache> cache_;
    bool cache_enabled_ = true;
    int cache_ttl_minutes_ = 60;

    MarketDataProvider* getPrimaryProvider() {
        if (!primary_provider_name_.empty()) {
            for (auto& p : providers_) {
                if (p->getName() == primary_provider_name_) {
                    return p.get();
                }
            }
        }
        // Return first available
        return providers_.empty() ? nullptr : providers_[0].get();
    }

    MarketDataProvider* getFallbackProvider() {
        auto* primary = getPrimaryProvider();
        for (auto& p : providers_) {
            if (p.get() != primary) {
                return p.get();
            }
        }
        return nullptr;
    }
};

MarketDataClient::MarketDataClient(const utils::Configuration& config)
    : impl_(std::make_unique<Impl>()) {

    // Initialize cache
    std::string cache_path = utils::Configuration::getHomeDirectory() + "/data/market_cache.db";
    impl_->cache_ = std::make_unique<DataCache>(cache_path);

    // Get config settings
    const auto& md_config = config.getMarketDataConfig();
    impl_->cache_ttl_minutes_ = md_config.cache_ttl_minutes;
    impl_->primary_provider_name_ = md_config.primary_provider;

    // Set up providers based on config
    if (md_config.primary_provider == "alpha_vantage" ||
        std::find(md_config.fallback_providers.begin(),
                  md_config.fallback_providers.end(),
                  "alpha_vantage") != md_config.fallback_providers.end()) {
        auto av = std::make_unique<AlphaVantageProvider>();
        av->authenticate();
        if (av->isAuthenticated()) {
            impl_->providers_.push_back(std::move(av));
        }
    }

    // Yahoo Finance as fallback (no auth required)
    impl_->providers_.push_back(std::make_unique<YahooFinanceProvider>());
}

MarketDataClient::~MarketDataClient() = default;

PriceQuote MarketDataClient::getCurrentPrice(const std::string& ticker) {
    // Check cache first
    if (impl_->cache_enabled_) {
        auto cached = impl_->cache_->getPrice(ticker);
        if (cached) {
            return *cached;
        }
    }

    // Try primary provider
    auto* primary = impl_->getPrimaryProvider();
    if (primary) {
        auto quote = primary->getCurrentPrice(ticker);
        if (quote.price > 0) {
            if (impl_->cache_enabled_) {
                impl_->cache_->putPrice(ticker, quote, impl_->cache_ttl_minutes_);
            }
            return quote;
        }
    }

    // Try fallback
    auto* fallback = impl_->getFallbackProvider();
    if (fallback) {
        auto quote = fallback->getCurrentPrice(ticker);
        if (quote.price > 0) {
            if (impl_->cache_enabled_) {
                impl_->cache_->putPrice(ticker, quote, impl_->cache_ttl_minutes_);
            }
            return quote;
        }
    }

    // Return empty quote
    PriceQuote empty;
    empty.ticker = ticker;
    empty.timestamp = std::chrono::system_clock::now();
    return empty;
}

std::map<std::string, PriceQuote> MarketDataClient::getBatchPrices(
    const std::vector<std::string>& tickers) {

    std::map<std::string, PriceQuote> results;
    std::vector<std::string> uncached;

    // Check cache first
    if (impl_->cache_enabled_) {
        for (const auto& ticker : tickers) {
            auto cached = impl_->cache_->getPrice(ticker);
            if (cached) {
                results[ticker] = *cached;
            } else {
                uncached.push_back(ticker);
            }
        }
    } else {
        uncached = tickers;
    }

    // Fetch uncached
    if (!uncached.empty()) {
        auto* primary = impl_->getPrimaryProvider();
        if (primary) {
            auto fetched = primary->getBatchPrices(uncached);
            for (auto& [ticker, quote] : fetched) {
                if (quote.price > 0) {
                    results[ticker] = quote;
                    if (impl_->cache_enabled_) {
                        impl_->cache_->putPrice(ticker, quote, impl_->cache_ttl_minutes_);
                    }
                }
            }
        }
    }

    return results;
}

TimeSeries MarketDataClient::getHistoricalData(const std::string& ticker, int days_back) {
    // Check cache first
    if (impl_->cache_enabled_) {
        auto cached = impl_->cache_->getTimeSeries(ticker, "daily");
        if (cached) {
            return *cached;
        }
    }

    auto end = std::chrono::system_clock::now();
    auto start = end - std::chrono::hours(24 * days_back);

    // Try primary provider
    auto* primary = impl_->getPrimaryProvider();
    if (primary) {
        auto ts = primary->getHistoricalData(ticker, start, end);
        if (!ts.data.empty()) {
            if (impl_->cache_enabled_) {
                impl_->cache_->putTimeSeries(ticker, ts, 1440);  // 24 hours
            }
            return ts;
        }
    }

    // Try fallback
    auto* fallback = impl_->getFallbackProvider();
    if (fallback) {
        auto ts = fallback->getHistoricalData(ticker, start, end);
        if (!ts.data.empty()) {
            if (impl_->cache_enabled_) {
                impl_->cache_->putTimeSeries(ticker, ts, 1440);
            }
            return ts;
        }
    }

    // Return empty
    TimeSeries empty;
    empty.ticker = ticker;
    empty.interval = "daily";
    return empty;
}

std::vector<std::vector<double>> MarketDataClient::getCovarianceMatrix(
    const std::vector<std::string>& tickers) {

    std::vector<TimeSeries> series;
    series.reserve(tickers.size());

    for (const auto& ticker : tickers) {
        series.push_back(getHistoricalData(ticker, 252));
    }

    return CovarianceCalculator::calculate(series);
}

std::vector<double> MarketDataClient::getExpectedReturns(
    const std::vector<std::string>& tickers) {

    std::vector<TimeSeries> series;
    series.reserve(tickers.size());

    for (const auto& ticker : tickers) {
        series.push_back(getHistoricalData(ticker, 252));
    }

    return CovarianceCalculator::calculateExpectedReturns(series);
}

void MarketDataClient::addProvider(std::unique_ptr<MarketDataProvider> provider) {
    impl_->providers_.push_back(std::move(provider));
}

void MarketDataClient::setPrimaryProvider(const std::string& name) {
    impl_->primary_provider_name_ = name;
}

void MarketDataClient::enableCache(bool enable) {
    impl_->cache_enabled_ = enable;
}

void MarketDataClient::setCacheTTL(int minutes) {
    impl_->cache_ttl_minutes_ = minutes;
}

DataCache& MarketDataClient::getCache() {
    return *impl_->cache_;
}

}  // namespace lumen::data
