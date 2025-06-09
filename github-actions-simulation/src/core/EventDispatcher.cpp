#include "quiet/core/EventDispatcher.h"
#include <algorithm>
#include <chrono>

namespace quiet {
namespace core {

EventDispatcher::EventDispatcher() = default;

EventDispatcher::~EventDispatcher() {
    stop();
}

void EventDispatcher::start() {
    if (m_running.load()) {
        return;  // Already running
    }
    
    m_shouldStop.store(false);
    m_running.store(true);
    
    // Start processing thread
    m_processingThread = std::make_unique<std::thread>(&EventDispatcher::processEvents, this);
}

void EventDispatcher::stop() {
    if (!m_running.load()) {
        return;  // Not running
    }
    
    m_shouldStop.store(true);
    m_queueCondition.notify_all();
    
    if (m_processingThread && m_processingThread->joinable()) {
        m_processingThread->join();
    }
    
    m_processingThread.reset();
    m_running.store(false);
    
    // Clear any remaining events
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<Event> empty;
        std::swap(m_eventQueue, empty);
    }
}

bool EventDispatcher::isRunning() const {
    return m_running.load();
}

void EventDispatcher::publish(EventType type, std::shared_ptr<EventData> data) {
    if (!m_running.load()) {
        return;
    }
    
    // Check if event type is filtered
    if (isEventFiltered(type)) {
        return;
    }
    
    Event event(type, data);
    
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        // Check queue size limit
        if (m_eventQueue.size() >= m_maxQueueSize) {
            // Drop oldest event
            m_eventQueue.pop();
            {
                std::lock_guard<std::mutex> statsLock(m_statsMutex);
                m_stats.eventsDropped++;
            }
        }
        
        m_eventQueue.push(event);
        
        {
            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.eventsPublished++;
            m_stats.queueSize = m_eventQueue.size();
        }
    }
    
    m_queueCondition.notify_one();
}

void EventDispatcher::publishImmediate(EventType type, std::shared_ptr<EventData> data) {
    if (!m_running.load()) {
        return;
    }
    
    // Check if event type is filtered
    if (isEventFiltered(type)) {
        return;
    }
    
    Event event(type, data);
    deliverEvent(event);
    
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.eventsPublished++;
    }
}

EventDispatcher::ListenerHandle EventDispatcher::subscribe(EventType type, EventListener listener) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    
    ListenerHandle handle = m_nextHandle++;
    
    ListenerInfo info;
    info.type = type;
    info.listener = std::move(listener);
    info.lastActivity = std::chrono::steady_clock::now();
    
    m_listeners[handle] = std::move(info);
    m_typeListeners[type].push_back(handle);
    
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.activeListeners = m_listeners.size();
    }
    
    return handle;
}

EventDispatcher::ListenerHandle EventDispatcher::subscribeAll(EventListener listener) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    
    ListenerHandle handle = m_nextHandle++;
    
    ListenerInfo info;
    info.type = static_cast<EventType>(-1);  // Special value for global listeners
    info.listener = std::move(listener);
    info.lastActivity = std::chrono::steady_clock::now();
    
    m_listeners[handle] = std::move(info);
    m_globalListeners.push_back(handle);
    
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.activeListeners = m_listeners.size();
    }
    
    return handle;
}

bool EventDispatcher::unsubscribe(ListenerHandle handle) {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    
    auto it = m_listeners.find(handle);
    if (it == m_listeners.end()) {
        return false;
    }
    
    EventType type = it->second.type;
    
    // Remove from type-specific listeners
    if (type != static_cast<EventType>(-1)) {
        auto& typeListeners = m_typeListeners[type];
        typeListeners.erase(
            std::remove(typeListeners.begin(), typeListeners.end(), handle),
            typeListeners.end()
        );
        
        if (typeListeners.empty()) {
            m_typeListeners.erase(type);
        }
    } else {
        // Remove from global listeners
        m_globalListeners.erase(
            std::remove(m_globalListeners.begin(), m_globalListeners.end(), handle),
            m_globalListeners.end()
        );
    }
    
    m_listeners.erase(it);
    
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.activeListeners = m_listeners.size();
    }
    
    return true;
}

void EventDispatcher::unsubscribeAll() {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    
    m_listeners.clear();
    m_typeListeners.clear();
    m_globalListeners.clear();
    
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.activeListeners = 0;
    }
}

void EventDispatcher::setEventFilter(EventType type, bool enabled) {
    m_eventFilters[type] = !enabled;  // true means filtered (disabled)
}

bool EventDispatcher::isEventFiltered(EventType type) const {
    auto it = m_eventFilters.find(type);
    return (it != m_eventFilters.end()) && it->second;
}

EventDispatcher::Stats EventDispatcher::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void EventDispatcher::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = Stats{};
    m_stats.activeListeners = m_listeners.size();
    m_lastStatsUpdate = std::chrono::steady_clock::now();
}

void EventDispatcher::setMaxQueueSize(size_t maxSize) {
    m_maxQueueSize = maxSize;
}

void EventDispatcher::setDeliveryTimeout(std::chrono::milliseconds timeout) {
    m_deliveryTimeout = timeout;
}

// Private methods

void EventDispatcher::processEvents() {
    while (!m_shouldStop.load()) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        
        // Wait for events or stop signal
        m_queueCondition.wait_for(lock, std::chrono::milliseconds(100), [this] {
            return !m_eventQueue.empty() || m_shouldStop.load();
        });
        
        if (m_shouldStop.load()) {
            break;
        }
        
        // Process all available events
        while (!m_eventQueue.empty() && !m_shouldStop.load()) {
            Event event = m_eventQueue.front();
            m_eventQueue.pop();
            
            {
                std::lock_guard<std::mutex> statsLock(m_statsMutex);
                m_stats.queueSize = m_eventQueue.size();
            }
            
            lock.unlock();
            
            // Deliver event
            deliverEvent(event);
            
            lock.lock();
        }
        
        // Periodically clean up inactive listeners
        static auto lastCleanup = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastCleanup > std::chrono::minutes(5)) {
            lock.unlock();
            cleanupInactiveListeners();
            lastCleanup = now;
            lock.lock();
        }
    }
}

