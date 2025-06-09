#include "quiet/core/VirtualDeviceRouter.h"
#include "quiet/core/EventDispatcher.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
    #include <mmdeviceapi.h>
    #include <audioclient.h>
    #include <functiondiscoverykeys_devpkey.h>
#elif __APPLE__
    #include <CoreAudio/CoreAudio.h>
    #include <AudioToolbox/AudioToolbox.h>
#endif

namespace quiet {
namespace core {

// Platform-specific implementation base class
class VirtualDeviceRouter::PlatformImpl {
public:
    virtual ~PlatformImpl() = default;
    
    virtual std::vector<VirtualDeviceInfo> scanDevices() = 0;
    virtual bool openDevice(const std::string& deviceId) = 0;
    virtual void closeDevice() = 0;
    virtual bool writeAudio(const float* data, int numSamples, int numChannels) = 0;
    virtual bool isDeviceConnected() const = 0;
    virtual std::string getLastError() const = 0;
};

#ifdef _WIN32
// Windows implementation for VB-Cable
class WindowsVirtualDeviceImpl : public VirtualDeviceRouter::PlatformImpl {
private:
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioRenderClient* m_renderClient = nullptr;
    WAVEFORMATEX* m_waveFormat = nullptr;
    std::string m_lastError;
    
public:
    ~WindowsVirtualDeviceImpl() override {
        closeDevice();
    }
    
    std::vector<VirtualDeviceInfo> scanDevices() override {
        std::vector<VirtualDeviceInfo> devices;
        
        IMMDeviceEnumerator* enumerator = nullptr;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&enumerator
        );
        
        if (FAILED(hr)) {
            m_lastError = "Failed to create device enumerator";
            return devices;
        }
        
        IMMDeviceCollection* collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection);
        
        if (SUCCEEDED(hr)) {
            UINT count;
            collection->GetCount(&count);
            
            for (UINT i = 0; i < count; i++) {
                IMMDevice* device = nullptr;
                if (SUCCEEDED(collection->Item(i, &device))) {
                    LPWSTR deviceId = nullptr;
                    device->GetId(&deviceId);
                    
                    IPropertyStore* props = nullptr;
                    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                        PROPVARIANT varName;
                        PropVariantInit(&varName);
                        
                        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                            std::wstring wname(varName.pwszVal);
                            std::string name(wname.begin(), wname.end());
                            
                            // Check if this is a VB-Cable device
                            if (name.find("VB-Audio") != std::string::npos || 
                                name.find("CABLE Input") != std::string::npos ||
                                name.find("VB-Cable") != std::string::npos) {
                                
                                VirtualDeviceInfo info;
                                info.id = std::string(deviceId, deviceId + wcslen(deviceId));
                                info.name = name;
                                info.type = "VB-Cable";
                                info.maxChannels = 2;
                                info.supportedSampleRates = {44100.0, 48000.0, 96000.0};
                                info.isAvailable = true;
                                info.isConnected = false;
                                
                                devices.push_back(info);
                            }
                        }
                        
                        PropVariantClear(&varName);
                        props->Release();
                    }
                    
                    if (deviceId) CoTaskMemFree(deviceId);
                    device->Release();
                }
            }
            
