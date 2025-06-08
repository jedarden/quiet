#include "quiet/core/AudioBuffer.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cmath>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

namespace quiet {
namespace core {

// AudioBuffer Implementation

AudioBuffer::AudioBuffer() = default;

AudioBuffer::AudioBuffer(int numChannels, int numSamples, double sampleRate)
    : m_numChannels(numChannels), m_numSamples(numSamples), m_sampleRate(sampleRate) {
    if (numChannels > 0 && numSamples > 0) {
        allocateMemory();
        clear();
    }
}

AudioBuffer::AudioBuffer(const AudioBuffer& other) {
    copyData(other);
}

AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
    : m_numChannels(other.m_numChannels)
    , m_numSamples(other.m_numSamples)
    , m_sampleRate(other.m_sampleRate)
    , m_channels(other.m_channels)
    , m_allocatedMemory(other.m_allocatedMemory)
    , m_allocatedSize(other.m_allocatedSize) {
    
    // Reset moved-from object
    other.m_numChannels = 0;
    other.m_numSamples = 0;
    other.m_channels = nullptr;
    other.m_allocatedMemory = nullptr;
    other.m_allocatedSize = 0;
}

AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) {
    if (this != &other) {
        deallocateMemory();
        copyData(other);
    }
    return *this;
}

AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
        deallocateMemory();
        
        m_numChannels = other.m_numChannels;
        m_numSamples = other.m_numSamples;
        m_sampleRate = other.m_sampleRate;
        m_channels = other.m_channels;
        m_allocatedMemory = other.m_allocatedMemory;
        m_allocatedSize = other.m_allocatedSize;
        
        // Reset moved-from object
        other.m_numChannels = 0;
        other.m_numSamples = 0;
        other.m_channels = nullptr;
        other.m_allocatedMemory = nullptr;
        other.m_allocatedSize = 0;
    }
    return *this;
}

AudioBuffer::~AudioBuffer() {
    deallocateMemory();
}

void AudioBuffer::setSize(int numChannels, int numSamples, bool clearExistingContent) {
    if (numChannels == m_numChannels && numSamples == m_numSamples) {
        if (clearExistingContent) {
            clear();
        }
        return;
    }
    
    deallocateMemory();
    m_numChannels = numChannels;
    m_numSamples = numSamples;
    
    if (numChannels > 0 && numSamples > 0) {
        allocateMemory();
        if (clearExistingContent) {
            clear();
        }
    }
}

void AudioBuffer::clear() {
    if (m_allocatedMemory && m_allocatedSize > 0) {
        clearSIMD(m_allocatedMemory, static_cast<int>(m_allocatedSize / sizeof(float)));
    }
}

void AudioBuffer::clear(int channel) {
    if (channel >= 0 && channel < m_numChannels && m_channels) {
        clearSIMD(m_channels[channel], m_numSamples);
    }
}

void AudioBuffer::clear(int channel, int startSample, int numSamples) {
    if (channel >= 0 && channel < m_numChannels && m_channels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples) {
        clearSIMD(m_channels[channel] + startSample, numSamples);
    }
}

float* AudioBuffer::getWritePointer(int channel) {
    return (channel >= 0 && channel < m_numChannels && m_channels) ? m_channels[channel] : nullptr;
}

const float* AudioBuffer::getReadPointer(int channel) const {
    return (channel >= 0 && channel < m_numChannels && m_channels) ? m_channels[channel] : nullptr;
}

float** AudioBuffer::getArrayOfWritePointers() {
    return m_channels;
}

const float* const* AudioBuffer::getArrayOfReadPointers() const {
    // Ensure temp pointer array is large enough
    if (m_tempChannelPointersSize < m_numChannels) {
        delete[] m_tempChannelPointers;
        m_tempChannelPointers = new const float*[m_numChannels];
        m_tempChannelPointersSize = m_numChannels;
    }
    
    for (int i = 0; i < m_numChannels; ++i) {
        m_tempChannelPointers[i] = m_channels[i];
    }
    
    return m_tempChannelPointers;
}

float AudioBuffer::getSample(int channel, int sample) const {
    if (channel >= 0 && channel < m_numChannels && 
        sample >= 0 && sample < m_numSamples && m_channels) {
        return m_channels[channel][sample];
    }
    return 0.0f;
}

void AudioBuffer::setSample(int channel, int sample, float value) {
    if (channel >= 0 && channel < m_numChannels && 
        sample >= 0 && sample < m_numSamples && m_channels) {
        m_channels[channel][sample] = value;
    }
}

