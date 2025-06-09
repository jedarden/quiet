#include <gtest/gtest.h>
#include "quiet/utils/Logger.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace quiet::utils;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing log files
        CleanupLogFiles();
        
        // Reset logger to default state
        Logger& logger = Logger::getInstance();
        logger.setLogLevel(LogLevel::DEBUG);
    }
    
    void TearDown() override {
        // Ensure logger is flushed
        Logger::getInstance().flush();
        
        // Clean up log files
        CleanupLogFiles();
    }
    
private:
    void CleanupLogFiles() {
        std::filesystem::path logDir("logs");
        if (std::filesystem::exists(logDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
                if (entry.path().extension() == ".log") {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    }
};

TEST_F(LoggerTest, SingletonInstance) {
    Logger& instance1 = Logger::getInstance();
    Logger& instance2 = Logger::getInstance();
    
    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(LoggerTest, LogLevelFiltering) {
    Logger& logger = Logger::getInstance();
    
    // Configure to file only for testing
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_level.log";
    config.minLevel = LogLevel::WARNING;
    logger.configure(config);
    
    // Log at different levels
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARNING("Warning message");
    LOG_ERROR("Error message");
    
    // Flush and read log file
    logger.flush();
    
    std::ifstream logFile(config.logFilePath);
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(logFile, line)) {
        lines.push_back(line);
    }
    
    // Should only have WARNING and ERROR
    EXPECT_EQ(lines.size(), 2);
    EXPECT_TRUE(lines[0].find("Warning message") != std::string::npos);
    EXPECT_TRUE(lines[1].find("Error message") != std::string::npos);
}

TEST_F(LoggerTest, MultiThreadedLogging) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_multithread.log";
    config.minLevel = LogLevel::INFO;
    logger.configure(config);
    
    const int numThreads = 10;
    const int logsPerThread = 100;
    std::vector<std::thread> threads;
    
    // Launch threads
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, logsPerThread]() {
            for (int j = 0; j < logsPerThread; ++j) {
                LOG_INFO("Thread " + std::to_string(i) + " log " + std::to_string(j));
            }
        });
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Flush and count lines
    logger.flush();
    
    std::ifstream logFile(config.logFilePath);
    int lineCount = 0;
    std::string line;
    
    while (std::getline(logFile, line)) {
        lineCount++;
    }
    
    EXPECT_EQ(lineCount, numThreads * logsPerThread);
}

TEST_F(LoggerTest, PerformanceLogging) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_performance.log";
    logger.configure(config);
    
    // Test performance logging
    LOG_PERF_START("test_operation");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG_PERF_METRIC("test_operation", "items_processed", 1000);
    LOG_PERF_END("test_operation");
    
    logger.flush();
    
    // Check log contains performance data
    std::ifstream logFile(config.logFilePath);
    std::string content((std::istreambuf_iterator<char>(logFile)),
                       std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("Performance: test_operation") != std::string::npos);
    EXPECT_TRUE(content.find("items_processed=1000") != std::string::npos);
}

TEST_F(LoggerTest, ScopedPerformanceLogging) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_scoped_perf.log";
    logger.configure(config);
    
    {
        LOG_PERF_SCOPE("scoped_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    logger.flush();
    
    // Check log contains scoped performance data
    std::ifstream logFile(config.logFilePath);
    std::string content((std::istreambuf_iterator<char>(logFile)),
                       std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("Performance: scoped_operation") != std::string::npos);
}

TEST_F(LoggerTest, ContextLogging) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_context.log";
    logger.configure(config);
    
    std::unordered_map<std::string, std::string> context = {
        {"user_id", "123"},
        {"session", "abc"},
        {"action", "test"}
    };
    
    LOG_WITH_CONTEXT(LogLevel::INFO, "Test message with context", context);
    
    logger.flush();
    
    // Check log contains context
    std::ifstream logFile(config.logFilePath);
    std::string content((std::istreambuf_iterator<char>(logFile)),
                       std::istreambuf_iterator<char>());
    
    EXPECT_TRUE(content.find("Test message with context") != std::string::npos);
    EXPECT_TRUE(content.find("user_id=123") != std::string::npos);
    EXPECT_TRUE(content.find("session=abc") != std::string::npos);
    EXPECT_TRUE(content.find("action=test") != std::string::npos);
}

TEST_F(LoggerTest, LogRotationBySize) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_rotation.log";
    config.maxFileSize = 1024; // 1KB for easy testing
    config.maxFiles = 3;
    logger.configure(config);
    
    // Write enough data to trigger rotation
    std::string longMessage(100, 'X'); // 100 bytes per message
    for (int i = 0; i < 20; ++i) {
        LOG_INFO(longMessage);
    }
    
    logger.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check that rotation occurred
    std::vector<std::filesystem::path> logFiles;
    for (const auto& entry : std::filesystem::directory_iterator("logs")) {
        if (entry.path().stem().string().find("test_rotation") == 0) {
            logFiles.push_back(entry.path());
        }
    }
    
    EXPECT_GT(logFiles.size(), 1);
    EXPECT_LE(logFiles.size(), config.maxFiles + 1); // +1 for current file
}

TEST_F(LoggerTest, CustomFormatter) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_formatter.log";
    logger.configure(config);
    
    // Set custom formatter
    logger.setFormatter([](const LogEntry& entry) {
        return "CUSTOM: " + entry.message;
    });
    
    LOG_INFO("Test message");
    logger.flush();
    
    // Check custom format
    std::ifstream logFile(config.logFilePath);
    std::string line;
    std::getline(logFile, line);
    
    EXPECT_EQ(line, "CUSTOM: Test message");
}

TEST_F(LoggerTest, QueueOverflow) {
    Logger& logger = Logger::getInstance();
    
    LoggerConfig config;
    config.enableConsole = false;
    config.enableFile = true;
    config.logFilePath = "logs/test_overflow.log";
    config.queueSize = 10; // Small queue for testing
    logger.configure(config);
    
    // Try to overflow the queue
    for (int i = 0; i < 100; ++i) {
        LOG_INFO("Message " + std::to_string(i));
    }
    
    logger.flush();
    
    // Some messages should have been logged
    std::ifstream logFile(config.logFilePath);
    int lineCount = 0;
    std::string line;
    
    while (std::getline(logFile, line)) {
        lineCount++;
    }
    
    // Should have some messages, but not all 100
    EXPECT_GT(lineCount, 0);
    EXPECT_LT(lineCount, 100);
}