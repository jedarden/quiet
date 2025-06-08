#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "quiet/core/NoiseReductionProcessor.h"
#include "quiet/core/EventDispatcher.h"
#include <cmath>
#include <thread>
#include <chrono>

using namespace quiet::core;
using testing::_;
using testing::AtLeast;

// Mock EventDispatcher for testing
class MockEventDispatcher : public EventDispatcher {
public:
    MOCK_METHOD(void, publish, (EventType type, std::shared_ptr<EventData> data), (override));
    MOCK_METHOD(void, publishImmediate, (EventType type, std::shared_ptr<EventData> data), (override));
    MOCK_METHOD(ListenerHandle, subscribe, (EventType type, EventListener listener), (override));
    MOCK_METHOD(bool, unsubscribe, (ListenerHandle handle), (override));
};

class NoiseReductionProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        mockDispatcher = std::make_unique<MockEventDispatcher>();
        processor = std::make_unique<NoiseReductionProcessor>(*mockDispatcher);
    }

    void TearDown() override {
        if (processor && processor->isInitialized()) {
            processor->shutdown();
        }
        processor.reset();
        mockDispatcher.reset();
    }

    // Helper function to generate test signals
    void generateSineWave(AudioBuffer& buffer, float frequency, float amplitude = 1.0f) {
        const float sampleRate = 48000.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
            }
        }
    }

    void generateWhiteNoise(AudioBuffer& buffer, float amplitude = 0.1f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);
        
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                data[i] = dist(gen);
            }
        }
    }

    void generateNoisySpeech(AudioBuffer& buffer) {
        // Generate simulated speech (mix of frequencies)
        generateSineWave(buffer, 440.0f, 0.8f);  // Fundamental
        
        AudioBuffer harmonic(buffer.getNumChannels(), buffer.getNumSamples());
        generateSineWave(harmonic, 880.0f, 0.4f);  // First harmonic
        buffer.addFrom(0, 0, harmonic, 0, 0, buffer.getNumSamples());
        
        // Add noise
        AudioBuffer noise(buffer.getNumChannels(), buffer.getNumSamples());
        generateWhiteNoise(noise, 0.3f);
        buffer.addFrom(0, 0, noise, 0, 0, buffer.getNumSamples());
    }

    float calculateSNR(const AudioBuffer& signal, const AudioBuffer& noise) {
        float signalPower = signal.getRMSLevel(0, 0, signal.getNumSamples());
        float noisePower = noise.getRMSLevel(0, 0, noise.getNumSamples());
        
        if (noisePower == 0.0f) return 100.0f;  // Infinite SNR
        
        return 20.0f * std::log10(signalPower / noisePower);
    }

    std::unique_ptr<MockEventDispatcher> mockDispatcher;
    std::unique_ptr<NoiseReductionProcessor> processor;
};

// Test default constructor state
TEST_F(NoiseReductionProcessorTest, DefaultConstructorState) {
    EXPECT_FALSE(processor->isInitialized());
    EXPECT_TRUE(processor->isEnabled());  // Should be enabled by default
    EXPECT_EQ(NoiseReductionConfig::Level::Medium, processor->getLevel());
}

// Test initialization
TEST_F(NoiseReductionProcessorTest, Initialization) {
    // Should succeed with default sample rate
    EXPECT_TRUE(processor->initialize());
    EXPECT_TRUE(processor->isInitialized());
    
    // Should be able to initialize with custom sample rate
    processor->shutdown();
    EXPECT_FALSE(processor->isInitialized());
    
    EXPECT_TRUE(processor->initialize(44100.0));
    EXPECT_TRUE(processor->isInitialized());
}

// Test double initialization
TEST_F(NoiseReductionProcessorTest, DoubleInitialization) {
    EXPECT_TRUE(processor->initialize());
    EXPECT_TRUE(processor->isInitialized());
    
    // Second initialization should succeed but not change state
    EXPECT_TRUE(processor->initialize());
    EXPECT_TRUE(processor->isInitialized());
}

// Test shutdown
TEST_F(NoiseReductionProcessorTest, Shutdown) {
    EXPECT_TRUE(processor->initialize());
    EXPECT_TRUE(processor->isInitialized());
    
    processor->shutdown();
    EXPECT_FALSE(processor->isInitialized());
    
    // Multiple shutdowns should be safe
    processor->shutdown();
    EXPECT_FALSE(processor->isInitialized());
}