            collection->Release();
        }
        
        enumerator->Release();
        return devices;
    }
    
    bool openDevice(const std::string& deviceId) override {
        closeDevice();
        
        IMMDeviceEnumerator* enumerator = nullptr;
        HRESULT hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&enumerator
        );
        
        if (FAILED(hr)) {
            m_lastError = "Failed to create device enumerator";
            return false;
        }
        
        std::wstring wideDeviceId(deviceId.begin(), deviceId.end());
        hr = enumerator->GetDevice(wideDeviceId.c_str(), &m_device);
        enumerator->Release();
        
        if (FAILED(hr)) {
            m_lastError = "Failed to get device";
            return false;
        }
        
        hr = m_device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            (void**)&m_audioClient
        );
        
        if (FAILED(hr)) {
            m_lastError = "Failed to activate audio client";
            closeDevice();
            return false;
        }
        
        hr = m_audioClient->GetMixFormat(&m_waveFormat);
        if (FAILED(hr)) {
            m_lastError = "Failed to get mix format";
            closeDevice();
            return false;
        }
        
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            10000000,  // 1 second buffer
            0,
            m_waveFormat,
            nullptr
        );
        
        if (FAILED(hr)) {
            m_lastError = "Failed to initialize audio client";
            closeDevice();
            return false;
        }
        
        hr = m_audioClient->GetService(
            __uuidof(IAudioRenderClient),
            (void**)&m_renderClient
        );
        
        if (FAILED(hr)) {
            m_lastError = "Failed to get render client";
            closeDevice();
            return false;
        }
        
        hr = m_audioClient->Start();
        if (FAILED(hr)) {
            m_lastError = "Failed to start audio client";
            closeDevice();
            return false;
        }
        
        return true;
    }
    
    void closeDevice() override {
        if (m_audioClient) {
            m_audioClient->Stop();
        }
        
        if (m_renderClient) {
            m_renderClient->Release();
            m_renderClient = nullptr;
        }
        
        if (m_audioClient) {
            m_audioClient->Release();
            m_audioClient = nullptr;
        }
        
        if (m_waveFormat) {
            CoTaskMemFree(m_waveFormat);
            m_waveFormat = nullptr;
        }
        
        if (m_device) {
            m_device->Release();
            m_device = nullptr;
        }
    }
    
    bool writeAudio(const float* data, int numSamples, int numChannels) override {
        if (!m_renderClient || !m_audioClient) {
            return false;
        }
        
        UINT32 bufferFrameCount;
        HRESULT hr = m_audioClient->GetBufferSize(&bufferFrameCount);
        if (FAILED(hr)) {
            return false;
        }
        
        UINT32 numFramesPadding;
        hr = m_audioClient->GetCurrentPadding(&numFramesPadding);
        if (FAILED(hr)) {
            return false;
        }
        
        UINT32 numFramesAvailable = bufferFrameCount - numFramesPadding;
        if (numFramesAvailable < numSamples) {
            // Not enough space, would block
            return false;
        }
        
        BYTE* buffer;
        hr = m_renderClient->GetBuffer(numSamples, &buffer);
        if (FAILED(hr)) {
            return false;
        }
        
        // Convert float to format expected by device
        if (m_waveFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            memcpy(buffer, data, numSamples * numChannels * sizeof(float));
        } else if (m_waveFormat->wBitsPerSample == 16) {
            // Convert to 16-bit PCM
            int16_t* dest = reinterpret_cast<int16_t*>(buffer);
            for (int i = 0; i < numSamples * numChannels; i++) {
                float sample = data[i];
                sample = std::max(-1.0f, std::min(1.0f, sample));
                dest[i] = static_cast<int16_t>(sample * 32767.0f);
            }
        }
        
        hr = m_renderClient->ReleaseBuffer(numSamples, 0);
        return SUCCEEDED(hr);
    }
    
    bool isDeviceConnected() const override {
        return m_device != nullptr && m_audioClient != nullptr;
    }
    
    std::string getLastError() const override {
        return m_lastError;
    }
};

#elif __APPLE__
// macOS implementation for BlackHole
class MacOSVirtualDeviceImpl : public VirtualDeviceRouter::PlatformImpl {
private:
    AudioDeviceID m_deviceId = 0;
    AudioUnit m_outputUnit = nullptr;
    std::string m_lastError;
    
    struct RenderCallbackData {
        const float* buffer;
        int numSamples;
        int numChannels;
        std::atomic<bool> dataReady{false};
    };
    
    RenderCallbackData m_renderData;
    
    static OSStatus renderCallback(void* inRefCon,
                                 AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList* ioData) {
        auto* impl = static_cast<MacOSVirtualDeviceImpl*>(inRefCon);
        
        if (!impl->m_renderData.dataReady.load()) {
            // No data available, fill with silence
            for (UInt32 i = 0; i < ioData->mNumberBuffers; i++) {
                memset(ioData->mBuffers[i].mData, 0, 
                       ioData->mBuffers[i].mDataByteSize);
            }
            return noErr;
        }
        
        // Copy data from our buffer
        const float* sourceData = impl->m_renderData.buffer;
        int sourceSamples = impl->m_renderData.numSamples;
        int sourceChannels = impl->m_renderData.numChannels;
        
        UInt32 framesToCopy = std::min(inNumberFrames, 
                                      static_cast<UInt32>(sourceSamples));
        
        for (UInt32 bufferIndex = 0; bufferIndex < ioData->mNumberBuffers; 
             bufferIndex++) {
            float* outputBuffer = static_cast<float*>(
                ioData->mBuffers[bufferIndex].mData);
            UInt32 channelCount = ioData->mBuffers[bufferIndex].mNumberChannels;
            
            for (UInt32 frame = 0; frame < framesToCopy; frame++) {
                for (UInt32 channel = 0; channel < channelCount; channel++) {
                    int sourceChannel = std::min(static_cast<int>(channel), 
                                               sourceChannels - 1);
                    outputBuffer[frame * channelCount + channel] = 
                        sourceData[frame * sourceChannels + sourceChannel];
                }
            }
            
            // Fill remaining frames with silence
            if (framesToCopy < inNumberFrames) {
                size_t remainingBytes = (inNumberFrames - framesToCopy) * 
                                      channelCount * sizeof(float);
                memset(outputBuffer + framesToCopy * channelCount, 0, 
                       remainingBytes);
            }
        }
        
        impl->m_renderData.dataReady = false;
        return noErr;
    }
    
public:
    ~MacOSVirtualDeviceImpl() override {
        closeDevice();
    }
    