void AudioBuffer::addSample(int channel, int sample, float value) {
    if (channel >= 0 && channel < m_numChannels && 
        sample >= 0 && sample < m_numSamples && m_channels) {
        m_channels[channel][sample] += value;
    }
}

void AudioBuffer::copyFrom(int destChannel, int destStartSample,
                          const AudioBuffer& source, int sourceChannel,
                          int sourceStartSample, int numSamples) {
    if (destChannel >= 0 && destChannel < m_numChannels &&
        sourceChannel >= 0 && sourceChannel < source.m_numChannels &&
        destStartSample >= 0 && destStartSample + numSamples <= m_numSamples &&
        sourceStartSample >= 0 && sourceStartSample + numSamples <= source.m_numSamples &&
        m_channels && source.m_channels) {
        
        copySIMD(m_channels[destChannel] + destStartSample,
                source.m_channels[sourceChannel] + sourceStartSample,
                numSamples);
    }
}

void AudioBuffer::copyFrom(int destChannel, int destStartSample,
                          const float* source, int numSamples) {
    if (destChannel >= 0 && destChannel < m_numChannels &&
        destStartSample >= 0 && destStartSample + numSamples <= m_numSamples &&
        m_channels && source) {
        
        copySIMD(m_channels[destChannel] + destStartSample, source, numSamples);
    }
}

void AudioBuffer::addFrom(int destChannel, int destStartSample,
                         const AudioBuffer& source, int sourceChannel,
                         int sourceStartSample, int numSamples) {
    if (destChannel >= 0 && destChannel < m_numChannels &&
        sourceChannel >= 0 && sourceChannel < source.m_numChannels &&
        destStartSample >= 0 && destStartSample + numSamples <= m_numSamples &&
        sourceStartSample >= 0 && sourceStartSample + numSamples <= source.m_numSamples &&
        m_channels && source.m_channels) {
        
        addSIMD(m_channels[destChannel] + destStartSample,
               source.m_channels[sourceChannel] + sourceStartSample,
               numSamples);
    }
}

void AudioBuffer::addFrom(int destChannel, int destStartSample,
                         const float* source, int numSamples) {
    if (destChannel >= 0 && destChannel < m_numChannels &&
        destStartSample >= 0 && destStartSample + numSamples <= m_numSamples &&
        m_channels && source) {
        
        addSIMD(m_channels[destChannel] + destStartSample, source, numSamples);
    }
}

void AudioBuffer::addFromWithMultiply(int destChannel, int destStartSample,
                                     const float* source, int numSamples, float gain) {
    if (destChannel >= 0 && destChannel < m_numChannels &&
        destStartSample >= 0 && destStartSample + numSamples <= m_numSamples &&
        m_channels && source) {
        
        float* dest = m_channels[destChannel] + destStartSample;
        for (int i = 0; i < numSamples; ++i) {
            dest[i] += source[i] * gain;
        }
    }
}

void AudioBuffer::applyGain(float gain) {
    for (int ch = 0; ch < m_numChannels; ++ch) {
        if (m_channels[ch]) {
            multiplySIMD(m_channels[ch], m_numSamples, gain);
        }
    }
}

void AudioBuffer::applyGain(int channel, float gain) {
    if (channel >= 0 && channel < m_numChannels && m_channels) {
        multiplySIMD(m_channels[channel], m_numSamples, gain);
    }
}

void AudioBuffer::applyGain(int channel, int startSample, int numSamples, float gain) {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels) {
        
        multiplySIMD(m_channels[channel] + startSample, numSamples, gain);
    }
}

void AudioBuffer::applyGainRamp(int channel, int startSample, int numSamples,
                               float startGain, float endGain) {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels && numSamples > 0) {
        
        float* data = m_channels[channel] + startSample;
        float gainStep = (endGain - startGain) / static_cast<float>(numSamples - 1);
        
        for (int i = 0; i < numSamples; ++i) {
            data[i] *= startGain + i * gainStep;
        }
    }
}

float AudioBuffer::getMagnitude(int channel, int startSample, int numSamples) const {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels) {
        
        return getMagnitudeSIMD(m_channels[channel] + startSample, numSamples);
    }
    return 0.0f;
}

float AudioBuffer::getMagnitude(int startSample, int numSamples) const {
    float maxMagnitude = 0.0f;
    for (int ch = 0; ch < m_numChannels; ++ch) {
        float magnitude = getMagnitude(ch, startSample, numSamples);
        maxMagnitude = std::max(maxMagnitude, magnitude);
    }
    return maxMagnitude;
}

