#include <gtest/gtest.h>
#include "quiet/core/VirtualDeviceRouter.h"
#include "quiet/core/EventDispatcher.h"
#include "quiet/core/AudioBuffer.h"
#include <thread>
#include <chrono>

using namespace quiet::core;

class VirtualDeviceRouterTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_eventDispatcher = std::make_unique<EventDispatcher>();
        m_eventDispatcher->start();
        m_router = std::make_unique<VirtualDeviceRouter>(*m_eventDispatcher);
    }
    
    void TearDown() override {
        m_router.reset();
        m_eventDispatcher->stop();
        m_eventDispatcher.reset();
    }
    
    std::unique_ptr<EventDispatcher> m_eventDispatcher;
    std::unique_ptr<VirtualDeviceRouter> m_router;
};

TEST_F(VirtualDeviceRouterTest, Initialization) {
    EXPECT_FALSE(m_router->isInitialized());
    EXPECT_FALSE(m_router->isRouting());
    
    bool result = m_router->initialize();
    EXPECT_TRUE(result);
    EXPECT_TRUE(m_router->isInitialized());
}

TEST_F(VirtualDeviceRouterTest, DeviceDetection) {
    m_router->initialize();
    
    auto devices = m_router->getAvailableVirtualDevices();
    
    // This test will pass regardless of whether virtual devices are installed
    if (!devices.empty()) {
        // Virtual device found
        for (const auto& device : devices) {
            EXPECT_FALSE(device.id.empty());
            EXPECT_FALSE(device.name.empty());
            EXPECT_FALSE(device.type.empty());
            EXPECT_GT(device.maxChannels, 0);
            EXPECT_FALSE(device.supportedSampleRates.empty());
        }
    }
}

TEST_F(VirtualDeviceRouterTest, InstallationCheck) {
    bool installed = VirtualDeviceRouter::isVirtualDeviceInstalled();
    
    if (!installed) {
        std::string instructions = VirtualDeviceRouter::getVirtualDeviceInstallInstructions();
        EXPECT_FALSE(instructions.empty());
        
#ifdef _WIN32
        EXPECT_NE(instructions.find("VB-Cable"), std::string::npos);
#elif __APPLE__
        EXPECT_NE(instructions.find("BlackHole"), std::string::npos);
#endif
    }
}

TEST_F(VirtualDeviceRouterTest, ConfigurationSettings) {
    m_router->initialize();
    
    // Test default configuration
    EXPECT_EQ(m_router->getOutputSampleRate(), 48000.0);
    EXPECT_EQ(m_router->getOutputBufferSize(), 256);
    EXPECT_EQ(m_router->getOutputChannels(), 2);
    
    // Test configuration changes
    bool result = m_router->setOutputConfiguration(44100.0, 512, 1);
    EXPECT_TRUE(result);
    
    EXPECT_EQ(m_router->getOutputSampleRate(), 44100.0);
    EXPECT_EQ(m_router->getOutputBufferSize(), 512);
    EXPECT_EQ(m_router->getOutputChannels(), 1);
}

TEST_F(VirtualDeviceRouterTest, RoutingWithoutDevice) {
    m_router->initialize();
    
    // Should fail to start routing without a device
    bool result = m_router->startRouting();
    EXPECT_FALSE(result);
    EXPECT_FALSE(m_router->isRouting());
}

TEST_F(VirtualDeviceRouterTest, BufferRoutingSimulation) {
    m_router->initialize();
    
    // Create test audio buffer
    AudioBuffer testBuffer(2, 256, 48000.0);
    
    // Fill with test data
    for (int ch = 0; ch < testBuffer.getNumChannels(); ++ch) {
        float* data = testBuffer.getWritePointer(ch);
        for (int i = 0; i < testBuffer.getNumSamples(); ++i) {
            data[i] = std::sin(2.0f * M_PI * 440.0f * i / 48000.0f) * 0.5f;
        }
    }
    
    // Without a device, routing should fail
    bool result = m_router->routeAudioBuffer(testBuffer);
    EXPECT_FALSE(result);
    
    // Check statistics
    EXPECT_EQ(m_router->getBuffersRouted(), 0);
    EXPECT_EQ(m_router->getDroppedBuffers(), 0);
}

TEST_F(VirtualDeviceRouterTest, ErrorCallbackTest) {
    m_router->initialize();
    
    bool errorReceived = false;
    std::string errorMessage;
    int errorCode = 0;
    
    m_router->setErrorCallback([&](const std::string& msg, int code) {
        errorReceived = true;
        errorMessage = msg;
        errorCode = code;
    });
    
    // Trying to start routing without a device should trigger an error
    m_router->startRouting();
    
    // Give time for error handling
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_TRUE(errorReceived);
    EXPECT_FALSE(errorMessage.empty());
    EXPECT_NE(errorCode, 0);
}

TEST_F(VirtualDeviceRouterTest, DeviceChangeCallbackTest) {
    m_router->initialize();
    
    bool deviceChanged = false;
    VirtualDeviceInfo changedDevice;
    
    m_router->setDeviceChangeCallback([&](const VirtualDeviceInfo& device) {
        deviceChanged = true;
        changedDevice = device;
    });
    
    auto devices = m_router->getAvailableVirtualDevices();
    if (!devices.empty()) {
        // Select first available device
        bool result = m_router->selectVirtualDevice(devices[0].id);
        
        if (result) {
            EXPECT_TRUE(deviceChanged);
            EXPECT_EQ(changedDevice.id, devices[0].id);
            EXPECT_TRUE(changedDevice.isConnected);
        }
    }
}

TEST_F(VirtualDeviceRouterTest, HotPlugDetectionLifecycle) {
    // Initialize starts hot-plug detection
    m_router->initialize();
    
    // Give time for hot-plug thread to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Shutdown should stop hot-plug detection
    m_router->shutdown();
    
    EXPECT_FALSE(m_router->isInitialized());
}

TEST_F(VirtualDeviceRouterTest, MonitoringMetrics) {
    m_router->initialize();
    
    // Initial metrics should be zero
    EXPECT_EQ(m_router->getOutputLevel(), 0.0f);
    EXPECT_EQ(m_router->getBuffersRouted(), 0);
    EXPECT_EQ(m_router->getAverageLatency(), 0.0);
    EXPECT_EQ(m_router->getDroppedBuffers(), 0);
}

TEST_F(VirtualDeviceRouterTest, MultipleInitializeShutdown) {
    // Multiple initialize calls should be safe
    EXPECT_TRUE(m_router->initialize());
    EXPECT_TRUE(m_router->initialize());
    EXPECT_TRUE(m_router->isInitialized());
    
    // Multiple shutdown calls should be safe
    m_router->shutdown();
    m_router->shutdown();
    EXPECT_FALSE(m_router->isInitialized());
    
    // Can reinitialize after shutdown
    EXPECT_TRUE(m_router->initialize());
    EXPECT_TRUE(m_router->isInitialized());
}