    std::vector<VirtualDeviceInfo> scanDevices() override {
        std::vector<VirtualDeviceInfo> devices;
        
        AudioObjectPropertyAddress propertyAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };
        
        UInt32 dataSize = 0;
        OSStatus status = AudioObjectGetPropertyDataSize(
            kAudioObjectSystemObject,
            &propertyAddress,
            0, nullptr,
            &dataSize
        );
        
        if (status != noErr) {
            m_lastError = "Failed to get device list size";
            return devices;
        }
        
        int deviceCount = dataSize / sizeof(AudioDeviceID);
        std::vector<AudioDeviceID> deviceIds(deviceCount);
        
        status = AudioObjectGetPropertyData(
            kAudioObjectSystemObject,
            &propertyAddress,
            0, nullptr,
            &dataSize,
            deviceIds.data()
        );
        
        if (status != noErr) {
            m_lastError = "Failed to get device list";
            return devices;
        }
        
        for (AudioDeviceID deviceId : deviceIds) {
            // Get device name
            CFStringRef deviceName = nullptr;
            dataSize = sizeof(deviceName);
            propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
            
            status = AudioObjectGetPropertyData(
                deviceId,
                &propertyAddress,
                0, nullptr,
                &dataSize,
                &deviceName
            );
            
            if (status == noErr && deviceName != nullptr) {
                char name[256];
                CFStringGetCString(deviceName, name, sizeof(name), 
                                 kCFStringEncodingUTF8);
                CFRelease(deviceName);
                
                std::string deviceNameStr(name);
                
                // Check if this is BlackHole
                if (deviceNameStr.find("BlackHole") != std::string::npos) {
                    VirtualDeviceInfo info;
                    info.id = std::to_string(deviceId);
                    info.name = deviceNameStr;
                    info.type = "BlackHole";
                    
                    // Get channel count
                    propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
                    propertyAddress.mScope = kAudioDevicePropertyScopeOutput;
                    
                    AudioObjectGetPropertyDataSize(deviceId, &propertyAddress, 
                                                 0, nullptr, &dataSize);
                    
                    std::vector<uint8_t> bufferListData(dataSize);
                    auto* bufferList = reinterpret_cast<AudioBufferList*>(
                        bufferListData.data());
                    
                    if (AudioObjectGetPropertyData(deviceId, &propertyAddress,
                                                 0, nullptr, &dataSize, 
                                                 bufferList) == noErr) {
                        int channels = 0;
                        for (UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
                            channels += bufferList->mBuffers[i].mNumberChannels;
                        }
                        info.maxChannels = channels;
                    } else {
                        info.maxChannels = 2; // Default
                    }
                    
                    info.supportedSampleRates = {44100.0, 48000.0, 96000.0};
                    info.isAvailable = true;
                    info.isConnected = false;
                    
                    devices.push_back(info);
                }
            }
        }
        
