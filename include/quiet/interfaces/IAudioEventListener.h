/*
  ==============================================================================

    IAudioEventListener.h
    Created: 2025
    Author:  QUIET Application

    Interface for components that listen to audio-related events.
    Part of the event-driven architecture for decoupled communication.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
/**
    Audio event types that can be dispatched through the system
*/
enum class AudioEvent
{
    // Device events
    DeviceChanged,          /**< Audio input device was changed */
    DeviceConnected,        /**< New audio device connected to system */
    DeviceDisconnected,     /**< Audio device disconnected from system */
    DeviceListUpdated,      /**< List of available devices changed */
    
    // Processing events
    ProcessingToggled,      /**< Noise reduction enabled/disabled */
    ProcessingLevelChanged, /**< Noise reduction level adjusted */
    BufferProcessed,        /**< Audio buffer has been processed */
    LatencyChanged,         /**< Processing latency has changed */
    
    // Error events
    ErrorOccurred,          /**< An error occurred during processing */
    DeviceError,           /**< Audio device error */
    ProcessingError,       /**< Processing pipeline error */
    
    // Virtual device events
    VirtualDeviceReady,    /**< Virtual audio device is ready */
    VirtualDeviceError,    /**< Virtual device error occurred */
    
    // Configuration events
    ConfigurationChanged,   /**< Application configuration changed */
    BufferSizeChanged,     /**< Audio buffer size changed */
    SampleRateChanged      /**< Sample rate changed */
};

//==============================================================================
/**
    Buffer type enumeration for identifying audio buffers
*/
enum class BufferType
{
    Input,      /**< Raw input from microphone */
    Output,     /**< Processed output */
    Reference   /**< Reference signal for analysis */
};

//==============================================================================
/**
    Event data structure passed with audio events
*/
struct EventData
{
    // Common data
    bool enabled = false;                     /**< Generic enabled/disabled flag */
    float level = 0.0f;                      /**< Generic level value (0.0 - 1.0) */
    
    // Device data
    std::string deviceId;                    /**< Device identifier */
    std::string deviceName;                  /**< Human-readable device name */
    
    // Audio buffer data
    juce::AudioBuffer<float> buffer;         /**< Audio buffer for buffer events */
    BufferType bufferType = BufferType::Input; /**< Type of buffer */
    int sampleRate = 48000;                  /**< Sample rate of buffer */
    
    // Processing data
    float reductionLevel = 0.0f;             /**< Noise reduction in dB */
    float latencyMs = 0.0f;                  /**< Processing latency in milliseconds */
    
    // Error data
    std::string errorMessage;                /**< Error description */
    int errorCode = 0;                       /**< Error code for programmatic handling */
    
    // Configuration data
    std::string configKey;                   /**< Configuration key that changed */
    juce::var configValue;                   /**< New configuration value */
    
    /** Default constructor */
    EventData() = default;
    
    /** Convenience constructor for simple events */
    EventData(bool isEnabled) : enabled(isEnabled) {}
    
    /** Convenience constructor for level events */
    EventData(float levelValue) : level(levelValue) {}
    
    /** Convenience constructor for error events */
    EventData(const std::string& error, int code = -1) 
        : errorMessage(error), errorCode(code) {}
};

//==============================================================================
/**
    Interface for objects that want to receive audio events
*/
class IAudioEventListener
{
public:
    /** Virtual destructor */
    virtual ~IAudioEventListener() = default;
    
    /**
        Called when an audio event occurs.
        
        @param event    The type of event that occurred
        @param data     Additional data associated with the event
        
        Note: This method may be called from any thread, including the
        audio thread. Implementations should be thread-safe and avoid
        blocking operations when called from the audio thread.
    */
    virtual void onAudioEvent(AudioEvent event, const EventData& data) = 0;
    
    /**
        Optional method to filter which events this listener is interested in.
        
        @param event    The type of event to check
        @return         True if this listener wants to receive this event type
        
        Default implementation returns true for all events.
    */
    virtual bool interestedInEvent(AudioEvent event) const { return true; }
};

//==============================================================================
/**
    Weak reference wrapper for audio event listeners to prevent circular dependencies
*/
class WeakAudioEventListener
{
public:
    WeakAudioEventListener(IAudioEventListener* listener) : weakListener(listener) {}
    
    IAudioEventListener* get() const { return weakListener; }
    
    bool isValid() const { return weakListener != nullptr; }
    
    void reset() { weakListener = nullptr; }
    
private:
    IAudioEventListener* weakListener;
};

//==============================================================================
/**
    Helper class for creating event data
*/
class EventDataBuilder
{
public:
    EventDataBuilder() = default;
    
    EventDataBuilder& withEnabled(bool enabled)
    {
        data.enabled = enabled;
        return *this;
    }
    
    EventDataBuilder& withLevel(float level)
    {
        data.level = level;
        return *this;
    }
    
    EventDataBuilder& withDevice(const std::string& id, const std::string& name)
    {
        data.deviceId = id;
        data.deviceName = name;
        return *this;
    }
    
    EventDataBuilder& withBuffer(const juce::AudioBuffer<float>& buffer, BufferType type)
    {
        data.buffer = buffer;
        data.bufferType = type;
        return *this;
    }
    
    EventDataBuilder& withReductionLevel(float reductionDb)
    {
        data.reductionLevel = reductionDb;
        return *this;
    }
    
    EventDataBuilder& withLatency(float latencyMs)
    {
        data.latencyMs = latencyMs;
        return *this;
    }
    
    EventDataBuilder& withError(const std::string& message, int code = -1)
    {
        data.errorMessage = message;
        data.errorCode = code;
        return *this;
    }
    
    EventDataBuilder& withConfig(const std::string& key, const juce::var& value)
    {
        data.configKey = key;
        data.configValue = value;
        return *this;
    }
    
    EventDataBuilder& withSampleRate(int sampleRate)
    {
        data.sampleRate = sampleRate;
        return *this;
    }
    
    EventData build() const { return data; }
    
    operator EventData() const { return data; }
    
private:
    EventData data;
};