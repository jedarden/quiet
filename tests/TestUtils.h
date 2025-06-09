#ifndef QUIET_TEST_UTILS_H
#define QUIET_TEST_UTILS_H

#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <cmath>
#include <memory>
#include <algorithm>
#include <cassert>

namespace quiet {
namespace test {

/**
 * @brief Types of test signals for audio testing
 */
enum class TestSignalType {
    Sine440,     // 440Hz sine wave
    Sine1000,    // 1000Hz sine wave
    WhiteNoise,  // White noise
    PinkNoise,   // Pink noise
    Chirp,       // Frequency sweep
    Silence,     // Silent signal
    MultiTone    // Multiple sine waves
};

/**
 * @brief Test signal data structure
 */
struct AudioTestSignal {
    std::vector<float> samples;
    float sampleRate;
    TestSignalType type;
    std::string description;
};

/**
 * @brief Audio quality metrics for testing
 */
struct AudioQualityMetrics {
    float originalRMS;      // RMS level of original signal
    float processedRMS;     // RMS level of processed signal
    float snr;              // Signal-to-noise ratio (dB)
    float thd;              // Total harmonic distortion (%)
    float noiseFloor;       // Background noise level
    float dynamicRange;     // Dynamic range (dB)
};

/**
 * @brief Utility class for audio testing functions
 */
class TestUtils {
public:
    // Signal generation - TDD first implementations
    static void generateSineWave(float* buffer, int numSamples, float frequency, 
                                float sampleRate, float amplitude = 1.0f, float phase = 0.0f) {
        assert(buffer != nullptr);
        assert(numSamples > 0);
        assert(frequency > 0 && frequency < sampleRate / 2); // Nyquist
        assert(amplitude >= 0 && amplitude <= 1.0f);
        
        const float omega = 2.0f * M_PI * frequency / sampleRate;
        for (int i = 0; i < numSamples; ++i) {
            buffer[i] = amplitude * std::sin(omega * i + phase);
        }
    }
    
    static void generateWhiteNoise(float* buffer, int numSamples, float amplitude = 1.0f, 
                                  unsigned int seed = 12345) {
        assert(buffer != nullptr);
        assert(numSamples > 0);
        assert(amplitude >= 0 && amplitude <= 1.0f);
        
        std::mt19937 gen(seed);
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);
        
        for (int i = 0; i < numSamples; ++i) {
            buffer[i] = dist(gen);
        }
    }
    
    static AudioTestSignal createTestSignal(TestSignalType type, int numSamples, float sampleRate = 48000.0f) {
        AudioTestSignal signal;
        signal.samples.resize(numSamples);
        signal.sampleRate = sampleRate;
        signal.type = type;
        
        switch (type) {
            case TestSignalType::Sine440:
                generateSineWave(signal.samples.data(), numSamples, 440.0f, sampleRate);
                signal.description = "440Hz sine wave";
                break;
            case TestSignalType::Sine1000:
                generateSineWave(signal.samples.data(), numSamples, 1000.0f, sampleRate);
                signal.description = "1000Hz sine wave";
                break;
            case TestSignalType::WhiteNoise:
                generateWhiteNoise(signal.samples.data(), numSamples);
                signal.description = "White noise";
                break;
            case TestSignalType::Silence:
                std::fill(signal.samples.begin(), signal.samples.end(), 0.0f);
                signal.description = "Silence";
                break;
            default:
                signal.description = "Unknown signal type";
                break;
        }
        
        return signal;
    }
    
    // Signal analysis - TDD implementations
    static float calculateRMS(const float* buffer, int numSamples) {
        assert(buffer != nullptr);
        assert(numSamples > 0);
        
        float sum = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            sum += buffer[i] * buffer[i];
        }
        
        return std::sqrt(sum / numSamples);
    }
    
    static float calculateSNR(const float* signal, const float* noise, int numSamples) {
        assert(signal != nullptr && noise != nullptr);
        assert(numSamples > 0);
        
        float signalPower = 0.0f;
        float noisePower = 0.0f;
        
        for (int i = 0; i < numSamples; ++i) {
            signalPower += signal[i] * signal[i];
            noisePower += noise[i] * noise[i];
        }
        
        if (noisePower == 0.0f) return INFINITY;
        
        return 10.0f * std::log10(signalPower / noisePower);
    }
    
    static float findPeakAmplitude(const float* buffer, int numSamples) {
        assert(buffer != nullptr);
        assert(numSamples > 0);
        
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            peak = std::max(peak, std::abs(buffer[i]));
        }
        
        return peak;
    }
    
    // Signal modification
    static void addNoise(float* signal, int numSamples, float* noise, float noiseLevel) {
        assert(signal != nullptr && noise != nullptr);
        assert(numSamples > 0);
        assert(noiseLevel >= 0 && noiseLevel <= 1.0f);
        
        for (int i = 0; i < numSamples; ++i) {
            signal[i] = signal[i] * (1.0f - noiseLevel) + noise[i] * noiseLevel;
        }
    }
    
    // Comparison and validation
    static bool compareBuffers(const float* buffer1, const float* buffer2, int numSamples, 
                              float tolerance = 0.001f) {
        assert(buffer1 != nullptr && buffer2 != nullptr);
        assert(numSamples > 0);
        assert(tolerance >= 0);
        
        for (int i = 0; i < numSamples; ++i) {
            if (std::abs(buffer1[i] - buffer2[i]) > tolerance) {
                return false;
            }
        }
        
        return true;
    }
};

/**
 * @brief High-precision timer for performance testing
 */
class PerformanceTimer {
public:
    PerformanceTimer();
    
    void reset();
    double getElapsedMilliseconds() const;
    double getElapsedMicroseconds() const;
    
private:
    std::chrono::high_resolution_clock::time_point m_startTime;
};

// Mock classes for testing
class MockAudioDevice {
public:
    MockAudioDevice(const std::string& id, const std::string& name) 
        : deviceId(id), deviceName(name), isOpen(false) {}
    
    bool open() { isOpen = true; return true; }
    void close() { isOpen = false; }
    bool isDeviceOpen() const { return isOpen; }
    
    std::string getDeviceId() const { return deviceId; }
    std::string getDeviceName() const { return deviceName; }
    
private:
    std::string deviceId;
    std::string deviceName;
    bool isOpen;
};

} // namespace test
} // namespace quiet

#endif // QUIET_TEST_UTILS_H