/// @file pyflare.cpp
/// @brief PyFlare telemetry implementation
///
/// Implementation of PyFlare telemetry integration for observability.

#include "lumen/utils/pyflare.hpp"
#include "lumen/utils/logging.hpp"

#include <chrono>
#include <sstream>
#include <iomanip>
#include <queue>

namespace lumen::utils {

// =============================================================================
// TelemetryEvent
// =============================================================================

TelemetryEvent::TelemetryEvent(std::string name, std::string category)
    : name_(std::move(name)), category_(std::move(category)),
      duration_ms_(0.0) {
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    timestamp_ = ss.str();
}

void TelemetryEvent::setAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value;
}

void TelemetryEvent::setAttribute(const std::string& key, int value) {
    attributes_[key] = std::to_string(value);
}

void TelemetryEvent::setAttribute(const std::string& key, double value) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6) << value;
    attributes_[key] = ss.str();
}

void TelemetryEvent::setDuration(double duration_ms) {
    duration_ms_ = duration_ms;
}

void TelemetryEvent::setSuccess(bool success) {
    setAttribute("success", success ? "true" : "false");
}

void TelemetryEvent::setError(const std::string& error) {
    setAttribute("error", error);
    setSuccess(false);
}

nlohmann::json TelemetryEvent::toJSON() const {
    nlohmann::json j = {
        {"name", name_},
        {"category", category_},
        {"timestamp", timestamp_},
        {"duration_ms", duration_ms_},
        {"attributes", attributes_}
    };
    return j;
}

// =============================================================================
// PyFlareExporter Implementation
// =============================================================================

class PyFlareExporterImpl {
public:
    PyFlareExporterImpl()
        : enabled_(false),
          batch_size_(100),
          flush_interval_ms_(5000) {}

    void configure(const nlohmann::json& config) {
        if (config.contains("endpoint")) {
            endpoint_ = config["endpoint"].get<std::string>();
        }
        if (config.contains("api_key")) {
            api_key_ = config["api_key"].get<std::string>();
        }
        if (config.contains("service_name")) {
            service_name_ = config["service_name"].get<std::string>();
        }
        if (config.contains("batch_size")) {
            batch_size_ = config["batch_size"].get<size_t>();
        }
        if (config.contains("flush_interval_ms")) {
            flush_interval_ms_ = config["flush_interval_ms"].get<int>();
        }
    }

    void enable() {
        enabled_ = true;
    }

    void disable() {
        enabled_ = false;
    }

    bool isEnabled() const {
        return enabled_;
    }

    void recordEvent(const TelemetryEvent& event) {
        if (!enabled_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        event_queue_.push(event);

        if (event_queue_.size() >= batch_size_) {
            flushInternal();
        }
    }

    void recordMetric(const std::string& name, double value,
                      const std::map<std::string, std::string>& tags) {
        if (!enabled_) return;

        TelemetryEvent event(name, "metric");
        event.setAttribute("value", value);
        for (const auto& [key, val] : tags) {
            event.setAttribute(key, val);
        }
        recordEvent(event);
    }

    void recordSpan(const std::string& operation, double duration_ms,
                    bool success, const std::string& error) {
        if (!enabled_) return;

        TelemetryEvent event(operation, "span");
        event.setDuration(duration_ms);
        event.setSuccess(success);
        if (!error.empty()) {
            event.setError(error);
        }
        recordEvent(event);
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        flushInternal();
    }

private:
    void flushInternal() {
        if (event_queue_.empty()) return;

        // Build batch
        nlohmann::json batch = nlohmann::json::array();
        while (!event_queue_.empty()) {
            batch.push_back(event_queue_.front().toJSON());
            event_queue_.pop();
        }

        // TODO: Send to PyFlare endpoint
        // For now, just log that we would send
        if (!endpoint_.empty()) {
            Logger::getInstance().debug(
                "Would send " + std::to_string(batch.size()) +
                " events to PyFlare at " + endpoint_);
        }
    }

    bool enabled_;
    std::string endpoint_;
    std::string api_key_;
    std::string service_name_;
    size_t batch_size_;
    int flush_interval_ms_;
    std::queue<TelemetryEvent> event_queue_;
    std::mutex mutex_;
};

// =============================================================================
// PyFlareExporter Singleton
// =============================================================================

static std::unique_ptr<PyFlareExporterImpl> g_exporter;
static std::once_flag g_exporter_init;

static PyFlareExporterImpl& getExporterImpl() {
    std::call_once(g_exporter_init, []() {
        g_exporter = std::make_unique<PyFlareExporterImpl>();
    });
    return *g_exporter;
}

PyFlareExporter& PyFlareExporter::getInstance() {
    static PyFlareExporter instance;
    return instance;
}

void PyFlareExporter::configure(const nlohmann::json& config) {
    getExporterImpl().configure(config);
}

void PyFlareExporter::enable() {
    getExporterImpl().enable();
}

void PyFlareExporter::disable() {
    getExporterImpl().disable();
}

bool PyFlareExporter::isEnabled() const {
    return getExporterImpl().isEnabled();
}

void PyFlareExporter::recordEvent(const TelemetryEvent& event) {
    getExporterImpl().recordEvent(event);
}

void PyFlareExporter::recordMetric(const std::string& name, double value,
                                    const std::map<std::string, std::string>& tags) {
    getExporterImpl().recordMetric(name, value, tags);
}

void PyFlareExporter::recordSpan(const std::string& operation, double duration_ms,
                                  bool success, const std::string& error) {
    getExporterImpl().recordSpan(operation, duration_ms, success, error);
}

void PyFlareExporter::flush() {
    getExporterImpl().flush();
}

// =============================================================================
// ScopedSpan
// =============================================================================

ScopedSpan::ScopedSpan(std::string operation)
    : operation_(std::move(operation)),
      start_time_(std::chrono::steady_clock::now()),
      success_(true) {}

ScopedSpan::~ScopedSpan() {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time_).count() / 1000.0;

    PyFlareExporter::getInstance().recordSpan(operation_, duration, success_, error_);
}

void ScopedSpan::setSuccess(bool success) {
    success_ = success;
}

void ScopedSpan::setError(const std::string& error) {
    error_ = error;
    success_ = false;
}

}  // namespace lumen::utils
