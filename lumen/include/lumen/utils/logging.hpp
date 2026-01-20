#pragma once

/// @file logging.hpp
/// @brief Structured logging utilities
///
/// This module provides a thread-safe logging system with structured
/// context and multiple output targets.

#include <map>
#include <memory>
#include <string>

namespace lumen::utils {

/// Log severity levels
enum class LogLevel {
    TRACE,  ///< Very detailed debugging
    DEBUG,  ///< Debugging information
    INFO,   ///< General information
    WARN,   ///< Warning conditions
    ERROR,  ///< Error conditions
    FATAL   ///< Fatal errors
};

/// Convert LogLevel to string
std::string logLevelToString(LogLevel level);

/// Parse LogLevel from string
LogLevel logLevelFromString(const std::string& str);

/// Context information for log messages
struct LogContext {
    std::string component;  ///< Component name (e.g., "solver", "market_data")
    std::string operation;  ///< Operation being performed
    std::map<std::string, std::string> tags;  ///< Additional key-value tags

    /// Builder pattern for adding tags
    LogContext& set(const std::string& key, const std::string& value) {
        tags[key] = value;
        return *this;
    }

    /// Add integer tag
    LogContext& set(const std::string& key, int value) {
        tags[key] = std::to_string(value);
        return *this;
    }

    /// Add double tag
    LogContext& set(const std::string& key, double value) {
        tags[key] = std::to_string(value);
        return *this;
    }
};

/// Singleton logger class
class Logger {
public:
    /// Get the singleton instance
    static Logger& getInstance();

    // Prevent copying
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /// Log at TRACE level
    void trace(const std::string& message, const LogContext& ctx = {});

    /// Log at DEBUG level
    void debug(const std::string& message, const LogContext& ctx = {});

    /// Log at INFO level
    void info(const std::string& message, const LogContext& ctx = {});

    /// Log at WARN level
    void warn(const std::string& message, const LogContext& ctx = {});

    /// Log at ERROR level
    void error(const std::string& message, const LogContext& ctx = {});

    /// Log at FATAL level
    void fatal(const std::string& message, const LogContext& ctx = {});

    /// Record a numeric metric
    void recordMetric(const std::string& name, double value, const std::string& unit = "");

    /// Record a timing measurement
    void recordTiming(const std::string& name, long milliseconds);

    /// Increment a counter
    void recordCounter(const std::string& name, int delta = 1);

    /// Set minimum log level
    void setLevel(LogLevel level);

    /// Get current log level
    LogLevel getLevel() const;

    /// Set output file path
    void setOutputFile(const std::string& filepath);

    /// Enable/disable console output
    void enableConsoleOutput(bool enable);

    /// Set log file rotation size
    void setRotationSize(size_t bytes);

    /// Flush pending log messages
    void flush();

private:
    Logger();
    ~Logger();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Convenience macros
#define LOG_TRACE(msg) ::lumen::utils::Logger::getInstance().trace(msg)
#define LOG_DEBUG(msg) ::lumen::utils::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) ::lumen::utils::Logger::getInstance().info(msg)
#define LOG_WARN(msg) ::lumen::utils::Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) ::lumen::utils::Logger::getInstance().error(msg)
#define LOG_FATAL(msg) ::lumen::utils::Logger::getInstance().fatal(msg)

#define LOG_TRACE_CTX(msg, ctx) ::lumen::utils::Logger::getInstance().trace(msg, ctx)
#define LOG_DEBUG_CTX(msg, ctx) ::lumen::utils::Logger::getInstance().debug(msg, ctx)
#define LOG_INFO_CTX(msg, ctx) ::lumen::utils::Logger::getInstance().info(msg, ctx)
#define LOG_WARN_CTX(msg, ctx) ::lumen::utils::Logger::getInstance().warn(msg, ctx)
#define LOG_ERROR_CTX(msg, ctx) ::lumen::utils::Logger::getInstance().error(msg, ctx)
#define LOG_FATAL_CTX(msg, ctx) ::lumen::utils::Logger::getInstance().fatal(msg, ctx)

}  // namespace lumen::utils