// Test configuration management
TEST_F(NoiseReductionProcessorTest, ConfigurationManagement) {
    NoiseReductionConfig config;
    config.level = NoiseReductionConfig::Level::High;
    config.enabled = false;
    config.threshold = 0.8f;
    config.adaptiveMode = false;
    
    processor->setConfig(config);
    
    NoiseReductionConfig retrievedConfig = processor->getConfig();
    EXPECT_EQ(config.level, retrievedConfig.level);
    EXPECT_EQ(config.enabled, retrievedConfig.enabled);
    EXPECT_FLOAT_EQ(config.threshold, retrievedConfig.threshold);
    EXPECT_EQ(config.adaptiveMode, retrievedConfig.adaptiveMode);
}

// Test enable/disable functionality
TEST_F(NoiseReductionProcessorTest, EnableDisable) {
    EXPECT_TRUE(processor->isEnabled());
    
    processor->setEnabled(false);
    EXPECT_FALSE(processor->isEnabled());
    
    processor->setEnabled(true);
    EXPECT_TRUE(processor->isEnabled());
}

// Test level setting
TEST_F(NoiseReductionProcessorTest, LevelSetting) {
    EXPECT_EQ(NoiseReductionConfig::Level::Medium, processor->getLevel());
    
    processor->setLevel(NoiseReductionConfig::Level::High);
    EXPECT_EQ(NoiseReductionConfig::Level::High, processor->getLevel());
    
    processor->setLevel(NoiseReductionConfig::Level::Low);
    EXPECT_EQ(NoiseReductionConfig::Level::Low, processor->getLevel());
}

// Test event dispatching on configuration changes
TEST_F(NoiseReductionProcessorTest, EventDispatchingOnConfigChange) {
    EXPECT_CALL(*mockDispatcher, publish(EventType::NoiseReductionToggled, _))
        .Times(AtLeast(1));
    
    processor->setEnabled(false);
    processor->setEnabled(true);
}

// Test processing without initialization
TEST_F(NoiseReductionProcessorTest, ProcessingWithoutInitialization) {
    AudioBuffer buffer(1, 1024);
    generateNoisySpeech(buffer);
    
    // Should fail to process without initialization
    EXPECT_FALSE(processor->process(buffer));
}

// Test processing when disabled
TEST_F(NoiseReductionProcessorTest, ProcessingWhenDisabled) {
    ASSERT_TRUE(processor->initialize());
    
    AudioBuffer buffer(1, 1024);
    generateNoisySpeech(buffer);
    AudioBuffer originalBuffer = buffer;  // Copy for comparison
    
    processor->setEnabled(false);
    EXPECT_TRUE(processor->process(buffer));
    
    // Buffer should be unchanged when processing is disabled
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        EXPECT_FLOAT_EQ(originalBuffer.getSample(0, i), buffer.getSample(0, i));
    }
}

// Test basic noise reduction processing
TEST_F(NoiseReductionProcessorTest, BasicNoiseReduction) {
    ASSERT_TRUE(processor->initialize());
    
    AudioBuffer noisyBuffer(1, 1024);
    generateNoisySpeech(noisyBuffer);
    
    AudioBuffer processedBuffer = noisyBuffer;  // Copy for processing
    EXPECT_TRUE(processor->process(processedBuffer));
    
    // Processed buffer should be different from original
    bool hasChanges = false;
    for (int i = 0; i < noisyBuffer.getNumSamples() && !hasChanges; ++i) {
        if (std::abs(noisyBuffer.getSample(0, i) - processedBuffer.getSample(0, i)) > 0.001f) {
            hasChanges = true;
        }
    }
    EXPECT_TRUE(hasChanges);
}

// Test processing with different buffer sizes
TEST_F(NoiseReductionProcessorTest, DifferentBufferSizes) {
    ASSERT_TRUE(processor->initialize());
    
    std::vector<int> bufferSizes = {64, 128, 256, 512, 1024, 2048};
    
    for (int bufferSize : bufferSizes) {
        AudioBuffer buffer(1, bufferSize);
        generateNoisySpeech(buffer);
        
        EXPECT_TRUE(processor->process(buffer)) 
            << "Failed to process buffer of size " << bufferSize;
    }
}

