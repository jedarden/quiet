#include "quiet/utils/Logger.h"
#include <thread>
#include <vector>
#include <random>
#include <chrono>

using namespace quiet::utils;

// Example function that performs some work and logs
void performWork(int workerId) {
    // Thread-local random generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 500);
    
    for (int i = 0; i < 10; ++i) {
        // Log with context
        std::unordered_map<std::string, std::string> context = {
            {"worker_id", std::to_string(workerId)},
            {"iteration", std::to_string(i)},
            {"task", "data_processing"}
        };
        
        LOG_WITH_CONTEXT(LogLevel::INFO, 
                        "Starting work iteration", context);
        
        // Simulate work with performance logging
        std::string operation = "work_" + std::to_string(workerId) + "_" + std::to_string(i);
        LOG_PERF_START(operation);
        
        // Simulate some work
        int workDuration = dis(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(workDuration));
        
        // Log some metrics
        LOG_PERF_METRIC(operation, "items_processed", i * 100);
        LOG_PERF_METRIC(operation, "memory_usage_mb", 50 + i * 10);
        
        LOG_PERF_END(operation);
        
        // Random log levels
        if (i % 4 == 0) {
            LOG_DEBUG("Debug information for iteration " + std::to_string(i));
        }
        if (i % 3 == 0) {
            LOG_WARNING("Warning: Resource usage high at iteration " + std::to_string(i));
        }
    }
}

// Example of scoped performance logging
void processLargeDataset() {
    LOG_PERF_SCOPE("large_dataset_processing");
    
    LOG_INFO("Starting large dataset processing");
    
    // Simulate processing stages
    for (int stage = 0; stage < 3; ++stage) {
        LOG_PERF_SCOPE("stage_" + std::to_string(stage));
        
        LOG_INFO("Processing stage " + std::to_string(stage));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Add metrics to the parent scope
        auto& logger = Logger::getInstance();
        logger.logPerformanceMetric("large_dataset_processing", 
                                   "stage_" + std::to_string(stage) + "_complete", 1);
    }
    
    LOG_INFO("Large dataset processing complete");
}

// Example of custom formatter
std::string customFormatter(const LogEntry& entry) {
    std::stringstream ss;
    
    // Simple custom format: LEVEL | MESSAGE | THREAD
    ss << getLogLevelString(entry.level) 
       << " | " << entry.message 
       << " | Thread:" << entry.threadId;
    
    return ss.str();
}

int main() {
    // Configure logger
    LoggerConfig config;
    config.enableConsole = true;
    config.enableFile = true;
    config.logFilePath = "logs/quiet_example.log";
    config.maxFileSize = 5 * 1024 * 1024;  // 5MB
    config.maxFiles = 3;
    config.minLevel = LogLevel::DEBUG;
    config.includeThreadId = true;
    config.includeSourceLocation = true;
    
    Logger& logger = Logger::getInstance();
    logger.configure(config);
    
    // Example 1: Basic logging
    LOG_INFO("Application starting");
    LOG_DEBUG("Debug mode enabled");
    
    // Example 2: Configure remote logging (optional)
    RemoteLogConfig remoteConfig;
    remoteConfig.host = "127.0.0.1";
    remoteConfig.port = 9999;
    remoteConfig.protocol = "tcp";
    remoteConfig.timeout = std::chrono::milliseconds(1000);
    // logger.configureRemote(remoteConfig);  // Uncomment to enable
    
    // Example 3: Multi-threaded logging
    LOG_INFO("Starting multi-threaded test");
    std::vector<std::thread> workers;
    
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back(performWork, i);
    }
    
    // Example 4: Performance logging
    processLargeDataset();
    
    // Example 5: Error handling
    try {
        throw std::runtime_error("Simulated error");
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception caught: ") + e.what());
    }
    
    // Example 6: Critical error
    LOG_CRITICAL("Critical system error simulation");
    
    // Wait for workers
    for (auto& worker : workers) {
        worker.join();
    }
    
    // Example 7: Custom formatter
    LOG_INFO("Switching to custom formatter");
    logger.setFormatter(customFormatter);
    
    LOG_INFO("This message uses custom format");
    LOG_WARNING("Custom formatted warning");
    
    // Example 8: Log with large context
    std::unordered_map<std::string, std::string> largeContext = {
        {"user_id", "12345"},
        {"session_id", "abc-def-ghi"},
        {"ip_address", "192.168.1.100"},
        {"user_agent", "Mozilla/5.0"},
        {"request_id", "req-98765"},
        {"api_version", "v2.0"}
    };
    
    LOG_WITH_CONTEXT(LogLevel::INFO, "User action completed", largeContext);
    
    // Ensure all logs are written
    LOG_INFO("Application shutting down");
    logger.flush();
    
    return 0;
}