        return devices;
    }
    
    bool openDevice(const std::string& deviceId) override {
        closeDevice();
        
        m_deviceId = std::stoi(deviceId);
        
        // Create output audio unit
        AudioComponentDescription desc;
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        desc.componentFlags = 0;
        desc.componentFlagsMask = 0;
        
        AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
        if (!comp) {
            m_lastError = "Failed to find audio component";
            return false;
        }
        
        OSStatus status = AudioComponentInstanceNew(comp, &m_outputUnit);
        if (status != noErr) {
            m_lastError = "Failed to create audio unit";
            return false;
        }
        
        // Set the output device
        status = AudioUnitSetProperty(
            m_outputUnit,
            kAudioOutputUnitProperty_CurrentDevice,
            kAudioUnitScope_Global,
            0,
            &m_deviceId,
            sizeof(m_deviceId)
        );
        
        if (status != noErr) {
            m_lastError = "Failed to set output device";
            closeDevice();
            return false;
        }
        
        // Set render callback
        AURenderCallbackStruct callbackStruct;
        callbackStruct.inputProc = renderCallback;
        callbackStruct.inputProcRefCon = this;
        
        status = AudioUnitSetProperty(
            m_outputUnit,
            kAudioUnitProperty_SetRenderCallback,
            kAudioUnitScope_Input,
            0,
            &callbackStruct,
            sizeof(callbackStruct)
        );
        
        if (status != noErr) {
            m_lastError = "Failed to set render callback";
            closeDevice();
            return false;
        }
        
        // Initialize and start
        status = AudioUnitInitialize(m_outputUnit);
        if (status != noErr) {
            m_lastError = "Failed to initialize audio unit";
            closeDevice();
            return false;
        }
        
        status = AudioOutputUnitStart(m_outputUnit);
        if (status != noErr) {
            m_lastError = "Failed to start audio unit";
            closeDevice();
            return false;
        }
        
        return true;
    }
    
    void closeDevice() override {
        if (m_outputUnit) {
            AudioOutputUnitStop(m_outputUnit);
            AudioUnitUninitialize(m_outputUnit);
            AudioComponentInstanceDispose(m_outputUnit);
            m_outputUnit = nullptr;
        }
        m_deviceId = 0;
    }
    
    bool writeAudio(const float* data, int numSamples, int numChannels) override {
        if (!m_outputUnit) {
            return false;
        }
        
        // Wait for previous data to be consumed
        int waitCount = 0;
        while (m_renderData.dataReady.load() && waitCount < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            waitCount++;
        }
        
        if (m_renderData.dataReady.load()) {
            // Still not consumed, drop this buffer
            return false;
        }
        
        m_renderData.buffer = data;
        m_renderData.numSamples = numSamples;
        m_renderData.numChannels = numChannels;
        m_renderData.dataReady = true;
        
        return true;
    }
    
    bool isDeviceConnected() const override {
        return m_outputUnit != nullptr;
    }
    
    std::string getLastError() const override {
        return m_lastError;
    }
};
#endif

// Main VirtualDeviceRouter implementation
VirtualDeviceRouter::VirtualDeviceRouter(EventDispatcher& eventDispatcher)
    : m_eventDispatcher(eventDispatcher) {
#ifdef _WIN32
    m_platformImpl = std::make_unique<WindowsVirtualDeviceImpl>();
#elif __APPLE__
    m_platformImpl = std::make_unique<MacOSVirtualDeviceImpl>();
#else
    // Linux or other platforms - could implement JACK or PulseAudio support
    m_platformImpl = nullptr;
#endif
}

VirtualDeviceRouter::~VirtualDeviceRouter() {
    shutdown();
}

bool VirtualDeviceRouter::initialize() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    if (m_isInitialized) {
        return true;
    }
    
    if (!m_platformImpl) {
        handleDeviceError("Virtual device routing not supported on this platform", -1);
        return false;
    }
    
    // Initialize JUCE audio format manager
    m_formatManager = std::make_unique<juce::AudioFormatManager>();
    m_formatManager->registerBasicFormats();
    
    // Scan for available virtual devices
    scanForVirtualDevices();
    
    // Start hot-plug detection
    startHotPlugDetection();
    
    m_isInitialized = true;
    
    // Notify that initialization is complete
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("component", std::string("VirtualDeviceRouter"));
    eventData->setValue("initialized", true);
    m_eventDispatcher.publish(EventType::AudioProcessingStarted, eventData);
    
    return true;
}

void VirtualDeviceRouter::shutdown() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    if (!m_isInitialized) {
        return;
    }
    
    // Stop routing
    stopRouting();
    
    // Stop hot-plug detection
    stopHotPlugDetection();
    
    // Close any open device
    if (m_platformImpl) {
        m_platformImpl->closeDevice();
    }
    
    m_isInitialized = false;
    
    // Notify shutdown
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("component", std::string("VirtualDeviceRouter"));
    eventData->setValue("shutdown", true);
    m_eventDispatcher.publish(EventType::AudioProcessingStopped, eventData);
}

