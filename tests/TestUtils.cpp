#include "TestUtils.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace quiet {
namespace test {

void TestUtils::generateSineWave(float* buffer, int numSamples, float frequency, 
                                float sampleRate, float amplitude, float phase) {
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = amplitude * std::sin(2.0f * M_PI * frequency * i / sampleRate + phase);
    }
}

void TestUtils::generateWhiteNoise(float* buffer, int numSamples, float amplitude, unsigned int seed) {
    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> distribution(-amplitude, amplitude);
    
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] = distribution(generator);
    }
}

void TestUtils::generatePinkNoise(float* buffer, int numSamples, float amplitude, unsigned int seed) {
    // Simple pink noise approximation using multiple octaves
    std::mt19937 generator(seed);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    
    std::fill(buffer, buffer + numSamples, 0.0f);
    
    // Add multiple octaves with decreasing amplitude
    for (int octave = 0; octave < 8; ++octave) {
        float octaveAmplitude = amplitude / std::pow(2.0f, octave * 0.5f);
        int stepSize = 1 << octave;
        
        for (int i = 0; i < numSamples; i += stepSize) {
            float value = distribution(generator) * octaveAmplitude;
            for (int j = 0; j < stepSize && i + j < numSamples; ++j) {
                buffer[i + j] += value;
            }
        }
    }
}

void TestUtils::addNoise(float* signal, int numSamples, float* noise, float noiseLevel) {
    for (int i = 0; i < numSamples; ++i) {
        signal[i] += noise[i] * noiseLevel;
    }
}

float TestUtils::calculateRMS(const float* buffer, int numSamples) {
    if (numSamples <= 0) return 0.0f;
    
    double sum = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        sum += buffer[i] * buffer[i];
    }
    
    return std::sqrt(sum / numSamples);
}

float TestUtils::calculateSNR(const float* signal, const float* noise, int numSamples) {
    float signalRMS = calculateRMS(signal, numSamples);
    float noiseRMS = calculateRMS(noise, numSamples);
    
    if (noiseRMS == 0.0f) {
        return 100.0f;  // Infinite SNR
    }
    
    return 20.0f * std::log10(signalRMS / noiseRMS);
}

float TestUtils::calculateTHD(const float* signal, int numSamples, float fundamentalFreq, float sampleRate) {
    // Simplified THD calculation
    // In a real implementation, you'd use FFT to analyze harmonics
    
    float fundamental = calculateRMS(signal, numSamples);
    
    // Estimate harmonic content by analyzing high-frequency components
    float harmonicSum = 0.0f;
    int harmonicCount = 0;
    
    // Simple high-pass filter to estimate harmonic content
    for (int i = 1; i < numSamples; ++i) {
        float diff = signal[i] - signal[i-1];
        harmonicSum += diff * diff;
        harmonicCount++;
    }
    
    float harmonicRMS = harmonicCount > 0 ? std::sqrt(harmonicSum / harmonicCount) : 0.0f;
    
    if (fundamental == 0.0f) {
        return 0.0f;
    }
    
    return (harmonicRMS / fundamental) * 100.0f;
}

bool TestUtils::compareBuffers(const float* buffer1, const float* buffer2, int numSamples, float tolerance) {
    for (int i = 0; i < numSamples; ++i) {
        if (std::abs(buffer1[i] - buffer2[i]) > tolerance) {
            return false;
        }
    }
    return true;
}

float TestUtils::findPeakAmplitude(const float* buffer, int numSamples) {
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        peak = std::max(peak, std::abs(buffer[i]));
    }
    return peak;
}

void TestUtils::normalizeBuffer(float* buffer, int numSamples, float targetLevel) {
    float peak = findPeakAmplitude(buffer, numSamples);
    if (peak > 0.0f) {
        float scale = targetLevel / peak;
        for (int i = 0; i < numSamples; ++i) {
            buffer[i] *= scale;
        }
    }
}

void TestUtils::applyFadeIn(float* buffer, int numSamples, int fadeLength) {
    int actualFadeLength = std::min(fadeLength, numSamples);
    for (int i = 0; i < actualFadeLength; ++i) {
        float gain = static_cast<float>(i) / actualFadeLength;
        buffer[i] *= gain;
    }
}

void TestUtils::applyFadeOut(float* buffer, int numSamples, int fadeLength) {
    int actualFadeLength = std::min(fadeLength, numSamples);
    int startPos = numSamples - actualFadeLength;
    
    for (int i = 0; i < actualFadeLength; ++i) {
        float gain = static_cast<float>(actualFadeLength - i) / actualFadeLength;
        buffer[startPos + i] *= gain;
    }
}

