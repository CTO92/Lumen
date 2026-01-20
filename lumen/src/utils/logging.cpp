/// @file logging.cpp
/// @brief Logging system implementation
///
/// Implementation of the logging system for Lumen.

#include "lumen/utils/logging.hpp"

#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace lumen::utils {

// =============================================================================
// LogContext
// =============================================================================

LogContext::LogContext(std::string component)
    : component_(std::move(component)) {}

LogContext& LogContext::with(const std::string& key, const std::string& value) {
    context_[key] = value;
    return *this;
}

LogContext& LogContext::with(const std::string& key, int value) {
    context_[key] = std::to_string(value);
    return *this;
}

LogContext& LogContext::with(const std::string& key, double value) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4) << value;
    context_[key] = ss.str();
    return *this;
}

std::string LogContext::format() const {
    if (context_.empty()) {
        return "[" + component_ + "]";
    }

    std::ostringstream ss;
    ss << "[" << component_;
    for (const auto& [key, value] : context_) {
        ss << " " << key << "=" << value;
    }
    ss << "]";
    return ss.str();
}

// =============================================================================
// Logger Implementation
// =============================================================================

class LoggerImpl {
public:
    LoggerImpl()
        : level_(LogLevel::INFO),
          console_enabled_(true),
          file_enabled_(false),
          json_format_(false) {}

    void setLevel(LogLevel level) {
        level_ = level;
    }

    LogLevel getLevel() const {
        return level_;
    }

    void enableConsole(bool enable) {
        console_enabled_ = enable;
    }

    void enableFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        file_stream_.open(path, std::ios::app);
        file_enabled_ = file_stream_.is_open();
    }

    void disableFile() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        file_enabled_ = false;
    }

    void setJsonFormat(bool enable) {
        json_format_ = enable;
    }

    void log(LogLevel level, const std::string& message, const LogContext* context) {
        if (level < level_) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        std::string formatted = formatMessage(level, message, context);

        if (console_enabled_) {
            std::ostream& out = (level >= LogLevel::WARN) ? std::cerr : std::cout;
            out << formatted << std::endl;
        }

        if (file_enabled_ && file_stream_.is_open()) {
            file_stream_ << formatted << std::endl;
            file_stream_.flush();
        }
    }

private:
    std::string formatMessage(LogLevel level, const std::string& message,
                              const LogContext* context) {
        if (json_format_) {
            return formatJson(level, message, context);
        }
        return formatText(level, message, context);
    }

    std::string formatText(LogLevel level, const std::string& message,
                           const LogContext* context) {
        std::ostringstream ss;

        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();

        // Level
        ss << " " << levelToString(level);

        // Context
        if (context) {
            ss << " " << context->format();
        }

        // Message
        ss << " " << message;

        return ss.str();
    }

    std::string formatJson(LogLevel level, const std::string& message,
                           const LogContext* context) {
        std::ostringstream ss;

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        ss << "{";
        ss << "\"timestamp\":\"" << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ") << "\",";
        ss << "\"level\":\"" << levelToString(level) << "\",";
        if (context) {
            ss << "\"component\":\"" << context->getComponent() << "\",";
        }
        ss << "\"message\":\"" << escapeJson(message) << "\"";
        ss << "}";

        return ss.str();
    }

    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default: return "?????";
        }
    }

    std::string escapeJson(const std::string& s) {
        std::ostringstream ss;
        for (char c : s) {
            switch (c) {
                case '"': ss << "\\\""; break;
                case '\\': ss << "\\\\"; break;
                case '\n': ss << "\\n"; break;
                case '\r': ss << "\\r"; break;
                case '\t': ss << "\\t"; break;
                default: ss << c; break;
            }
        }
        return ss.str();
    }

    LogLevel level_;
    bool console_enabled_;
    bool file_enabled_;
    bool json_format_;
    std::ofstream file_stream_;
    std::mutex mutex_;
};

// =============================================================================
// Logger Singleton
// =============================================================================

static std::unique_ptr<LoggerImpl> g_logger;
static std::once_flag g_logger_init;

static LoggerImpl& getLoggerImpl() {
    std::call_once(g_logger_init, []() {
        g_logger = std::make_unique<LoggerImpl>();
    });
    return *g_logger;
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLevel(LogLevel level) {
    getLoggerImpl().setLevel(level);
}

LogLevel Logger::getLevel() const {
    return getLoggerImpl().getLevel();
}

void Logger::enableConsole(bool enable) {
    getLoggerImpl().enableConsole(enable);
}

void Logger::enableFile(const std::string& path) {
    getLoggerImpl().enableFile(path);
}

void Logger::disableFile() {
    getLoggerImpl().disableFile();
}

void Logger::setJsonFormat(bool enable) {
    getLoggerImpl().setJsonFormat(enable);
}

void Logger::trace(const std::string& message) {
    getLoggerImpl().log(LogLevel::TRACE, message, nullptr);
}

void Logger::debug(const std::string& message) {
    getLoggerImpl().log(LogLevel::DEBUG, message, nullptr);
}

void Logger::info(const std::string& message) {
    getLoggerImpl().log(LogLevel::INFO, message, nullptr);
}

void Logger::warn(const std::string& message) {
    getLoggerImpl().log(LogLevel::WARN, message, nullptr);
}

void Logger::error(const std::string& message) {
    getLoggerImpl().log(LogLevel::ERROR, message, nullptr);
}

void Logger::fatal(const std::string& message) {
    getLoggerImpl().log(LogLevel::FATAL, message, nullptr);
}

void Logger::trace(const std::string& message, const LogContext& context) {
    getLoggerImpl().log(LogLevel::TRACE, message, &context);
}

void Logger::debug(const std::string& message, const LogContext& context) {
    getLoggerImpl().log(LogLevel::DEBUG, message, &context);
}

void Logger::info(const std::string& message, const LogContext& context) {
    getLoggerImpl().log(LogLevel::INFO, message, &context);
}

void Logger::warn(const std::string& message, const LogContext& context) {
    getLoggerImpl().log(LogLevel::WARN, message, &context);
}

void Logger::error(const std::string& message, const LogContext& context) {
    getLoggerImpl().log(LogLevel::ERROR, message, &context);
}

void Logger::fatal(const std::string& message, const LogContext& context) {
    getLoggerImpl().log(LogLevel::FATAL, message, &context);
}

}  // namespace lumen::utils
