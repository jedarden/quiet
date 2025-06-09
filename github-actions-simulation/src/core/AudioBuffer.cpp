#include "quiet/core/AudioBuffer.h"
#include <cstring>
#include <algorithm>
#include <new>
#include <cstdlib>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace quiet {
namespace core {

// Memory alignment helpers
void* AudioBuffer::allocateAligned(size_t bytes) {
    if (bytes == 0) return nullptr;
    
    // Allocate with alignment for SIMD operations
    void* ptr = nullptr;
#ifdef _WIN32
    ptr = _aligned_malloc(bytes, kAlignment);
    if (!ptr) throw std::bad_alloc();
#else
    if (posix_memalign(&ptr, kAlignment, bytes) != 0) {
        throw std::bad_alloc();
    }
#endif
    return ptr;
}

void AudioBuffer::deallocateAligned(void* ptr) {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

// Default constructor
AudioBuffer::AudioBuffer() 
    : numChannels_(0)
    , numSamples_(0)
    , sampleRate_(48000.0)
    , allocatedBytes_(0) {
}

// Parameterized constructor
AudioBuffer::AudioBuffer(int numChannels, int numSamples, double sampleRate)
    : numChannels_(numChannels)
    , numSamples_(numSamples)
    , sampleRate_(sampleRate)
    , allocatedBytes_(0) {
    
    if (numChannels > 0 && numSamples > 0) {
        allocateChannels();
        clear();  // Initialize to zeros
    }
}

// Copy constructor
AudioBuffer::AudioBuffer(const AudioBuffer& other)
    : numChannels_(other.numChannels_)
    , numSamples_(other.numSamples_)
    , sampleRate_(other.sampleRate_)
    , allocatedBytes_(0) {
    
    if (numChannels_ > 0 && numSamples_ > 0) {
        allocateChannels();
        copyFrom(other);
    }
}

// Move constructor
AudioBuffer::AudioBuffer(AudioBuffer&& other) noexcept
    : numChannels_(other.numChannels_)
    , numSamples_(other.numSamples_)
    , sampleRate_(other.sampleRate_)
    , allocatedBytes_(other.allocatedBytes_)
    , channels_(std::move(other.channels_))
    , data_(std::move(other.data_)) {
    
    // Reset other to valid empty state
    other.numChannels_ = 0;
    other.numSamples_ = 0;
    other.allocatedBytes_ = 0;
}

// Destructor
AudioBuffer::~AudioBuffer() {
    deallocateChannels();
}

// Copy assignment
AudioBuffer& AudioBuffer::operator=(const AudioBuffer& other) {
    if (this != &other) {
        // Reallocate if size doesn't match
        if (numChannels_ != other.numChannels_ || numSamples_ != other.numSamples_) {
            deallocateChannels();
            numChannels_ = other.numChannels_;
            numSamples_ = other.numSamples_;
            sampleRate_ = other.sampleRate_;
            allocateChannels();
        } else {
            sampleRate_ = other.sampleRate_;
        }
        
        // Copy data
        if (numChannels_ > 0 && numSamples_ > 0) {
            copyFrom(other);
        }
    }
    return *this;
}

// Move assignment
AudioBuffer& AudioBuffer::operator=(AudioBuffer&& other) noexcept {
    if (this != &other) {
        deallocateChannels();
        
        numChannels_ = other.numChannels_;
        numSamples_ = other.numSamples_;
        sampleRate_ = other.sampleRate_;
        allocatedBytes_ = other.allocatedBytes_;
        channels_ = std::move(other.channels_);
        data_ = std::move(other.data_);
        
        // Reset other
        other.numChannels_ = 0;
        other.numSamples_ = 0;
        other.allocatedBytes_ = 0;
    }
    return *this;
}

// Set buffer size
void AudioBuffer::setSize(int numChannels, int numSamples, bool clearBuffer) {
    if (numChannels == numChannels_ && numSamples == numSamples_) {
        if (clearBuffer) {
            clear();
        }
        return;
    }
    
    deallocateChannels();
    numChannels_ = numChannels;
    numSamples_ = numSamples;
    
    if (numChannels > 0 && numSamples > 0) {
        allocateChannels();
        if (clearBuffer) {
            clear();
        }
    }
}

// Clear operations
void AudioBuffer::clear() {
    if (!isEmpty()) {
        for (int ch = 0; ch < numChannels_; ++ch) {
            clearSIMD(channels_[ch], numSamples_);
        }
    }
}

void AudioBuffer::clear(int channel) {
    if (channel >= 0 && channel < numChannels_) {
        clearSIMD(channels_[channel], numSamples_);
    }
}

void AudioBuffer::clear(int channel, int startSample, int numSamples) {
    if (channel < 0 || channel >= numChannels_ || startSample < 0) return;
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int samplesToFill = endSample - startSample;
    
    if (samplesToFill > 0) {
        clearSIMD(channels_[channel] + startSample, samplesToFill);
    }
}

// Copy operations
void AudioBuffer::copyFrom(int destChannel, int destStartSample,
                          const AudioBuffer& source, int sourceChannel,
                          int sourceStartSample, int numSamples) {
    if (destChannel < 0 || destChannel >= numChannels_ ||
        sourceChannel < 0 || sourceChannel >= source.numChannels_) {
        return;
    }
    
    int sourceEnd = std::min(sourceStartSample + numSamples, source.numSamples_);
    int destEnd = std::min(destStartSample + numSamples, numSamples_);
    int actualSamples = std::min(sourceEnd - sourceStartSample, destEnd - destStartSample);
    
    if (actualSamples > 0) {
        copySIMD(channels_[destChannel] + destStartSample,
                source.channels_[sourceChannel] + sourceStartSample,
                actualSamples);
    }
}

void AudioBuffer::copyFrom(const AudioBuffer& source) {
    int channelsToCopy = std::min(numChannels_, source.numChannels_);
    int samplesToCopy = std::min(numSamples_, source.numSamples_);
    
    for (int ch = 0; ch < channelsToCopy; ++ch) {
        copySIMD(channels_[ch], source.channels_[ch], samplesToCopy);
    }
}

void AudioBuffer::copyFrom(int destChannel, int destStartSample,
                          const float* source, int numSamples) {
    if (destChannel < 0 || destChannel >= numChannels_ || !source) {
        return;
    }
    
    int destEnd = std::min(destStartSample + numSamples, numSamples_);
    int actualSamples = destEnd - destStartSample;
    
    if (actualSamples > 0) {
        copySIMD(channels_[destChannel] + destStartSample, source, actualSamples);
    }
}

// Add operations
void AudioBuffer::addFrom(int destChannel, int destStartSample,
                         const AudioBuffer& source, int sourceChannel,
                         int sourceStartSample, int numSamples, float gain) {
    if (destChannel < 0 || destChannel >= numChannels_ ||
        sourceChannel < 0 || sourceChannel >= source.numChannels_) {
        return;
    }
    
    int sourceEnd = std::min(sourceStartSample + numSamples, source.numSamples_);
    int destEnd = std::min(destStartSample + numSamples, numSamples_);
    int actualSamples = std::min(sourceEnd - sourceStartSample, destEnd - destStartSample);
    
    if (actualSamples > 0) {
        addSIMD(channels_[destChannel] + destStartSample,
               source.channels_[sourceChannel] + sourceStartSample,
               actualSamples, gain);
    }
}

void AudioBuffer::addFrom(const AudioBuffer& source, float gain) {
    int channelsToAdd = std::min(numChannels_, source.numChannels_);
    int samplesToAdd = std::min(numSamples_, source.numSamples_);
    
    for (int ch = 0; ch < channelsToAdd; ++ch) {
        addSIMD(channels_[ch], source.channels_[ch], samplesToAdd, gain);
    }
}

// Gain operations
void AudioBuffer::applyGain(float gain) {
    for (int ch = 0; ch < numChannels_; ++ch) {
        applySIMD(channels_[ch], numSamples_, gain);
    }
}

void AudioBuffer::applyGain(int channel, float gain) {
    if (channel >= 0 && channel < numChannels_) {
        applySIMD(channels_[channel], numSamples_, gain);
    }
}

void AudioBuffer::applyGain(int channel, int startSample, int numSamples, float gain) {
    if (channel < 0 || channel >= numChannels_ || startSample < 0) return;
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int samplesToProcess = endSample - startSample;
    
    if (samplesToProcess > 0) {
        applySIMD(channels_[channel] + startSample, samplesToProcess, gain);
    }
}

void AudioBuffer::applyGainRamp(int channel, int startSample, int numSamples,
                               float startGain, float endGain) {
    if (channel < 0 || channel >= numChannels_ || startSample < 0) return;
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int samplesToProcess = endSample - startSample;
    
    if (samplesToProcess > 0) {
        float* buffer = channels_[channel] + startSample;
        float gainStep = (endGain - startGain) / samplesToProcess;
        float currentGain = startGain;
        
        for (int i = 0; i < samplesToProcess; ++i) {
            buffer[i] *= currentGain;
            currentGain += gainStep;
        }
    }
}

// Level analysis
float AudioBuffer::getRMSLevel(int channel, int startSample, int numSamples) const {
    if (channel < 0 || channel >= numChannels_ || startSample < 0 || numSamples <= 0) {
        return 0.0f;
    }
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int actualSamples = endSample - startSample;
    
    if (actualSamples <= 0) return 0.0f;
    
    const float* buffer = channels_[channel] + startSample;
    float sum = 0.0f;
    
    // Use SIMD for sum of squares
    for (int i = 0; i < actualSamples; ++i) {
        sum += buffer[i] * buffer[i];
    }
    
    return std::sqrt(sum / actualSamples);
}

float AudioBuffer::getMagnitude(int channel, int startSample, int numSamples) const {
    if (channel < 0 || channel >= numChannels_ || startSample < 0 || numSamples <= 0) {
        return 0.0f;
    }
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int actualSamples = endSample - startSample;
    
    if (actualSamples <= 0) return 0.0f;
    
    const float* buffer = channels_[channel] + startSample;
    float maxMag = 0.0f;
    
    for (int i = 0; i < actualSamples; ++i) {
        maxMag = std::max(maxMag, std::abs(buffer[i]));
    }
    
    return maxMag;
}

void AudioBuffer::findMinAndMax(int channel, int startSample, int numSamples,
                               float& minVal, float& maxVal) const {
    if (channel < 0 || channel >= numChannels_ || startSample < 0 || numSamples <= 0) {
        minVal = maxVal = 0.0f;
        return;
    }
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int actualSamples = endSample - startSample;
    
    if (actualSamples <= 0) {
        minVal = maxVal = 0.0f;
        return;
    }
    
    const float* buffer = channels_[channel] + startSample;
    minVal = maxVal = buffer[0];
    
    for (int i = 1; i < actualSamples; ++i) {
        minVal = std::min(minVal, buffer[i]);
        maxVal = std::max(maxVal, buffer[i]);
    }
}

// Format conversion
void AudioBuffer::convertToMono(AudioBuffer& destination) const {
    destination.setSize(1, numSamples_);
    destination.setSampleRate(sampleRate_);
    
    if (numChannels_ == 0 || numSamples_ == 0) return;
    
    float* destBuffer = destination.getWritePointer(0);
    
    // Clear destination first
    destination.clear();
    
    // Sum all channels
    for (int ch = 0; ch < numChannels_; ++ch) {
        for (int i = 0; i < numSamples_; ++i) {
            destBuffer[i] += channels_[ch][i];
        }
    }
    
    // Average
    float scale = 1.0f / numChannels_;
    destination.applyGain(0, scale);
}

void AudioBuffer::convertToStereo(AudioBuffer& destination) const {
    destination.setSize(2, numSamples_);
    destination.setSampleRate(sampleRate_);
    
    if (numChannels_ == 0 || numSamples_ == 0) return;
    
    if (numChannels_ == 1) {
        // Mono to stereo - duplicate channel
        destination.copyFrom(0, 0, *this, 0, 0, numSamples_);
        destination.copyFrom(1, 0, *this, 0, 0, numSamples_);
    } else {
        // Multi-channel to stereo - copy first two channels
        destination.copyFrom(0, 0, *this, 0, 0, numSamples_);
        destination.copyFrom(1, 0, *this, 1, 0, numSamples_);
    }
}

void AudioBuffer::convertToInterleaved(std::vector<float>& destination) const {
    destination.resize(numChannels_ * numSamples_);
    
    if (numChannels_ == 0 || numSamples_ == 0) return;
    
    // Interleave channels
    size_t destIndex = 0;
    for (int sample = 0; sample < numSamples_; ++sample) {
        for (int ch = 0; ch < numChannels_; ++ch) {
            destination[destIndex++] = channels_[ch][sample];
        }
    }
}

void AudioBuffer::convertFromInterleaved(const float* source, int numSamples) {
    if (!source || numChannels_ == 0) return;
    
    int samplesToCopy = std::min(numSamples, numSamples_);
    
    // De-interleave
    size_t sourceIndex = 0;
    for (int sample = 0; sample < samplesToCopy; ++sample) {
        for (int ch = 0; ch < numChannels_; ++ch) {
            channels_[ch][sample] = source[sourceIndex++];
        }
    }
}

// Utility functions
bool AudioBuffer::hasBeenClipped() const {
    for (int ch = 0; ch < numChannels_; ++ch) {
        for (int i = 0; i < numSamples_; ++i) {
            float sample = std::abs(channels_[ch][i]);
            if (sample >= 1.0f) {
                return true;
            }
        }
    }
    return false;
}

void AudioBuffer::reverse(int channel, int startSample, int numSamples) {
    if (channel < 0 || channel >= numChannels_ || startSample < 0) return;
    
    int endSample = std::min(startSample + numSamples, numSamples_);
    int actualSamples = endSample - startSample;
    
    if (actualSamples <= 1) return;
    
    float* buffer = channels_[channel] + startSample;
    std::reverse(buffer, buffer + actualSamples);
}

// Memory management
void AudioBuffer::allocateChannels() {
    if (numChannels_ <= 0 || numSamples_ <= 0) return;
    
    // Calculate total bytes needed
    size_t bytesPerChannel = numSamples_ * sizeof(float);
    size_t totalBytes = numChannels_ * bytesPerChannel;
    
    // Allocate contiguous memory for all channels
    data_.reset(static_cast<float*>(allocateAligned(totalBytes)));
    allocatedBytes_ = totalBytes;
    
    // Set up channel pointers
    channels_.reset(new float*[numChannels_]);
    for (int ch = 0; ch < numChannels_; ++ch) {
        channels_[ch] = data_.get() + (ch * numSamples_);
    }
}

void AudioBuffer::deallocateChannels() {
    channels_.reset();
    if (data_) {
        deallocateAligned(data_.release());
    }
    allocatedBytes_ = 0;
}

// SIMD operations
void AudioBuffer::clearSIMD(float* buffer, int numSamples) {
    if (!buffer || numSamples <= 0) return;
    
#ifdef __AVX2__
    // Use SIMD for aligned portions
    int simdSamples = numSamples & ~7;  // Process 8 samples at a time
    
    __m256 zero = _mm256_setzero_ps();
    for (int i = 0; i < simdSamples; i += 8) {
        _mm256_store_ps(buffer + i, zero);
    }
    
    // Handle remaining samples
    for (int i = simdSamples; i < numSamples; ++i) {
        buffer[i] = 0.0f;
    }
#else
    // Fallback to memset
    std::memset(buffer, 0, numSamples * sizeof(float));
#endif
}

void AudioBuffer::copySIMD(float* dest, const float* source, int numSamples) {
    if (!dest || !source || numSamples <= 0) return;
    
#ifdef __AVX2__
    // Use SIMD for aligned portions
    int simdSamples = numSamples & ~7;
    
    for (int i = 0; i < simdSamples; i += 8) {
        __m256 data = _mm256_load_ps(source + i);
        _mm256_store_ps(dest + i, data);
    }
    
    // Handle remaining samples
    for (int i = simdSamples; i < numSamples; ++i) {
        dest[i] = source[i];
    }
#else
    // Fallback to memcpy
    std::memcpy(dest, source, numSamples * sizeof(float));
#endif
}

void AudioBuffer::addSIMD(float* dest, const float* source, int numSamples, float gain) {
    if (!dest || !source || numSamples <= 0) return;
    
#ifdef __AVX2__
    // Use SIMD for aligned portions
    int simdSamples = numSamples & ~7;
    __m256 gainVec = _mm256_set1_ps(gain);
    
    for (int i = 0; i < simdSamples; i += 8) {
        __m256 srcData = _mm256_load_ps(source + i);
        __m256 destData = _mm256_load_ps(dest + i);
        __m256 scaled = _mm256_mul_ps(srcData, gainVec);
        __m256 result = _mm256_add_ps(destData, scaled);
        _mm256_store_ps(dest + i, result);
    }
    
    // Handle remaining samples
    for (int i = simdSamples; i < numSamples; ++i) {
        dest[i] += source[i] * gain;
    }
#else
    // Fallback implementation
    for (int i = 0; i < numSamples; ++i) {
        dest[i] += source[i] * gain;
    }
#endif
}

void AudioBuffer::applySIMD(float* buffer, int numSamples, float gain) {
    if (!buffer || numSamples <= 0) return;
    
#ifdef __AVX2__
    // Use SIMD for aligned portions
    int simdSamples = numSamples & ~7;
    __m256 gainVec = _mm256_set1_ps(gain);
    
    for (int i = 0; i < simdSamples; i += 8) {
        __m256 data = _mm256_load_ps(buffer + i);
        __m256 result = _mm256_mul_ps(data, gainVec);
        _mm256_store_ps(buffer + i, result);
    }
    
    // Handle remaining samples
    for (int i = simdSamples; i < numSamples; ++i) {
        buffer[i] *= gain;
    }
#else
    // Fallback implementation
    for (int i = 0; i < numSamples; ++i) {
        buffer[i] *= gain;
    }
#endif
}

} // namespace core
} // namespace quiet