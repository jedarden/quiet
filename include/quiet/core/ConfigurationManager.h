#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <any>
#include <functional>
#include "EventDispatcher.h"

namespace quiet {
namespace core {

/**
 * @brief Configuration value variant type
 */
class ConfigValue {
public:
    ConfigValue() = default;
    
    template<typename T>
    ConfigValue(const T& value) : m_value(value) {}
    
    template<typename T>
    T get(const T& defaultValue = T{}) const {
        try {
            return std::any_cast<T>(m_value);
        } catch (const std::bad_any_cast&) {
            return defaultValue;
        }
    }
    
    template<typename T>
    void set(const T& value) {
        m_value = value;
    }
    
    bool empty() const {
        return !m_value.has_value();
    }
    
    void clear() {
        m_value.reset();
    }

private:
    std::any m_value;
};

/**
 * @brief Thread-safe configuration management system
 * 
 * Provides hierarchical configuration storage with:
 * - JSON file persistence
 * - Type-safe value access
 * - Change notifications
 * - Default value handling
 * - Automatic saving
 */
class ConfigurationManager {
public:
    using ChangeCallback = std::function<void(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue)>;
    using CallbackHandle = uint64_t;

    explicit ConfigurationManager(EventDispatcher& eventDispatcher);
    ~ConfigurationManager();

    // Lifecycle
    bool initialize(const std::string& configFilePath = "");
    void shutdown();
    bool isInitialized() const;

    // Value access
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            return it->second.get<T>(defaultValue);
        }
        return defaultValue;
    }
    
    template<typename T>
    void setValue(const std::string& key, const T& value, bool saveImmediately = false) {
        ConfigValue oldValue;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_values.find(key);
            if (it != m_values.end()) {
                oldValue = it->second;
            }
            m_values[key].set(value);
            m_isDirty = true;
        }
        
        // Notify listeners
        notifyChange(key, oldValue, ConfigValue(value));
        
        if (saveImmediately) {
            saveConfiguration();
        }
    }
    
    bool hasValue(const std::string& key) const;
    void removeValue(const std::string& key);
    void clear();

    // Configuration persistence
    bool loadConfiguration();
    bool saveConfiguration();
    void setAutoSave(bool enabled, int intervalSeconds = 30);

    // Change notifications
    CallbackHandle addChangeCallback(const std::string& keyPattern, ChangeCallback callback);
    CallbackHandle addGlobalChangeCallback(ChangeCallback callback);
    bool removeChangeCallback(CallbackHandle handle);

    // Utility methods
    std::vector<std::string> getKeys() const;
    std::vector<std::string> getKeysWithPrefix(const std::string& prefix) const;
    
    // Default values management
    void setDefaults(const std::unordered_map<std::string, ConfigValue>& defaults);
    void restoreDefaults();
    void restoreDefault(const std::string& key);

    // File operations
    std::string getConfigFilePath() const;
    void setConfigFilePath(const std::string& path);
    
    // Statistics
    struct Stats {
        size_t totalKeys = 0;
        size_t callbacks = 0;
        uint64_t loadCount = 0;
        uint64_t saveCount = 0;
        uint64_t changeNotifications = 0;
        std::string lastError;
    };
    
    Stats getStats() const;

private:
    // Internal methods
    void initializeDefaults();
    void notifyChange(const std::string& key, const ConfigValue& oldValue, const ConfigValue& newValue);
    void autoSaveWorker();
    bool matchesPattern(const std::string& key, const std::string& pattern) const;
    
    // JSON serialization
    bool loadFromJson(const std::string& filePath);
    bool saveToJson(const std::string& filePath);
    ConfigValue parseJsonValue(const std::string& jsonStr);
    std::string serializeValue(const ConfigValue& value);
    
    // Member variables
    EventDispatcher& m_eventDispatcher;
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, ConfigValue> m_values;
    std::unordered_map<std::string, ConfigValue> m_defaults;
    
    // Change callbacks
    struct CallbackInfo {
        std::string pattern;
        ChangeCallback callback;
        bool isGlobal;
    };
    
    std::unordered_map<CallbackHandle, CallbackInfo> m_callbacks;
    CallbackHandle m_nextCallbackHandle{1};
    
    // File management
    std::string m_configFilePath;
    bool m_isDirty{false};
    bool m_isInitialized{false};
    
    // Auto-save
    bool m_autoSaveEnabled{true};
    int m_autoSaveInterval{30};  // seconds
    std::unique_ptr<std::thread> m_autoSaveThread;
    std::atomic<bool> m_shouldStopAutoSave{false};
    
    // Statistics
    mutable Stats m_stats;
};

} // namespace core
} // namespace quiet