#include "quiet/core/AudioDeviceManager.h"
#include "quiet/core/EventDispatcher.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace quiet {
namespace core {

// Constants for audio processing
namespace {
    constexpr float MIN_LEVEL_DB = -60.0f;
    constexpr float MAX_LEVEL_DB = 0.0f;
    constexpr int MAX_CHANNELS = 2;
    constexpr int DEFAULT_SAMPLE_RATE = 48000;
    constexpr int DEFAULT_BUFFER_SIZE = 256;
    constexpr double LEVEL_SMOOTHING_FACTOR = 0.9;
}

// Device change listener implementation
class AudioDeviceManager::DeviceChangeListener : public juce::ChangeListener {
public:
    DeviceChangeListener(AudioDeviceManager* manager) : m_manager(manager) {}
    
    void changeListenerCallback(juce::ChangeBroadcaster* source) override {
        if (m_manager) {
            m_manager->handleDeviceChange();
        }
    }
    
private:
    AudioDeviceManager* m_manager;
};

AudioDeviceManager::AudioDeviceManager(EventDispatcher& eventDispatcher)
    : m_eventDispatcher(eventDispatcher)
    , m_juceDeviceManager(std::make_unique<juce::AudioDeviceManager>())
{
}

AudioDeviceManager::~AudioDeviceManager()
{
    shutdown();
}

bool AudioDeviceManager::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_isInitialized) {
        return true;
    }
    
    try {
        // Initialize the JUCE device manager with default audio device types
        auto result = m_juceDeviceManager->initialiseWithDefaultDevices(
            MAX_CHANNELS,  // max input channels
            0              // max output channels (we don't need output for noise cancellation)
        );
        
        if (result.isNotEmpty()) {
            m_eventDispatcher.publish(
                EventType::AudioDeviceError,
                EventDataFactory::createErrorData(result.toStdString())
            );
            return false;
        }
        
        // Set up device change listener
        m_deviceChangeListener = std::make_unique<DeviceChangeListener>(this);
        m_juceDeviceManager->addChangeListener(m_deviceChangeListener.get());
        
        // Update available devices list
        updateDeviceList();
        
        // Create input buffer with default configuration
        m_inputBuffer = std::make_unique<AudioBuffer>(
            MAX_CHANNELS, 
            DEFAULT_BUFFER_SIZE, 
            DEFAULT_SAMPLE_RATE
        );
        
        m_isInitialized = true;
        
        // Notify initialization complete
        m_eventDispatcher.publish(EventType::ApplicationStarted);
        
        return true;
    }
    catch (const std::exception& e) {
        m_eventDispatcher.publish(
            EventType::AudioDeviceError,
            EventDataFactory::createErrorData(
                std::string("Failed to initialize audio device manager: ") + e.what()
            )
        );
        return false;
    }
}

void AudioDeviceManager::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isInitialized) {
        return;
    }
    
    // Stop audio if running
    if (m_isAudioActive) {
        stopAudio();
    }
    
    // Remove change listener
    if (m_deviceChangeListener && m_juceDeviceManager) {
        m_juceDeviceManager->removeChangeListener(m_deviceChangeListener.get());
        m_deviceChangeListener.reset();
    }
    
    // Close audio device
    m_juceDeviceManager->closeAudioDevice();
    
    // Clear callbacks
    m_audioCallback = nullptr;
    m_errorCallback = nullptr;
    
    // Reset state
    m_isInitialized = false;
    m_availableDevices.clear();
    m_currentDevice = AudioDeviceInfo{};
    
    // Notify shutdown
    m_eventDispatcher.publish(EventType::ApplicationShutdown);
}

std::vector<AudioDeviceInfo> AudioDeviceManager::getAvailableInputDevices() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_availableDevices;
}

