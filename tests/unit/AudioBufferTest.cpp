#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "quiet/core/AudioBuffer.h"
#include <cmath>
#include <vector>

using namespace quiet::core;
using testing::FloatEq;
using testing::FloatNear;

class AudioBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup for tests
    }

    void TearDown() override {
        // Cleanup after tests
    }
    
    // Helper function to generate test sine wave
    void generateSineWave(float* buffer, int numSamples, float frequency, float sampleRate, float amplitude = 1.0f) {
        for (int i = 0; i < numSamples; ++i) {
            buffer[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate);
        }
    }
    
    // Helper function to verify buffer contains zeros
    void expectBufferIsZero(const AudioBuffer& buffer, int channel = 0) {
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            EXPECT_FLOAT_EQ(0.0f, buffer.getSample(channel, i)) 
                << "Sample " << i << " in channel " << channel << " is not zero";
        }
    }
};

// Test default constructor
TEST_F(AudioBufferTest, DefaultConstructor) {
    AudioBuffer buffer;
    
    EXPECT_EQ(0, buffer.getNumChannels());
    EXPECT_EQ(0, buffer.getNumSamples());
    EXPECT_TRUE(buffer.isEmpty());
    EXPECT_EQ(48000.0, buffer.getSampleRate());  // Default sample rate
}

// Test parameterized constructor
TEST_F(AudioBufferTest, ParameterizedConstructor) {
    const int numChannels = 2;
    const int numSamples = 1024;
    const double sampleRate = 44100.0;
    
    AudioBuffer buffer(numChannels, numSamples, sampleRate);
    
    EXPECT_EQ(numChannels, buffer.getNumChannels());
    EXPECT_EQ(numSamples, buffer.getNumSamples());
    EXPECT_EQ(sampleRate, buffer.getSampleRate());
    EXPECT_FALSE(buffer.isEmpty());
    
    // Buffer should be initialized to zeros
    for (int ch = 0; ch < numChannels; ++ch) {
        expectBufferIsZero(buffer, ch);
    }
}

// Test copy constructor
TEST_F(AudioBufferTest, CopyConstructor) {
    AudioBuffer original(2, 512, 48000.0);
    
    // Fill with test data
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            original.setSample(ch, i, static_cast<float>(ch * 100 + i));
        }
    }
    
    AudioBuffer copy(original);
    
    EXPECT_EQ(original.getNumChannels(), copy.getNumChannels());
    EXPECT_EQ(original.getNumSamples(), copy.getNumSamples());
    EXPECT_EQ(original.getSampleRate(), copy.getSampleRate());
    
    // Verify data is copied correctly
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            EXPECT_FLOAT_EQ(original.getSample(ch, i), copy.getSample(ch, i));
        }
    }
}

// Test move constructor
TEST_F(AudioBufferTest, MoveConstructor) {
    AudioBuffer original(1, 256, 44100.0);
    original.setSample(0, 100, 0.5f);
    
    AudioBuffer moved(std::move(original));
    
    EXPECT_EQ(1, moved.getNumChannels());
    EXPECT_EQ(256, moved.getNumSamples());
    EXPECT_EQ(44100.0, moved.getSampleRate());
    EXPECT_FLOAT_EQ(0.5f, moved.getSample(0, 100));
    
    // Original should be in valid but unspecified state
    EXPECT_TRUE(original.isEmpty());
}

// Test setSize method
TEST_F(AudioBufferTest, SetSize) {
    AudioBuffer buffer;
    
    buffer.setSize(2, 1024);
    
    EXPECT_EQ(2, buffer.getNumChannels());
    EXPECT_EQ(1024, buffer.getNumSamples());
    EXPECT_FALSE(buffer.isEmpty());
    
    // Should be cleared by default
    expectBufferIsZero(buffer, 0);
    expectBufferIsZero(buffer, 1);
}

// Test setSample and getSample
TEST_F(AudioBufferTest, SampleAccess) {
    AudioBuffer buffer(2, 100);
    
    // Set some test values
    buffer.setSample(0, 50, 0.7f);
    buffer.setSample(1, 25, -0.3f);
    
    EXPECT_FLOAT_EQ(0.7f, buffer.getSample(0, 50));
    EXPECT_FLOAT_EQ(-0.3f, buffer.getSample(1, 25));
    EXPECT_FLOAT_EQ(0.0f, buffer.getSample(0, 0));  // Should still be zero
}

