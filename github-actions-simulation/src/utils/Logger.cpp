#include "quiet/utils/Logger.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace quiet {
namespace utils {

// LockFreeQueue implementation
template<typename T>
LockFreeQueue<T>::LockFreeQueue(size_t capacity) 
    : capacity(capacity), count(0) {
    Node* dummy = new Node;
    head.store(dummy);
    tail.store(dummy);
}

template<typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    T item;
    while (pop(item)) {
        // Drain the queue
    }
    
    Node* current = head.load();
    while (current) {
        Node* next = current->next.load();
        delete current;
        current = next;
    }
}

template<typename T>
bool LockFreeQueue<T>::push(T&& item) {
    if (count.load() >= capacity) {
        return false;
    }
    
    Node* newNode = new Node;
    T* data = new T(std::move(item));
    newNode->data.store(data);
    
    while (true) {
        Node* last = tail.load();
        Node* next = last->next.load();
        
        if (last == tail.load()) {
            if (next == nullptr) {
                if (last->next.compare_exchange_weak(next, newNode)) {
                    tail.compare_exchange_weak(last, newNode);
                    count.fetch_add(1);
                    return true;
                }
            } else {
                tail.compare_exchange_weak(last, next);
            }
        }
    }
}

template<typename T>
bool LockFreeQueue<T>::pop(T& item) {
    while (true) {
        Node* first = head.load();
        Node* last = tail.load();
        Node* next = first->next.load();
        
        if (first == head.load()) {
            if (first == last) {
                if (next == nullptr) {
                    return false;
                }
                tail.compare_exchange_weak(last, next);
            } else {
                T* data = next->data.load();
                if (data == nullptr) {
                    continue;
                }
                
                if (head.compare_exchange_weak(first, next)) {
                    item = std::move(*data);
                    delete data;
                    delete first;
                    count.fetch_sub(1);
                    return true;
                }
            }
        }
    }
}

template<typename T>
size_t LockFreeQueue<T>::size() const {
    return count.load();
}

template<typename T>
bool LockFreeQueue<T>::empty() const {
    return count.load() == 0;
}

// Explicit template instantiation
template class LockFreeQueue<LogEntry>;

// Logger implementation
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : minLevel_(LogLevel::INFO)
    , running_(true)
    , currentFileSize_(0)
    , remoteEnabled_(false) {
    
    // Initialize with default configuration
    configure(LoggerConfig{});
    
    // Set default formatter
    formatter_ = [this](const LogEntry& entry) {
        return formatLogEntry(entry);
    };
    
    // Start worker thread
    workerThread_ = std::make_unique<std::thread>(&Logger::workerThread, this);
}

Logger::~Logger() {
    shutdown();
}

void Logger::configure(const LoggerConfig& config) {
    config_ = config;
    minLevel_.store(config.minLevel);
    
    // Initialize queues
    queue_ = std::make_unique<LockFreeQueue<LogEntry>>(config.queueSize);
    
    // Create log directory if needed
    if (config.enableFile) {
        std::filesystem::path logPath(config.logFilePath);
        std::filesystem::create_directories(logPath.parent_path());
        
        // Open log file
        std::lock_guard<std::mutex> lock(fileMutex_);
        fileStream_ = std::make_unique<std::ofstream>(config.logFilePath, std::ios::app);
        if (fileStream_->is_open()) {
            currentFileSize_ = std::filesystem::file_size(config.logFilePath);
        }
    }
}