float AudioBuffer::getRMSLevel(int channel, int startSample, int numSamples) const {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels) {
        
        return getRMSLevelSIMD(m_channels[channel] + startSample, numSamples);
    }
    return 0.0f;
}

float AudioBuffer::getRMSLevel(int startSample, int numSamples) const {
    float sumSquares = 0.0f;
    int totalSamples = 0;
    
    for (int ch = 0; ch < m_numChannels; ++ch) {
        if (m_channels[ch]) {
            const float* data = m_channels[ch] + startSample;
            for (int i = 0; i < numSamples; ++i) {
                sumSquares += data[i] * data[i];
            }
            totalSamples += numSamples;
        }
    }
    
    return totalSamples > 0 ? std::sqrt(sumSquares / totalSamples) : 0.0f;
}

float AudioBuffer::findMinimum(int channel, int startSample, int numSamples) const {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels && numSamples > 0) {
        
        const float* data = m_channels[channel] + startSample;
        return *std::min_element(data, data + numSamples);
    }
    return 0.0f;
}

float AudioBuffer::findMaximum(int channel, int startSample, int numSamples) const {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels && numSamples > 0) {
        
        const float* data = m_channels[channel] + startSample;
        return *std::max_element(data, data + numSamples);
    }
    return 0.0f;
}

void AudioBuffer::findMinAndMax(int channel, int startSample, int numSamples,
                               float& minValue, float& maxValue) const {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels && numSamples > 0) {
        
        const float* data = m_channels[channel] + startSample;
        auto [minIt, maxIt] = std::minmax_element(data, data + numSamples);
        minValue = *minIt;
        maxValue = *maxIt;
    } else {
        minValue = maxValue = 0.0f;
    }
}

void AudioBuffer::convertToInterleaved(std::vector<float>& interleavedData) const {
    interleavedData.resize(m_numChannels * m_numSamples);
    
    for (int sample = 0; sample < m_numSamples; ++sample) {
        for (int ch = 0; ch < m_numChannels; ++ch) {
            interleavedData[sample * m_numChannels + ch] = getSample(ch, sample);
        }
    }
}

void AudioBuffer::convertFromInterleaved(const float* interleavedData, int numSamples) {
    if (!interleavedData || numSamples <= 0) return;
    
    setSize(m_numChannels, numSamples, false);
    
    for (int sample = 0; sample < numSamples; ++sample) {
        for (int ch = 0; ch < m_numChannels; ++ch) {
            setSample(ch, sample, interleavedData[sample * m_numChannels + ch]);
        }
    }
}

void AudioBuffer::convertToMono(AudioBuffer& monoBuffer) const {
    if (m_numChannels == 0) {
        monoBuffer.setSize(0, 0);
        return;
    }
    
    monoBuffer.setSize(1, m_numSamples, true);
    
    if (m_numChannels == 1) {
        // Already mono, just copy
        monoBuffer.copyFrom(0, 0, *this, 0, 0, m_numSamples);
    } else {
        // Mix down to mono
        float* monoData = monoBuffer.getWritePointer(0);
        const float scale = 1.0f / m_numChannels;
        
        for (int sample = 0; sample < m_numSamples; ++sample) {
            float sum = 0.0f;
            for (int ch = 0; ch < m_numChannels; ++ch) {
                sum += getSample(ch, sample);
            }
            monoData[sample] = sum * scale;
        }
    }
}

void AudioBuffer::convertToStereo(AudioBuffer& stereoBuffer) const {
    stereoBuffer.setSize(2, m_numSamples, true);
    
    if (m_numChannels == 0) {
        return;
    } else if (m_numChannels == 1) {
        // Duplicate mono to both channels
        stereoBuffer.copyFrom(0, 0, *this, 0, 0, m_numSamples);
        stereoBuffer.copyFrom(1, 0, *this, 0, 0, m_numSamples);
    } else {
        // Copy first two channels
        stereoBuffer.copyFrom(0, 0, *this, 0, 0, m_numSamples);
        if (m_numChannels > 1) {
            stereoBuffer.copyFrom(1, 0, *this, 1, 0, m_numSamples);
        } else {
            stereoBuffer.copyFrom(1, 0, *this, 0, 0, m_numSamples);
        }
    }
}

size_t AudioBuffer::getSizeInBytes() const {
    return m_allocatedSize;
}

