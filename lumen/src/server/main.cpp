/// @file main.cpp
/// @brief Lumen HTTP server entry point
///
/// Main entry point for the Lumen HTTP API server with security hardening:
/// - Token-based authentication
/// - TLS/SSL support
/// - Rate limiting
/// - Security headers
/// - Input validation

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/utils/logging.hpp"
#include "lumen/utils/config.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <random>
#include <regex>
#include <fstream>

using namespace lumen;
using json = nlohmann::json;

// =============================================================================
// Security Constants
// =============================================================================

constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;  // 10 MB max request body
constexpr size_t MAX_JSON_DEPTH = 32;                        // Max JSON nesting depth
constexpr int RATE_LIMIT_REQUESTS = 100;                     // Requests per window
constexpr int RATE_LIMIT_WINDOW_SECONDS = 60;                // Rate limit window
constexpr size_t MAX_TICKER_LENGTH = 10;                     // Max ticker symbol length

// =============================================================================
// Rate Limiter
// =============================================================================

class RateLimiter {
public:
    struct ClientInfo {
        int request_count = 0;
        std::chrono::steady_clock::time_point window_start;
    };

    bool isAllowed(const std::string& client_ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        auto it = clients_.find(client_ip);
        if (it == clients_.end()) {
            clients_[client_ip] = {1, now};
            return true;
        }

        auto& info = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - info.window_start).count();

        if (elapsed >= RATE_LIMIT_WINDOW_SECONDS) {
            // Reset window
            info.request_count = 1;
            info.window_start = now;
            return true;
        }

        if (info.request_count >= RATE_LIMIT_REQUESTS) {
            return false;
        }

        info.request_count++;
        return true;
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        for (auto it = clients_.begin(); it != clients_.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.window_start).count();
            if (elapsed >= RATE_LIMIT_WINDOW_SECONDS * 2) {
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<std::string, ClientInfo> clients_;
    std::mutex mutex_;
};

// =============================================================================
// Authentication Manager
// =============================================================================

class AuthManager {
public:
    AuthManager() {
        // Load API keys from environment or generate a default one
        const char* api_key_env = std::getenv("LUMEN_API_KEY");
        if (api_key_env && std::strlen(api_key_env) > 0) {
            valid_api_keys_.insert(api_key_env);
        }

        // Load additional keys from file if exists
        loadKeysFromFile();

        // If no keys configured, generate a random one and log it
        if (valid_api_keys_.empty()) {
            std::string generated_key = generateSecureToken();
            valid_api_keys_.insert(generated_key);
            utils::Logger::getInstance().warn(
                "No API keys configured. Generated temporary key: " + generated_key);
            utils::Logger::getInstance().warn(
                "Set LUMEN_API_KEY environment variable for production use.");
        }
    }

    bool authenticate(const httplib::Request& req) const {
        // Check Authorization header
        auto auth_header = req.get_header_value("Authorization");
        if (auth_header.empty()) {
            return false;
        }

        // Support "Bearer <token>" format
        const std::string bearer_prefix = "Bearer ";
        if (auth_header.compare(0, bearer_prefix.size(), bearer_prefix) == 0) {
            std::string token = auth_header.substr(bearer_prefix.size());
            return valid_api_keys_.count(token) > 0;
        }

        // Also support "ApiKey <token>" format
        const std::string apikey_prefix = "ApiKey ";
        if (auth_header.compare(0, apikey_prefix.size(), apikey_prefix) == 0) {
            std::string token = auth_header.substr(apikey_prefix.size());
            return valid_api_keys_.count(token) > 0;
        }

        return false;
    }

    bool isPublicEndpoint(const std::string& path) const {
        // Only health check is public
        return path == "/health";
    }

private:
    std::set<std::string> valid_api_keys_;

