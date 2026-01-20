/// @file test_logging.cpp
/// @brief Unit tests for logging utilities

#include <gtest/gtest.h>
#include "lumen/utils/logging.hpp"

using namespace lumen::utils;

class LogContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        context_ = std::make_unique<LogContext>("TestComponent");
    }

    std::unique_ptr<LogContext> context_;
};

TEST_F(LogContextTest, GetComponentWorks) {
    EXPECT_EQ(context_->getComponent(), "TestComponent");
}

TEST_F(LogContextTest, FormatWithoutContextWorks) {
    EXPECT_EQ(context_->format(), "[TestComponent]");
}

TEST_F(LogContextTest, FormatWithStringContextWorks) {
    context_->with("key", "value");
    std::string formatted = context_->format();
    EXPECT_NE(formatted.find("key=value"), std::string::npos);
}

TEST_F(LogContextTest, FormatWithIntContextWorks) {
    context_->with("count", 42);
    std::string formatted = context_->format();
    EXPECT_NE(formatted.find("count=42"), std::string::npos);
}

TEST_F(LogContextTest, FormatWithDoubleContextWorks) {
    context_->with("rate", 3.14159);
    std::string formatted = context_->format();
    EXPECT_NE(formatted.find("rate="), std::string::npos);
}

TEST_F(LogContextTest, ChainingWorksCorrectly) {
    context_->with("a", "1").with("b", "2").with("c", "3");
    std::string formatted = context_->format();
    EXPECT_NE(formatted.find("a=1"), std::string::npos);
    EXPECT_NE(formatted.find("b=2"), std::string::npos);
    EXPECT_NE(formatted.find("c=3"), std::string::npos);
}

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger_ = &Logger::getInstance();
        // Reset to default state
        logger_->setLevel(LogLevel::INFO);
        logger_->enableConsole(false);  // Disable console for tests
        logger_->disableFile();
    }

    void TearDown() override {
        // Re-enable console after tests
        logger_->enableConsole(true);
    }

    Logger* logger_;
};

TEST_F(LoggerTest, GetInstanceReturnsSameInstance) {
    auto& instance1 = Logger::getInstance();
    auto& instance2 = Logger::getInstance();
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(LoggerTest, SetLevelWorks) {
    logger_->setLevel(LogLevel::DEBUG);
    EXPECT_EQ(logger_->getLevel(), LogLevel::DEBUG);
}

TEST_F(LoggerTest, LoggingDoesNotThrow) {
    logger_->setLevel(LogLevel::TRACE);

    EXPECT_NO_THROW(logger_->trace("Trace message"));
    EXPECT_NO_THROW(logger_->debug("Debug message"));
    EXPECT_NO_THROW(logger_->info("Info message"));
    EXPECT_NO_THROW(logger_->warn("Warning message"));
    EXPECT_NO_THROW(logger_->error("Error message"));
    EXPECT_NO_THROW(logger_->fatal("Fatal message"));
}

TEST_F(LoggerTest, LoggingWithContextDoesNotThrow) {
    logger_->setLevel(LogLevel::TRACE);
    LogContext ctx("TestContext");
    ctx.with("key", "value");

    EXPECT_NO_THROW(logger_->trace("Trace message", ctx));
    EXPECT_NO_THROW(logger_->debug("Debug message", ctx));
    EXPECT_NO_THROW(logger_->info("Info message", ctx));
    EXPECT_NO_THROW(logger_->warn("Warning message", ctx));
    EXPECT_NO_THROW(logger_->error("Error message", ctx));
    EXPECT_NO_THROW(logger_->fatal("Fatal message", ctx));
}

TEST_F(LoggerTest, EnableFileDoesNotThrow) {
    EXPECT_NO_THROW(logger_->enableFile("/tmp/lumen_test.log"));
    EXPECT_NO_THROW(logger_->disableFile());
}

TEST_F(LoggerTest, SetJsonFormatDoesNotThrow) {
    EXPECT_NO_THROW(logger_->setJsonFormat(true));
    EXPECT_NO_THROW(logger_->setJsonFormat(false));
}

// Test log level filtering
TEST_F(LoggerTest, LogLevelFilteringWorks) {
    // Set level to ERROR
    logger_->setLevel(LogLevel::ERROR);

    // These should be filtered out (level below ERROR)
    // We can't easily verify they're filtered, but at least they shouldn't crash
    EXPECT_NO_THROW(logger_->trace("Should be filtered"));
    EXPECT_NO_THROW(logger_->debug("Should be filtered"));
    EXPECT_NO_THROW(logger_->info("Should be filtered"));
    EXPECT_NO_THROW(logger_->warn("Should be filtered"));

    // These should pass through
    EXPECT_NO_THROW(logger_->error("Should pass"));
    EXPECT_NO_THROW(logger_->fatal("Should pass"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