void AudioBuffer::reverse() {
    for (int ch = 0; ch < m_numChannels; ++ch) {
        reverse(ch);
    }
}

void AudioBuffer::reverse(int channel) {
    reverse(channel, 0, m_numSamples);
}

void AudioBuffer::reverse(int channel, int startSample, int numSamples) {
    if (channel >= 0 && channel < m_numChannels &&
        startSample >= 0 && startSample + numSamples <= m_numSamples &&
        m_channels) {
        
        float* data = m_channels[channel] + startSample;
        std::reverse(data, data + numSamples);
    }
}

// Private methods

void AudioBuffer::allocateMemory() {
    const size_t channelSize = m_numSamples * sizeof(float);
    const size_t totalSize = m_numChannels * channelSize;
    const size_t alignment = 32;  // For SIMD
    
    // Allocate aligned memory for audio data
    m_allocatedSize = totalSize + alignment - 1;
    m_allocatedMemory = static_cast<float*>(std::aligned_alloc(alignment, m_allocatedSize));
    
    if (!m_allocatedMemory) {
        throw std::bad_alloc();
    }
    
    // Allocate channel pointer array
    m_channels = new float*[m_numChannels];
    
    // Set up channel pointers
    float* dataPtr = m_allocatedMemory;
    for (int ch = 0; ch < m_numChannels; ++ch) {
        m_channels[ch] = dataPtr;
        dataPtr += m_numSamples;
    }
}

void AudioBuffer::deallocateMemory() {
    delete[] m_tempChannelPointers;
    m_tempChannelPointers = nullptr;
    m_tempChannelPointersSize = 0;
    
    delete[] m_channels;
    m_channels = nullptr;
    
    std::free(m_allocatedMemory);
    m_allocatedMemory = nullptr;
    m_allocatedSize = 0;
}

void AudioBuffer::copyData(const AudioBuffer& other) {
    m_numChannels = other.m_numChannels;
    m_numSamples = other.m_numSamples;
    m_sampleRate = other.m_sampleRate;
    
    if (other.m_allocatedMemory && other.m_allocatedSize > 0) {
        allocateMemory();
        std::memcpy(m_allocatedMemory, other.m_allocatedMemory, other.m_allocatedSize);
    }
}

// SIMD-optimized operations

void AudioBuffer::clearSIMD(float* data, int numSamples) {
    if (!data || numSamples <= 0) return;
    
#ifdef __AVX2__
    // AVX2 implementation - process 8 floats at once
    const int simdCount = numSamples / 8;
    const __m256 zero = _mm256_setzero_ps();
    
    for (int i = 0; i < simdCount; ++i) {
        _mm256_store_ps(data + i * 8, zero);
    }
    
    // Handle remaining samples
    const int remaining = numSamples % 8;
    std::memset(data + simdCount * 8, 0, remaining * sizeof(float));
    
#elif defined(__ARM_NEON__)
    // NEON implementation - process 4 floats at once
    const int simdCount = numSamples / 4;
    const float32x4_t zero = vdupq_n_f32(0.0f);
    
    for (int i = 0; i < simdCount; ++i) {
        vst1q_f32(data + i * 4, zero);
    }
    
    // Handle remaining samples
    const int remaining = numSamples % 4;
    std::memset(data + simdCount * 4, 0, remaining * sizeof(float));
    
#else
    // Fallback implementation
    std::memset(data, 0, numSamples * sizeof(float));
#endif
}

void AudioBuffer::copySIMD(float* dest, const float* src, int numSamples) {
    if (!dest || !src || numSamples <= 0) return;
    
#ifdef __AVX2__
    const int simdCount = numSamples / 8;
    
    for (int i = 0; i < simdCount; ++i) {
        __m256 srcVec = _mm256_load_ps(src + i * 8);
        _mm256_store_ps(dest + i * 8, srcVec);
    }
    
    // Handle remaining samples
    const int remaining = numSamples % 8;
    std::memcpy(dest + simdCount * 8, src + simdCount * 8, remaining * sizeof(float));
    
#elif defined(__ARM_NEON__)
    const int simdCount = numSamples / 4;
    
    for (int i = 0; i < simdCount; ++i) {
        float32x4_t srcVec = vld1q_f32(src + i * 4);
        vst1q_f32(dest + i * 4, srcVec);
    }
    
    // Handle remaining samples
    const int remaining = numSamples % 4;
    std::memcpy(dest + simdCount * 4, src + simdCount * 4, remaining * sizeof(float));
    
#else
    std::memcpy(dest, src, numSamples * sizeof(float));
#endif
}