void EventDispatcher::deliverEvent(const Event& event) {
    auto deliveryStart = std::chrono::high_resolution_clock::now();
    
    std::vector<EventListener> listenersToNotify;
    
    // Collect listeners to notify
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        
        // Add global listeners
        for (ListenerHandle handle : m_globalListeners) {
            auto it = m_listeners.find(handle);
            if (it != m_listeners.end()) {
                listenersToNotify.push_back(it->second.listener);
            }
        }
        
        // Add type-specific listeners
        auto typeIt = m_typeListeners.find(event.type);
        if (typeIt != m_typeListeners.end()) {
            for (ListenerHandle handle : typeIt->second) {
                auto it = m_listeners.find(handle);
                if (it != m_listeners.end()) {
                    listenersToNotify.push_back(it->second.listener);
                    
                    // Update activity timestamp
                    it->second.lastActivity = std::chrono::steady_clock::now();
                    it->second.eventsReceived++;
                }
            }
        }
    }
    
    // Deliver to listeners (outside of lock to avoid blocking)
    for (const auto& listener : listenersToNotify) {
        try {
            deliverToListener(listener, event);
        } catch (const std::exception& e) {
            // Log error but continue with other listeners
            // In a real implementation, you'd use a proper logging system
        }
    }
    
    auto deliveryEnd = std::chrono::high_resolution_clock::now();
    auto deliveryTime = std::chrono::duration_cast<std::chrono::microseconds>(
        deliveryEnd - deliveryStart).count() / 1000.0;  // Convert to milliseconds
    
    updateStats(event, deliveryTime);
}

void EventDispatcher::deliverToListener(const EventListener& listener, const Event& event) {
    // Set up timeout for delivery
    std::atomic<bool> completed{false};
    std::exception_ptr exceptionPtr;
    
    std::thread deliveryThread([&]() {
        try {
            listener(event);
            completed.store(true);
        } catch (...) {
            exceptionPtr = std::current_exception();
            completed.store(true);
        }
    });
    
    // Wait for completion or timeout
    auto start = std::chrono::steady_clock::now();
    while (!completed.load()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > m_deliveryTimeout) {
            // Timeout occurred - detach thread and give up
            deliveryThread.detach();
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    if (deliveryThread.joinable()) {
        deliveryThread.join();
    }
    
    // Re-throw any exception that occurred
    if (exceptionPtr) {
        std::rethrow_exception(exceptionPtr);
    }
}

void EventDispatcher::updateStats(const Event& event, double deliveryTime) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_stats.eventsDelivered++;
    
    // Update average delivery time (exponential moving average)
    const double alpha = 0.1;
    m_stats.averageDeliveryTime = alpha * deliveryTime + 
                                  (1.0 - alpha) * m_stats.averageDeliveryTime;
}

void EventDispatcher::cleanupInactiveListeners() {
    std::lock_guard<std::mutex> lock(m_listenersMutex);
    
    auto now = std::chrono::steady_clock::now();
    const auto inactiveThreshold = std::chrono::hours(1);  // Remove listeners inactive for 1 hour
    
    std::vector<ListenerHandle> toRemove;
    
    for (const auto& [handle, info] : m_listeners) {
        if (now - info.lastActivity > inactiveThreshold) {
            toRemove.push_back(handle);
        }
    }
    
    for (ListenerHandle handle : toRemove) {
        // Remove without lock (we already have it)
        auto it = m_listeners.find(handle);
        if (it != m_listeners.end()) {
            EventType type = it->second.type;
            
            if (type != static_cast<EventType>(-1)) {
                auto& typeListeners = m_typeListeners[type];
                typeListeners.erase(
                    std::remove(typeListeners.begin(), typeListeners.end(), handle),
                    typeListeners.end()
                );
                
                if (typeListeners.empty()) {
                    m_typeListeners.erase(type);
                }
            } else {
                m_globalListeners.erase(
                    std::remove(m_globalListeners.begin(), m_globalListeners.end(), handle),
                    m_globalListeners.end()
                );
            }
            
            m_listeners.erase(it);
        }
    }
    
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.activeListeners = m_listeners.size();
    }
}

// EventDataFactory implementation

namespace EventDataFactory {

std::shared_ptr<EventData> createAudioLevelData(float level) {
    auto data = std::make_shared<EventData>();
    data->setValue("level", level);
    return data;
}

std::shared_ptr<EventData> createDeviceChangedData(const std::string& deviceId, 
                                                   const std::string& deviceName) {
    auto data = std::make_shared<EventData>();
    data->setValue("device_id", deviceId);
    data->setValue("device_name", deviceName);
    return data;
}

std::shared_ptr<EventData> createErrorData(const std::string& message, int errorCode) {
    auto data = std::make_shared<EventData>();
    data->setValue("message", message);
    data->setValue("error_code", errorCode);
    return data;
}

std::shared_ptr<EventData> createProcessingStatsData(float cpuUsage, float latency, 
                                                     float reductionLevel) {
    auto data = std::make_shared<EventData>();
    data->setValue("cpu_usage", cpuUsage);
    data->setValue("latency", latency);
    data->setValue("reduction_level", reductionLevel);
    return data;
}

} // namespace EventDataFactory

} // namespace core
} // namespace quiet