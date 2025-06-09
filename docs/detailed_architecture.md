# QUIET - Detailed System Architecture

## 1. Overview

This document provides the detailed technical architecture for QUIET, including component specifications, interface contracts, data models, and infrastructure design.

## 2. Component Architecture

### 2.1 System Component Diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           QUIET Application                             │
├──────────────────────────────────────────────────────────────────────────┤
│                         Application Core                                │
│  ┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐  │
│  │  QuietApplication  │  │  EventDispatcher   │  │ ConfigurationMgr   │  │
│  │   (Singleton)      │  │  (Thread-Safe)     │  │  (Persistent)      │  │
│  └────────────────────┘  └────────────────────┘  └────────────────────┘  │
├──────────────────────────────────────────────────────────────────────────┤
│                      Audio Processing Layer                             │
│  ┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐  │
│  │ AudioDeviceManager │  │NoiseReductionProc  │  │VirtualDeviceRouter │  │
│  │ • Device Enum      │  │ • RNNoise Core    │  │ • VB-Cable (Win)  │  │
│  │ • Hot-plug Detect  │  │ • 48kHz Resample  │  │ • BlackHole (Mac) │  │
│  │ • Buffer Manage    │  │ • Level Control   │  │ • Auto-reconnect  │  │
│  └────────────────────┘  └────────────────────┘  └────────────────────┘  │
│                                                                         │
│  ┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐  │
│  │  AudioBuffer       │  │  LockFreeQueue     │  │  AudioResampler    │  │
│  │ • Lock-free ops   │  │ • SPSC optimized  │  │ • High quality    │  │
│  │ • Zero-copy       │  │ • Cache aligned   │  │ • Low latency     │  │
│  └────────────────────┘  └────────────────────┘  └────────────────────┘  │
├──────────────────────────────────────────────────────────────────────────┤
│                     User Interface Layer                                │
│  ┌────────────────────┐  ┌────────────────────┐  ┌────────────────────┐  │
│  │    MainWindow      │  │ SystemTrayControl  │  │  Visualizations    │  │
│  │ • Device Select   │  │ • Quick Toggle    │  │ • WaveformDisplay │  │
│  │ • Enable Toggle   │  │ • Status Menu     │  │ • SpectrumAnalyze │  │
│  │ • Level Control   │  │ • Auto-minimize   │  │ • LevelMeters     │  │
│  └────────────────────┘  └────────────────────┘  └────────────────────┘  │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Core Component Specifications

#### AudioDeviceManager
```cpp
class AudioDeviceManager : public juce::AudioIODeviceCallback,
                          public juce::ChangeListener {
public:
    // Interface
    struct DeviceInfo {
        String id;
        String name;
        int numInputChannels;
        int numOutputChannels;
        Array<double> sampleRates;
        Array<int> bufferSizes;
        bool isDefault;
    };
    
    // Public API
    Array<DeviceInfo> getAvailableInputDevices() const;
    bool selectInputDevice(const String& deviceId);
    DeviceInfo getCurrentInputDevice() const;
    
    // Audio callback
    void audioDeviceIOCallback(const float** inputChannelData,
                              int numInputChannels,
                              float** outputChannelData,
                              int numOutputChannels,
                              int numSamples) override;
    
    // Configuration
    void setSampleRate(double sampleRate);
    void setBufferSize(int bufferSize);
    
    // Listeners
    void addDeviceChangeListener(DeviceChangeListener* listener);
    void removeDeviceChangeListener(DeviceChangeListener* listener);
    
private:
    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    std::unique_ptr<AudioProcessor> audioProcessor;
    CriticalSection deviceLock;
    
    // Hot-plug detection
    void changeListenerCallback(ChangeBroadcaster* source) override;
    void handleDeviceChange();
};
```

