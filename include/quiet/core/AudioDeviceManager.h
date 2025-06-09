#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>
#include <vector>
#include <functional>
#include "AudioBuffer.h"
#include "EventDispatcher.h"

namespace quiet {
namespace core {

/**
 * @brief Information about an audio device
 */
struct AudioDeviceInfo {
    std::string id;
    std::string name;
    int maxInputChannels;
    int maxOutputChannels;
    std::vector<double> availableSampleRates;
    std::vector<int> availableBufferSizes;
    bool isDefault;
};

/**
 * @brief Manages audio input devices and handles audio callbacks
 * 
 * This class provides a high-level interface for:
 * - Enumerating available audio input devices
 * - Selecting and configuring audio devices
 * - Managing audio callbacks for real-time processing
 * - Handling device changes and errors
 */
class AudioDeviceManager : public juce::AudioIODeviceCallback {
public:
    using AudioCallback = std::function<void(const AudioBuffer&)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    AudioDeviceManager(EventDispatcher& eventDispatcher);
    ~AudioDeviceManager() override;

    // Device management
    bool initialize();
    void shutdown();
    
    std::vector<AudioDeviceInfo> getAvailableInputDevices() const;
    bool selectInputDevice(const std::string& deviceId);
    AudioDeviceInfo getCurrentInputDevice() const;
    
    // Audio configuration
    bool setAudioConfiguration(double sampleRate, int bufferSize);
    double getCurrentSampleRate() const;
    int getCurrentBufferSize() const;
    
    // Callbacks
    void setAudioCallback(AudioCallback callback);
    void setErrorCallback(ErrorCallback callback);
    
    // Control
    bool startAudio();
    void stopAudio();
    bool isAudioActive() const;
    
    // Audio levels
    float getInputLevel() const;
    bool isInputMuted() const;
    void setInputMuted(bool muted);

private:
    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

    // Internal methods
    void updateDeviceList();
    void handleDeviceChange();
    void processAudioBlock(const float* const* inputData, int numChannels, int numSamples);
    
    // Member variables
    EventDispatcher& m_eventDispatcher;
    std::unique_ptr<juce::AudioDeviceManager> m_juceDeviceManager;
    
    AudioCallback m_audioCallback;
    ErrorCallback m_errorCallback;
    
    mutable std::mutex m_mutex;
    std::vector<AudioDeviceInfo> m_availableDevices;
    AudioDeviceInfo m_currentDevice;
    
    // Audio processing
    std::unique_ptr<AudioBuffer> m_inputBuffer;
    std::atomic<float> m_inputLevel{0.0f};
    std::atomic<bool> m_inputMuted{false};
    
    // Configuration
    double m_currentSampleRate{48000.0};
    int m_currentBufferSize{256};
    bool m_isInitialized{false};
    bool m_isAudioActive{false};
    
    // Device change listener
    class DeviceChangeListener;
    std::unique_ptr<DeviceChangeListener> m_deviceChangeListener;
};

} // namespace core
} // namespace quiet