bool AudioDeviceManager::selectInputDevice(const std::string& deviceId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isInitialized) {
        return false;
    }
    
    // Find the device in our list
    auto it = std::find_if(m_availableDevices.begin(), m_availableDevices.end(),
        [&deviceId](const AudioDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it == m_availableDevices.end()) {
        m_eventDispatcher.publish(
            EventType::AudioDeviceError,
            EventDataFactory::createErrorData("Device not found: " + deviceId)
        );
        return false;
    }
    
    // Get the current audio device type
    auto* currentDevice = m_juceDeviceManager->getCurrentAudioDevice();
    auto* deviceType = currentDevice ? currentDevice->getTypeName() : 
                       m_juceDeviceManager->getAvailableDeviceTypes()[0]->getTypeName();
    
    // Set up audio device setup
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    m_juceDeviceManager->getAudioDeviceSetup(setup);
    
    setup.inputDeviceName = it->name;
    setup.outputDeviceName = "";  // No output device needed
    setup.sampleRate = m_currentSampleRate;
    setup.bufferSize = m_currentBufferSize;
    setup.inputChannels.setRange(0, std::min(MAX_CHANNELS, it->maxInputChannels), true);
    setup.outputChannels.clear();
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = true;
    
    // Apply the setup
    auto result = m_juceDeviceManager->setAudioDeviceSetup(setup, false);
    
    if (result.isNotEmpty()) {
        m_eventDispatcher.publish(
            EventType::AudioDeviceError,
            EventDataFactory::createErrorData(
                "Failed to select device: " + result.toStdString()
            )
        );
        return false;
    }
    
    // Update current device info
    m_currentDevice = *it;
    
    // Publish device change event
    m_eventDispatcher.publish(
        EventType::AudioDeviceChanged,
        EventDataFactory::createDeviceChangedData(deviceId, it->name)
    );
    
    return true;
}

AudioDeviceInfo AudioDeviceManager::getCurrentInputDevice() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentDevice;
}

bool AudioDeviceManager::setAudioConfiguration(double sampleRate, int bufferSize)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isInitialized) {
        return false;
    }
    
    // Validate parameters
    if (sampleRate < 8000 || sampleRate > 192000) {
        m_eventDispatcher.publish(
            EventType::AudioDeviceError,
            EventDataFactory::createErrorData("Invalid sample rate: " + std::to_string(sampleRate))
        );
        return false;
    }
    
    if (bufferSize < 32 || bufferSize > 8192 || (bufferSize & (bufferSize - 1)) != 0) {
        m_eventDispatcher.publish(
            EventType::AudioDeviceError,
            EventDataFactory::createErrorData("Invalid buffer size: " + std::to_string(bufferSize))
        );
        return false;
    }
    
    // Stop audio before reconfiguring
    bool wasActive = m_isAudioActive;
    if (wasActive) {
        stopAudio();
    }
    
    // Update configuration
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    m_juceDeviceManager->getAudioDeviceSetup(setup);
    
    setup.sampleRate = sampleRate;
    setup.bufferSize = bufferSize;
    
    auto result = m_juceDeviceManager->setAudioDeviceSetup(setup, false);
    
    if (result.isNotEmpty()) {
        m_eventDispatcher.publish(
            EventType::AudioDeviceError,
            EventDataFactory::createErrorData(
                "Failed to set audio configuration: " + result.toStdString()
            )
        );
        return false;
    }
    
    // Update internal state
    m_currentSampleRate = sampleRate;
    m_currentBufferSize = bufferSize;
    
    // Recreate input buffer with new configuration
    m_inputBuffer = std::make_unique<AudioBuffer>(
        MAX_CHANNELS, 
        bufferSize, 
        sampleRate
    );
    
    // Restart audio if it was active
    if (wasActive) {
        startAudio();
    }
    
    return true;
}

double AudioDeviceManager::getCurrentSampleRate() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentSampleRate;
}

int AudioDeviceManager::getCurrentBufferSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentBufferSize;
}

void AudioDeviceManager::setAudioCallback(AudioCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_audioCallback = callback;
}

void AudioDeviceManager::setErrorCallback(ErrorCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = callback;
}