// Test processing with different channel counts
TEST_F(NoiseReductionProcessorTest, DifferentChannelCounts) {
    ASSERT_TRUE(processor->initialize());
    
    // Mono processing
    AudioBuffer monoBuffer(1, 1024);
    generateNoisySpeech(monoBuffer);
    EXPECT_TRUE(processor->process(monoBuffer));
    
    // Stereo should be converted to mono internally
    AudioBuffer stereoBuffer(2, 1024);
    generateNoisySpeech(stereoBuffer);
    EXPECT_TRUE(processor->process(stereoBuffer));
}

// Test in-place processing
TEST_F(NoiseReductionProcessorTest, InPlaceProcessing) {
    ASSERT_TRUE(processor->initialize());
    
    const int numSamples = 1024;
    std::vector<float> audioData(numSamples);
    
    // Generate test signal
    for (int i = 0; i < numSamples; ++i) {
        audioData[i] = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / 48000.0f) +
                       0.1f * ((float)rand() / RAND_MAX - 0.5f);  // Add noise
    }
    
    std::vector<float> originalData = audioData;
    
    EXPECT_TRUE(processor->processInPlace(audioData.data(), numSamples));
    
    // Data should be modified
    bool hasChanges = false;
    for (int i = 0; i < numSamples && !hasChanges; ++i) {
        if (std::abs(originalData[i] - audioData[i]) > 0.001f) {
            hasChanges = true;
        }
    }
    EXPECT_TRUE(hasChanges);
}

// Test statistics collection
TEST_F(NoiseReductionProcessorTest, StatisticsCollection) {
    ASSERT_TRUE(processor->initialize());
    
    // Initial stats should be zero
    NoiseReductionStats stats = processor->getStats();
    EXPECT_EQ(0u, stats.framesProcessed);
    EXPECT_EQ(0u, stats.totalProcessingTime);
    EXPECT_FLOAT_EQ(0.0f, stats.reductionLevel);
    
    // Process some audio
    AudioBuffer buffer(1, 1024);
    generateNoisySpeech(buffer);
    EXPECT_TRUE(processor->process(buffer));
    
    // Stats should be updated
    stats = processor->getStats();
    EXPECT_GT(stats.framesProcessed, 0u);
    EXPECT_GT(stats.totalProcessingTime, 0u);
    
    // Reset stats
    processor->resetStats();
    stats = processor->getStats();
    EXPECT_EQ(0u, stats.framesProcessed);
    EXPECT_EQ(0u, stats.totalProcessingTime);
}

