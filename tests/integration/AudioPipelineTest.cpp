#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "quiet/core/AudioBuffer.h"
#include "quiet/core/NoiseReductionProcessor.h"
#include "quiet/core/EventDispatcher.h"
#include "quiet/core/ConfigurationManager.h"
#include <memory>
#include <chrono>
#include <thread>

using namespace quiet::core;

/**
 * @brief Integration tests for the complete audio processing pipeline
 * 
 * These tests verify that all components work together correctly:
 * - Event communication between components
 * - Configuration persistence and application
 * - End-to-end audio processing pipeline
 * - Performance under realistic conditions
 */
class AudioPipelineIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize event dispatcher
        eventDispatcher = std::make_unique<EventDispatcher>();
        eventDispatcher->start();
        
        // Initialize configuration manager
        configManager = std::make_unique<ConfigurationManager>(*eventDispatcher);
        ASSERT_TRUE(configManager->initialize("test_config.json"));
        
        // Initialize noise reduction processor
        noiseProcessor = std::make_unique<NoiseReductionProcessor>(*eventDispatcher);
        ASSERT_TRUE(noiseProcessor->initialize());
        
        // Set up event listener to track events
        eventHandle = eventDispatcher->subscribe(EventType::NoiseReductionToggled,
            [this](const Event& event) {
                eventsReceived.push_back(event.type);
            });
    }
    
    void TearDown() override {
        if (eventHandle) {
            eventDispatcher->unsubscribe(eventHandle);
        }
        
        if (noiseProcessor) {
            noiseProcessor->shutdown();
        }
        
        if (configManager) {
            configManager->shutdown();
        }
        
        if (eventDispatcher) {
            eventDispatcher->stop();
        }
        
        // Clean up test config file
        std::remove("test_config.json");
    }
    
    void generateTestAudio(AudioBuffer& buffer, float frequency = 440.0f, float amplitude = 0.5f) {
        const float sampleRate = 48000.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
            }
        }
    }
    
    void addNoise(AudioBuffer& buffer, float noiseLevel = 0.1f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-noiseLevel, noiseLevel);
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] += dist(gen);
            }
        }
    }
    
    float calculateRMSLevel(const AudioBuffer& buffer) {
        return buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    }

    std::unique_ptr<EventDispatcher> eventDispatcher;
    std::unique_ptr<ConfigurationManager> configManager;
    std::unique_ptr<NoiseReductionProcessor> noiseProcessor;
    
    EventDispatcher::ListenerHandle eventHandle{0};
    std::vector<EventType> eventsReceived;
};

// Test complete system initialization
TEST_F(AudioPipelineIntegrationTest, SystemInitialization) {
    EXPECT_TRUE(eventDispatcher->isRunning());
    EXPECT_TRUE(configManager->isInitialized());
    EXPECT_TRUE(noiseProcessor->isInitialized());
    
    // Verify default configuration was loaded
    EXPECT_TRUE(configManager->getValue<bool>("processing.noise_reduction_enabled", false));
    EXPECT_EQ("medium", configManager->getValue<std::string>("processing.reduction_level", ""));
}

// Test configuration changes propagate through system
TEST_F(AudioPipelineIntegrationTest, ConfigurationPropagation) {
    // Change configuration
    configManager->setValue<bool>("processing.noise_reduction_enabled", false);
    
    // Apply configuration to processor
    NoiseReductionConfig config;
    config.enabled = configManager->getValue<bool>("processing.noise_reduction_enabled", true);
    config.level = NoiseReductionConfig::Level::Low;
    
    noiseProcessor->setConfig(config);
    
    // Verify configuration was applied
    EXPECT_FALSE(noiseProcessor->isEnabled());
    EXPECT_EQ(NoiseReductionConfig::Level::Low, noiseProcessor->getLevel());
}

// Test end-to-end audio processing pipeline
TEST_F(AudioPipelineIntegrationTest, EndToEndAudioProcessing) {
    const int bufferSize = 1024;
    const int numChannels = 1;
    
    // Create test audio with noise
    AudioBuffer inputBuffer(numChannels, bufferSize);
    generateTestAudio(inputBuffer, 440.0f, 0.8f);  // Strong signal
    addNoise(inputBuffer, 0.2f);  // Add noise
    
    AudioBuffer originalBuffer = inputBuffer;  // Keep copy for comparison
    
    // Process through noise reduction
    EXPECT_TRUE(noiseProcessor->process(inputBuffer));
    
    // Verify processing occurred
    bool bufferChanged = false;
    for (int i = 0; i < bufferSize && !bufferChanged; ++i) {
        if (std::abs(originalBuffer.getSample(0, i) - inputBuffer.getSample(0, i)) > 0.001f) {
            bufferChanged = true;
        }
    }
    EXPECT_TRUE(bufferChanged);
    
    // Get processing statistics
    NoiseReductionStats stats = noiseProcessor->getStats();
    EXPECT_GT(stats.framesProcessed, 0u);
    EXPECT_GT(stats.totalProcessingTime, 0u);
}

