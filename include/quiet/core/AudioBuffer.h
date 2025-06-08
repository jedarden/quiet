#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <cstring>

namespace quiet {
namespace core {

/**
 * @brief Thread-safe audio buffer for real-time audio processing
 * 
 * This class provides:
 * - Lock-free operations for real-time audio threads
 * - Automatic memory management
 * - Format conversion utilities
 * - Peak/RMS level calculation
 * - SIMD-optimized operations where available
 */
class AudioBuffer {
public:
    AudioBuffer();
    AudioBuffer(int numChannels, int numSamples, double sampleRate = 48000.0);
    AudioBuffer(const AudioBuffer& other);
    AudioBuffer(AudioBuffer&& other) noexcept;
    
    AudioBuffer& operator=(const AudioBuffer& other);
    AudioBuffer& operator=(AudioBuffer&& other) noexcept;
    
    ~AudioBuffer();

    // Buffer management
    void setSize(int numChannels, int numSamples, bool clearExistingContent = true);
    void clear();
    void clear(int channel);
    void clear(int channel, int startSample, int numSamples);

    // Accessors
    int getNumChannels() const { return m_numChannels; }
    int getNumSamples() const { return m_numSamples; }
    double getSampleRate() const { return m_sampleRate; }
    void setSampleRate(double sampleRate) { m_sampleRate = sampleRate; }
    
    // Data access
    float* getWritePointer(int channel);
    const float* getReadPointer(int channel) const;
    float** getArrayOfWritePointers();
    const float* const* getArrayOfReadPointers() const;
    
    float getSample(int channel, int sample) const;
    void setSample(int channel, int sample, float value);
    void addSample(int channel, int sample, float value);

    // Audio operations
    void copyFrom(int destChannel, int destStartSample, 
                  const AudioBuffer& source, int sourceChannel, 
                  int sourceStartSample, int numSamples);
    
    void copyFrom(int destChannel, int destStartSample,
                  const float* source, int numSamples);
    
    void addFrom(int destChannel, int destStartSample,
                 const AudioBuffer& source, int sourceChannel,
                 int sourceStartSample, int numSamples);
    
    void addFrom(int destChannel, int destStartSample,
                 const float* source, int numSamples);
    
    void addFromWithMultiply(int destChannel, int destStartSample,
                            const float* source, int numSamples, float gain);
    
    void applyGain(float gain);
    void applyGain(int channel, float gain);
    void applyGain(int channel, int startSample, int numSamples, float gain);
    
    void applyGainRamp(int channel, int startSample, int numSamples,
                       float startGain, float endGain);

    // Level analysis
    float getMagnitude(int channel, int startSample, int numSamples) const;
    float getMagnitude(int startSample, int numSamples) const;
    float getRMSLevel(int channel, int startSample, int numSamples) const;
    float getRMSLevel(int startSample, int numSamples) const;
    
    float findMinimum(int channel, int startSample, int numSamples) const;
    float findMaximum(int channel, int startSample, int numSamples) const;
    void findMinAndMax(int channel, int startSample, int numSamples,
                       float& minValue, float& maxValue) const;

    // Format conversion
    void convertToInterleaved(std::vector<float>& interleavedData) const;
    void convertFromInterleaved(const float* interleavedData, int numSamples);
    
    void convertToMono(AudioBuffer& monoBuffer) const;
    void convertToStereo(AudioBuffer& stereoBuffer) const;

    // Utility methods
    bool isEmpty() const { return m_numChannels == 0 || m_numSamples == 0; }
    size_t getSizeInBytes() const;
    
    void reverse();
    void reverse(int channel);
    void reverse(int channel, int startSample, int numSamples);

private:
    // Memory management
    void allocateMemory();
    void deallocateMemory();
    void copyData(const AudioBuffer& other);

    // SIMD-optimized operations (when available)
    void clearSIMD(float* data, int numSamples);
    void copySIMD(float* dest, const float* src, int numSamples);
    void addSIMD(float* dest, const float* src, int numSamples);
    void multiplySIMD(float* data, int numSamples, float gain);
    float getMagnitudeSIMD(const float* data, int numSamples) const;
    float getRMSLevelSIMD(const float* data, int numSamples) const;

    // Member variables
    int m_numChannels{0};
    int m_numSamples{0};
    double m_sampleRate{48000.0};
    
    float** m_channels{nullptr};
    float* m_allocatedMemory{nullptr};
    size_t m_allocatedSize{0};
    
    mutable float** m_tempChannelPointers{nullptr};
    mutable int m_tempChannelPointersSize{0};
};

/**
 * @brief Lock-free ring buffer for audio data
 * 
 * Thread-safe circular buffer optimized for real-time audio applications.
 * Uses atomic operations to ensure lock-free operation between producer
 * and consumer threads.
 */
class AudioRingBuffer {
public:
    AudioRingBuffer(int numChannels, int numSamples);
    ~AudioRingBuffer();

    // Buffer operations
    bool write(const AudioBuffer& source);
    bool write(const float* const* data, int numSamples);
    bool read(AudioBuffer& destination);
    bool read(float* const* data, int numSamples);
    
    // State queries
    int getAvailableToRead() const;
    int getAvailableToWrite() const;
    int getCapacity() const { return m_bufferSize; }
    bool isEmpty() const { return getAvailableToRead() == 0; }
    bool isFull() const { return getAvailableToWrite() == 0; }
    
    void clear();
    
private:
    const int m_numChannels;
    const int m_bufferSize;
    
    std::unique_ptr<AudioBuffer> m_buffer;
    std::atomic<int> m_readPosition{0};
    std::atomic<int> m_writePosition{0};
};

} // namespace core
} // namespace quiet