    void loadKeysFromFile() {
        auto& config = utils::Configuration::getInstance();
        std::string keys_path = config.getHomeDirectory() + "/api_keys.txt";

        std::ifstream file(keys_path);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                // Skip comments and empty lines
                if (line.empty() || line[0] == '#') continue;
                // Trim whitespace
                size_t start = line.find_first_not_of(" \t\r\n");
                size_t end = line.find_last_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    valid_api_keys_.insert(line.substr(start, end - start + 1));
                }
            }
        }
    }

    static std::string generateSecureToken() {
        std::random_device rd;
        std::array<uint8_t, 32> bytes;
        for (auto& byte : bytes) {
            byte = static_cast<uint8_t>(rd());
        }

        static const char hex_chars[] = "0123456789abcdef";
        std::string token;
        token.reserve(64);
        for (uint8_t byte : bytes) {
            token += hex_chars[byte >> 4];
            token += hex_chars[byte & 0x0F];
        }
        return token;
    }
};

// =============================================================================
// Input Validation
// =============================================================================

namespace validation {

/// @brief Validate ticker symbol format
bool isValidTicker(const std::string& ticker) {
    if (ticker.empty() || ticker.length() > MAX_TICKER_LENGTH) {
        return false;
    }
    // Allow alphanumeric, dots, and hyphens (e.g., BRK.A, BRK-B)
    static const std::regex ticker_pattern("^[A-Za-z0-9][A-Za-z0-9.-]*$");
    return std::regex_match(ticker, ticker_pattern);
}

/// @brief Sanitize error message for external display
std::string sanitizeErrorMessage(const std::string& internal_msg) {
    // Remove file paths, line numbers, and other internal details
    // Return a generic message
    (void)internal_msg;  // Log internally but don't expose
    return "An internal error occurred. Please try again later.";
}

/// @brief Check JSON depth to prevent stack overflow
bool checkJsonDepth(const json& j, size_t current_depth = 0) {
    if (current_depth > MAX_JSON_DEPTH) {
        return false;
    }
    if (j.is_object()) {
        for (auto& [key, val] : j.items()) {
            if (!checkJsonDepth(val, current_depth + 1)) {
                return false;
            }
        }
    } else if (j.is_array()) {
        for (const auto& item : j) {
            if (!checkJsonDepth(item, current_depth + 1)) {
                return false;
            }
        }
    }
    return true;
}

/// @brief Parse JSON with size and depth limits
std::optional<json> parseJsonSafe(const std::string& body) {
    if (body.size() > MAX_REQUEST_BODY_SIZE) {
        return std::nullopt;
    }
    try {
        json j = json::parse(body);
        if (!checkJsonDepth(j)) {
            return std::nullopt;
        }
        return j;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace validation

// =============================================================================
// Security Headers
// =============================================================================

void addSecurityHeaders(httplib::Response& res) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Content-Security-Policy", "default-src 'none'; frame-ancestors 'none'");
    res.set_header("Cache-Control", "no-store, no-cache, must-revalidate");
    res.set_header("Pragma", "no-cache");
    res.set_header("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
}

// =============================================================================
// Global State
// =============================================================================

static std::atomic<bool> g_running{true};
static RateLimiter g_rate_limiter;
static std::unique_ptr<AuthManager> g_auth_manager;

/// @brief Signal handler for graceful shutdown
void signalHandler(int signal) {
    utils::Logger::getInstance().info("Received signal " + std::to_string(signal) + ", shutting down...");
    g_running = false;
}

// =============================================================================
// Route Setup with Security Middleware
// =============================================================================

template<typename ServerType>
void setupRoutes(ServerType& server) {
    auto& logger = utils::Logger::getInstance();

    // Pre-routing handler for security checks
    server.set_pre_routing_handler([&logger](const httplib::Request& req, httplib::Response& res) {
        // Rate limiting
        std::string client_ip = req.remote_addr;
        if (!g_rate_limiter.isAllowed(client_ip)) {
            logger.warn("Rate limit exceeded for: " + client_ip);
            res.status = 429;
            json error = {{"error", "Too Many Requests"}, {"retry_after", RATE_LIMIT_WINDOW_SECONDS}};
            addSecurityHeaders(res);
            res.set_content(error.dump(), "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }

        // Request body size check
        auto content_length = req.get_header_value("Content-Length");
        if (!content_length.empty()) {
            try {
                size_t length = std::stoull(content_length);
                if (length > MAX_REQUEST_BODY_SIZE) {
                    res.status = 413;
                    json error = {{"error", "Request Entity Too Large"}};
                    addSecurityHeaders(res);
                    res.set_content(error.dump(), "application/json");
                    return httplib::Server::HandlerResponse::Handled;
                }
            } catch (...) {
                // Invalid content length header
            }
        }

        // Authentication check (skip for public endpoints)
        if (!g_auth_manager->isPublicEndpoint(req.path)) {
            if (!g_auth_manager->authenticate(req)) {
                logger.warn("Authentication failed for: " + req.path + " from " + client_ip);
                res.status = 401;
                json error = {{"error", "Unauthorized"}, {"message", "Valid API key required"}};
                addSecurityHeaders(res);
                res.set_header("WWW-Authenticate", "Bearer realm=\"Lumen API\"");
                res.set_content(error.dump(), "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    // Health check endpoint (public - no auth required)
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"status", "healthy"},
            {"version", "1.0.0"}
        };
        addSecurityHeaders(res);
        res.set_content(response.dump(), "application/json");
    });

    // Version endpoint (requires auth)
    server.Get("/version", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"name", "Lumen"},
            {"version", "1.0.0"},
            {"description", "Quantum-Enhanced Portfolio Optimizer"}
        };
        addSecurityHeaders(res);
        res.set_content(response.dump(), "application/json");
    });

    // Status endpoint (requires auth)
    server.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        auto& config = utils::Configuration::getInstance();

        json response = {
            {"solvers", {
                {"highs", solvers::HighsOptimizer::isAvailable()},
                {"quantum_enabled", config.getSolverConfig().enable_quantum}
            }},
            {"market_data", {
                {"alpha_vantage", config.getMarketDataConfig().enable_alpha_vantage},
                {"yahoo_finance", config.getMarketDataConfig().enable_yahoo_finance}
            }}
        };
        addSecurityHeaders(res);
        res.set_content(response.dump(), "application/json");
    });

    // Optimize endpoint (requires auth)
    server.Post("/optimize", [&logger](const httplib::Request& req, httplib::Response& res) {
        addSecurityHeaders(res);

        // Parse and validate JSON
        auto json_opt = validation::parseJsonSafe(req.body);
        if (!json_opt) {
            res.status = 400;
            json error = {{"error", "Invalid request"}, {"message", "Invalid or oversized JSON payload"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            // TODO: Implement optimization
            json response = {
                {"status", "not_implemented"},
                {"message", "Optimization endpoint not yet implemented"},
                {"request_received", true}
            };
            res.status = 501;
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            logger.error("Optimization error: " + std::string(e.what()));
            res.status = 500;
            json error = {
                {"error", "Internal error"},
                {"message", validation::sanitizeErrorMessage(e.what())}
            };
            res.set_content(error.dump(), "application/json");
        }
    });

    // Portfolio management endpoints (requires auth)
    server.Get("/portfolios", [](const httplib::Request&, httplib::Response& res) {
        addSecurityHeaders(res);
        json response = {
            {"portfolios", json::array()},
            {"message", "Portfolio listing not yet implemented"}
        };
        res.status = 501;
        res.set_content(response.dump(), "application/json");
    });

    server.Post("/portfolios", [&logger](const httplib::Request& req, httplib::Response& res) {
        addSecurityHeaders(res);

        auto json_opt = validation::parseJsonSafe(req.body);
        if (!json_opt) {
            res.status = 400;
            json error = {{"error", "Invalid request"}, {"message", "Invalid or oversized JSON payload"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            json response = {{"message", "Portfolio creation not yet implemented"}};
            res.status = 501;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            logger.error("Portfolio creation error: " + std::string(e.what()));
            res.status = 500;
            json error = {{"error", "Internal error"}, {"message", validation::sanitizeErrorMessage(e.what())}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // Market data endpoint with ticker validation (requires auth)
    server.Get("/quote/:ticker", [&logger](const httplib::Request& req, httplib::Response& res) {
        addSecurityHeaders(res);

        std::string ticker = req.path_params.at("ticker");

        // Validate ticker format
        if (!validation::isValidTicker(ticker)) {
            res.status = 400;
            json error = {{"error", "Invalid ticker"}, {"message", "Ticker must be 1-10 alphanumeric characters"}};
            res.set_content(error.dump(), "application/json");
            return;
        }

        try {
            // Convert to uppercase for consistency
            std::transform(ticker.begin(), ticker.end(), ticker.begin(), ::toupper);

            json response = {
                {"ticker", ticker},
                {"message", "Market data endpoint not yet implemented"}
            };
            res.status = 501;
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            logger.error("Quote error: " + std::string(e.what()));
            res.status = 500;
            json error = {{"error", "Internal error"}, {"message", validation::sanitizeErrorMessage(e.what())}};
            res.set_content(error.dump(), "application/json");
        }
    });

    // Error handler - don't leak internal details
    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        addSecurityHeaders(res);
        json error = {{"error", "Not Found"}};
        res.set_content(error.dump(), "application/json");
    });

    // Exception handler - sanitize error messages
    server.set_exception_handler([&logger](const httplib::Request&, httplib::Response& res,
                                           std::exception_ptr ep) {
        addSecurityHeaders(res);
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            logger.error("Unhandled exception: " + std::string(e.what()));
            json error = {
                {"error", "Internal Server Error"},
                {"message", validation::sanitizeErrorMessage(e.what())}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Request logging (sanitized)
    server.set_logger([&logger](const httplib::Request& req, const httplib::Response& res) {
        utils::LogContext ctx("HTTP");
        ctx.with("method", req.method)
           .with("path", req.path)
           .with("status", res.status)
           .with("client", req.remote_addr);
        logger.info("Request handled", ctx);
    });
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    CLI::App app{"Lumen HTTP Server"};

    std::string host = "127.0.0.1";  // Default to localhost for security
    int port = 8080;
    std::string config_path;
    std::string cert_path;
    std::string key_path;
    bool use_tls = false;
    bool allow_external = false;

    app.add_option("-H,--host", host, "Host to bind to (default: 127.0.0.1)");
    app.add_option("-p,--port", port, "Port to listen on (default: 8080)");
    app.add_option("-c,--config", config_path, "Path to configuration file");
    app.add_option("--cert", cert_path, "Path to TLS certificate file");
    app.add_option("--key", key_path, "Path to TLS private key file");
    app.add_flag("--tls", use_tls, "Enable TLS (requires --cert and --key)");
    app.add_flag("--allow-external", allow_external,
        "Allow external connections (binds to 0.0.0.0). WARNING: Ensure TLS is enabled!");

    CLI11_PARSE(app, argc, argv);

    // Security warning for external binding without TLS
    if (allow_external && !use_tls) {
        std::cerr << "WARNING: Binding to external interface without TLS is insecure!\n";
        std::cerr << "Use --tls with --cert and --key for production deployments.\n";
        std::cerr << "Continuing in 5 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    if (allow_external) {
        host = "0.0.0.0";
    }

    // Validate TLS options
    if (use_tls && (cert_path.empty() || key_path.empty())) {
        std::cerr << "Error: TLS requires both --cert and --key options\n";
        return 1;
    }

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Initialize configuration
    auto& config = utils::Configuration::getInstance();
    config.loadFromEnvironment();
    if (!config_path.empty()) {
        config.loadFromFile(config_path);
    }

    // Initialize logging
    auto& logger = utils::Logger::getInstance();
    logger.setLevel(utils::LogLevel::INFO);

    // Initialize authentication manager
    g_auth_manager = std::make_unique<AuthManager>();

    logger.info("Starting Lumen HTTP Server...");
    logger.info("TLS: " + std::string(use_tls ? "enabled" : "disabled"));
    logger.info("Binding to " + host + ":" + std::to_string(port));

    bool server_started = false;

    if (use_tls) {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        httplib::SSLServer ssl_server(cert_path.c_str(), key_path.c_str());
        if (!ssl_server.is_valid()) {
            logger.error("Failed to initialize SSL server. Check certificate and key files.");
            return 1;
        }
        setupRoutes(ssl_server);
        server_started = ssl_server.listen(host, port);
#else
        logger.error("TLS support not compiled. Rebuild with OpenSSL support.");
        return 1;
#endif
    } else {
        httplib::Server server;
        setupRoutes(server);
        server_started = server.listen(host, port);
    }

    if (!server_started) {
        logger.error("Failed to start server on " + host + ":" + std::to_string(port));
        return 1;
    }

    logger.info("Server stopped");
    return 0;
}
