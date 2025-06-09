#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <any>

namespace quiet {
namespace core {

/**
 * @brief Event types for the QUIET application
 */
enum class EventType {
    // Audio events
    AudioDeviceChanged,
    AudioDeviceError,
    AudioLevelChanged,
    AudioProcessingStarted,
    AudioProcessingStopped,
    
    // Processing events
    NoiseReductionToggled,
    NoiseReductionLevelChanged,
    ProcessingStatsUpdated,
    
    // UI events
    WindowShown,
    WindowHidden,
    SettingsChanged,
    
    // System events
    ApplicationStarted,
    ApplicationShutdown,
    ErrorOccurred
};

/**
 * @brief Base class for event data
 */
class EventData {
public:
    virtual ~EventData() = default;
    
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const {
        auto it = m_data.find(key);
        if (it != m_data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return defaultValue;
            }
        }
        return defaultValue;
    }
    
    template<typename T>
    void setValue(const std::string& key, const T& value) {
        m_data[key] = value;
    }
    
    bool hasKey(const std::string& key) const {
        return m_data.find(key) != m_data.end();
    }

private:
    std::unordered_map<std::string, std::any> m_data;
};

/**
 * @brief Event structure containing type and data
 */
struct Event {
    EventType type;
    std::shared_ptr<EventData> data;
    std::chrono::steady_clock::time_point timestamp;
    
    Event(EventType t, std::shared_ptr<EventData> d = nullptr)
        : type(t), data(d), timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Thread-safe event dispatcher for decoupled communication
 * 
 * This class provides:
 * - Thread-safe event publishing and subscription
 * - Asynchronous event delivery
 * - Event filtering and prioritization
 * - Automatic listener cleanup
 * - Performance monitoring
 */
class EventDispatcher {
public:
    using EventListener = std::function<void(const Event&)>;
    using ListenerHandle = uint64_t;

    EventDispatcher();
    ~EventDispatcher();

    // Lifecycle
    void start();
    void stop();
    bool isRunning() const;

    // Event publishing
    void publish(EventType type, std::shared_ptr<EventData> data = nullptr);
    void publishImmediate(EventType type, std::shared_ptr<EventData> data = nullptr);
    
    // Event subscription
    ListenerHandle subscribe(EventType type, EventListener listener);
    ListenerHandle subscribeAll(EventListener listener);
    bool unsubscribe(ListenerHandle handle);
    void unsubscribeAll();

    // Event filtering
    void setEventFilter(EventType type, bool enabled);
    bool isEventFiltered(EventType type) const;

    // Statistics
    struct Stats {
        uint64_t eventsPublished = 0;
        uint64_t eventsDelivered = 0;
        uint64_t eventsDropped = 0;
        uint64_t activeListeners = 0;
        double averageDeliveryTime = 0.0;  // milliseconds
        size_t queueSize = 0;
    };
    
    Stats getStats() const;
    void resetStats();

    // Configuration
    void setMaxQueueSize(size_t maxSize);
    void setDeliveryTimeout(std::chrono::milliseconds timeout);

private:
    // Internal event processing
    void processEvents();
    void deliverEvent(const Event& event);
    void deliverToListener(const EventListener& listener, const Event& event);
    void updateStats(const Event& event, double deliveryTime);
    
    // Listener management
    struct ListenerInfo {
        EventType type;
        EventListener listener;
        std::chrono::steady_clock::time_point lastActivity;
        uint64_t eventsReceived = 0;
    };
    
    void cleanupInactiveListeners();
    
    // Thread safety
    mutable std::mutex m_queueMutex;
    mutable std::mutex m_listenersMutex;
    mutable std::mutex m_statsMutex;
    
    // Event queue
    std::queue<Event> m_eventQueue;
    size_t m_maxQueueSize{10000};
    std::condition_variable m_queueCondition;
    
    // Listeners
    std::unordered_map<ListenerHandle, ListenerInfo> m_listeners;
    std::unordered_map<EventType, std::vector<ListenerHandle>> m_typeListeners;
    std::vector<ListenerHandle> m_globalListeners;
    
    ListenerHandle m_nextHandle{1};
    
    // Event filtering
    std::unordered_map<EventType, bool> m_eventFilters;
    
    // Processing thread
    std::unique_ptr<std::thread> m_processingThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_shouldStop{false};
    
    // Configuration
    std::chrono::milliseconds m_deliveryTimeout{100};
    
    // Statistics
    mutable Stats m_stats;
    std::chrono::steady_clock::time_point m_lastStatsUpdate;
};

/**
 * @brief Helper functions for creating common event data
 */
namespace EventDataFactory {
    std::shared_ptr<EventData> createAudioLevelData(float level);
    std::shared_ptr<EventData> createDeviceChangedData(const std::string& deviceId, const std::string& deviceName);
    std::shared_ptr<EventData> createErrorData(const std::string& message, int errorCode = 0);
    std::shared_ptr<EventData> createProcessingStatsData(float cpuUsage, float latency, float reductionLevel);
}

} // namespace core
} // namespace quiet