std::vector<float> TestUtils::generateChirp(int numSamples, float startFreq, float endFreq, 
                                           float sampleRate, float amplitude) {
    std::vector<float> chirp(numSamples);
    
    for (int i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float freq = startFreq + (endFreq - startFreq) * t * sampleRate / numSamples;
        chirp[i] = amplitude * std::sin(2.0f * M_PI * freq * t);
    }
    
    return chirp;
}

AudioTestSignal TestUtils::createTestSignal(TestSignalType type, int numSamples, float sampleRate) {
    AudioTestSignal signal;
    signal.samples.resize(numSamples);
    signal.sampleRate = sampleRate;
    signal.type = type;
    
    switch (type) {
        case TestSignalType::Sine440:
            generateSineWave(signal.samples.data(), numSamples, 440.0f, sampleRate, 0.8f);
            signal.description = "440Hz sine wave";
            break;
            
        case TestSignalType::Sine1000:
            generateSineWave(signal.samples.data(), numSamples, 1000.0f, sampleRate, 0.8f);
            signal.description = "1000Hz sine wave";
            break;
            
        case TestSignalType::WhiteNoise:
            generateWhiteNoise(signal.samples.data(), numSamples, 0.5f);
            signal.description = "White noise";
            break;
            
        case TestSignalType::PinkNoise:
            generatePinkNoise(signal.samples.data(), numSamples, 0.5f);
            signal.description = "Pink noise";
            break;
            
        case TestSignalType::Chirp:
            signal.samples = generateChirp(numSamples, 100.0f, 8000.0f, sampleRate, 0.8f);
            signal.description = "Frequency sweep 100Hz-8kHz";
            break;
            
        case TestSignalType::Silence:
            std::fill(signal.samples.begin(), signal.samples.end(), 0.0f);
            signal.description = "Silence";
            break;
            
        case TestSignalType::MultiTone:
            // Mix of multiple frequencies
            std::fill(signal.samples.begin(), signal.samples.end(), 0.0f);
            generateSineWave(signal.samples.data(), numSamples, 440.0f, sampleRate, 0.3f);
            
            std::vector<float> temp(numSamples);
            generateSineWave(temp.data(), numSamples, 880.0f, sampleRate, 0.2f);
            addNoise(signal.samples.data(), numSamples, temp.data(), 1.0f);
            
            generateSineWave(temp.data(), numSamples, 1320.0f, sampleRate, 0.1f);
            addNoise(signal.samples.data(), numSamples, temp.data(), 1.0f);
            
            signal.description = "Multi-tone (440, 880, 1320 Hz)";
            break;
    }
    
    return signal;
}

PerformanceTimer::PerformanceTimer() {
    reset();
}

void PerformanceTimer::reset() {
    m_startTime = std::chrono::high_resolution_clock::now();
}

double PerformanceTimer::getElapsedMilliseconds() const {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_startTime);
    return duration.count() / 1000.0;
}

double PerformanceTimer::getElapsedMicroseconds() const {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_startTime);
    return static_cast<double>(duration.count());
}

AudioQualityMetrics TestUtils::calculateQualityMetrics(const float* original, const float* processed, 
                                                      int numSamples, float sampleRate) {
    AudioQualityMetrics metrics;
    
    // Basic level measurements
    metrics.originalRMS = calculateRMS(original, numSamples);
    metrics.processedRMS = calculateRMS(processed, numSamples);
    
    // Calculate difference signal for noise/distortion analysis
    std::vector<float> difference(numSamples);
    for (int i = 0; i < numSamples; ++i) {
        difference[i] = processed[i] - original[i];
    }
    
    metrics.noiseFloor = calculateRMS(difference.data(), numSamples);
    
    // Signal-to-Noise Ratio
    if (metrics.noiseFloor > 0.0f) {
        metrics.snr = 20.0f * std::log10(metrics.processedRMS / metrics.noiseFloor);
    } else {
        metrics.snr = 100.0f;  // Perfect SNR
    }
    
    // Total Harmonic Distortion (simplified)
    metrics.thd = calculateTHD(processed, numSamples, 440.0f, sampleRate);
    
    // Dynamic range (simplified as peak-to-RMS ratio)
    float processedPeak = findPeakAmplitude(processed, numSamples);
    if (metrics.processedRMS > 0.0f) {
        metrics.dynamicRange = 20.0f * std::log10(processedPeak / metrics.processedRMS);
    } else {
        metrics.dynamicRange = 0.0f;
    }
    
    return metrics;
}

} // namespace test
} // namespace quiet