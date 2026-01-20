/// @file main.cpp
/// @brief Lumen HTTP server entry point
///
/// Main entry point for the Lumen HTTP API server.

#include "lumen/core/portfolio.hpp"
#include "lumen/core/constraint.hpp"
#include "lumen/core/solver_dispatcher.hpp"
#include "lumen/utils/logging.hpp"
#include "lumen/utils/config.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>

#include <iostream>
#include <string>
#include <csignal>
#include <atomic>

using namespace lumen;
using json = nlohmann::json;

// Global server instance for signal handling
static std::atomic<bool> g_running{true};

/// @brief Signal handler for graceful shutdown
void signalHandler(int signal) {
    utils::Logger::getInstance().info("Received signal " + std::to_string(signal) + ", shutting down...");
    g_running = false;
}

/// @brief Setup API routes
void setupRoutes(httplib::Server& server) {
    auto& logger = utils::Logger::getInstance();

    // Health check endpoint
    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"status", "healthy"},
            {"version", "1.0.0"}
        };
        res.set_content(response.dump(), "application/json");
    });

    // Version endpoint
    server.Get("/version", [](const httplib::Request&, httplib::Response& res) {
        json response = {
            {"name", "Lumen"},
            {"version", "1.0.0"},
            {"description", "Quantum-Enhanced Portfolio Optimizer"}
        };
        res.set_content(response.dump(), "application/json");
    });

    // Status endpoint
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
        res.set_content(response.dump(), "application/json");
    });

    // Optimize endpoint
    server.Post("/optimize", [&logger](const httplib::Request& req, httplib::Response& res) {
        try {
            json request_body = json::parse(req.body);

            // TODO: Implement optimization
            // 1. Parse portfolio from request
            // 2. Parse target allocation from request
            // 3. Parse constraints from request
            // 4. Run optimization
            // 5. Return trades and provenance

            json response = {
                {"status", "not_implemented"},
                {"message", "Optimization endpoint not yet implemented"},
                {"request_received", true}
            };
            res.status = 501;
            res.set_content(response.dump(), "application/json");

        } catch (const json::parse_error& e) {
            json error = {
                {"error", "Invalid JSON"},
                {"message", e.what()}
            };
            res.status = 400;
            res.set_content(error.dump(), "application/json");

        } catch (const std::exception& e) {
            logger.error("Optimization error: " + std::string(e.what()));
            json error = {
                {"error", "Internal error"},
                {"message", e.what()}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Portfolio management endpoints
    server.Get("/portfolios", [](const httplib::Request&, httplib::Response& res) {
        // TODO: List portfolios from database
        json response = {
            {"portfolios", json::array()},
            {"message", "Portfolio listing not yet implemented"}
        };
        res.status = 501;
        res.set_content(response.dump(), "application/json");
    });

    server.Post("/portfolios", [](const httplib::Request& req, httplib::Response& res) {
        // TODO: Create/save portfolio
        json response = {
            {"message", "Portfolio creation not yet implemented"}
        };
        res.status = 501;
        res.set_content(response.dump(), "application/json");
    });

    // Market data endpoint
    server.Get("/quote/:ticker", [](const httplib::Request& req, httplib::Response& res) {
        std::string ticker = req.path_params.at("ticker");

        // TODO: Fetch quote from market data provider
        json response = {
            {"ticker", ticker},
            {"message", "Market data endpoint not yet implemented"}
        };
        res.status = 501;
        res.set_content(response.dump(), "application/json");
    });

    // Error handler
    server.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        json error = {
            {"error", "Not Found"},
            {"status", res.status}
        };
        res.set_content(error.dump(), "application/json");
    });

    // Exception handler
    server.set_exception_handler([&logger](const httplib::Request&, httplib::Response& res,
                                           std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            logger.error("Unhandled exception: " + std::string(e.what()));
            json error = {
                {"error", "Internal Server Error"},
                {"message", e.what()}
            };
            res.status = 500;
            res.set_content(error.dump(), "application/json");
        }
    });

    // Request logging
    server.set_logger([&logger](const httplib::Request& req, const httplib::Response& res) {
        utils::LogContext ctx("HTTP");
        ctx.with("method", req.method)
           .with("path", req.path)
           .with("status", res.status);
        logger.info("Request handled", ctx);
    });
}

int main(int argc, char* argv[]) {
    CLI::App app{"Lumen HTTP Server"};

    std::string host = "0.0.0.0";
    int port = 8080;
    std::string config_path;

    app.add_option("-H,--host", host, "Host to bind to");
    app.add_option("-p,--port", port, "Port to listen on");
    app.add_option("-c,--config", config_path, "Path to configuration file");

    CLI11_PARSE(app, argc, argv);

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

    logger.info("Starting Lumen HTTP Server...");
    logger.info("Listening on " + host + ":" + std::to_string(port));

    // Create and configure server
    httplib::Server server;
    setupRoutes(server);

    // Start server
    if (!server.listen(host, port)) {
        logger.error("Failed to start server on " + host + ":" + std::to_string(port));
        return 1;
    }

    logger.info("Server stopped");
    return 0;
}