void AudioBuffer::addSIMD(float* dest, const float* src, int numSamples) {
    if (!dest || !src || numSamples <= 0) return;
    
#ifdef __AVX2__
    const int simdCount = numSamples / 8;
    
    for (int i = 0; i < simdCount; ++i) {
        __m256 destVec = _mm256_load_ps(dest + i * 8);
        __m256 srcVec = _mm256_load_ps(src + i * 8);
        __m256 result = _mm256_add_ps(destVec, srcVec);
        _mm256_store_ps(dest + i * 8, result);
    }
    
    // Handle remaining samples
    for (int i = simdCount * 8; i < numSamples; ++i) {
        dest[i] += src[i];
    }
    
#elif defined(__ARM_NEON__)
    const int simdCount = numSamples / 4;
    
    for (int i = 0; i < simdCount; ++i) {
        float32x4_t destVec = vld1q_f32(dest + i * 4);
        float32x4_t srcVec = vld1q_f32(src + i * 4);
        float32x4_t result = vaddq_f32(destVec, srcVec);
        vst1q_f32(dest + i * 4, result);
    }
    
    // Handle remaining samples
    for (int i = simdCount * 4; i < numSamples; ++i) {
        dest[i] += src[i];
    }
    
#else
    for (int i = 0; i < numSamples; ++i) {
        dest[i] += src[i];
    }
#endif
}

void AudioBuffer::multiplySIMD(float* data, int numSamples, float gain) {
    if (!data || numSamples <= 0) return;
    
#ifdef __AVX2__
    const int simdCount = numSamples / 8;
    const __m256 gainVec = _mm256_set1_ps(gain);
    
    for (int i = 0; i < simdCount; ++i) {
        __m256 dataVec = _mm256_load_ps(data + i * 8);
        __m256 result = _mm256_mul_ps(dataVec, gainVec);
        _mm256_store_ps(data + i * 8, result);
    }
    
    // Handle remaining samples
    for (int i = simdCount * 8; i < numSamples; ++i) {
        data[i] *= gain;
    }
    
#elif defined(__ARM_NEON__)
    const int simdCount = numSamples / 4;
    const float32x4_t gainVec = vdupq_n_f32(gain);
    
    for (int i = 0; i < simdCount; ++i) {
        float32x4_t dataVec = vld1q_f32(data + i * 4);
        float32x4_t result = vmulq_f32(dataVec, gainVec);
        vst1q_f32(data + i * 4, result);
    }
    
    // Handle remaining samples
    for (int i = simdCount * 4; i < numSamples; ++i) {
        data[i] *= gain;
    }
    
#else
    for (int i = 0; i < numSamples; ++i) {
        data[i] *= gain;
    }
#endif
}

float AudioBuffer::getMagnitudeSIMD(const float* data, int numSamples) const {
    if (!data || numSamples <= 0) return 0.0f;
    
    float maxMagnitude = 0.0f;
    
#ifdef __AVX2__
    const int simdCount = numSamples / 8;
    __m256 maxVec = _mm256_setzero_ps();
    
    for (int i = 0; i < simdCount; ++i) {
        __m256 dataVec = _mm256_load_ps(data + i * 8);
        __m256 absVec = _mm256_andnot_ps(_mm256_set1_ps(-0.0f), dataVec);
        maxVec = _mm256_max_ps(maxVec, absVec);
    }
    
    // Extract maximum from vector
    alignas(32) float maxArray[8];
    _mm256_store_ps(maxArray, maxVec);
    for (int i = 0; i < 8; ++i) {
        maxMagnitude = std::max(maxMagnitude, maxArray[i]);
    }
    
    // Handle remaining samples
    for (int i = simdCount * 8; i < numSamples; ++i) {
        maxMagnitude = std::max(maxMagnitude, std::abs(data[i]));
    }
    
#else
    for (int i = 0; i < numSamples; ++i) {
        maxMagnitude = std::max(maxMagnitude, std::abs(data[i]));
    }
#endif
    
    return maxMagnitude;
}

