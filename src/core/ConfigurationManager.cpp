#include "quiet/core/ConfigurationManager.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <regex>

// Simple JSON parsing for configuration
#include <sstream>

namespace quiet {
namespace core {

namespace {
    // Simple JSON parsing functions
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }
    
    std::string getDefaultConfigPath() {
        #ifdef _WIN32
            const char* appData = std::getenv("APPDATA");
            if (appData) {
                return std::string(appData) + "\\QUIET\\config.json";
            }
            return "config.json";
        #else
            const char* home = std::getenv("HOME");
            if (home) {
                return std::string(home) + "/.config/quiet/config.json";
            }
            return "config.json";
        #endif
    }
}

ConfigurationManager::ConfigurationManager(EventDispatcher& eventDispatcher)
    : m_eventDispatcher(eventDispatcher) {
    
    initializeDefaults();
}

ConfigurationManager::~ConfigurationManager() {
    shutdown();
}

bool ConfigurationManager::initialize(const std::string& configFilePath) {
    if (m_isInitialized) {
        return true;
    }
    
    // Set config file path
    if (configFilePath.empty()) {
        m_configFilePath = getDefaultConfigPath();
    } else {
        m_configFilePath = configFilePath;
    }
    
    // Ensure config directory exists
    std::filesystem::path configPath(m_configFilePath);
    std::error_code ec;
    std::filesystem::create_directories(configPath.parent_path(), ec);
    
    // Load existing configuration or create with defaults
    if (std::filesystem::exists(m_configFilePath)) {
        if (!loadConfiguration()) {
            // If loading fails, backup the file and create new one
            std::string backupPath = m_configFilePath + ".backup";
            std::filesystem::copy_file(m_configFilePath, backupPath, ec);
            
            m_stats.lastError = "Failed to load config, created backup";
            
            // Initialize with defaults
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_values = m_defaults;
            }
            saveConfiguration();
        }
    } else {
        // Initialize with defaults
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_values = m_defaults;
        }
        saveConfiguration();
    }
    
    // Start auto-save thread
    if (m_autoSaveEnabled) {
        m_shouldStopAutoSave.store(false);
        m_autoSaveThread = std::make_unique<std::thread>(&ConfigurationManager::autoSaveWorker, this);
    }
    
    m_isInitialized = true;
    
    // Notify initialization
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("config_path", m_configFilePath);
    eventData->setValue("key_count", static_cast<int>(m_values.size()));
    m_eventDispatcher.publish(EventType::SettingsChanged, eventData);
    
    return true;
}

void ConfigurationManager::shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    // Stop auto-save thread
    if (m_autoSaveThread) {
        m_shouldStopAutoSave.store(true);
        if (m_autoSaveThread->joinable()) {
            m_autoSaveThread->join();
        }
        m_autoSaveThread.reset();
    }
    
    // Save any pending changes
    if (m_isDirty) {
        saveConfiguration();
    }
    
    // Clear all data
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_values.clear();
        m_callbacks.clear();
    }
    
    m_isInitialized = false;
}

bool ConfigurationManager::isInitialized() const {
    return m_isInitialized;
}

bool ConfigurationManager::hasValue(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_values.find(key) != m_values.end();
}

void ConfigurationManager::removeValue(const std::string& key) {
    ConfigValue oldValue;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            oldValue = it->second;
            m_values.erase(it);
            m_isDirty = true;
        } else {
            return;  // Key doesn't exist
        }
    }
    
    notifyChange(key, oldValue, ConfigValue());
}

void ConfigurationManager::clear() {
    std::unordered_map<std::string, ConfigValue> oldValues;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        oldValues = m_values;
        m_values.clear();
        m_isDirty = true;
    }
    
    // Notify about all changes
    for (const auto& [key, oldValue] : oldValues) {
        notifyChange(key, oldValue, ConfigValue());
    }
}