// Test addSample method
TEST_F(AudioBufferTest, AddSample) {
    AudioBuffer buffer(1, 10);
    
    buffer.setSample(0, 5, 0.3f);
    buffer.addSample(0, 5, 0.2f);
    
    EXPECT_FLOAT_EQ(0.5f, buffer.getSample(0, 5));
}

// Test clear methods
TEST_F(AudioBufferTest, ClearMethods) {
    AudioBuffer buffer(2, 100);
    
    // Fill with test data
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 100; ++i) {
            buffer.setSample(ch, i, 1.0f);
        }
    }
    
    // Clear entire buffer
    buffer.clear();
    expectBufferIsZero(buffer, 0);
    expectBufferIsZero(buffer, 1);
    
    // Fill again and clear single channel
    buffer.setSample(0, 50, 1.0f);
    buffer.setSample(1, 50, 1.0f);
    buffer.clear(0);
    
    EXPECT_FLOAT_EQ(0.0f, buffer.getSample(0, 50));
    EXPECT_FLOAT_EQ(1.0f, buffer.getSample(1, 50));
    
    // Clear partial range
    buffer.setSample(1, 25, 1.0f);
    buffer.setSample(1, 75, 1.0f);
    buffer.clear(1, 20, 10);  // Clear samples 20-29
    
    EXPECT_FLOAT_EQ(1.0f, buffer.getSample(1, 25));  // Should be cleared
    EXPECT_FLOAT_EQ(1.0f, buffer.getSample(1, 75));  // Should remain
}

// Test copyFrom methods
TEST_F(AudioBufferTest, CopyFrom) {
    AudioBuffer source(1, 100);
    AudioBuffer dest(1, 100);
    
    // Generate test sine wave in source
    generateSineWave(source.getWritePointer(0), 100, 440.0f, 48000.0f);
    
    // Copy entire channel
    dest.copyFrom(0, 0, source, 0, 0, 100);
    
    // Verify copy
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(source.getSample(0, i), dest.getSample(0, i));
    }
    
    // Test partial copy
    dest.clear();
    dest.copyFrom(0, 10, source, 0, 20, 30);  // Copy 30 samples from pos 20 to pos 10
    
    for (int i = 0; i < 30; ++i) {
        EXPECT_FLOAT_EQ(source.getSample(0, 20 + i), dest.getSample(0, 10 + i));
    }
    
    // Other samples should remain zero
    EXPECT_FLOAT_EQ(0.0f, dest.getSample(0, 5));
    EXPECT_FLOAT_EQ(0.0f, dest.getSample(0, 45));
}

// Test addFrom methods
TEST_F(AudioBufferTest, AddFrom) {
    AudioBuffer buffer1(1, 10);
    AudioBuffer buffer2(1, 10);
    
    // Fill buffers with test data
    for (int i = 0; i < 10; ++i) {
        buffer1.setSample(0, i, 0.3f);
        buffer2.setSample(0, i, 0.2f);
    }
    
    buffer1.addFrom(0, 0, buffer2, 0, 0, 10);
    
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(0.5f, buffer1.getSample(0, i));
    }
}

// Test apply gain methods
TEST_F(AudioBufferTest, ApplyGain) {
    AudioBuffer buffer(2, 10);
    
    // Fill with test data
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 10; ++i) {
            buffer.setSample(ch, i, 0.5f);
        }
    }
    
    // Apply gain to entire buffer
    buffer.applyGain(2.0f);
    
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 10; ++i) {
            EXPECT_FLOAT_EQ(1.0f, buffer.getSample(ch, i));
        }
    }
    
    // Apply gain to single channel
    buffer.applyGain(0, 0.5f);
    
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(0.5f, buffer.getSample(0, i));
        EXPECT_FLOAT_EQ(1.0f, buffer.getSample(1, i));
    }
}

// Test level analysis methods
TEST_F(AudioBufferTest, LevelAnalysis) {
    AudioBuffer buffer(1, 100);
    
    // Generate test sine wave (amplitude 0.5)
    generateSineWave(buffer.getWritePointer(0), 100, 440.0f, 48000.0f, 0.5f);
    
    float rms = buffer.getRMSLevel(0, 0, 100);
    EXPECT_NEAR(0.5f / std::sqrt(2.0f), rms, 0.01f);  // RMS of sine wave
    
    float magnitude = buffer.getMagnitude(0, 0, 100);
    EXPECT_NEAR(0.5f, magnitude, 0.01f);  // Peak magnitude
    
    float minVal, maxVal;
    buffer.findMinAndMax(0, 0, 100, minVal, maxVal);
    EXPECT_NEAR(-0.5f, minVal, 0.01f);
    EXPECT_NEAR(0.5f, maxVal, 0.01f);
}

