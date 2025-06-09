#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>
#include "AudioBuffer.h"
#include "EventDispatcher.h"

namespace quiet {
namespace core {

/**
 * @brief Information about a virtual audio device
 */
struct VirtualDeviceInfo {
    std::string id;
    std::string name;
    std::string type;  // "VB-Cable", "BlackHole", etc.
    int maxChannels;
    std::vector<double> supportedSampleRates;
    bool isAvailable;
    bool isConnected;
};

/**
 * @brief Routes processed audio to virtual audio devices for application consumption
 * 
 * This class provides:
 * - Detection and enumeration of virtual audio devices (VB-Cable, BlackHole)
 * - Cross-platform virtual device handling
 * - Real-time audio routing with minimal latency
 * - Hot-plug detection and recovery
 * - Format conversion and resampling as needed
 * - Performance monitoring
 */
class VirtualDeviceRouter {
public:
    using DeviceChangeCallback = std::function<void(const VirtualDeviceInfo&)>;
    using ErrorCallback = std::function<void(const std::string&, int errorCode)>;
    
    VirtualDeviceRouter(EventDispatcher& eventDispatcher);
    ~VirtualDeviceRouter();
    
    // Lifecycle management
    bool initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Device enumeration and selection
    std::vector<VirtualDeviceInfo> getAvailableVirtualDevices() const;
    bool selectVirtualDevice(const std::string& deviceId);
    VirtualDeviceInfo getCurrentVirtualDevice() const;
    bool hasVirtualDevice() const;
    
    // Audio routing
    bool startRouting();
    void stopRouting();
    bool isRouting() const;
    bool routeAudioBuffer(const AudioBuffer& buffer);
    
    // Configuration
    bool setOutputConfiguration(double sampleRate, int bufferSize, int channels);
    double getOutputSampleRate() const;
    int getOutputBufferSize() const;
    int getOutputChannels() const;
    
    // Monitoring
    float getOutputLevel() const;
    uint64_t getBuffersRouted() const;
    double getAverageLatency() const;
    size_t getDroppedBuffers() const;
    
    // Callbacks
    void setDeviceChangeCallback(DeviceChangeCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    // Platform-specific helpers
    static bool isVirtualDeviceInstalled();
    static std::string getVirtualDeviceInstallInstructions();
    
private:
    // Platform-specific implementations
    class PlatformImpl;
    std::unique_ptr<PlatformImpl> m_platformImpl;
    
    // Device detection
    void scanForVirtualDevices();
    void startHotPlugDetection();
    void stopHotPlugDetection();
    void hotPlugDetectionThread();
    bool isVirtualDevice(const std::string& deviceName) const;
    
    // Audio processing
    bool openVirtualDevice(const std::string& deviceId);
    void closeVirtualDevice();
    bool writeToDevice(const AudioBuffer& buffer);
    void handleBufferConversion(const AudioBuffer& input, AudioBuffer& output);
    
    // Error handling
    void handleDeviceError(const std::string& message, int errorCode);
    bool attemptDeviceReconnection();
    
    // Member variables
    EventDispatcher& m_eventDispatcher;
    mutable std::mutex m_deviceMutex;
    
    // Current device state
    VirtualDeviceInfo m_currentDevice;
    std::unique_ptr<juce::AudioIODevice> m_outputDevice;
    std::atomic<bool> m_isRouting{false};
    std::atomic<bool> m_isInitialized{false};
    
    // Device configuration
    double m_outputSampleRate{48000.0};
    int m_outputBufferSize{256};
    int m_outputChannels{2};
    
    // Hot-plug detection
    std::unique_ptr<std::thread> m_hotPlugThread;
    std::atomic<bool> m_hotPlugRunning{false};
    std::chrono::seconds m_hotPlugInterval{2};
    
    // Callbacks
    DeviceChangeCallback m_deviceChangeCallback;
    ErrorCallback m_errorCallback;
    
    // Buffer conversion
    std::unique_ptr<juce::AudioSampleBuffer> m_conversionBuffer;
    std::unique_ptr<juce::AudioFormatManager> m_formatManager;
    
    // Statistics
    std::atomic<uint64_t> m_buffersRouted{0};
    std::atomic<size_t> m_droppedBuffers{0};
    std::atomic<float> m_outputLevel{0.0f};
    std::chrono::steady_clock::time_point m_lastBufferTime;
    
    // Latency tracking
    struct LatencyTracker {
        std::atomic<double> totalLatency{0.0};
        std::atomic<uint64_t> sampleCount{0};
        
        void addSample(double latency) {
            totalLatency += latency;
            sampleCount++;
        }
        
        double getAverage() const {
            auto count = sampleCount.load();
            return count > 0 ? totalLatency.load() / count : 0.0;
        }
        
        void reset() {
            totalLatency = 0.0;
            sampleCount = 0;
        }
    };
    
    LatencyTracker m_latencyTracker;
};

} // namespace core
} // namespace quiet