#### NoiseReductionProcessor
```cpp
class NoiseReductionProcessor : public IAudioProcessor {
public:
    enum class ReductionLevel {
        Low,    // Light noise reduction
        Medium, // Balanced reduction
        High    // Maximum reduction
    };
    
    // Configuration
    void setReductionLevel(ReductionLevel level);
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    // Processing
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(AudioBuffer<float>& buffer) override;
    void releaseResources() override;
    
    // Metrics
    float getCurrentReductionDB() const;
    float getProcessingLatencyMS() const;
    
private:
    // RNNoise integration
    struct RNNoiseState;
    std::unique_ptr<RNNoiseState> denoiserState;
    
    // Processing state
    std::atomic<bool> processingEnabled{true};
    std::atomic<ReductionLevel> reductionLevel{ReductionLevel::Medium};
    
    // Resampling for 48kHz requirement
    std::unique_ptr<AudioResampler> inputResampler;
    std::unique_ptr<AudioResampler> outputResampler;
    
    // Metrics
    std::atomic<float> currentReductionDB{0.0f};
    
    // Internal processing
    void processFrame(float* frame);
    void updateReductionMetrics(const float* input, const float* output, int numSamples);
};
```

#### VirtualDeviceRouter
```cpp
class VirtualDeviceRouter {
public:
    enum class Status {
        Disconnected,
        Connecting,
        Connected,
        Error
    };
    
    // Initialization
    bool initialize();
    void shutdown();
    
    // Status
    Status getStatus() const;
    String getVirtualDeviceName() const;
    bool isAvailable() const;
    
    // Audio routing
    void routeAudio(const AudioBuffer<float>& buffer);
    
    // Error handling
    String getLastError() const;
    void clearError();
    
private:
    // Platform-specific implementation
    std::unique_ptr<IPlatformVirtualDevice> platformDevice;
    
    // State
    std::atomic<Status> currentStatus{Status::Disconnected};
    String lastError;
    CriticalSection statusLock;
    
    // Platform detection and initialization
    std::unique_ptr<IPlatformVirtualDevice> createPlatformDevice();
    void handleConnectionLoss();
    void attemptReconnection();
};
```

#### EventDispatcher
```cpp
class EventDispatcher {
public:
    // Event types
    enum class EventType {
        // Audio events
        AudioDeviceChanged,
        AudioBufferProcessed,
        ProcessingToggled,
        
        // System events
        ConfigurationChanged,
        ErrorOccurred,
        
        // UI events
        VisualizationDataReady,
        UserActionRequired
    };
    
    struct Event {
        EventType type;
        var data;
        Time timestamp;
        String source;
    };
    
    // Thread-safe event posting
    void postEvent(EventType type, const var& data = var());
    void postEventAsync(EventType type, const var& data = var());
    
    // Listener management
    void addEventListener(EventType type, std::function<void(const Event&)> handler);
    void removeEventListener(EventType type, int handlerId);
    
    // Event processing
    void processEvents();
    
private:
    // Lock-free event queue
    moodycamel::ConcurrentQueue<Event> eventQueue;
    
    // Listeners
    std::unordered_map<EventType, std::vector<std::function<void(const Event&)>>> listeners;
    ReadWriteLock listenerLock;
    
    // Async handling
    std::unique_ptr<Thread> eventThread;
    void eventThreadRun();
};
```

### 1.3 Interface Contracts

#### IAudioProcessor
```cpp
class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    
    // Lifecycle
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    virtual void releaseResources() = 0;
    
    // Processing
    virtual void processBlock(AudioBuffer<float>& buffer) = 0;
    
    // Properties
    virtual int getLatencySamples() const = 0;
    virtual bool supportsDoublePrecision() const { return false; }
};
```

#### IAudioEventListener
```cpp
class IAudioEventListener {
public:
    virtual ~IAudioEventListener() = default;
    
    // Audio events
    virtual void onAudioDeviceChanged(const AudioDevice& newDevice) {}
    virtual void onAudioBufferReady(const AudioBuffer<float>& buffer, bool isInput) {}
    virtual void onProcessingStateChanged(bool enabled) {}
    virtual void onLatencyChanged(float latencyMs) {}
    
    // Error events
    virtual void onAudioDeviceError(const String& error) {}
    virtual void onProcessingError(const String& error) {}
};
```

## 2. Data Architecture

### 2.1 Audio Buffer Management