bool AudioDeviceManager::startAudio()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isInitialized) {
        return false;
    }
    
    if (m_isAudioActive) {
        return true;
    }
    
    // Set this as the audio callback
    m_juceDeviceManager->addAudioCallback(this);
    
    m_isAudioActive = true;
    
    // Notify audio started
    m_eventDispatcher.publish(EventType::AudioProcessingStarted);
    
    return true;
}

void AudioDeviceManager::stopAudio()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_isAudioActive) {
        return;
    }
    
    // Remove audio callback
    m_juceDeviceManager->removeAudioCallback(this);
    
    m_isAudioActive = false;
    
    // Reset audio levels
    m_inputLevel = 0.0f;
    
    // Notify audio stopped
    m_eventDispatcher.publish(EventType::AudioProcessingStopped);
}

bool AudioDeviceManager::isAudioActive() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isAudioActive;
}

float AudioDeviceManager::getInputLevel() const
{
    return m_inputLevel.load(std::memory_order_relaxed);
}

bool AudioDeviceManager::isInputMuted() const
{
    return m_inputMuted.load(std::memory_order_relaxed);
}

void AudioDeviceManager::setInputMuted(bool muted)
{
    m_inputMuted.store(muted, std::memory_order_relaxed);
}

// JUCE AudioIODeviceCallback implementation
void AudioDeviceManager::audioDeviceIOCallbackWithContext(
    const float* const* inputChannelData,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& context)
{
    // Ignore output (we don't produce any)
    if (outputChannelData != nullptr) {
        for (int ch = 0; ch < numOutputChannels; ++ch) {
            if (outputChannelData[ch] != nullptr) {
                std::memset(outputChannelData[ch], 0, sizeof(float) * numSamples);
            }
        }
    }
    
    // Process input if available
    if (inputChannelData != nullptr && numInputChannels > 0 && numSamples > 0) {
        processAudioBlock(inputChannelData, numInputChannels, numSamples);
    }
}

void AudioDeviceManager::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr) {
        // Update sample rate and buffer size from the device
        m_currentSampleRate = device->getCurrentSampleRate();
        m_currentBufferSize = device->getCurrentBufferSizeSamples();
        
        // Recreate input buffer with actual device configuration
        m_inputBuffer = std::make_unique<AudioBuffer>(
            MAX_CHANNELS,
            m_currentBufferSize,
            m_currentSampleRate
        );
    }
}

void AudioDeviceManager::audioDeviceStopped()
{
    // Reset level when device stops
    m_inputLevel.store(0.0f, std::memory_order_relaxed);
}

void AudioDeviceManager::audioDeviceError(const juce::String& errorMessage)
{
    // Publish error event
    m_eventDispatcher.publish(
        EventType::AudioDeviceError,
        EventDataFactory::createErrorData(errorMessage.toStdString())
    );
    
    // Call error callback if set
    if (m_errorCallback) {
        m_errorCallback(errorMessage.toStdString());
    }
}

// Internal methods
void AudioDeviceManager::updateDeviceList()
{
    m_availableDevices.clear();
    
    // Get all available device types
    auto deviceTypes = m_juceDeviceManager->getAvailableDeviceTypes();
    
    for (auto* deviceType : deviceTypes) {
        // Get input devices for this type
        auto deviceNames = deviceType->getDeviceNames(true);  // true = input devices
        
        for (const auto& deviceName : deviceNames) {
            AudioDeviceInfo info;
            info.name = deviceName.toStdString();
            info.id = deviceType->getTypeName().toStdString() + ":" + deviceName.toStdString();
            
            // Create a temporary device to query capabilities
            std::unique_ptr<juce::AudioIODevice> tempDevice(
                deviceType->createDevice("", deviceName)
            );
            
            if (tempDevice) {
                // Get channel counts
                auto inputChannels = tempDevice->getInputChannelNames();
                info.maxInputChannels = inputChannels.size();
                info.maxOutputChannels = 0;  // We don't use output
                
                // Get available sample rates
                auto sampleRates = tempDevice->getAvailableSampleRates();
                info.availableSampleRates.reserve(sampleRates.size());
                for (auto rate : sampleRates) {
                    info.availableSampleRates.push_back(rate);
                }
                
                // Get available buffer sizes
                auto bufferSizes = tempDevice->getAvailableBufferSizes();
                info.availableBufferSizes.reserve(bufferSizes.size());
                for (auto size : bufferSizes) {
                    info.availableBufferSizes.push_back(size);
                }
                
                // Check if it's the default device
                info.isDefault = (deviceName == deviceType->getDefaultDeviceName(true));
                
                m_availableDevices.push_back(info);
            }
        }
    }
    
    // Update current device info if a device is selected
    auto* currentDevice = m_juceDeviceManager->getCurrentAudioDevice();
    if (currentDevice) {
        auto currentName = currentDevice->getName().toStdString();
        auto it = std::find_if(m_availableDevices.begin(), m_availableDevices.end(),
            [&currentName](const AudioDeviceInfo& info) {
                return info.name == currentName;
            });
        
        if (it != m_availableDevices.end()) {
            m_currentDevice = *it;
        }
    }
}