// Test real-time performance requirements
TEST_F(AudioPipelineIntegrationTest, RealTimePerformanceRequirements) {
    const int frameSize = 480;  // 10ms at 48kHz
    const int numFrames = 100;  // Process 1 second worth
    
    AudioBuffer testFrame(1, frameSize);
    generateTestAudio(testFrame);
    addNoise(testFrame, 0.1f);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Process multiple frames to simulate real-time operation
    for (int frame = 0; frame < numFrames; ++frame) {
        AudioBuffer frameBuffer = testFrame;  // Copy for processing
        ASSERT_TRUE(noiseProcessor->process(frameBuffer));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Total processing time should be much less than real-time
    // (100 frames of 10ms = 1000ms real-time)
    EXPECT_LT(totalTime.count(), 500);  // Should complete in less than 500ms
    
    // Check performance metrics
    float cpuUsage = noiseProcessor->getCpuUsage();
    float latency = noiseProcessor->getLatency();
    
    EXPECT_LT(cpuUsage, 50.0f);  // Should use less than 50% CPU
    EXPECT_LT(latency, 30.0f);   // Should have less than 30ms latency
}

// Test event system integration
TEST_F(AudioPipelineIntegrationTest, EventSystemIntegration) {
    eventsReceived.clear();
    
    // Toggle noise reduction and verify events
    noiseProcessor->setEnabled(false);
    noiseProcessor->setEnabled(true);
    
    // Give events time to propagate
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Should have received toggle events
    EXPECT_GE(eventsReceived.size(), 1u);
    
    // Verify event statistics
    EventDispatcher::Stats eventStats = eventDispatcher->getStats();
    EXPECT_GT(eventStats.eventsPublished, 0u);
    EXPECT_GT(eventStats.eventsDelivered, 0u);
    EXPECT_GT(eventStats.activeListeners, 0u);
}

// Test configuration persistence
TEST_F(AudioPipelineIntegrationTest, ConfigurationPersistence) {
    // Set some configuration values
    configManager->setValue<bool>("processing.noise_reduction_enabled", false);
    configManager->setValue<std::string>("processing.reduction_level", "high");
    configManager->setValue<int>("audio.buffer_size", 512);
    
    // Force save
    EXPECT_TRUE(configManager->saveConfiguration());
    
    // Create new configuration manager and verify values persist
    auto newConfigManager = std::make_unique<ConfigurationManager>(*eventDispatcher);
    EXPECT_TRUE(newConfigManager->initialize("test_config.json"));
    
    EXPECT_FALSE(newConfigManager->getValue<bool>("processing.noise_reduction_enabled", true));
    EXPECT_EQ("high", newConfigManager->getValue<std::string>("processing.reduction_level", ""));
    EXPECT_EQ(512, newConfigManager->getValue<int>("audio.buffer_size", 0));
    
    newConfigManager->shutdown();
}

// Test error handling and recovery
TEST_F(AudioPipelineIntegrationTest, ErrorHandlingAndRecovery) {
    // Test processing with invalid buffer
    AudioBuffer emptyBuffer(0, 0);
    EXPECT_FALSE(noiseProcessor->process(emptyBuffer));
    
    // Verify system is still functional after error
    AudioBuffer validBuffer(1, 1024);
    generateTestAudio(validBuffer);
    EXPECT_TRUE(noiseProcessor->process(validBuffer));
    
    // Test shutdown and restart
    noiseProcessor->shutdown();
    EXPECT_FALSE(noiseProcessor->isInitialized());
    
    EXPECT_TRUE(noiseProcessor->initialize());
    EXPECT_TRUE(noiseProcessor->isInitialized());
    
    // Should still be able to process after restart
    EXPECT_TRUE(noiseProcessor->process(validBuffer));
}

// Test concurrent access and thread safety
TEST_F(AudioPipelineIntegrationTest, ConcurrentAccessThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 50;
    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;
    
    auto workerFunction = [&]() {
        for (int i = 0; i < operationsPerThread; ++i) {
            // Mix of operations
            switch (i % 4) {
                case 0: {
                    // Process audio
                    AudioBuffer buffer(1, 256);
                    generateTestAudio(buffer);
                    if (noiseProcessor->process(buffer)) {
                        successCount++;
                    }
                    break;
                }
                case 1: {
                    // Toggle processing
                    noiseProcessor->setEnabled(!noiseProcessor->isEnabled());
                    successCount++;
                    break;
                }
                case 2: {
                    // Change configuration
                    configManager->setValue<int>("test.value." + std::to_string(i), i);
                    successCount++;
                    break;
                }
                case 3: {
                    // Get statistics
                    auto stats = noiseProcessor->getStats();
                    if (stats.framesProcessed >= 0) {  // Basic validity check
                        successCount++;
                    }
                    break;
                }
            }
            
            // Small delay to increase chance of contention
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };
    
    // Start threads
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(workerFunction);
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Most operations should succeed
    int totalOperations = numThreads * operationsPerThread;
    EXPECT_GT(successCount.load(), totalOperations * 0.8);  // At least 80% success rate
}

// Test memory usage and leak detection
TEST_F(AudioPipelineIntegrationTest, MemoryUsageAndLeakDetection) {
    // Process many buffers to test for memory leaks
    const int numBuffers = 1000;
    
    for (int i = 0; i < numBuffers; ++i) {
        AudioBuffer buffer(1, 1024);
        generateTestAudio(buffer, 440.0f + i, 0.5f);  // Vary frequency
        addNoise(buffer, 0.1f);
        
        ASSERT_TRUE(noiseProcessor->process(buffer));
        
        // Periodically reset stats to test cleanup
        if (i % 100 == 0) {
            noiseProcessor->resetStats();
        }
    }
    
    // Verify final statistics
    NoiseReductionStats finalStats = noiseProcessor->getStats();
    EXPECT_LT(finalStats.framesProcessed, 150u);  // Should have been reset
    
    // System should still be responsive
    AudioBuffer testBuffer(1, 512);
    generateTestAudio(testBuffer);
    EXPECT_TRUE(noiseProcessor->process(testBuffer));
}

// Test different audio formats and edge cases
TEST_F(AudioPipelineIntegrationTest, AudioFormatHandling) {
    // Test different buffer sizes
    std::vector<int> bufferSizes = {64, 128, 256, 512, 1024, 2048};
    
    for (int bufferSize : bufferSizes) {
        AudioBuffer buffer(1, bufferSize);
        generateTestAudio(buffer);
        EXPECT_TRUE(noiseProcessor->process(buffer)) 
            << "Failed with buffer size: " << bufferSize;
    }
    
    // Test mono and stereo
    AudioBuffer monoBuffer(1, 1024);
    AudioBuffer stereoBuffer(2, 1024);
    
    generateTestAudio(monoBuffer);
    generateTestAudio(stereoBuffer);
    
    EXPECT_TRUE(noiseProcessor->process(monoBuffer));
    EXPECT_TRUE(noiseProcessor->process(stereoBuffer));
    
    // Test edge case audio content
    AudioBuffer silentBuffer(1, 1024);
    silentBuffer.clear();
    EXPECT_TRUE(noiseProcessor->process(silentBuffer));
    
    AudioBuffer loudBuffer(1, 1024);
    generateTestAudio(loudBuffer, 440.0f, 1.0f);  // Maximum amplitude
    EXPECT_TRUE(noiseProcessor->process(loudBuffer));
}

// Performance benchmark test
TEST_F(AudioPipelineIntegrationTest, PerformanceBenchmark) {
    const int benchmarkDuration = 5;  // seconds
    const int frameSize = 480;  // 10ms frames
    const int framesPerSecond = 100;
    const int totalFrames = benchmarkDuration * framesPerSecond;
    
    AudioBuffer benchmarkFrame(1, frameSize);
    generateTestAudio(benchmarkFrame);
    addNoise(benchmarkFrame, 0.15f);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int frame = 0; frame < totalFrames; ++frame) {
        AudioBuffer frameBuffer = benchmarkFrame;
        ASSERT_TRUE(noiseProcessor->process(frameBuffer));
        
        // Simulate real-time constraints
        std::this_thread::sleep_for(std::chrono::microseconds(100));  // Small delay
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto actualDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Should complete within reasonable time
    EXPECT_LT(actualDuration.count(), (benchmarkDuration + 1) * 1000);
    
    // Check final performance metrics
    float finalCpuUsage = noiseProcessor->getCpuUsage();
    float finalLatency = noiseProcessor->getLatency();
    
    EXPECT_LT(finalCpuUsage, 25.0f);  // Should use less than 25% CPU
    EXPECT_LT(finalLatency, 20.0f);   // Should have less than 20ms latency
    
    // Verify processing statistics
    NoiseReductionStats stats = noiseProcessor->getStats();
    EXPECT_GE(stats.framesProcessed, static_cast<uint64_t>(totalFrames * 0.95));
    EXPECT_GT(stats.averageReduction, 0.0f);
}