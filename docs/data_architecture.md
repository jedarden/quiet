# QUIET - Data Architecture Document

## 1. Overview

This document defines the data models, flow patterns, storage mechanisms, and synchronization strategies for QUIET's real-time audio processing system.

## 2. Core Data Models

### 2.1 Audio Data Structures

```cpp
// Core audio buffer structure - zero-copy, cache-aligned
struct alignas(64) AudioBuffer {
    static constexpr size_t MAX_CHANNELS = 32;
    static constexpr size_t MAX_SAMPLES = 8192;
    
    // Audio data - interleaved for cache efficiency
    float* data[MAX_CHANNELS];
    
    // Buffer metadata
    uint32_t channelCount;
    uint32_t sampleCount;
    double sampleRate;
    
    // Timing information
    int64_t timestamp;      // Sample clock timestamp
    int64_t hostTime;       // System time in microseconds
    
    // Audio statistics
    float peakLevel[MAX_CHANNELS];
    float rmsLevel[MAX_CHANNELS];
    uint32_t clippedSamples;
    
    // Memory management
    bool ownsMemory;
    size_t allocatedSize;
    
    // Methods
    void clear() noexcept;
    void copyFrom(const AudioBuffer& other) noexcept;
    float getMagnitude(uint32_t channel, uint32_t sample) const noexcept;
};
```

### 2.2 Device and Configuration Models

```cpp
// Audio device information model
struct AudioDevice {
    // Identification
    std::string deviceId;       // Unique platform ID
    std::string displayName;    // User-friendly name
    DeviceType type;           // Input/Output/Duplex
    
    // Capabilities
    struct Capabilities {
        std::vector<double> sampleRates;     // Supported rates
        std::vector<uint32_t> bufferSizes;   // Supported buffer sizes
        uint32_t minChannels;
        uint32_t maxChannels;
        std::vector<AudioFormat> formats;     // PCM16, PCM24, Float32, etc.
        bool exclusiveModeSupported;
        bool loopbackSupported;
    } capabilities;
    
    // Current configuration
    struct Configuration {
        double sampleRate = 48000.0;
        uint32_t bufferSize = 256;
        uint32_t channels = 1;
        AudioFormat format = AudioFormat::Float32;
        bool exclusiveMode = false;
    } config;
    
    // Runtime state
    struct State {
        bool isActive = false;
        bool isDefault = false;
        bool isAvailable = true;
        std::chrono::time_point<std::chrono::steady_clock> lastSeen;
        uint32_t dropoutCount = 0;
        float cpuUsage = 0.0f;
    } state;
};
```

### 2.3 Processing State Models

#### ProcessingMetrics
```cpp
struct ProcessingMetrics {
    // Performance
    float cpuUsage;
    float processingLatencyMs;
    float bufferUtilization;
    
    // Audio quality
    float inputLevel;
    float outputLevel;
    float noiseReductionDB;
    float signalToNoiseRatio;
    
    // Statistics
    uint64_t samplesProcessed;
    uint64_t bufferUnderruns;
    uint64_t bufferOverruns;
    Time sessionStartTime;
};
```

### 1.2 Configuration Data Model

```cpp
class ConfigurationData {
public:
    struct AudioSettings {
        String inputDeviceId;
        double sampleRate = 48000.0;
        int bufferSize = 256;
        int bitDepth = 24;
        
        struct Processing {
            bool noiseReductionEnabled = true;
            NoiseReductionLevel level = NoiseReductionLevel::Medium;
            bool adaptiveMode = false;
            float sensitivity = 0.5f;
        } processing;
        
        struct Advanced {
            LatencyMode latencyMode = LatencyMode::Low;
            bool exclusiveMode = false;
            bool dithering = true;
        } advanced;
    };
    
    struct UISettings {
        Rectangle<int> windowBounds{100, 100, 800, 600};
        bool alwaysOnTop = false;
        bool startMinimized = false;
        
        struct Theme {
            String name = "dark";
            float uiScale = 1.0f;
            bool animations = true;
        } theme;
        
        struct Visualization {
            bool waveformEnabled = true;
            bool spectrumEnabled = true;
            int refreshRateFPS = 30;
            WaveformStyle waveformStyle = WaveformStyle::Filled;
            SpectrumScale spectrumScale = SpectrumScale::Logarithmic;
        } visualization;
    };
    
    struct SystemSettings {
        bool startWithOS = false;
        bool showInSystemTray = true;
        bool checkForUpdates = true;
        
        struct Logging {
            bool enabled = true;
            LogLevel level = LogLevel::Info;
            String logPath;
            int maxFileSizeMB = 10;
            int maxFiles = 5;
        } logging;
        
        struct Hotkeys {
            KeyPress toggleProcessing{"F9"};
            KeyPress toggleWindow{"Ctrl+Shift+Q"};
            KeyPress nextPreset{"Ctrl+]"};
            KeyPress previousPreset{"Ctrl+["};
        } hotkeys;
    };
    
    // Versioning
    String version = "1.0.0";
    int schemaVersion = 1;
    
    // Main sections
    AudioSettings audio;
    UISettings ui;
    SystemSettings system;
    
    // User presets
    std::vector<Preset> userPresets;
};
```