bool ConfigurationManager::loadConfiguration() {
    try {
        std::ifstream file(m_configFilePath);
        if (!file.is_open()) {
            m_stats.lastError = "Cannot open config file for reading";
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        if (content.empty()) {
            m_stats.lastError = "Config file is empty";
            return false;
        }
        
        // Simple JSON parsing (in a real implementation, use a proper JSON library)
        std::unordered_map<std::string, ConfigValue> loadedValues;
        
        // Remove outer braces and parse key-value pairs
        content = trim(content);
        if (content.front() == '{' && content.back() == '}') {
            content = content.substr(1, content.length() - 2);
        }
        
        std::istringstream iss(content);
        std::string line;
        
        while (std::getline(iss, line, ',')) {
            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;
            
            std::string key = trim(line.substr(0, colonPos));
            std::string value = trim(line.substr(colonPos + 1));
            
            // Remove quotes from key
            if (key.front() == '"' && key.back() == '"') {
                key = key.substr(1, key.length() - 2);
            }
            
            // Parse value based on type
            ConfigValue configValue;
            if (value == "true") {
                configValue.set(true);
            } else if (value == "false") {
                configValue.set(false);
            } else if (value.front() == '"' && value.back() == '"') {
                // String value
                configValue.set(value.substr(1, value.length() - 2));
            } else {
                // Try to parse as number
                try {
                    if (value.find('.') != std::string::npos) {
                        configValue.set(std::stod(value));
                    } else {
                        configValue.set(std::stoi(value));
                    }
                } catch (...) {
                    // Fall back to string
                    configValue.set(value);
                }
            }
            
            loadedValues[key] = configValue;
        }
        
        // Update values
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_values = std::move(loadedValues);
            m_isDirty = false;
        }
        
        m_stats.loadCount++;
        m_stats.totalKeys = m_values.size();
        
        return true;
        
    } catch (const std::exception& e) {
        m_stats.lastError = "Exception during load: " + std::string(e.what());
        return false;
    }
}

bool ConfigurationManager::saveConfiguration() {
    try {
        // Ensure directory exists
        std::filesystem::path configPath(m_configFilePath);
        std::error_code ec;
        std::filesystem::create_directories(configPath.parent_path(), ec);
        
        std::ofstream file(m_configFilePath);
        if (!file.is_open()) {
            m_stats.lastError = "Cannot open config file for writing";
            return false;
        }
        
        file << "{\n";
        
        bool first = true;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            for (const auto& [key, value] : m_values) {
                if (!first) {
                    file << ",\n";
                }
                first = false;
                
                file << "  \"" << key << "\": ";
                
                // Serialize value based on type
                // This is a simplified approach - in practice, use proper JSON serialization
                try {
                    // Try different types
                    if (auto strVal = value.get<std::string>(""); !strVal.empty() || 
                        (strVal.empty() && !value.empty())) {
                        file << "\"" << strVal << "\"";
                    } else if (auto boolVal = value.get<bool>(false); 
                              value.get<bool>(true) != value.get<bool>(false)) {
                        file << (boolVal ? "true" : "false");
                    } else if (auto intVal = value.get<int>(0); 
                              value.get<int>(-1) != value.get<int>(0)) {
                        file << intVal;
                    } else if (auto doubleVal = value.get<double>(0.0); 
                              value.get<double>(-1.0) != value.get<double>(0.0)) {
                        file << doubleVal;
                    } else {
                        file << "null";
                    }
                } catch (...) {
                    file << "null";
                }
            }
            
            m_isDirty = false;
        }
        
        file << "\n}\n";
        file.close();
        
        m_stats.saveCount++;
        
        return true;
        
    } catch (const std::exception& e) {
        m_stats.lastError = "Exception during save: " + std::string(e.what());
        return false;
    }
}