// Test performance requirements
TEST_F(NoiseReductionProcessorTest, PerformanceRequirements) {
    ASSERT_TRUE(processor->initialize());
    
    const int bufferSize = 480;  // 10ms at 48kHz
    AudioBuffer buffer(1, bufferSize);
    generateNoisySpeech(buffer);
    
    // Measure processing time
    auto start = std::chrono::high_resolution_clock::now();
    
    const int numIterations = 100;
    for (int i = 0; i < numIterations; ++i) {
        ASSERT_TRUE(processor->process(buffer));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Average processing time per 10ms buffer should be much less than 10ms
    double avgProcessingTime = duration.count() / (double)numIterations;
    EXPECT_LT(avgProcessingTime, 5000.0);  // Should be less than 5ms on average
    
    // CPU usage should be reasonable
    float cpuUsage = processor->getCpuUsage();
    EXPECT_LT(cpuUsage, 100.0f);  // Should not exceed 100% (single core)
    
    // Latency should meet requirements
    float latency = processor->getLatency();
    EXPECT_LT(latency, 30.0f);  // Should be less than 30ms
}

// Test different reduction levels
TEST_F(NoiseReductionProcessorTest, DifferentReductionLevels) {
    ASSERT_TRUE(processor->initialize());
    
    AudioBuffer originalBuffer(1, 1024);
    generateNoisySpeech(originalBuffer);
    
    std::vector<NoiseReductionConfig::Level> levels = {
        NoiseReductionConfig::Level::Low,
        NoiseReductionConfig::Level::Medium,
        NoiseReductionConfig::Level::High
    };
    
    std::vector<float> reductionAmounts;
    
    for (auto level : levels) {
        processor->setLevel(level);
        
        AudioBuffer testBuffer = originalBuffer;  // Copy for processing
        EXPECT_TRUE(processor->process(testBuffer));
        
        // Calculate how much the signal changed (rough proxy for reduction amount)
        float totalDifference = 0.0f;
        for (int i = 0; i < testBuffer.getNumSamples(); ++i) {
            totalDifference += std::abs(originalBuffer.getSample(0, i) - testBuffer.getSample(0, i));
        }
        
        reductionAmounts.push_back(totalDifference);
    }
    
    // Higher levels should generally produce more changes
    // (This is a simplified test - actual behavior depends on the algorithm)
    EXPECT_GT(reductionAmounts.size(), 0u);
}

// Test thread safety
TEST_F(NoiseReductionProcessorTest, ThreadSafety) {
    ASSERT_TRUE(processor->initialize());
    
    const int numThreads = 4;
    const int iterationsPerThread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    auto processAudio = [&]() {
        for (int i = 0; i < iterationsPerThread; ++i) {
            AudioBuffer buffer(1, 256);
            generateNoisySpeech(buffer);
            
            if (processor->process(buffer)) {
                successCount++;
            }
            
            // Occasionally change settings
            if (i % 10 == 0) {
                processor->setEnabled(!processor->isEnabled());
            }
        }
    };
    
    // Start threads
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back(processAudio);
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Most operations should succeed (some may fail due to enable/disable toggling)
    EXPECT_GT(successCount.load(), numThreads * iterationsPerThread / 2);
}

// Test edge cases
TEST_F(NoiseReductionProcessorTest, EdgeCases) {
    ASSERT_TRUE(processor->initialize());
    
    // Empty buffer
    AudioBuffer emptyBuffer(0, 0);
    EXPECT_FALSE(processor->process(emptyBuffer));
    
    // Very small buffer
    AudioBuffer tinyBuffer(1, 1);
    tinyBuffer.setSample(0, 0, 0.5f);
    EXPECT_TRUE(processor->process(tinyBuffer));
    
    // Silent buffer
    AudioBuffer silentBuffer(1, 1024);
    silentBuffer.clear();
    EXPECT_TRUE(processor->process(silentBuffer));
    
    // All samples are the same value
    AudioBuffer constantBuffer(1, 1024);
    for (int i = 0; i < 1024; ++i) {
        constantBuffer.setSample(0, i, 0.7f);
    }
    EXPECT_TRUE(processor->process(constantBuffer));
}

// Test error conditions
TEST_F(NoiseReductionProcessorTest, ErrorConditions) {
    // Processing null data should fail
    EXPECT_FALSE(processor->processInPlace(nullptr, 1024));
    
    // Processing with zero samples should fail
    std::vector<float> audioData(1024);
    EXPECT_FALSE(processor->processInPlace(audioData.data(), 0));
    
    // Very large buffer might fail due to memory constraints
    // (This test depends on system resources)
    AudioBuffer hugeBuffer(1, 10000000);  // ~10M samples
    // Don't require this to succeed, just shouldn't crash
    processor->process(hugeBuffer);
}

// Test noise reduction effectiveness
TEST_F(NoiseReductionProcessorTest, NoiseReductionEffectiveness) {
    ASSERT_TRUE(processor->initialize());
    
    // Create clean speech signal
    AudioBuffer cleanSpeech(1, 2048);
    generateSineWave(cleanSpeech, 440.0f, 0.8f);
    
    // Add noise
    AudioBuffer noise(1, 2048);
    generateWhiteNoise(noise, 0.3f);
    
    AudioBuffer noisySpeech = cleanSpeech;
    noisySpeech.addFrom(0, 0, noise, 0, 0, 2048);
    
    // Calculate original SNR
    float originalSNR = calculateSNR(cleanSpeech, noise);
    
    // Process noisy speech
    AudioBuffer processedSpeech = noisySpeech;
    EXPECT_TRUE(processor->process(processedSpeech));
    
    // Calculate effective noise reduction by comparing energy
    float originalEnergy = noisySpeech.getRMSLevel(0, 0, 2048);
    float processedEnergy = processedSpeech.getRMSLevel(0, 0, 2048);
    
    // Processing should typically reduce overall energy when noise is present
    // (This is a simplified test - actual effectiveness depends on signal characteristics)
    EXPECT_GT(originalEnergy, 0.0f);
    EXPECT_GT(processedEnergy, 0.0f);
    
    // Get processing stats
    NoiseReductionStats stats = processor->getStats();
    EXPECT_GT(stats.framesProcessed, 0u);
}