### 1.3 Runtime Data Structures

#### AudioPipeline State
```cpp
class AudioPipelineState {
public:
    enum class State {
        Uninitialized,
        Initializing,
        Ready,
        Processing,
        Stopping,
        Error
    };
    
    struct BufferState {
        size_t inputBufferFillLevel;
        size_t outputBufferFillLevel;
        float inputLatencyMs;
        float outputLatencyMs;
        uint32_t droppedFrames;
    };
    
    struct ProcessorState {
        bool enabled;
        float reductionAmount;
        float cpuLoad;
        ProcessingQuality quality;
    };
    
private:
    std::atomic<State> currentState{State::Uninitialized};
    std::atomic<uint64_t> framesProcessed{0};
    
    // Per-component state
    BufferState bufferState;
    ProcessorState noiseReductionState;
    DeviceState inputDeviceState;
    DeviceState outputDeviceState;
    
    // Thread-safe accessors
    mutable std::shared_mutex stateMutex;
};
```

## 2. Data Flow Architecture

### 2.1 Audio Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                        Audio Data Pipeline                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐    │
│  │ Audio Input │───▶│ Ring Buffer  │───▶│ Frame Assembler │    │
│  │   (HAL)     │    │  (Lock-free) │    │                 │    │
│  └─────────────┘    └──────────────┘    └─────────────────┘    │
│                                                    │             │
│                                                    ▼             │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐    │
│  │   Format    │◀───│  Resampler   │◀───│ Noise Reduction │    │
│  │  Converter  │    │   (48kHz)    │    │   Processor     │    │
│  └─────────────┘    └──────────────┘    └─────────────────┘    │
│         │                                                        │
│         ▼                                                        │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐    │
│  │   Virtual   │───▶│ Ring Buffer  │───▶│ Visualization   │    │
│  │   Device    │    │  (Triple)    │    │   Data Queue    │    │
│  └─────────────┘    └──────────────┘    └─────────────────┘    │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Control Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                      Control Data Flow                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐    │
│  │     UI      │───▶│    Event     │───▶│   Command       │    │
│  │  Controls   │    │  Dispatcher  │    │   Processor     │    │
│  └─────────────┘    └──────────────┘    └─────────────────┘    │
│                              │                      │             │
│                              ▼                      ▼             │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐    │
│  │   Config    │◀───│    State     │◀───│  Audio Engine   │    │
│  │   Manager   │───▶│   Manager    │───▶│   Controller    │    │
│  └─────────────┘    └──────────────┘    └─────────────────┘    │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

## 3. Data Storage Architecture

### 3.1 Configuration Storage

```cpp
class ConfigurationStorage {
public:
    // Platform-specific paths
    static File getConfigDirectory() {
        #ifdef _WIN32
            return File::getSpecialLocation(File::userApplicationDataDirectory)
                       .getChildFile("QUIET");
        #elif __APPLE__
            return File::getSpecialLocation(File::userApplicationDataDirectory)
                       .getChildFile("Application Support/QUIET");
        #else
            return File::getSpecialLocation(File::userApplicationDataDirectory)
                       .getChildFile(".quiet");
        #endif
    }
    
    // File structure
    static constexpr const char* CONFIG_FILENAME = "config.json";
    static constexpr const char* PRESETS_FILENAME = "presets.json";
    static constexpr const char* DEVICE_CACHE_FILENAME = "devices.cache";
    
    // Storage operations
    Result<void> saveConfiguration(const ConfigurationData& config) {
        try {
            auto configFile = getConfigDirectory().getChildFile(CONFIG_FILENAME);
            
            // Serialize to JSON
            auto json = serializeConfig(config);
            
            // Write atomically
            TemporaryFile temp(configFile);
            if (!temp.getFile().replaceWithText(json)) {
                return Error("Failed to write configuration");
            }
            
            return temp.overwriteTargetFileWithTemporary();
        } catch (const std::exception& e) {
            return Error(e.what());
        }
    }
    
    Result<ConfigurationData> loadConfiguration() {
        auto configFile = getConfigDirectory().getChildFile(CONFIG_FILENAME);
        
        if (!configFile.existsAsFile()) {
            return ConfigurationData{}; // Return defaults
        }
        
        try {
            auto json = configFile.loadFileAsString();
            return deserializeConfig(json);
        } catch (const std::exception& e) {
            return Error(e.what());
        }
    }
    
private:
    static String serializeConfig(const ConfigurationData& config);
    static ConfigurationData deserializeConfig(const String& json);
};
```