void AudioDeviceManager::handleDeviceChange()
{
    // Update device list
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        updateDeviceList();
    }
    
    // Notify about device change
    m_eventDispatcher.publish(EventType::AudioDeviceChanged);
}

void AudioDeviceManager::processAudioBlock(const float* const* inputData, 
                                          int numChannels, int numSamples)
{
    // Check if input is muted
    if (m_inputMuted.load(std::memory_order_relaxed)) {
        m_inputLevel.store(0.0f, std::memory_order_relaxed);
        return;
    }
    
    // Ensure we have a valid buffer
    if (!m_inputBuffer || m_inputBuffer->getNumSamples() < numSamples) {
        m_inputBuffer = std::make_unique<AudioBuffer>(
            std::min(numChannels, MAX_CHANNELS),
            numSamples,
            m_currentSampleRate
        );
    }
    
    // Copy input data to our buffer
    int channelsToProcess = std::min(numChannels, MAX_CHANNELS);
    m_inputBuffer->setSize(channelsToProcess, numSamples, false);
    
    for (int ch = 0; ch < channelsToProcess; ++ch) {
        if (inputData[ch] != nullptr) {
            m_inputBuffer->copyFrom(ch, 0, inputData[ch], numSamples);
        }
    }
    
    // Calculate input level (RMS across all channels)
    float rmsLevel = 0.0f;
    for (int ch = 0; ch < channelsToProcess; ++ch) {
        float channelRms = m_inputBuffer->getRMSLevel(ch, 0, numSamples);
        rmsLevel += channelRms * channelRms;
    }
    rmsLevel = std::sqrt(rmsLevel / channelsToProcess);
    
    // Convert to dB and smooth
    float levelDb = 20.0f * std::log10(std::max(rmsLevel, 1e-6f));
    levelDb = std::max(levelDb, MIN_LEVEL_DB);
    levelDb = std::min(levelDb, MAX_LEVEL_DB);
    
    // Normalize to 0-1 range
    float normalizedLevel = (levelDb - MIN_LEVEL_DB) / (MAX_LEVEL_DB - MIN_LEVEL_DB);
    
    // Apply smoothing
    float currentLevel = m_inputLevel.load(std::memory_order_relaxed);
    float smoothedLevel = currentLevel * LEVEL_SMOOTHING_FACTOR + 
                         normalizedLevel * (1.0f - LEVEL_SMOOTHING_FACTOR);
    
    m_inputLevel.store(smoothedLevel, std::memory_order_relaxed);
    
    // Publish level update (throttled to avoid flooding)
    static std::chrono::steady_clock::time_point lastLevelUpdate;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLevelUpdate).count() > 50) {
        m_eventDispatcher.publish(
            EventType::AudioLevelChanged,
            EventDataFactory::createAudioLevelData(smoothedLevel)
        );
        lastLevelUpdate = now;
    }
    
    // Call audio callback if set
    if (m_audioCallback) {
        m_audioCallback(*m_inputBuffer);
    }
}

} // namespace core
} // namespace quiet