// Test format conversion
TEST_F(AudioBufferTest, FormatConversion) {
    AudioBuffer stereoBuffer(2, 10);
    
    // Fill with different values per channel
    for (int i = 0; i < 10; ++i) {
        stereoBuffer.setSample(0, i, 0.3f);  // Left
        stereoBuffer.setSample(1, i, 0.7f);  // Right
    }
    
    // Convert to mono
    AudioBuffer monoBuffer;
    stereoBuffer.convertToMono(monoBuffer);
    
    EXPECT_EQ(1, monoBuffer.getNumChannels());
    EXPECT_EQ(10, monoBuffer.getNumSamples());
    
    // Should be average of both channels
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(0.5f, monoBuffer.getSample(0, i));
    }
    
    // Convert back to stereo
    AudioBuffer newStereoBuffer;
    monoBuffer.convertToStereo(newStereoBuffer);
    
    EXPECT_EQ(2, newStereoBuffer.getNumChannels());
    EXPECT_EQ(10, newStereoBuffer.getNumSamples());
    
    // Both channels should be identical
    for (int i = 0; i < 10; ++i) {
        EXPECT_FLOAT_EQ(0.5f, newStereoBuffer.getSample(0, i));
        EXPECT_FLOAT_EQ(0.5f, newStereoBuffer.getSample(1, i));
    }
}

// Test interleaved conversion
TEST_F(AudioBufferTest, InterleavedConversion) {
    AudioBuffer buffer(2, 4);
    
    // Set test pattern
    buffer.setSample(0, 0, 1.0f); buffer.setSample(1, 0, 2.0f);
    buffer.setSample(0, 1, 3.0f); buffer.setSample(1, 1, 4.0f);
    buffer.setSample(0, 2, 5.0f); buffer.setSample(1, 2, 6.0f);
    buffer.setSample(0, 3, 7.0f); buffer.setSample(1, 3, 8.0f);
    
    std::vector<float> interleavedData;
    buffer.convertToInterleaved(interleavedData);
    
    ASSERT_EQ(8u, interleavedData.size());
    
    // Check interleaved pattern: L0, R0, L1, R1, L2, R2, L3, R3
    EXPECT_FLOAT_EQ(1.0f, interleavedData[0]);
    EXPECT_FLOAT_EQ(2.0f, interleavedData[1]);
    EXPECT_FLOAT_EQ(3.0f, interleavedData[2]);
    EXPECT_FLOAT_EQ(4.0f, interleavedData[3]);
    EXPECT_FLOAT_EQ(5.0f, interleavedData[4]);
    EXPECT_FLOAT_EQ(6.0f, interleavedData[5]);
    EXPECT_FLOAT_EQ(7.0f, interleavedData[6]);
    EXPECT_FLOAT_EQ(8.0f, interleavedData[7]);
    
    // Convert back
    AudioBuffer newBuffer(2, 4);
    newBuffer.convertFromInterleaved(interleavedData.data(), 4);
    
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 4; ++i) {
            EXPECT_FLOAT_EQ(buffer.getSample(ch, i), newBuffer.getSample(ch, i));
        }
    }
}

// Test edge cases and error conditions
TEST_F(AudioBufferTest, EdgeCases) {
    AudioBuffer buffer;
    
    // Operations on empty buffer should not crash
    EXPECT_NO_THROW(buffer.clear());
    EXPECT_NO_THROW(buffer.applyGain(2.0f));
    EXPECT_EQ(0u, buffer.getSizeInBytes());
    
    // Large buffer allocation
    AudioBuffer largeBuffer(8, 48000);  // 8 channels, 1 second at 48kHz
    EXPECT_EQ(8, largeBuffer.getNumChannels());
    EXPECT_EQ(48000, largeBuffer.getNumSamples());
    EXPECT_FALSE(largeBuffer.isEmpty());
}

// Performance test for SIMD operations
TEST_F(AudioBufferTest, PerformanceTest) {
    const int numSamples = 48000;  // 1 second at 48kHz
    AudioBuffer buffer(2, numSamples);
    
    // Time the clear operation
    auto start = std::chrono::high_resolution_clock::now();
    buffer.clear();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete within reasonable time (< 1ms for clear operation)
    EXPECT_LT(duration.count(), 1000);
}