### 3.2 Cache Management

```cpp
class CacheManager {
public:
    struct CacheEntry {
        String key;
        var data;
        Time created;
        Time lastAccessed;
        size_t sizeBytes;
        int accessCount;
    };
    
    // Cache policies
    enum class EvictionPolicy {
        LRU,    // Least Recently Used
        LFU,    // Least Frequently Used
        FIFO,   // First In First Out
        TTL     // Time To Live
    };
    
    CacheManager(size_t maxSizeBytes, EvictionPolicy policy)
        : maxSize(maxSizeBytes)
        , evictionPolicy(policy) {}
    
    // Cache operations
    void put(const String& key, const var& value) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        
        auto size = estimateSize(value);
        
        // Evict if necessary
        while (currentSize + size > maxSize && !entries.empty()) {
            evictOne();
        }
        
        // Add new entry
        entries[key] = CacheEntry{
            key, value, Time::getCurrentTime(),
            Time::getCurrentTime(), size, 1
        };
        
        currentSize += size;
        updateIndex(key);
    }
    
    std::optional<var> get(const String& key) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        
        auto it = entries.find(key);
        if (it == entries.end()) {
            return std::nullopt;
        }
        
        // Update access info
        it->second.lastAccessed = Time::getCurrentTime();
        it->second.accessCount++;
        updateIndex(key);
        
        return it->second.data;
    }
    
private:
    std::unordered_map<String, CacheEntry> entries;
    size_t maxSize;
    size_t currentSize = 0;
    EvictionPolicy evictionPolicy;
    std::mutex cacheMutex;
    
    void evictOne();
    void updateIndex(const String& key);
    size_t estimateSize(const var& value);
};
```

### 3.3 Logging Architecture

```cpp
class LogManager {
public:
    struct LogEntry {
        LogLevel level;
        String category;
        String message;
        Time timestamp;
        std::thread::id threadId;
        String filename;
        int lineNumber;
    };
    
    // Async logging with ring buffer
    class AsyncLogger {
    public:
        void log(LogLevel level, const String& category, 
                const String& message, const SourceLocation& location) {
            LogEntry entry{
                level, category, message,
                Time::getCurrentTime(),
                std::this_thread::get_id(),
                location.file_name(),
                location.line()
            };
            
            // Non-blocking write to ring buffer
            if (!ringBuffer.try_enqueue(std::move(entry))) {
                droppedLogs.fetch_add(1, std::memory_order_relaxed);
            }
        }
        
    private:
        moodycamel::ConcurrentQueue<LogEntry> ringBuffer{10000};
        std::atomic<uint64_t> droppedLogs{0};
        std::thread writerThread;
        
        void writerThreadFunc();
    };
    
    // Log rotation
    class LogRotator {
    public:
        LogRotator(const File& logDir, size_t maxFileSize, int maxFiles)
            : logDirectory(logDir)
            , maxSizeBytes(maxFileSize)
            , maxFileCount(maxFiles) {}
            
        File getCurrentLogFile() {
            if (!currentFile.existsAsFile() || 
                currentFile.getSize() >= maxSizeBytes) {
                rotateLog();
            }
            return currentFile;
        }
        
    private:
        File logDirectory;
        File currentFile;
        size_t maxSizeBytes;
        int maxFileCount;
        
        void rotateLog();
        void cleanupOldLogs();
    };
};
```

## 4. Data Synchronization

### 4.1 Thread-Safe Data Access

```cpp
template<typename T>
class ThreadSafeData {
public:
    // Read access
    template<typename Func>
    auto read(Func&& func) const {
        std::shared_lock lock(mutex);
        return func(data);
    }
    
    // Write access
    template<typename Func>
    auto write(Func&& func) {
        std::unique_lock lock(mutex);
        return func(data);
    }
    
    // Try write (non-blocking)
    template<typename Func>
    bool tryWrite(Func&& func) {
        std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) return false;
        func(data);
        return true;
    }
    
    // Copy snapshot
    T snapshot() const {
        std::shared_lock lock(mutex);
        return data;
    }
    
private:
    mutable std::shared_mutex mutex;
    T data;
};
```

### 4.2 Event-Driven Updates