void Logger::configureRemote(const RemoteLogConfig& config) {
    remoteConfig_ = config;
    remoteEnabled_.store(true);
    
    // Initialize remote queue
    remoteQueue_ = std::make_unique<LockFreeQueue<LogEntry>>(config_.queueSize);
    
    // Start remote thread
    if (!remoteThread_) {
        remoteThread_ = std::make_unique<std::thread>([this]() {
            while (running_.load()) {
                LogEntry entry;
                if (remoteQueue_->pop(entry)) {
                    sendToRemote(entry);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        });
    }
}

void Logger::log(LogLevel level, const std::string& message,
                 const std::string& file, const std::string& function, int line) {
    if (level < minLevel_.load()) {
        return;
    }
    
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.threadId = std::this_thread::get_id();
    entry.message = message;
    entry.file = file;
    entry.function = function;
    entry.line = line;
    
    if (!queue_->push(std::move(entry))) {
        // Queue is full, log to stderr
        std::cerr << "Logger queue full, dropping message: " << message << std::endl;
    } else {
        cv_.notify_one();
    }
}

void Logger::logWithContext(LogLevel level, const std::string& message,
                           const std::unordered_map<std::string, std::string>& context,
                           const std::string& file, const std::string& function, int line) {
    if (level < minLevel_.load()) {
        return;
    }
    
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.threadId = std::this_thread::get_id();
    entry.message = message;
    entry.file = file;
    entry.function = function;
    entry.line = line;
    entry.context = context;
    
    if (!queue_->push(std::move(entry))) {
        std::cerr << "Logger queue full, dropping message: " << message << std::endl;
    } else {
        cv_.notify_one();
    }
}

void Logger::startPerformanceLog(const std::string& operation) {
    std::lock_guard<std::mutex> lock(perfMutex_);
    performanceMetrics_[operation] = {
        std::chrono::steady_clock::now(),
        operation,
        {}
    };
}

void Logger::endPerformanceLog(const std::string& operation) {
    std::lock_guard<std::mutex> lock(perfMutex_);
    auto it = performanceMetrics_.find(operation);
    if (it != performanceMetrics_.end()) {
        auto duration = std::chrono::steady_clock::now() - it->second.startTime;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        
        std::stringstream ss;
        ss << "Performance: " << operation << " took " << ms << "ms";
        
        // Add any additional metrics
        for (const auto& [metric, value] : it->second.metrics) {
            ss << ", " << metric << "=" << value;
        }
        
        log(LogLevel::INFO, ss.str());
        performanceMetrics_.erase(it);
    }
}

void Logger::logPerformanceMetric(const std::string& operation, 
                                 const std::string& metric, double value) {
    std::lock_guard<std::mutex> lock(perfMutex_);
    auto it = performanceMetrics_.find(operation);
    if (it != performanceMetrics_.end()) {
        it->second.metrics[metric] = value;
    }
}

void Logger::setLogLevel(LogLevel level) {
    minLevel_.store(level);
}

LogLevel Logger::getLogLevel() const {
    return minLevel_.load();
}

void Logger::flush() {
    // Wait for queue to empty
    while (!queue_->empty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Flush file stream
    if (fileStream_ && fileStream_->is_open()) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        fileStream_->flush();
    }
}

void Logger::shutdown() {
    running_.store(false);
    cv_.notify_all();
    
    if (workerThread_ && workerThread_->joinable()) {
        workerThread_->join();
    }
    
    if (remoteThread_ && remoteThread_->joinable()) {
        remoteThread_->join();
    }
    
    // Process remaining entries
    LogEntry entry;
    while (queue_->pop(entry)) {
        processLogEntry(entry);
    }
    
    // Close file stream
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->close();
    }
}

void Logger::setFormatter(Formatter formatter) {
    formatter_ = formatter;
}

void Logger::workerThread() {
    while (running_.load()) {
        LogEntry entry;
        bool hasEntry = false;
        
        // Try to get an entry from the queue
        if (queue_->pop(entry)) {
            hasEntry = true;
        } else {
            // Wait for notification or timeout
            std::unique_lock<std::mutex> lock(cvMutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(100));
        }
        
        if (hasEntry) {
            processLogEntry(entry);
        }
        
        // Check log rotation periodically
        static auto lastCheck = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastCheck > std::chrono::seconds(60)) {
            checkLogRotation();
            lastCheck = now;
        }
    }
}

void Logger::processLogEntry(const LogEntry& entry) {
    if (config_.enableConsole) {
        writeToConsole(entry);
    }
    
    if (config_.enableFile) {
        writeToFile(entry);
    }
    
    if (remoteEnabled_.load() && remoteQueue_) {
        remoteQueue_->push(LogEntry(entry));
    }
}

void Logger::writeToConsole(const LogEntry& entry) {
    std::string formatted = formatter_(entry);
    
    // Use different streams based on log level
    if (entry.level >= LogLevel::ERROR) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }
}

