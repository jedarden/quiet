#include <gtest/gtest.h>
#include "quiet/core/AudioDeviceManager.h"
#include "quiet/core/EventDispatcher.h"
#include <thread>
#include <chrono>

namespace quiet {
namespace core {
namespace test {

class AudioDeviceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_eventDispatcher = std::make_unique<EventDispatcher>();
        m_eventDispatcher->start();
        m_deviceManager = std::make_unique<AudioDeviceManager>(*m_eventDispatcher);
    }
    
    void TearDown() override {
        m_deviceManager.reset();
        m_eventDispatcher->stop();
        m_eventDispatcher.reset();
    }
    
    std::unique_ptr<EventDispatcher> m_eventDispatcher;
    std::unique_ptr<AudioDeviceManager> m_deviceManager;
};

TEST_F(AudioDeviceManagerTest, InitializeShutdown) {
    EXPECT_FALSE(m_deviceManager->isAudioActive());
    
    // Initialize
    EXPECT_TRUE(m_deviceManager->initialize());
    
    // Should not be active until started
    EXPECT_FALSE(m_deviceManager->isAudioActive());
    
    // Shutdown
    m_deviceManager->shutdown();
    EXPECT_FALSE(m_deviceManager->isAudioActive());
}

TEST_F(AudioDeviceManagerTest, GetAvailableDevices) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    auto devices = m_deviceManager->getAvailableInputDevices();
    
    // Should have at least one device (default system device)
    EXPECT_GT(devices.size(), 0);
    
    // Check device info validity
    for (const auto& device : devices) {
        EXPECT_FALSE(device.id.empty());
        EXPECT_FALSE(device.name.empty());
        EXPECT_GT(device.maxInputChannels, 0);
        EXPECT_FALSE(device.availableSampleRates.empty());
        EXPECT_FALSE(device.availableBufferSizes.empty());
    }
}

TEST_F(AudioDeviceManagerTest, AudioConfiguration) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    // Test valid configurations
    EXPECT_TRUE(m_deviceManager->setAudioConfiguration(48000.0, 256));
    EXPECT_EQ(m_deviceManager->getCurrentSampleRate(), 48000.0);
    EXPECT_EQ(m_deviceManager->getCurrentBufferSize(), 256);
    
    EXPECT_TRUE(m_deviceManager->setAudioConfiguration(44100.0, 512));
    EXPECT_EQ(m_deviceManager->getCurrentSampleRate(), 44100.0);
    EXPECT_EQ(m_deviceManager->getCurrentBufferSize(), 512);
    
    // Test invalid configurations
    EXPECT_FALSE(m_deviceManager->setAudioConfiguration(1000.0, 256));  // Invalid sample rate
    EXPECT_FALSE(m_deviceManager->setAudioConfiguration(48000.0, 100)); // Invalid buffer size
}

TEST_F(AudioDeviceManagerTest, StartStopAudio) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    // Start audio
    EXPECT_TRUE(m_deviceManager->startAudio());
    EXPECT_TRUE(m_deviceManager->isAudioActive());
    
    // Starting again should succeed
    EXPECT_TRUE(m_deviceManager->startAudio());
    EXPECT_TRUE(m_deviceManager->isAudioActive());
    
    // Stop audio
    m_deviceManager->stopAudio();
    EXPECT_FALSE(m_deviceManager->isAudioActive());
    
    // Stopping again should be safe
    m_deviceManager->stopAudio();
    EXPECT_FALSE(m_deviceManager->isAudioActive());
}

TEST_F(AudioDeviceManagerTest, InputLevelAndMuting) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    // Initial state
    EXPECT_EQ(m_deviceManager->getInputLevel(), 0.0f);
    EXPECT_FALSE(m_deviceManager->isInputMuted());
    
    // Test muting
    m_deviceManager->setInputMuted(true);
    EXPECT_TRUE(m_deviceManager->isInputMuted());
    
    m_deviceManager->setInputMuted(false);
    EXPECT_FALSE(m_deviceManager->isInputMuted());
}

TEST_F(AudioDeviceManagerTest, AudioCallback) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    bool callbackCalled = false;
    AudioBuffer receivedBuffer;
    
    m_deviceManager->setAudioCallback([&](const AudioBuffer& buffer) {
        callbackCalled = true;
        receivedBuffer = buffer;
    });
    
    // Start audio and wait briefly
    EXPECT_TRUE(m_deviceManager->startAudio());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Callback should have been called (if audio device is available)
    auto devices = m_deviceManager->getAvailableInputDevices();
    if (!devices.empty()) {
        EXPECT_TRUE(callbackCalled);
        EXPECT_GT(receivedBuffer.getNumChannels(), 0);
        EXPECT_GT(receivedBuffer.getNumSamples(), 0);
    }
    
    m_deviceManager->stopAudio();
}

TEST_F(AudioDeviceManagerTest, ErrorCallback) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    bool errorCallbackCalled = false;
    std::string errorMessage;
    
    m_deviceManager->setErrorCallback([&](const std::string& message) {
        errorCallbackCalled = true;
        errorMessage = message;
    });
    
    // Try to select a non-existent device
    EXPECT_FALSE(m_deviceManager->selectInputDevice("non_existent_device_id"));
    
    // Error should have been reported through event dispatcher
    // (Error callback is called for device-level errors, not API errors)
}

TEST_F(AudioDeviceManagerTest, DeviceSelection) {
    ASSERT_TRUE(m_deviceManager->initialize());
    
    auto devices = m_deviceManager->getAvailableInputDevices();
    if (devices.empty()) {
        GTEST_SKIP() << "No audio input devices available";
    }
    
    // Select the first available device
    const auto& firstDevice = devices[0];
    EXPECT_TRUE(m_deviceManager->selectInputDevice(firstDevice.id));
    
    // Verify current device
    auto currentDevice = m_deviceManager->getCurrentInputDevice();
    EXPECT_EQ(currentDevice.id, firstDevice.id);
    EXPECT_EQ(currentDevice.name, firstDevice.name);
}

TEST_F(AudioDeviceManagerTest, EventNotifications) {
    std::atomic<int> startEventCount{0};
    std::atomic<int> stopEventCount{0};
    std::atomic<int> levelEventCount{0};
    
    // Subscribe to events
    m_eventDispatcher->subscribe(EventType::AudioProcessingStarted, 
        [&](const Event&) { startEventCount++; });
    
    m_eventDispatcher->subscribe(EventType::AudioProcessingStopped, 
        [&](const Event&) { stopEventCount++; });
    
    m_eventDispatcher->subscribe(EventType::AudioLevelChanged, 
        [&](const Event&) { levelEventCount++; });
    
    ASSERT_TRUE(m_deviceManager->initialize());
    
    // Start should trigger event
    m_deviceManager->startAudio();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(startEventCount.load(), 1);
    
    // Wait for level events
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Stop should trigger event
    m_deviceManager->stopAudio();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(stopEventCount.load(), 1);
    
    // Should have received some level events if audio is running
    auto devices = m_deviceManager->getAvailableInputDevices();
    if (!devices.empty()) {
        EXPECT_GT(levelEventCount.load(), 0);
    }
}

} // namespace test
} // namespace core
} // namespace quiet