bool VirtualDeviceRouter::isInitialized() const {
    return m_isInitialized;
}

std::vector<VirtualDeviceInfo> VirtualDeviceRouter::getAvailableVirtualDevices() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    if (!m_platformImpl) {
        return {};
    }
    
    return m_platformImpl->scanDevices();
}

bool VirtualDeviceRouter::selectVirtualDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    if (!m_platformImpl) {
        return false;
    }
    
    // Find device info
    auto devices = m_platformImpl->scanDevices();
    auto it = std::find_if(devices.begin(), devices.end(),
        [&deviceId](const VirtualDeviceInfo& info) {
            return info.id == deviceId;
        });
    
    if (it == devices.end()) {
        handleDeviceError("Virtual device not found: " + deviceId, -2);
        return false;
    }
    
    // Stop current routing if active
    bool wasRouting = m_isRouting;
    if (wasRouting) {
        m_isRouting = false;
        m_platformImpl->closeDevice();
    }
    
    // Open new device
    if (!m_platformImpl->openDevice(deviceId)) {
        handleDeviceError("Failed to open virtual device: " + 
                         m_platformImpl->getLastError(), -3);
        return false;
    }
    
    // Update current device info
    m_currentDevice = *it;
    m_currentDevice.isConnected = true;
    
    // Restart routing if it was active
    if (wasRouting) {
        m_isRouting = true;
    }
    
    // Notify device change
    if (m_deviceChangeCallback) {
        m_deviceChangeCallback(m_currentDevice);
    }
    
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("deviceId", deviceId);
    eventData->setValue("deviceName", m_currentDevice.name);
    eventData->setValue("deviceType", m_currentDevice.type);
    m_eventDispatcher.publish(EventType::AudioDeviceChanged, eventData);
    
    return true;
}

VirtualDeviceInfo VirtualDeviceRouter::getCurrentVirtualDevice() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_currentDevice;
}

bool VirtualDeviceRouter::hasVirtualDevice() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_currentDevice.isConnected && m_platformImpl && 
           m_platformImpl->isDeviceConnected();
}

bool VirtualDeviceRouter::startRouting() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    if (m_isRouting) {
        return true;
    }
    
    if (!m_currentDevice.isConnected || !m_platformImpl || 
        !m_platformImpl->isDeviceConnected()) {
        handleDeviceError("No virtual device connected", -4);
        return false;
    }
    
    m_isRouting = true;
    m_latencyTracker.reset();
    
    // Notify routing started
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("routing", true);
    m_eventDispatcher.publish(EventType::AudioProcessingStarted, eventData);
    
    return true;
}

void VirtualDeviceRouter::stopRouting() {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    if (!m_isRouting) {
        return;
    }
    
    m_isRouting = false;
    
    // Notify routing stopped
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("routing", false);
    m_eventDispatcher.publish(EventType::AudioProcessingStopped, eventData);
}

bool VirtualDeviceRouter::isRouting() const {
    return m_isRouting;
}

bool VirtualDeviceRouter::routeAudioBuffer(const AudioBuffer& buffer) {
    if (!m_isRouting || !m_platformImpl) {
        return false;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Check if conversion is needed
    bool needsConversion = false;
    if (buffer.getSampleRate() != m_outputSampleRate ||
        buffer.getNumChannels() != m_outputChannels) {
        needsConversion = true;
    }
    
    const float* dataToWrite = buffer.getReadPointer(0);
    int samplesToWrite = buffer.getNumSamples();
    int channelsToWrite = buffer.getNumChannels();
    
    // Handle format conversion if needed
    if (needsConversion) {
        if (!m_conversionBuffer || 
            m_conversionBuffer->getNumSamples() < samplesToWrite ||
            m_conversionBuffer->getNumChannels() != m_outputChannels) {
            m_conversionBuffer = std::make_unique<juce::AudioSampleBuffer>(
                m_outputChannels, samplesToWrite);
        }
        
        handleBufferConversion(buffer, *m_conversionBuffer);
        dataToWrite = m_conversionBuffer->getReadPointer(0);
        channelsToWrite = m_outputChannels;
    }
    
    // Write to device
    bool success = m_platformImpl->writeAudio(dataToWrite, samplesToWrite, 
                                             channelsToWrite);
    
    if (success) {
        // Update statistics
        m_buffersRouted++;
        
        // Calculate output level
        float level = 0.0f;
        for (int ch = 0; ch < channelsToWrite; ++ch) {
            const float* channelData = dataToWrite + (ch * samplesToWrite);
            for (int i = 0; i < samplesToWrite; ++i) {
                level = std::max(level, std::abs(channelData[i]));
            }
        }
        m_outputLevel = level;
        
        // Track latency
        auto endTime = std::chrono::steady_clock::now();
        auto latency = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();
        m_latencyTracker.addSample(latency);
        
        m_lastBufferTime = endTime;
    } else {
        m_droppedBuffers++;
        
        // Check if device is still connected
        if (!m_platformImpl->isDeviceConnected()) {
            handleDeviceError("Virtual device disconnected", -5);
            attemptDeviceReconnection();
        }
    }
    
    return success;
}

bool VirtualDeviceRouter::setOutputConfiguration(double sampleRate, int bufferSize, 
                                                int channels) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    
    m_outputSampleRate = sampleRate;
    m_outputBufferSize = bufferSize;
    m_outputChannels = channels;
    
    // If routing is active, we may need to restart with new configuration
    if (m_isRouting && m_currentDevice.isConnected) {
        // Reopen device with new configuration
        return selectVirtualDevice(m_currentDevice.id);
    }
    
    return true;
}