void Logger::writeToFile(const LogEntry& entry) {
    if (!fileStream_ || !fileStream_->is_open()) {
        return;
    }
    
    std::string formatted = formatter_(entry);
    
    std::lock_guard<std::mutex> lock(fileMutex_);
    *fileStream_ << formatted << std::endl;
    
    currentFileSize_ += formatted.size() + 1; // +1 for newline
    
    // Check if rotation is needed
    if (currentFileSize_ >= config_.maxFileSize) {
        rotateLogFile();
    }
}

void Logger::sendToRemote(const LogEntry& entry) {
    // Simple TCP implementation
    if (remoteConfig_.protocol != "tcp") {
        return; // Only TCP supported for now
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(remoteConfig_.port);
    
    if (inet_pton(AF_INET, remoteConfig_.host.c_str(), &serverAddr.sin_addr) <= 0) {
        close(sock);
        return;
    }
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = remoteConfig_.timeout.count() / 1000;
    tv.tv_usec = (remoteConfig_.timeout.count() % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(sock);
        return;
    }
    
    std::string formatted = formatter_(entry);
    send(sock, formatted.c_str(), formatted.length(), 0);
    
    close(sock);
}

void Logger::rotateLogFile() {
    if (!fileStream_ || !fileStream_->is_open()) {
        return;
    }
    
    fileStream_->close();
    
    // Generate timestamp for rotated file
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    // Rotate files
    std::filesystem::path logPath(config_.logFilePath);
    std::string baseName = logPath.stem().string();
    std::string extension = logPath.extension().string();
    std::filesystem::path directory = logPath.parent_path();
    
    // Create rotated filename
    std::string rotatedName = baseName + "_" + ss.str() + extension;
    std::filesystem::path rotatedPath = directory / rotatedName;
    
    // Rename current log file
    std::filesystem::rename(config_.logFilePath, rotatedPath);
    
    // Clean up old files
    std::vector<std::filesystem::path> logFiles;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().stem().string().find(baseName) == 0) {
            logFiles.push_back(entry.path());
        }
    }
    
    // Sort by modification time
    std::sort(logFiles.begin(), logFiles.end(),
              [](const auto& a, const auto& b) {
                  return std::filesystem::last_write_time(a) > 
                         std::filesystem::last_write_time(b);
              });
    
    // Remove old files
    if (logFiles.size() > config_.maxFiles) {
        for (size_t i = config_.maxFiles; i < logFiles.size(); ++i) {
            std::filesystem::remove(logFiles[i]);
        }
    }
    
    // Open new file
    fileStream_ = std::make_unique<std::ofstream>(config_.logFilePath, std::ios::app);
    currentFileSize_ = 0;
}

void Logger::checkLogRotation() {
    // Check if date has changed
    static auto lastDate = std::chrono::system_clock::now();
    auto now = std::chrono::system_clock::now();
    
    auto lastDay = std::chrono::time_point_cast<std::chrono::days>(lastDate);
    auto currentDay = std::chrono::time_point_cast<std::chrono::days>(now);
    
    if (currentDay > lastDay) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        rotateLogFile();
        lastDate = now;
    }
}

std::string Logger::formatLogEntry(const LogEntry& entry) {
    std::stringstream ss;
    
    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;
    ss << std::put_time(std::localtime(&time_t), config_.dateFormat.c_str());
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    // Log level
    ss << " [" << getLogLevelString(entry.level) << "]";
    
    // Thread ID
    if (config_.includeThreadId) {
        ss << " [" << entry.threadId << "]";
    }
    
    // Source location
    if (config_.includeSourceLocation && !entry.file.empty()) {
        std::filesystem::path filePath(entry.file);
        ss << " [" << filePath.filename().string() << ":" << entry.line 
           << " " << entry.function << "]";
    }
    
    // Message
    ss << " " << entry.message;
    
    // Context
    if (!entry.context.empty()) {
        ss << " {";
        bool first = true;
        for (const auto& [key, value] : entry.context) {
            if (!first) ss << ", ";
            ss << key << "=" << value;
            first = false;
        }
        ss << "}";
    }
    
    return ss.str();
}

std::string Logger::getLogLevelString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARN";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRIT";
        default:                 return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), config_.dateFormat.c_str());
    return ss.str();
}

} // namespace utils
} // namespace quiet