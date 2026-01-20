/// @file test_config.cpp
/// @brief Unit tests for configuration utilities

#include <gtest/gtest.h>
#include "lumen/utils/config.hpp"
#include <filesystem>
#include <fstream>

using namespace lumen::utils;
namespace fs = std::filesystem;

class SolverConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_unique<SolverConfig>();
    }

    std::unique_ptr<SolverConfig> config_;
};

TEST_F(SolverConfigTest, DefaultValuesAreSet) {
    EXPECT_EQ(config_->default_timeout_ms, 30000);
    EXPECT_DOUBLE_EQ(config_->mip_gap, 1e-4);
    EXPECT_FALSE(config_->enable_quantum);
    EXPECT_EQ(config_->quantum_min_positions, 50);
}

TEST_F(SolverConfigTest, ToJsonWorks) {
    auto json = config_->toJSON();
    EXPECT_EQ(json["default_timeout_ms"], 30000);
    EXPECT_DOUBLE_EQ(json["mip_gap"].get<double>(), 1e-4);
    EXPECT_FALSE(json["enable_quantum"].get<bool>());
}

TEST_F(SolverConfigTest, FromJsonWorks) {
    nlohmann::json j = {
        {"default_timeout_ms", 60000},
        {"mip_gap", 1e-6},
        {"enable_quantum", true},
        {"quantum_min_positions", 100},
        {"preferred_solver", "highs"}
    };

    config_->fromJSON(j);

    EXPECT_EQ(config_->default_timeout_ms, 60000);
    EXPECT_DOUBLE_EQ(config_->mip_gap, 1e-6);
    EXPECT_TRUE(config_->enable_quantum);
    EXPECT_EQ(config_->quantum_min_positions, 100);
    EXPECT_EQ(config_->preferred_solver, "highs");
}

class MarketDataConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_unique<MarketDataConfig>();
    }

    std::unique_ptr<MarketDataConfig> config_;
};

TEST_F(MarketDataConfigTest, DefaultValuesAreSet) {
    EXPECT_EQ(config_->cache_duration_seconds, 300);
    EXPECT_FALSE(config_->enable_alpha_vantage);
    EXPECT_TRUE(config_->enable_yahoo_finance);
    EXPECT_EQ(config_->alpha_vantage_rate_limit, 5);
}

TEST_F(MarketDataConfigTest, ToJsonWorks) {
    auto json = config_->toJSON();
    EXPECT_EQ(json["cache_duration_seconds"], 300);
    EXPECT_FALSE(json["enable_alpha_vantage"].get<bool>());
    EXPECT_TRUE(json["enable_yahoo_finance"].get<bool>());
}

TEST_F(MarketDataConfigTest, FromJsonWorks) {
    nlohmann::json j = {
        {"cache_duration_seconds", 600},
        {"enable_alpha_vantage", true},
        {"enable_yahoo_finance", false},
        {"alpha_vantage_rate_limit", 10}
    };

    config_->fromJSON(j);

    EXPECT_EQ(config_->cache_duration_seconds, 600);
    EXPECT_TRUE(config_->enable_alpha_vantage);
    EXPECT_FALSE(config_->enable_yahoo_finance);
    EXPECT_EQ(config_->alpha_vantage_rate_limit, 10);
}

class ObservabilityConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_unique<ObservabilityConfig>();
    }

    std::unique_ptr<ObservabilityConfig> config_;
};

TEST_F(ObservabilityConfigTest, DefaultValuesAreSet) {
    EXPECT_EQ(config_->log_level, "INFO");
    EXPECT_TRUE(config_->enable_pyflare);
    EXPECT_FALSE(config_->enable_file_logging);
    EXPECT_FALSE(config_->json_logs);
}

TEST_F(ObservabilityConfigTest, ToJsonWorks) {
    auto json = config_->toJSON();
    EXPECT_EQ(json["log_level"], "INFO");
    EXPECT_TRUE(json["enable_pyflare"].get<bool>());
}

TEST_F(ObservabilityConfigTest, FromJsonWorks) {
    nlohmann::json j = {
        {"log_level", "DEBUG"},
        {"log_file_path", "/var/log/lumen.log"},
        {"enable_pyflare", false},
        {"enable_file_logging", true},
        {"json_logs", true}
    };

    config_->fromJSON(j);

    EXPECT_EQ(config_->log_level, "DEBUG");
    EXPECT_EQ(config_->log_file_path, "/var/log/lumen.log");
    EXPECT_FALSE(config_->enable_pyflare);
    EXPECT_TRUE(config_->enable_file_logging);
    EXPECT_TRUE(config_->json_logs);
}

class ConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = &Configuration::getInstance();
        test_dir_ = fs::temp_directory_path() / "lumen_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        fs::remove_all(test_dir_);
    }

    Configuration* config_;
    fs::path test_dir_;
};

TEST_F(ConfigurationTest, GetInstanceReturnsSameInstance) {
    auto& instance1 = Configuration::getInstance();
    auto& instance2 = Configuration::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(ConfigurationTest, SetAndGetApiKeyWorks) {
    config_->setApiKey("test_service", "test_key_123");
    EXPECT_EQ(config_->getApiKey("test_service"), "test_key_123");
}

TEST_F(ConfigurationTest, GetApiKeyReturnsEmptyForUnknown) {
    EXPECT_EQ(config_->getApiKey("unknown_service"), "");
}

TEST_F(ConfigurationTest, GetHomeDirectoryReturnsNonEmpty) {
    EXPECT_FALSE(config_->getHomeDirectory().empty());
}

TEST_F(ConfigurationTest, GetDatabasePathReturnsNonEmpty) {
    EXPECT_FALSE(config_->getDatabasePath().empty());
}

TEST_F(ConfigurationTest, GetConfigPathReturnsNonEmpty) {
    EXPECT_FALSE(config_->getConfigPath().empty());
}

TEST_F(ConfigurationTest, SaveAndLoadFromFileWorks) {
    fs::path config_file = test_dir_ / "test_config.json";

    // Save configuration
    EXPECT_TRUE(config_->saveToFile(config_file.string()));

    // Verify file was created
    EXPECT_TRUE(fs::exists(config_file));

    // Load configuration
    EXPECT_TRUE(config_->loadFromFile(config_file.string()));
}

TEST_F(ConfigurationTest, LoadFromFileReturnsFalseForNonexistent) {
    EXPECT_FALSE(config_->loadFromFile("/nonexistent/path/config.json"));
}

TEST_F(ConfigurationTest, ToJsonWorks) {
    auto json = config_->toJSON();

    EXPECT_TRUE(json.contains("solver"));
    EXPECT_TRUE(json.contains("market_data"));
    EXPECT_TRUE(json.contains("observability"));
}

TEST_F(ConfigurationTest, EnsureDirectoriesWorks) {
    // This should not throw, but we can't easily verify directory creation
    // without modifying the home directory path
    EXPECT_NO_THROW(config_->ensureDirectories());
}

TEST_F(ConfigurationTest, GetSolverConfigReturnsValidConfig) {
    const auto& solver_config = config_->getSolverConfig();
    EXPECT_GT(solver_config.default_timeout_ms, 0u);
}

TEST_F(ConfigurationTest, GetMarketDataConfigReturnsValidConfig) {
    const auto& md_config = config_->getMarketDataConfig();
    EXPECT_GT(md_config.cache_duration_seconds, 0);
}

TEST_F(ConfigurationTest, GetObservabilityConfigReturnsValidConfig) {
    const auto& obs_config = config_->getObservabilityConfig();
    EXPECT_FALSE(obs_config.log_level.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