double VirtualDeviceRouter::getOutputSampleRate() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_outputSampleRate;
}

int VirtualDeviceRouter::getOutputBufferSize() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_outputBufferSize;
}

int VirtualDeviceRouter::getOutputChannels() const {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    return m_outputChannels;
}

float VirtualDeviceRouter::getOutputLevel() const {
    return m_outputLevel.load();
}

uint64_t VirtualDeviceRouter::getBuffersRouted() const {
    return m_buffersRouted.load();
}

double VirtualDeviceRouter::getAverageLatency() const {
    return m_latencyTracker.getAverage();
}

size_t VirtualDeviceRouter::getDroppedBuffers() const {
    return m_droppedBuffers.load();
}

void VirtualDeviceRouter::setDeviceChangeCallback(DeviceChangeCallback callback) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    m_deviceChangeCallback = callback;
}

void VirtualDeviceRouter::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_deviceMutex);
    m_errorCallback = callback;
}

bool VirtualDeviceRouter::isVirtualDeviceInstalled() {
#ifdef _WIN32
    WindowsVirtualDeviceImpl impl;
    auto devices = impl.scanDevices();
    return !devices.empty();
#elif __APPLE__
    MacOSVirtualDeviceImpl impl;
    auto devices = impl.scanDevices();
    return !devices.empty();
#else
    return false;
#endif
}

std::string VirtualDeviceRouter::getVirtualDeviceInstallInstructions() {
#ifdef _WIN32
    return "To use QUIET, you need to install VB-Cable:\n\n"
           "1. Download VB-Cable from https://vb-audio.com/Cable/\n"
           "2. Extract the ZIP file\n"
           "3. Right-click on VBCABLE_Setup_x64.exe and select 'Run as administrator'\n"
           "4. Follow the installation prompts\n"
           "5. Restart your computer\n"
           "6. VB-Cable will appear as 'CABLE Input' in your audio devices";
#elif __APPLE__
    return "To use QUIET, you need to install BlackHole:\n\n"
           "1. Download BlackHole from https://existential.audio/blackhole/\n"
           "2. Choose the 2ch version for stereo or 16ch for multi-channel\n"
           "3. Open the downloaded PKG file\n"
           "4. Follow the installation prompts\n"
           "5. Grant necessary permissions when prompted\n"
           "6. BlackHole will appear in your audio devices";
#else
    return "Virtual audio device routing is not yet supported on this platform.";
#endif
}

void VirtualDeviceRouter::scanForVirtualDevices() {
    if (!m_platformImpl) {
        return;
    }
    
    auto devices = m_platformImpl->scanDevices();
    
    // Auto-select first available device if none selected
    if (!m_currentDevice.isConnected && !devices.empty()) {
        selectVirtualDevice(devices[0].id);
    }
}

void VirtualDeviceRouter::startHotPlugDetection() {
    if (m_hotPlugRunning) {
        return;
    }
    
    m_hotPlugRunning = true;
    m_hotPlugThread = std::make_unique<std::thread>(
        &VirtualDeviceRouter::hotPlugDetectionThread, this);
}