```cpp
// Lock-free ring buffer for audio data
template<typename T>
class AudioRingBuffer {
public:
    explicit AudioRingBuffer(size_t size)
        : buffer(size)
        , bufferSize(size) {
        static_assert(std::is_trivially_copyable_v<T>);
    }
    
    bool write(const T* data, size_t count) {
        const auto currentWrite = writeIndex.load(std::memory_order_relaxed);
        const auto currentRead = readIndex.load(std::memory_order_acquire);
        
        const size_t available = bufferSize - (currentWrite - currentRead);
        if (count > available) return false;
        
        const size_t writePos = currentWrite % bufferSize;
        const size_t firstPart = std::min(count, bufferSize - writePos);
        const size_t secondPart = count - firstPart;
        
        std::memcpy(&buffer[writePos], data, firstPart * sizeof(T));
        if (secondPart > 0) {
            std::memcpy(&buffer[0], data + firstPart, secondPart * sizeof(T));
        }
        
        writeIndex.store(currentWrite + count, std::memory_order_release);
        return true;
    }
    
    bool read(T* data, size_t count) {
        const auto currentWrite = writeIndex.load(std::memory_order_acquire);
        const auto currentRead = readIndex.load(std::memory_order_relaxed);
        
        const size_t available = currentWrite - currentRead;
        if (count > available) return false;
        
        const size_t readPos = currentRead % bufferSize;
        const size_t firstPart = std::min(count, bufferSize - readPos);
        const size_t secondPart = count - firstPart;
        
        std::memcpy(data, &buffer[readPos], firstPart * sizeof(T));
        if (secondPart > 0) {
            std::memcpy(data + firstPart, &buffer[0], secondPart * sizeof(T));
        }
        
        readIndex.store(currentRead + count, std::memory_order_release);
        return true;
    }
    
    size_t getAvailableRead() const {
        return writeIndex.load(std::memory_order_acquire) - 
               readIndex.load(std::memory_order_acquire);
    }
    
private:
    std::vector<T> buffer;
    const size_t bufferSize;
    alignas(64) std::atomic<size_t> writeIndex{0};
    alignas(64) std::atomic<size_t> readIndex{0};
};
```

### 2.2 Configuration Schema

```json
{
  "version": "1.0.0",
  "audio": {
    "input": {
      "deviceId": "string",
      "sampleRate": 48000,
      "bufferSize": 256,
      "bitDepth": 24
    },
    "processing": {
      "noiseReduction": {
        "enabled": true,
        "level": "medium",
        "adaptiveMode": false
      },
      "latencyMode": "low"
    }
  },
  "ui": {
    "window": {
      "x": 100,
      "y": 100,
      "width": 800,
      "height": 600,
      "alwaysOnTop": false
    },
    "theme": "dark",
    "visualization": {
      "waveformEnabled": true,
      "spectrumEnabled": true,
      "refreshRate": 30
    }
  },
  "system": {
    "startWithOS": false,
    "startMinimized": false,
    "showInSystemTray": true,
    "checkForUpdates": true,
    "logging": {
      "enabled": true,
      "level": "info",
      "maxFileSize": 10485760
    }
  }
}
```

### 2.3 Data Flow Patterns

```
Audio Input Flow:
┌─────────────┐    ┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│   Microphone│───▶│Audio Device │───▶│Ring Buffer   │───▶│Audio        │
│             │    │Manager      │    │(Lock-free)   │    │Processor    │
└─────────────┘    └─────────────┘    └──────────────┘    └─────────────┘
                                                                    │
                                                                    ▼
┌─────────────┐    ┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│Communication│◀───│Virtual      │◀───│Ring Buffer   │◀───│Noise        │
│Apps         │    │Device Router│    │(Lock-free)   │    │Reduction    │
└─────────────┘    └─────────────┘    └──────────────┘    └─────────────┘

Visualization Data Flow:
┌─────────────┐    ┌─────────────┐    ┌──────────────┐
│Audio Buffer │───▶│Triple Buffer│───▶│UI Thread     │
│(Audio Thread)    │Manager      │    │Processing    │
└─────────────┘    └─────────────┘    └──────────────┘
                                              │
                                              ▼
                                       ┌─────────────┐
                                       │Visualization│
                                       │Components   │
                                       └─────────────┘
```

## 3. Infrastructure Architecture

### 3.1 Build System Configuration

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(Quiet VERSION 1.0.0 LANGUAGES CXX C)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_TRACY "Enable Tracy Profiler" OFF)

# Dependencies
include(FetchContent)

# JUCE
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.0
)

# RNNoise
FetchContent_Declare(
    RNNoise
    GIT_REPOSITORY https://github.com/xiph/rnnoise.git
    GIT_TAG master
)

# Google Test
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
endif()

