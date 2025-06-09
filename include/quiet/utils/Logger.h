#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace quiet {
namespace utils {

// Log levels
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

// Log entry structure
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    std::thread::id threadId;
    std::string message;
    std::string file;
    std::string function;
    int line;
    std::unordered_map<std::string, std::string> context;
};

// Logger configuration
struct LoggerConfig {
    bool enableConsole = true;
    bool enableFile = true;
    bool enableRemote = false;
    std::string logFilePath = "logs/quiet.log";
    size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    size_t maxFiles = 5;
    LogLevel minLevel = LogLevel::INFO;
    std::string dateFormat = "%Y-%m-%d %H:%M:%S";
    bool includeThreadId = true;
    bool includeSourceLocation = true;
    size_t queueSize = 10000;
};

// Remote logging configuration
struct RemoteLogConfig {
    std::string host;
    int port;
    std::string protocol = "tcp"; // tcp or udp
    bool useSSL = false;
    std::chrono::milliseconds timeout{5000};
};

// Performance metrics
struct PerformanceMetrics {
    std::chrono::steady_clock::time_point startTime;
    std::string operation;
    std::unordered_map<std::string, double> metrics;
};

// Lock-free queue for high-performance logging
template<typename T>
class LockFreeQueue {
public:
    explicit LockFreeQueue(size_t capacity);
    ~LockFreeQueue();
    
    bool push(T&& item);
    bool pop(T& item);
    size_t size() const;
    bool empty() const;
    
private:
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
        
        Node() : data(nullptr), next(nullptr) {}
    };
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    std::atomic<size_t> count;
    const size_t capacity;
};

// Logger class
class Logger {
public:
    static Logger& getInstance();
    
    // Configuration
    void configure(const LoggerConfig& config);
    void configureRemote(const RemoteLogConfig& config);
    
    // Logging methods
    void log(LogLevel level, const std::string& message,
             const std::string& file = "", const std::string& function = "", int line = 0);
    
    void logWithContext(LogLevel level, const std::string& message,
                       const std::unordered_map<std::string, std::string>& context,
                       const std::string& file = "", const std::string& function = "", int line = 0);
    
    // Performance logging
    void startPerformanceLog(const std::string& operation);
    void endPerformanceLog(const std::string& operation);
    void logPerformanceMetric(const std::string& operation, const std::string& metric, double value);
    
    // Log level control
    void setLogLevel(LogLevel level);
    LogLevel getLogLevel() const;
    
    // Flush and shutdown
    void flush();
    void shutdown();
    
    // Custom formatting
    using Formatter = std::function<std::string(const LogEntry&)>;
    void setFormatter(Formatter formatter);
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void workerThread();
    void processLogEntry(const LogEntry& entry);
    void writeToConsole(const LogEntry& entry);
    void writeToFile(const LogEntry& entry);
    void sendToRemote(const LogEntry& entry);
    void rotateLogFile();
    void checkLogRotation();
    std::string formatLogEntry(const LogEntry& entry);
    std::string getLogLevelString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
    
    // Configuration
    LoggerConfig config_;
    RemoteLogConfig remoteConfig_;
    std::atomic<LogLevel> minLevel_;
    
    // Threading
    std::unique_ptr<std::thread> workerThread_;
    std::atomic<bool> running_;
    std::condition_variable cv_;
    std::mutex cvMutex_;
    
    // Queue
    std::unique_ptr<LockFreeQueue<LogEntry>> queue_;
    
    // File handling
    std::unique_ptr<std::ofstream> fileStream_;
    std::mutex fileMutex_;
    size_t currentFileSize_;
    
    // Performance tracking
    std::unordered_map<std::string, PerformanceMetrics> performanceMetrics_;
    std::mutex perfMutex_;
    
    // Custom formatter
    Formatter formatter_;
    
    // Remote logging
    std::atomic<bool> remoteEnabled_;
    std::unique_ptr<std::thread> remoteThread_;
    std::unique_ptr<LockFreeQueue<LogEntry>> remoteQueue_;
};

// Convenience macros
#define LOG_DEBUG(msg) \
    quiet::utils::Logger::getInstance().log(quiet::utils::LogLevel::DEBUG, msg, __FILE__, __FUNCTION__, __LINE__)

#define LOG_INFO(msg) \
    quiet::utils::Logger::getInstance().log(quiet::utils::LogLevel::INFO, msg, __FILE__, __FUNCTION__, __LINE__)

#define LOG_WARNING(msg) \
    quiet::utils::Logger::getInstance().log(quiet::utils::LogLevel::WARNING, msg, __FILE__, __FUNCTION__, __LINE__)

#define LOG_ERROR(msg) \
    quiet::utils::Logger::getInstance().log(quiet::utils::LogLevel::ERROR, msg, __FILE__, __FUNCTION__, __LINE__)

#define LOG_CRITICAL(msg) \
    quiet::utils::Logger::getInstance().log(quiet::utils::LogLevel::CRITICAL, msg, __FILE__, __FUNCTION__, __LINE__)

// Context logging macros
#define LOG_WITH_CONTEXT(level, msg, context) \
    quiet::utils::Logger::getInstance().logWithContext(level, msg, context, __FILE__, __FUNCTION__, __LINE__)

// Performance logging macros
#define LOG_PERF_START(operation) \
    quiet::utils::Logger::getInstance().startPerformanceLog(operation)

#define LOG_PERF_END(operation) \
    quiet::utils::Logger::getInstance().endPerformanceLog(operation)

#define LOG_PERF_METRIC(operation, metric, value) \
    quiet::utils::Logger::getInstance().logPerformanceMetric(operation, metric, value)

// Scoped performance logger
class ScopedPerformanceLogger {
public:
    explicit ScopedPerformanceLogger(const std::string& operation)
        : operation_(operation) {
        Logger::getInstance().startPerformanceLog(operation_);
    }
    
    ~ScopedPerformanceLogger() {
        Logger::getInstance().endPerformanceLog(operation_);
    }
    
    void addMetric(const std::string& metric, double value) {
        Logger::getInstance().logPerformanceMetric(operation_, metric, value);
    }
    
private:
    std::string operation_;
};

#define LOG_PERF_SCOPE(operation) \
    quiet::utils::ScopedPerformanceLogger _perf_logger_##__LINE__(operation)

} // namespace utils
} // namespace quiet