void VirtualDeviceRouter::stopHotPlugDetection() {
    if (!m_hotPlugRunning) {
        return;
    }
    
    m_hotPlugRunning = false;
    
    if (m_hotPlugThread && m_hotPlugThread->joinable()) {
        m_hotPlugThread->join();
    }
    
    m_hotPlugThread.reset();
}

void VirtualDeviceRouter::hotPlugDetectionThread() {
    while (m_hotPlugRunning) {
        // Sleep for the detection interval
        std::this_thread::sleep_for(m_hotPlugInterval);
        
        if (!m_hotPlugRunning) {
            break;
        }
        
        // Scan for devices
        std::lock_guard<std::mutex> lock(m_deviceMutex);
        
        if (m_platformImpl) {
            auto currentDevices = m_platformImpl->scanDevices();
            
            // Check if selected device is still available
            if (m_currentDevice.isConnected) {
                auto it = std::find_if(currentDevices.begin(), currentDevices.end(),
                    [this](const VirtualDeviceInfo& info) {
                        return info.id == m_currentDevice.id;
                    });
                
                if (it == currentDevices.end()) {
                    // Device disconnected
                    m_currentDevice.isConnected = false;
                    handleDeviceError("Virtual device disconnected", -6);
                    
                    if (m_deviceChangeCallback) {
                        m_deviceChangeCallback(m_currentDevice);
                    }
                }
            } else {
                // Check if any new devices appeared
                if (!currentDevices.empty()) {
                    // Notify about new devices
                    auto eventData = std::make_shared<EventData>();
                    eventData->setValue("deviceCount", 
                                      static_cast<int>(currentDevices.size()));
                    m_eventDispatcher.publish(EventType::AudioDeviceChanged, 
                                            eventData);
                }
            }
        }
    }
}

bool VirtualDeviceRouter::isVirtualDevice(const std::string& deviceName) const {
    // Check for known virtual device names
    const std::vector<std::string> virtualDevicePatterns = {
        "VB-Audio", "CABLE Input", "VB-Cable",  // Windows
        "BlackHole",                             // macOS
        "JACK", "PulseAudio"                    // Linux
    };
    
    for (const auto& pattern : virtualDevicePatterns) {
        if (deviceName.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

void VirtualDeviceRouter::handleBufferConversion(const AudioBuffer& input, 
                                                juce::AudioSampleBuffer& output) {
    // Simple channel mapping and resampling if needed
    int inputChannels = input.getNumChannels();
    int outputChannels = output.getNumChannels();
    int numSamples = std::min(input.getNumSamples(), output.getNumSamples());
    
    for (int ch = 0; ch < outputChannels; ++ch) {
        float* outChannel = output.getWritePointer(ch);
        
        if (ch < inputChannels) {
            // Copy from input channel
            const float* inChannel = input.getReadPointer(ch);
            std::memcpy(outChannel, inChannel, numSamples * sizeof(float));
        } else if (inputChannels > 0) {
            // Duplicate first channel for missing channels
            const float* inChannel = input.getReadPointer(0);
            std::memcpy(outChannel, inChannel, numSamples * sizeof(float));
        } else {
            // Fill with silence
            std::memset(outChannel, 0, numSamples * sizeof(float));
        }
    }
    
    // TODO: Add proper resampling if sample rates differ
}

void VirtualDeviceRouter::handleDeviceError(const std::string& message, 
                                           int errorCode) {
    // Log error
    auto eventData = EventDataFactory::createErrorData(message, errorCode);
    m_eventDispatcher.publish(EventType::AudioDeviceError, eventData);
    
    // Call error callback if set
    if (m_errorCallback) {
        m_errorCallback(message, errorCode);
    }
}

bool VirtualDeviceRouter::attemptDeviceReconnection() {
    if (!m_currentDevice.id.empty() && m_platformImpl) {
        // Try to reconnect to the same device
        if (m_platformImpl->openDevice(m_currentDevice.id)) {
            m_currentDevice.isConnected = true;
            
            // Notify reconnection
            if (m_deviceChangeCallback) {
                m_deviceChangeCallback(m_currentDevice);
            }
            
            auto eventData = std::make_shared<EventData>();
            eventData->setValue("reconnected", true);
            eventData->setValue("deviceId", m_currentDevice.id);
            m_eventDispatcher.publish(EventType::AudioDeviceChanged, eventData);
            
            return true;
        }
    }
    
    return false;
}

} // namespace core
} // namespace quiet