void ConfigurationManager::setAutoSave(bool enabled, int intervalSeconds) {
    m_autoSaveEnabled = enabled;
    m_autoSaveInterval = intervalSeconds;
    
    // Restart auto-save thread if running
    if (m_isInitialized && m_autoSaveThread) {
        m_shouldStopAutoSave.store(true);
        if (m_autoSaveThread->joinable()) {
            m_autoSaveThread->join();
        }
        
        if (enabled) {
            m_shouldStopAutoSave.store(false);
            m_autoSaveThread = std::make_unique<std::thread>(&ConfigurationManager::autoSaveWorker, this);
        }
    }
}

ConfigurationManager::CallbackHandle ConfigurationManager::addChangeCallback(
    const std::string& keyPattern, ChangeCallback callback) {
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    CallbackHandle handle = m_nextCallbackHandle++;
    CallbackInfo info;
    info.pattern = keyPattern;
    info.callback = std::move(callback);
    info.isGlobal = false;
    
    m_callbacks[handle] = std::move(info);
    m_stats.callbacks = m_callbacks.size();
    
    return handle;
}

ConfigurationManager::CallbackHandle ConfigurationManager::addGlobalChangeCallback(ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    CallbackHandle handle = m_nextCallbackHandle++;
    CallbackInfo info;
    info.pattern = "*";
    info.callback = std::move(callback);
    info.isGlobal = true;
    
    m_callbacks[handle] = std::move(info);
    m_stats.callbacks = m_callbacks.size();
    
    return handle;
}

bool ConfigurationManager::removeChangeCallback(CallbackHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_callbacks.find(handle);
    if (it != m_callbacks.end()) {
        m_callbacks.erase(it);
        m_stats.callbacks = m_callbacks.size();
        return true;
    }
    
    return false;
}

std::vector<std::string> ConfigurationManager::getKeys() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> keys;
    keys.reserve(m_values.size());
    
    for (const auto& [key, value] : m_values) {
        keys.push_back(key);
    }
    
    return keys;
}

std::vector<std::string> ConfigurationManager::getKeysWithPrefix(const std::string& prefix) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::string> keys;
    
    for (const auto& [key, value] : m_values) {
        if (key.find(prefix) == 0) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

void ConfigurationManager::setDefaults(const std::unordered_map<std::string, ConfigValue>& defaults) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaults = defaults;
}

void ConfigurationManager::restoreDefaults() {
    std::unordered_map<std::string, ConfigValue> oldValues;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        oldValues = m_values;
        m_values = m_defaults;
        m_isDirty = true;
    }
    
    // Notify about changes
    for (const auto& [key, defaultValue] : m_defaults) {
        auto oldIt = oldValues.find(key);
        ConfigValue oldValue = (oldIt != oldValues.end()) ? oldIt->second : ConfigValue();
        notifyChange(key, oldValue, defaultValue);
    }
}

void ConfigurationManager::restoreDefault(const std::string& key) {
    auto it = m_defaults.find(key);
    if (it != m_defaults.end()) {
        ConfigValue oldValue;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto valueIt = m_values.find(key);
            if (valueIt != m_values.end()) {
                oldValue = valueIt->second;
            }
            m_values[key] = it->second;
            m_isDirty = true;
        }
        
        notifyChange(key, oldValue, it->second);
    }
}

std::string ConfigurationManager::getConfigFilePath() const {
    return m_configFilePath;
}

void ConfigurationManager::setConfigFilePath(const std::string& path) {
    m_configFilePath = path;
}

ConfigurationManager::Stats ConfigurationManager::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalKeys = m_values.size();
    return m_stats;
}

// Private methods