float AudioBuffer::getRMSLevelSIMD(const float* data, int numSamples) const {
    if (!data || numSamples <= 0) return 0.0f;
    
    float sumSquares = 0.0f;
    
#ifdef __AVX2__
    const int simdCount = numSamples / 8;
    __m256 sumVec = _mm256_setzero_ps();
    
    for (int i = 0; i < simdCount; ++i) {
        __m256 dataVec = _mm256_load_ps(data + i * 8);
        __m256 squaredVec = _mm256_mul_ps(dataVec, dataVec);
        sumVec = _mm256_add_ps(sumVec, squaredVec);
    }
    
    // Extract sum from vector
    alignas(32) float sumArray[8];
    _mm256_store_ps(sumArray, sumVec);
    for (int i = 0; i < 8; ++i) {
        sumSquares += sumArray[i];
    }
    
    // Handle remaining samples
    for (int i = simdCount * 8; i < numSamples; ++i) {
        sumSquares += data[i] * data[i];
    }
    
#else
    for (int i = 0; i < numSamples; ++i) {
        sumSquares += data[i] * data[i];
    }
#endif
    
    return std::sqrt(sumSquares / numSamples);
}

// AudioRingBuffer Implementation

AudioRingBuffer::AudioRingBuffer(int numChannels, int numSamples)
    : m_numChannels(numChannels), m_bufferSize(numSamples) {
    m_buffer = std::make_unique<AudioBuffer>(numChannels, numSamples);
}

AudioRingBuffer::~AudioRingBuffer() = default;

bool AudioRingBuffer::write(const AudioBuffer& source) {
    if (source.getNumChannels() != m_numChannels) {
        return false;
    }
    
    const int samplesToWrite = source.getNumSamples();
    if (getAvailableToWrite() < samplesToWrite) {
        return false;  // Not enough space
    }
    
    const int writePos = m_writePosition.load();
    
    for (int ch = 0; ch < m_numChannels; ++ch) {
        const float* sourceData = source.getReadPointer(ch);
        float* bufferData = m_buffer->getWritePointer(ch);
        
        for (int i = 0; i < samplesToWrite; ++i) {
            bufferData[(writePos + i) % m_bufferSize] = sourceData[i];
        }
    }
    
    m_writePosition.store((writePos + samplesToWrite) % m_bufferSize);
    return true;
}

bool AudioRingBuffer::write(const float* const* data, int numSamples) {
    if (getAvailableToWrite() < numSamples) {
        return false;
    }
    
    const int writePos = m_writePosition.load();
    
    for (int ch = 0; ch < m_numChannels; ++ch) {
        if (data[ch]) {
            float* bufferData = m_buffer->getWritePointer(ch);
            
            for (int i = 0; i < numSamples; ++i) {
                bufferData[(writePos + i) % m_bufferSize] = data[ch][i];
            }
        }
    }
    
    m_writePosition.store((writePos + numSamples) % m_bufferSize);
    return true;
}

bool AudioRingBuffer::read(AudioBuffer& destination) {
    const int samplesToRead = destination.getNumSamples();
    if (destination.getNumChannels() != m_numChannels ||
        getAvailableToRead() < samplesToRead) {
        return false;
    }
    
    const int readPos = m_readPosition.load();
    
    for (int ch = 0; ch < m_numChannels; ++ch) {
        const float* bufferData = m_buffer->getReadPointer(ch);
        float* destData = destination.getWritePointer(ch);
        
        for (int i = 0; i < samplesToRead; ++i) {
            destData[i] = bufferData[(readPos + i) % m_bufferSize];
        }
    }
    
    m_readPosition.store((readPos + samplesToRead) % m_bufferSize);
    return true;
}

bool AudioRingBuffer::read(float* const* data, int numSamples) {
    if (getAvailableToRead() < numSamples) {
        return false;
    }
    
    const int readPos = m_readPosition.load();
    
    for (int ch = 0; ch < m_numChannels; ++ch) {
        if (data[ch]) {
            const float* bufferData = m_buffer->getReadPointer(ch);
            
            for (int i = 0; i < numSamples; ++i) {
                data[ch][i] = bufferData[(readPos + i) % m_bufferSize];
            }
        }
    }
    
    m_readPosition.store((readPos + numSamples) % m_bufferSize);
    return true;
}

int AudioRingBuffer::getAvailableToRead() const {
    const int writePos = m_writePosition.load();
    const int readPos = m_readPosition.load();
    return (writePos - readPos + m_bufferSize) % m_bufferSize;
}

int AudioRingBuffer::getAvailableToWrite() const {
    return m_bufferSize - getAvailableToRead() - 1;  // Leave one sample gap
}

void AudioRingBuffer::clear() {
    m_readPosition.store(0);
    m_writePosition.store(0);
    m_buffer->clear();
}

} // namespace core
} // namespace quiet