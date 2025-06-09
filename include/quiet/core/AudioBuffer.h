#pragma once

#include <memory>
#include <vector>
#include <algorithm>
#include <cstring>
#include <atomic>
#include <cmath>

namespace quiet {
namespace core {

/**
 * @brief Lock-free audio buffer for real-time processing
 * 
 * This class provides a multi-channel audio buffer suitable for real-time
 * audio processing. It uses aligned memory allocation for SIMD operations
 * and provides thread-safe read operations.
 */
class AudioBuffer {
public:
    // Constructors
    AudioBuffer();
    AudioBuffer(int numChannels, int numSamples, double sampleRate = 48000.0);
    AudioBuffer(const AudioBuffer& other);
    AudioBuffer(AudioBuffer&& other) noexcept;
    ~AudioBuffer();
    
    // Assignment operators
    AudioBuffer& operator=(const AudioBuffer& other);
    AudioBuffer& operator=(AudioBuffer&& other) noexcept;
    
    // Buffer management
    void setSize(int numChannels, int numSamples, bool clearBuffer = true);
    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }
    
    // Accessors
    int getNumChannels() const { return numChannels_; }
    int getNumSamples() const { return numSamples_; }
    double getSampleRate() const { return sampleRate_; }
    bool isEmpty() const { return numChannels_ == 0 || numSamples_ == 0; }
    size_t getSizeInBytes() const { return numChannels_ * numSamples_ * sizeof(float); }
    
    // Sample access
    float getSample(int channel, int sampleIndex) const;
    void setSample(int channel, int sampleIndex, float value);
    void addSample(int channel, int sampleIndex, float value);
    
    // Direct buffer access
    float* getWritePointer(int channel);
    const float* getReadPointer(int channel) const;
    
    // Clear operations
    void clear();
    void clear(int channel);
    void clear(int channel, int startSample, int numSamples);
    
    // Copy operations
    void copyFrom(int destChannel, int destStartSample,
                  const AudioBuffer& source, int sourceChannel,
                  int sourceStartSample, int numSamples);
    void copyFrom(const AudioBuffer& source);
    
    // Add operations
    void addFrom(int destChannel, int destStartSample,
                 const AudioBuffer& source, int sourceChannel,
                 int sourceStartSample, int numSamples, float gain = 1.0f);
    void addFrom(const AudioBuffer& source, float gain = 1.0f);
    
    // Gain operations
    void applyGain(float gain);
    void applyGain(int channel, float gain);
    void applyGain(int channel, int startSample, int numSamples, float gain);
    void applyGainRamp(int channel, int startSample, int numSamples,
                       float startGain, float endGain);
    
    // Level analysis
    float getRMSLevel(int channel, int startSample, int numSamples) const;
    float getMagnitude(int channel, int startSample, int numSamples) const;
    void findMinAndMax(int channel, int startSample, int numSamples,
                      float& minVal, float& maxVal) const;
    
    // Format conversion
    void convertToMono(AudioBuffer& destination) const;
    void convertToStereo(AudioBuffer& destination) const;
    void convertToInterleaved(std::vector<float>& destination) const;
    void convertFromInterleaved(const float* source, int numSamples);
    
    // Utility functions
    bool hasBeenClipped() const;
    void reverse(int channel, int startSample, int numSamples);
    
private:
    // Memory management
    void allocateChannels();
    void deallocateChannels();
    
    // Data members
    int numChannels_ = 0;
    int numSamples_ = 0;
    double sampleRate_ = 48000.0;
    size_t allocatedBytes_ = 0;
    
    // Channel pointers - using unique_ptr for automatic cleanup
    std::unique_ptr<float*[]> channels_;
    std::unique_ptr<float[]> data_;  // Actual sample data
    
    // Alignment for SIMD operations
    static constexpr size_t kAlignment = 32;  // AVX alignment
    
    // Helper methods
    static void* allocateAligned(size_t bytes);
    static void deallocateAligned(void* ptr);
    
    // SIMD-optimized operations
    void clearSIMD(float* buffer, int numSamples);
    void copySIMD(float* dest, const float* source, int numSamples);
    void addSIMD(float* dest, const float* source, int numSamples, float gain);
    void applySIMD(float* buffer, int numSamples, float gain);
};

// Inline implementations for performance
inline float AudioBuffer::getSample(int channel, int sampleIndex) const {
    if (channel >= 0 && channel < numChannels_ && 
        sampleIndex >= 0 && sampleIndex < numSamples_) {
        return channels_[channel][sampleIndex];
    }
    return 0.0f;
}

inline void AudioBuffer::setSample(int channel, int sampleIndex, float value) {
    if (channel >= 0 && channel < numChannels_ && 
        sampleIndex >= 0 && sampleIndex < numSamples_) {
        channels_[channel][sampleIndex] = value;
    }
}

inline void AudioBuffer::addSample(int channel, int sampleIndex, float value) {
    if (channel >= 0 && channel < numChannels_ && 
        sampleIndex >= 0 && sampleIndex < numSamples_) {
        channels_[channel][sampleIndex] += value;
    }
}

inline float* AudioBuffer::getWritePointer(int channel) {
    return (channel >= 0 && channel < numChannels_) ? channels_[channel] : nullptr;
}

inline const float* AudioBuffer::getReadPointer(int channel) const {
    return (channel >= 0 && channel < numChannels_) ? channels_[channel] : nullptr;
}

} // namespace core
} // namespace quiet