```cpp
class DataUpdateManager {
public:
    // Registration
    using UpdateCallback = std::function<void(const String& key, const var& newValue)>;
    
    int subscribe(const String& dataKey, UpdateCallback callback) {
        std::lock_guard lock(subscribersMutex);
        int id = nextSubscriberId++;
        subscribers[dataKey].emplace_back(id, callback);
        return id;
    }
    
    void unsubscribe(const String& dataKey, int subscriberId) {
        std::lock_guard lock(subscribersMutex);
        auto& subs = subscribers[dataKey];
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [subscriberId](const auto& pair) { return pair.first == subscriberId; }),
            subs.end()
        );
    }
    
    // Update notification
    void notifyUpdate(const String& dataKey, const var& newValue) {
        std::shared_lock lock(subscribersMutex);
        
        auto it = subscribers.find(dataKey);
        if (it == subscribers.end()) return;
        
        // Copy callbacks to avoid holding lock during calls
        auto callbacks = it->second;
        lock.unlock();
        
        for (const auto& [id, callback] : callbacks) {
            callback(dataKey, newValue);
        }
    }
    
private:
    std::shared_mutex subscribersMutex;
    std::unordered_map<String, std::vector<std::pair<int, UpdateCallback>>> subscribers;
    std::atomic<int> nextSubscriberId{1};
};
```

## 5. Data Validation

### 5.1 Input Validation

```cpp
class DataValidator {
public:
    struct ValidationResult {
        bool isValid;
        std::vector<String> errors;
        std::vector<String> warnings;
    };
    
    // Audio configuration validation
    static ValidationResult validateAudioConfig(const AudioSettings& settings) {
        ValidationResult result{true, {}, {}};
        
        // Sample rate validation
        const std::array validSampleRates = {16000.0, 44100.0, 48000.0, 96000.0};
        if (std::find(validSampleRates.begin(), validSampleRates.end(), 
                     settings.sampleRate) == validSampleRates.end()) {
            result.errors.push_back("Invalid sample rate: " + String(settings.sampleRate));
            result.isValid = false;
        }
        
        // Buffer size validation
        if (settings.bufferSize < 64 || settings.bufferSize > 4096 ||
            (settings.bufferSize & (settings.bufferSize - 1)) != 0) {
            result.errors.push_back("Buffer size must be power of 2 between 64-4096");
            result.isValid = false;
        }
        
        // Bit depth validation
        const std::array validBitDepths = {16, 24, 32};
        if (std::find(validBitDepths.begin(), validBitDepths.end(),
                     settings.bitDepth) == validBitDepths.end()) {
            result.errors.push_back("Invalid bit depth: " + String(settings.bitDepth));
            result.isValid = false;
        }
        
        // Performance warnings
        if (settings.bufferSize < 128) {
            result.warnings.push_back("Low buffer size may cause dropouts");
        }
        
        if (settings.sampleRate > 48000.0 && settings.processing.noiseReductionEnabled) {
            result.warnings.push_back("High sample rate increases CPU usage");
        }
        
        return result;
    }
    
    // Device ID validation
    static bool isValidDeviceId(const String& deviceId) {
        // Platform-specific validation
        #ifdef _WIN32
            // Windows device IDs are GUIDs
            return deviceId.matches("{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}}");
        #elif __APPLE__
            // macOS device IDs are numeric
            return deviceId.containsOnly("0123456789");
        #else
            // Linux ALSA names
            return deviceId.matches("hw:[0-9]+,[0-9]+") || 
                   deviceId.matches("plughw:[0-9]+,[0-9]+");
        #endif
    }
};
```

## 6. Performance Monitoring Data

### 6.1 Metrics Collection

```cpp
class MetricsCollector {
public:
    struct AudioMetrics {
        // Timing
        double callbackDuration;
        double processingDuration;
        double waitingDuration;
        
        // Buffer health
        float inputBufferUtilization;
        float outputBufferUtilization;
        int underruns;
        int overruns;
        
        // Quality
        float inputPeakLevel;
        float outputPeakLevel;
        float noiseFloor;
        float reductionAmount;
    };
    
    void recordMetrics(const AudioMetrics& metrics) {
        // Update running statistics
        stats.update(metrics);
        
        // Store for history
        history.push_back({Time::getCurrentTime(), metrics});
        
        // Trim old data
        const auto cutoff = Time::getCurrentTime() - RelativeTime::hours(1);
        history.erase(
            std::remove_if(history.begin(), history.end(),
                [cutoff](const auto& entry) { return entry.first < cutoff; }),
            history.end()
        );
    }
    
    Statistics getStatistics() const {
        return stats.get();
    }
    
private:
    struct Statistics {
        RunningStats<double> callbackDuration;
        RunningStats<double> processingDuration;
        RunningStats<float> cpuUsage;
        std::atomic<uint64_t> totalUnderruns{0};
        std::atomic<uint64_t> totalOverruns{0};
    };
    
    ThreadSafeData<Statistics> stats;
    std::vector<std::pair<Time, AudioMetrics>> history;
};
```

## Conclusion

This data architecture provides:
- Efficient, thread-safe data structures for real-time audio
- Comprehensive configuration and state management
- Robust validation and error handling
- Performance monitoring and metrics collection
- Scalable caching and storage solutions

The architecture ensures data integrity while maintaining the low-latency requirements of real-time audio processing.