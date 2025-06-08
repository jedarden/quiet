#pragma once

#include <vector>
#include <string>
#include <chrono>

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
    // Signal generation
    static void generateSineWave(float* buffer, int numSamples, float frequency, 
                                float sampleRate, float amplitude = 1.0f, float phase = 0.0f);
    
    static void generateWhiteNoise(float* buffer, int numSamples, float amplitude = 1.0f, 
                                  unsigned int seed = 12345);
    
    static void generatePinkNoise(float* buffer, int numSamples, float amplitude = 1.0f, 
                                 unsigned int seed = 12345);
    
    static std::vector<float> generateChirp(int numSamples, float startFreq, float endFreq, 
                                           float sampleRate, float amplitude = 1.0f);
    
    static AudioTestSignal createTestSignal(TestSignalType type, int numSamples, float sampleRate = 48000.0f);
    
    // Signal analysis
    static float calculateRMS(const float* buffer, int numSamples);
    static float calculateSNR(const float* signal, const float* noise, int numSamples);
    static float calculateTHD(const float* signal, int numSamples, float fundamentalFreq, float sampleRate);
    static float findPeakAmplitude(const float* buffer, int numSamples);
    
    // Signal modification
    static void addNoise(float* signal, int numSamples, float* noise, float noiseLevel);
    static void normalizeBuffer(float* buffer, int numSamples, float targetLevel = 1.0f);
    static void applyFadeIn(float* buffer, int numSamples, int fadeLength);
    static void applyFadeOut(float* buffer, int numSamples, int fadeLength);
    
    // Comparison and validation
    static bool compareBuffers(const float* buffer1, const float* buffer2, int numSamples, 
                              float tolerance = 0.001f);
    
    static AudioQualityMetrics calculateQualityMetrics(const float* original, const float* processed, 
                                                       int numSamples, float sampleRate);
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

} // namespace test
} // namespace quiet