FetchContent_MakeAvailable(JUCE RNNoise)
if(BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()

# Platform-specific settings
if(WIN32)
    add_compile_definitions(NOMINMAX _WIN32_WINNT=0x0A00)
    set(PLATFORM_LIBS winmm wsock32 version)
elseif(APPLE)
    find_library(COREAUDIO CoreAudio REQUIRED)
    find_library(COREMIDI CoreMIDI REQUIRED)
    find_library(AUDIOTOOLBOX AudioToolbox REQUIRED)
    find_library(ACCELERATE Accelerate REQUIRED)
    set(PLATFORM_LIBS ${COREAUDIO} ${COREMIDI} ${AUDIOTOOLBOX} ${ACCELERATE})
endif()

# Main application
juce_add_gui_app(Quiet
    PRODUCT_NAME "QUIET"
    COMPANY_NAME "QUIET Audio"
    BUNDLE_ID "com.quietaudio.quiet"
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon.png"
)

# Sources
target_sources(Quiet PRIVATE
    src/main.cpp
    src/core/AudioDeviceManager.cpp
    src/core/NoiseReductionProcessor.cpp
    src/core/VirtualDeviceRouter.cpp
    src/core/AudioBuffer.cpp
    src/core/ConfigurationManager.cpp
    src/core/EventDispatcher.cpp
    src/ui/MainWindow.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SpectrumAnalyzer.cpp
    src/utils/Logger.cpp
)

# Platform-specific sources
if(WIN32)
    target_sources(Quiet PRIVATE
        src/platform/windows/WASAPIDevice.cpp
        src/platform/windows/VBCableIntegration.cpp
    )
elseif(APPLE)
    target_sources(Quiet PRIVATE
        src/platform/macos/CoreAudioDevice.cpp
        src/platform/macos/BlackHoleIntegration.cpp
    )
endif()

# Includes
target_include_directories(Quiet PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${rnnoise_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(Quiet PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    rnnoise
    ${PLATFORM_LIBS}
)

# Compile definitions
target_compile_definitions(Quiet PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS Quiet
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
    # Windows installer with WiX
    include(CPackWIX)
elseif(APPLE)
    # macOS DMG
    include(CPackDMG)
endif()
```

### 3.2 CI/CD Pipeline

```yaml
# .github/workflows/build.yml
name: Build and Test

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build-windows:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup MSVC
      uses: microsoft/setup-msbuild@v1
      
    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      
    - name: Build
      run: cmake --build build --config Release
      
    - name: Test
      run: ctest --test-dir build -C Release --output-on-failure
      
    - name: Create Installer
      run: |
        cd build
        cpack -C Release
        
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: windows-installer
        path: build/*.msi

  build-macos:
    runs-on: macos-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release
      
    - name: Build
      run: cmake --build build --config Release
      
    - name: Test
      run: ctest --test-dir build -C Release --output-on-failure
      
    - name: Code Sign
      env:
        MACOS_CERTIFICATE: ${{ secrets.MACOS_CERTIFICATE }}
        MACOS_CERTIFICATE_PWD: ${{ secrets.MACOS_CERTIFICATE_PWD }}
      run: |
        echo $MACOS_CERTIFICATE | base64 --decode > certificate.p12
        security create-keychain -p actions temp.keychain
        security import certificate.p12 -k temp.keychain -P $MACOS_CERTIFICATE_PWD -T /usr/bin/codesign
        security set-keychain-settings -t 3600 -u temp.keychain
        codesign --deep --force --verify --verbose --sign "Developer ID Application" build/Quiet.app
        
    - name: Create DMG
      run: |
        cd build
        cpack -C Release
        
    - name: Notarize
      env:
        APPLE_ID: ${{ secrets.APPLE_ID }}
        APPLE_PASSWORD: ${{ secrets.APPLE_PASSWORD }}
      run: |
        xcrun notarytool submit build/*.dmg --apple-id $APPLE_ID --password $APPLE_PASSWORD --wait
        
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: macos-dmg
        path: build/*.dmg
```

### 3.3 Deployment Architecture

```yaml
# deployment/docker-compose.yml
version: '3.8'

services:
  update-server:
    image: quiet-update-server:latest
    ports:
      - "443:443"
    environment:
      - DATABASE_URL=postgresql://quiet:password@db:5432/updates
      - S3_BUCKET=quiet-releases
      - CDN_URL=https://cdn.quietaudio.com
    volumes:
      - ./certs:/etc/ssl/certs
    depends_on:
      - db
      - redis

  db:
    image: postgres:15
    environment:
      - POSTGRES_DB=updates
      - POSTGRES_USER=quiet
      - POSTGRES_PASSWORD=password
    volumes:
      - postgres_data:/var/lib/postgresql/data

  redis:
    image: redis:7-alpine
    command: redis-server --requirepass password

  metrics:
    image: prom/prometheus:latest
    ports:
      - "9090:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
      - prometheus_data:/prometheus

volumes:
  postgres_data:
  prometheus_data:
```

### 3.4 Security Architecture

```cpp
// Security measures implementation
class SecurityManager {
public:
    // Configuration encryption
    static std::string encryptConfig(const std::string& plaintext) {
        // Use platform-specific secure storage
        #ifdef _WIN32
            return encryptWithDPAPI(plaintext);
        #elif __APPLE__
            return encryptWithKeychain(plaintext);
        #else
            return encryptWithLibSecret(plaintext);
        #endif
    }
    
    // Code signing verification
    static bool verifyCodeSignature() {
        #ifdef _WIN32
            return verifyAuthenticode();
        #elif __APPLE__
            return verifyAppleCodeSign();
        #else
            return true; // Linux doesn't require code signing
        #endif
    }
    
    // Anti-tampering
    static bool checkIntegrity() {
        const auto expectedHash = getEmbeddedHash();
        const auto actualHash = calculateSelfHash();
        return constantTimeCompare(expectedHash, actualHash);
    }
    
private:
    // Platform-specific implementations
    static std::string encryptWithDPAPI(const std::string& data);
    static std::string encryptWithKeychain(const std::string& data);
    static std::string encryptWithLibSecret(const std::string& data);
    
    static bool verifyAuthenticode();
    static bool verifyAppleCodeSign();
    
    static Hash getEmbeddedHash();
    static Hash calculateSelfHash();
    static bool constantTimeCompare(const Hash& a, const Hash& b);
};
```

## 4. Performance Architecture

### 4.1 Memory Pool Design

```cpp
template<typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initialSize = 16) {
        pool.reserve(initialSize);
        for (size_t i = 0; i < initialSize; ++i) {
            pool.emplace_back(std::make_unique<T>());
        }
    }
    
    std::unique_ptr<T, std::function<void(T*)>> acquire() {
        std::lock_guard<std::mutex> lock(mutex);
        
        if (pool.empty()) {
            // Grow pool
            return std::unique_ptr<T, std::function<void(T*)>>(
                new T(),
                [this](T* obj) { this->release(obj); }
            );
        }
        
        auto obj = std::move(pool.back());
        pool.pop_back();
        
        return std::unique_ptr<T, std::function<void(T*)>>(
            obj.release(),
            [this](T* obj) { this->release(obj); }
        );
    }
    
private:
    void release(T* obj) {
        std::lock_guard<std::mutex> lock(mutex);
        pool.emplace_back(obj);
    }
    
    std::vector<std::unique_ptr<T>> pool;
    std::mutex mutex;
};
```

### 4.2 SIMD Optimization Architecture

```cpp
class SIMDProcessor {
public:
    static void processAudioBuffer(float* buffer, size_t numSamples, float gain) {
        #if defined(__AVX__)
            processAVX(buffer, numSamples, gain);
        #elif defined(__SSE__)
            processSSE(buffer, numSamples, gain);
        #elif defined(__ARM_NEON)
            processNEON(buffer, numSamples, gain);
        #else
            processScalar(buffer, numSamples, gain);
        #endif
    }
    
private:
    static void processAVX(float* buffer, size_t numSamples, float gain);
    static void processSSE(float* buffer, size_t numSamples, float gain);
    static void processNEON(float* buffer, size_t numSamples, float gain);
    static void processScalar(float* buffer, size_t numSamples, float gain);
};
```

## Conclusion

This detailed architecture provides a robust foundation for the QUIET application, ensuring:
- High performance through lock-free designs and SIMD optimization
- Cross-platform compatibility with proper abstractions
- Scalability through modular component design
- Security through platform-specific best practices
- Maintainability through clear interfaces and separation of concerns