/**
 * Example demonstrating VirtualDeviceRouter usage
 * 
 * This example shows how to:
 * 1. Initialize the virtual device router
 * 2. Detect available virtual audio devices
 * 3. Select and configure a virtual device
 * 4. Route audio buffers to the virtual device
 * 5. Monitor routing performance
 */

#include "quiet/core/VirtualDeviceRouter.h"
#include "quiet/core/EventDispatcher.h"
#include "quiet/core/AudioBuffer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace quiet::core;

// Generate a simple sine wave for testing
void generateSineWave(AudioBuffer& buffer, float frequency, float amplitude) {
    const float sampleRate = buffer.getSampleRate();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    for (int ch = 0; ch < numChannels; ++ch) {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            data[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
        }
    }
}

int main() {
    std::cout << "Virtual Device Router Example\n";
    std::cout << "=============================\n\n";
    
    // Create event dispatcher
    EventDispatcher eventDispatcher;
    eventDispatcher.start();
    
    // Subscribe to audio events
    eventDispatcher.subscribe(EventType::AudioDeviceChanged, [](const Event& event) {
        auto deviceName = event.data->getValue<std::string>("deviceName", "Unknown");
        std::cout << "Device changed: " << deviceName << "\n";
    });
    
    eventDispatcher.subscribe(EventType::AudioDeviceError, [](const Event& event) {
        auto message = event.data->getValue<std::string>("message", "Unknown error");
        auto errorCode = event.data->getValue<int>("error_code", 0);
        std::cerr << "Audio device error: " << message << " (code: " << errorCode << ")\n";
    });
    
    // Create virtual device router
    VirtualDeviceRouter router(eventDispatcher);
    
    // Set callbacks
    router.setDeviceChangeCallback([](const VirtualDeviceInfo& device) {
        std::cout << "Virtual device status changed: " << device.name 
                  << " - " << (device.isConnected ? "Connected" : "Disconnected") << "\n";
    });
    
    router.setErrorCallback([](const std::string& message, int errorCode) {
        std::cerr << "Router error: " << message << " (code: " << errorCode << ")\n";
    });
    
    // Initialize router
    std::cout << "Initializing virtual device router...\n";
    if (!router.initialize()) {
        std::cerr << "Failed to initialize virtual device router\n";
        return 1;
    }
    
    // Check if virtual device is installed
    if (!VirtualDeviceRouter::isVirtualDeviceInstalled()) {
        std::cout << "\nNo virtual audio device detected!\n";
        std::cout << "Installation instructions:\n";
        std::cout << VirtualDeviceRouter::getVirtualDeviceInstallInstructions() << "\n";
        return 1;
    }
    
    // Get available virtual devices
    auto devices = router.getAvailableVirtualDevices();
    std::cout << "\nFound " << devices.size() << " virtual audio device(s):\n";
    
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        std::cout << i + 1 << ". " << device.name 
                  << " (" << device.type << ")"
                  << " - " << device.maxChannels << " channels\n";
        
        std::cout << "   Supported sample rates: ";
        for (auto rate : device.supportedSampleRates) {
            std::cout << rate << "Hz ";
        }
        std::cout << "\n";
    }
    
    if (devices.empty()) {
        std::cerr << "No virtual devices available\n";
        return 1;
    }
    
    // Select first available device
    std::cout << "\nSelecting device: " << devices[0].name << "\n";
    if (!router.selectVirtualDevice(devices[0].id)) {
        std::cerr << "Failed to select virtual device\n";
        return 1;
    }
    
    // Configure output
    std::cout << "Configuring output: 48kHz, 256 samples, 2 channels\n";
    router.setOutputConfiguration(48000.0, 256, 2);
    
    // Start routing
    std::cout << "Starting audio routing...\n";
    if (!router.startRouting()) {
        std::cerr << "Failed to start routing\n";
        return 1;
    }
    
    // Generate and route test audio
    std::cout << "\nRouting test tone (440Hz sine wave) for 5 seconds...\n";
    
    AudioBuffer testBuffer(2, 256, 48000.0);
    const int totalBuffers = (48000 * 5) / 256;  // 5 seconds worth
    
    auto startTime = std::chrono::steady_clock::now();
    
    for (int i = 0; i < totalBuffers; ++i) {
        // Generate sine wave
        generateSineWave(testBuffer, 440.0f, 0.5f);
        
        // Route to virtual device
        if (!router.routeAudioBuffer(testBuffer)) {
            std::cerr << "Failed to route buffer " << i << "\n";
        }
        
        // Print progress every second
        if (i % (48000 / 256) == 0) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<float>(currentTime - startTime).count();
            
            std::cout << "Time: " << elapsed << "s"
                      << " | Buffers routed: " << router.getBuffersRouted()
                      << " | Dropped: " << router.getDroppedBuffers()
                      << " | Level: " << router.getOutputLevel()
                      << " | Latency: " << router.getAverageLatency() << "ms\n";
        }
        
        // Sleep to simulate real-time processing
        std::this_thread::sleep_for(std::chrono::microseconds(5333));  // ~256/48000
    }
    
    // Stop routing
    std::cout << "\nStopping audio routing...\n";
    router.stopRouting();
    
    // Print final statistics
    std::cout << "\nFinal Statistics:\n";
    std::cout << "- Total buffers routed: " << router.getBuffersRouted() << "\n";
    std::cout << "- Dropped buffers: " << router.getDroppedBuffers() << "\n";
    std::cout << "- Average latency: " << router.getAverageLatency() << "ms\n";
    
    // Cleanup
    router.shutdown();
    eventDispatcher.stop();
    
    std::cout << "\nExample completed successfully!\n";
    return 0;
}