void ConfigurationManager::initializeDefaults() {
    // Set up default configuration values
    m_defaults["audio.input_device_id"] = ConfigValue(std::string(""));
    m_defaults["audio.buffer_size"] = ConfigValue(256);
    m_defaults["audio.sample_rate"] = ConfigValue(48000);
    m_defaults["audio.auto_select_device"] = ConfigValue(true);
    m_defaults["audio.monitor_input"] = ConfigValue(true);
    
    m_defaults["processing.noise_reduction_enabled"] = ConfigValue(true);
    m_defaults["processing.reduction_level"] = ConfigValue(std::string("medium"));
    m_defaults["processing.adaptive_mode"] = ConfigValue(true);
    m_defaults["processing.vad_threshold"] = ConfigValue(0.5);
    m_defaults["processing.preserve_speech"] = ConfigValue(true);
    
    m_defaults["ui.window_position.x"] = ConfigValue(100);
    m_defaults["ui.window_position.y"] = ConfigValue(100);
    m_defaults["ui.window_size.width"] = ConfigValue(800);
    m_defaults["ui.window_size.height"] = ConfigValue(600);
    m_defaults["ui.start_minimized"] = ConfigValue(false);
    m_defaults["ui.close_to_tray"] = ConfigValue(true);
    m_defaults["ui.theme"] = ConfigValue(std::string("dark"));
    m_defaults["ui.show_advanced_controls"] = ConfigValue(false);
    m_defaults["ui.visualization_fps"] = ConfigValue(30);
    
    m_defaults["system.auto_start"] = ConfigValue(false);
    m_defaults["system.check_updates"] = ConfigValue(true);
    m_defaults["system.send_usage_stats"] = ConfigValue(false);
    m_defaults["system.log_level"] = ConfigValue(std::string("info"));
    
    m_defaults["virtual_device.auto_create"] = ConfigValue(true);
    m_defaults["virtual_device.device_name"] = ConfigValue(std::string("QUIET Virtual Mic"));
    m_defaults["virtual_device.sample_rate"] = ConfigValue(48000);
    m_defaults["virtual_device.channels"] = ConfigValue(1);
    
    m_defaults["performance.cpu_limit"] = ConfigValue(15.0);
    m_defaults["performance.memory_limit"] = ConfigValue(200);
    m_defaults["performance.priority"] = ConfigValue(std::string("normal"));
    m_defaults["performance.use_simd"] = ConfigValue(true);
}

void ConfigurationManager::notifyChange(const std::string& key, const ConfigValue& oldValue, 
                                       const ConfigValue& newValue) {
    std::vector<ChangeCallback> callbacksToNotify;
    
    // Collect matching callbacks
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [handle, info] : m_callbacks) {
            if (info.isGlobal || matchesPattern(key, info.pattern)) {
                callbacksToNotify.push_back(info.callback);
            }
        }
    }
    
    // Execute callbacks (outside of lock)
    for (const auto& callback : callbacksToNotify) {
        try {
            callback(key, oldValue, newValue);
        } catch (...) {
            // Log error but continue
        }
    }
    
    m_stats.changeNotifications++;
    
    // Publish event
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("key", key);
    m_eventDispatcher.publish(EventType::SettingsChanged, eventData);
}

void ConfigurationManager::autoSaveWorker() {
    while (!m_shouldStopAutoSave.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(m_autoSaveInterval));
        
        if (m_shouldStopAutoSave.load()) {
            break;
        }
        
        if (m_isDirty) {
            saveConfiguration();
        }
    }
}

bool ConfigurationManager::matchesPattern(const std::string& key, const std::string& pattern) const {
    if (pattern == "*") {
        return true;  // Global pattern
    }
    
    // Simple wildcard matching (in practice, use regex or glob)
    if (pattern.find('*') != std::string::npos) {
        // Convert wildcard to regex
        std::string regexPattern = pattern;
        std::replace(regexPattern.begin(), regexPattern.end(), '*', '.');
        regexPattern = "^" + regexPattern + "$";
        
        try {
            std::regex regex(regexPattern);
            return std::regex_match(key, regex);
        } catch (...) {
            return false;
        }
    }
    
    // Exact match
    return key == pattern;
}

} // namespace core
} // namespace quiet