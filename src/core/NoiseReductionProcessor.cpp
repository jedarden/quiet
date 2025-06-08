#include "quiet/core/NoiseReductionProcessor.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <stdexcept>

// Note: In a real implementation, you would include the actual RNNoise headers
// For this implementation, we'll create a stub that simulates RNNoise behavior
extern "C" {
    typedef struct DenoiseState DenoiseState;
    
    // Stub implementations for RNNoise functions
    DenoiseState* rnnoise_create(void* model) {
        (void)model;
        return reinterpret_cast<DenoiseState*>(malloc(sizeof(int)));  // Dummy allocation
    }
    
    void rnnoise_destroy(DenoiseState* st) {
        free(st);
    }
    
    float rnnoise_process_frame(DenoiseState* st, float* out, const float* in) {
        (void)st;
        // Simple noise reduction simulation: apply low-pass filter and slight attenuation
        constexpr int FRAME_SIZE = 480;
        constexpr float ATTENUATION = 0.8f;
        
        for (int i = 0; i < FRAME_SIZE; ++i) {
            // Simple moving average for noise reduction simulation
            float filtered = in[i] * ATTENUATION;
            if (i > 0) filtered = (filtered + out[i-1] * 0.1f) / 1.1f;
            out[i] = filtered;
        }
        
        return 0.7f;  // Simulated voice activity detection probability
    }
}

namespace quiet {
namespace core {

NoiseReductionProcessor::NoiseReductionProcessor(EventDispatcher& eventDispatcher)
    : m_eventDispatcher(eventDispatcher) {
    
    // Initialize default configuration
    m_config.level = NoiseReductionConfig::Level::Medium;
    m_config.enabled = true;
    m_config.threshold = 0.5f;
    m_config.adaptiveMode = true;
    
    // Initialize overlap buffer for frame-based processing
    m_overlapSize = RNNOISE_FRAME_SIZE / 4;  // 25% overlap
    m_overlapBuffer.resize(m_overlapSize, 0.0f);
}

NoiseReductionProcessor::~NoiseReductionProcessor() {
    shutdown();
}

bool NoiseReductionProcessor::initialize(double sampleRate) {
    if (m_isInitialized) {
        return true;  // Already initialized
    }
    
    m_sampleRate = sampleRate;
    
    // Initialize RNNoise
    if (!initializeRNNoise()) {
        return false;
    }
    
    // Allocate working buffers
    m_workingBuffer.resize(RNNOISE_FRAME_SIZE);
    m_tempBuffer.resize(RNNOISE_FRAME_SIZE);
    
    // Reset statistics
    resetStats();
    
    m_isInitialized = true;
    
    // Notify event dispatcher
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("sample_rate", sampleRate);
    m_eventDispatcher.publish(EventType::AudioProcessingStarted, eventData);
    
    return true;
}

void NoiseReductionProcessor::shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    cleanupRNNoise();
    
    m_workingBuffer.clear();
    m_tempBuffer.clear();
    m_overlapBuffer.clear();
    
    m_isInitialized = false;
    
    // Notify event dispatcher
    m_eventDispatcher.publish(EventType::AudioProcessingStopped);
}

bool NoiseReductionProcessor::isInitialized() const {
    return m_isInitialized;
}

void NoiseReductionProcessor::setConfig(const NoiseReductionConfig& config) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_config = config;
    m_enabled.store(config.enabled);
    
    // Notify about configuration change
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("enabled", config.enabled);
    eventData->setValue("level", static_cast<int>(config.level));
    eventData->setValue("threshold", config.threshold);
    eventData->setValue("adaptive", config.adaptiveMode);
    
    m_eventDispatcher.publish(EventType::NoiseReductionLevelChanged, eventData);
}

NoiseReductionConfig NoiseReductionProcessor::getConfig() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_config;
}

void NoiseReductionProcessor::setEnabled(bool enabled) {
    bool wasEnabled = m_enabled.exchange(enabled);
    
    if (wasEnabled != enabled) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_config.enabled = enabled;
        
        // Notify about toggle
        auto eventData = std::make_shared<EventData>();
        eventData->setValue("enabled", enabled);
        m_eventDispatcher.publish(EventType::NoiseReductionToggled, eventData);
    }
}

bool NoiseReductionProcessor::isEnabled() const {
    return m_enabled.load();
}

void NoiseReductionProcessor::setLevel(NoiseReductionConfig::Level level) {
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_config.level = level;
    }
    
    // Notify about level change
    auto eventData = std::make_shared<EventData>();
    eventData->setValue("level", static_cast<int>(level));
    m_eventDispatcher.publish(EventType::NoiseReductionLevelChanged, eventData);
}

NoiseReductionConfig::Level NoiseReductionProcessor::getLevel() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_config.level;
}

bool NoiseReductionProcessor::process(AudioBuffer& buffer) {
    if (!m_isInitialized || buffer.isEmpty()) {
        return false;
    }
    
    if (!m_enabled.load()) {
        // Processing disabled - just return success without modifying buffer
        return true;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Convert to mono if necessary (RNNoise processes mono)
    AudioBuffer monoBuffer;
    if (buffer.getNumChannels() == 1) {
        monoBuffer = buffer;
    } else {
        buffer.convertToMono(monoBuffer);
    }
    
    // Process the mono buffer
    bool success = processMonoBuffer(monoBuffer);
    
    // If original was stereo, convert back
    if (success && buffer.getNumChannels() > 1) {
        monoBuffer.convertToStereo(buffer);
    } else if (success && buffer.getNumChannels() == 1) {
        buffer = monoBuffer;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Update statistics
    if (success) {
        float voiceProb = 0.7f;  // Placeholder - would come from actual RNNoise
        float reductionDb = calculateReductionAmount(monoBuffer);
        updateStats(reductionDb, voiceProb, processingTime.count());
    }
    
    return success;
}

bool NoiseReductionProcessor::processInPlace(float* samples, int numSamples) {
    if (!m_isInitialized || !samples || numSamples <= 0) {
        return false;
    }
    
    if (!m_enabled.load()) {
        return true;  // Processing disabled
    }
    
    // Create temporary buffer for processing
    AudioBuffer tempBuffer(1, numSamples, m_sampleRate);
    tempBuffer.copyFrom(0, 0, samples, numSamples);
    
    bool success = process(tempBuffer);
    
    if (success) {
        // Copy processed data back
        const float* processedData = tempBuffer.getReadPointer(0);
        std::memcpy(samples, processedData, numSamples * sizeof(float));
    }
    
    return success;
}

NoiseReductionStats NoiseReductionProcessor::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void NoiseReductionProcessor::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = NoiseReductionStats{};
}

float NoiseReductionProcessor::getCpuUsage() const {
    return m_cpuUsage.load();
}

float NoiseReductionProcessor::getLatency() const {
    return m_latency.load();
}

// Private methods

bool NoiseReductionProcessor::processMonoBuffer(AudioBuffer& monoBuffer) {
    const int numSamples = monoBuffer.getNumSamples();
    float* data = monoBuffer.getWritePointer(0);
    
    if (!data) {
        return false;
    }
    
    // Process in RNNOISE_FRAME_SIZE chunks with overlap
    int processedSamples = 0;
    
    while (processedSamples < numSamples) {
        int samplesToProcess = std::min(RNNOISE_FRAME_SIZE, numSamples - processedSamples);
        
        // Handle partial frames at the end
        if (samplesToProcess < RNNOISE_FRAME_SIZE) {
            // Zero-pad the working buffer
            std::fill(m_workingBuffer.begin(), m_workingBuffer.end(), 0.0f);
            std::copy(data + processedSamples, data + processedSamples + samplesToProcess,
                     m_workingBuffer.begin());
        } else {
            // Copy full frame
            std::copy(data + processedSamples, data + processedSamples + RNNOISE_FRAME_SIZE,
                     m_workingBuffer.begin());
        }
        
        // Apply overlap from previous frame
        if (processedSamples > 0 && m_overlapSize > 0) {
            for (int i = 0; i < std::min(m_overlapSize, samplesToProcess); ++i) {
                m_workingBuffer[i] = (m_workingBuffer[i] + m_overlapBuffer[i]) * 0.5f;
            }
        }
        
        // Process the frame
        if (!processFrame(m_workingBuffer.data(), RNNOISE_FRAME_SIZE)) {
            return false;
        }
        
        // Store overlap for next frame
        if (samplesToProcess >= m_overlapSize) {
            std::copy(m_workingBuffer.end() - m_overlapSize, m_workingBuffer.end(),
                     m_overlapBuffer.begin());
        }
        
        // Copy processed data back (excluding overlap region)
        int copyStart = (processedSamples > 0) ? m_overlapSize : 0;
        int copyCount = std::min(samplesToProcess - copyStart, numSamples - processedSamples - copyStart);
        
        if (copyCount > 0) {
            std::copy(m_workingBuffer.begin() + copyStart,
                     m_workingBuffer.begin() + copyStart + copyCount,
                     data + processedSamples + copyStart);
        }
        
        processedSamples += samplesToProcess;
    }
    
    return true;
}

bool NoiseReductionProcessor::processFrame(float* frame, int frameSize) {
    if (!m_rnnoise || frameSize != RNNOISE_FRAME_SIZE) {
        return false;
    }
    
    // Convert to format expected by RNNoise if necessary
    convertToRNNoiseFormat(frame, m_tempBuffer.data(), frameSize);
    
    // Apply RNNoise processing
    float voiceProb = rnnoise_process_frame(m_rnnoise, m_tempBuffer.data(), m_tempBuffer.data());
    
    // Apply additional processing based on configuration
    applyReductionLevel(m_tempBuffer.data(), frameSize, voiceProb);
    
    // Convert back to our format
    convertFromRNNoiseFormat(m_tempBuffer.data(), frame, frameSize);
    
    return true;
}

void NoiseReductionProcessor::updateStats(float reductionDb, float voiceProb, uint64_t processingTime) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_stats.framesProcessed++;
    m_stats.totalProcessingTime += processingTime;
    m_stats.voiceProbability = voiceProb;
    
    // Update reduction level (exponential moving average)
    const float alpha = 0.1f;  // Smoothing factor
    m_stats.reductionLevel = alpha * reductionDb + (1.0f - alpha) * m_stats.reductionLevel;
    
    // Update average reduction
    m_stats.averageReduction = (m_stats.averageReduction * (m_stats.framesProcessed - 1) + reductionDb) 
                              / m_stats.framesProcessed;
    
    // Update performance metrics
    if (m_stats.framesProcessed > 0) {
        double avgProcessingTime = static_cast<double>(m_stats.totalProcessingTime) / m_stats.framesProcessed;
        m_latency.store(static_cast<float>(avgProcessingTime / 1000.0));  // Convert to milliseconds
        
        // Estimate CPU usage (rough approximation)
        double frameTime = 1000000.0 * RNNOISE_FRAME_SIZE / m_sampleRate;  // Frame time in microseconds
        float cpuUsage = static_cast<float>((avgProcessingTime / frameTime) * 100.0);
        m_cpuUsage.store(std::min(cpuUsage, 100.0f));
    }
}

void NoiseReductionProcessor::applyReductionLevel(float* frame, int frameSize, float voiceProb) {
    // Get current configuration
    NoiseReductionConfig config;
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        config = m_config;
    }
    
    // Determine reduction strength based on level
    float reductionStrength = 1.0f;
    switch (config.level) {
        case NoiseReductionConfig::Level::Low:
            reductionStrength = 0.5f;
            break;
        case NoiseReductionConfig::Level::Medium:
            reductionStrength = 0.7f;
            break;
        case NoiseReductionConfig::Level::High:
            reductionStrength = 0.9f;
            break;
    }
    
    // Apply adaptive processing if enabled
    if (config.adaptiveMode) {
        // Reduce strength when voice is detected
        if (voiceProb > config.threshold) {
            reductionStrength *= (1.0f - voiceProb * 0.5f);
        }
    }
    
    // Apply additional attenuation for non-voice segments
    if (voiceProb < config.threshold) {
        for (int i = 0; i < frameSize; ++i) {
            frame[i] *= (1.0f - reductionStrength * 0.3f);
        }
    }
}

bool NoiseReductionProcessor::initializeRNNoise() {
    try {
        m_rnnoise = rnnoise_create(nullptr);  // Use default model
        return m_rnnoise != nullptr;
    } catch (...) {
        return false;
    }
}

void NoiseReductionProcessor::cleanupRNNoise() {
    if (m_rnnoise) {
        rnnoise_destroy(m_rnnoise);
        m_rnnoise = nullptr;
    }
}

void NoiseReductionProcessor::convertToRNNoiseFormat(const float* input, float* output, int numSamples) {
    // RNNoise expects specific format - for now just copy
    std::memcpy(output, input, numSamples * sizeof(float));
}

void NoiseReductionProcessor::convertFromRNNoiseFormat(const float* input, float* output, int numSamples) {
    // Convert back from RNNoise format - for now just copy
    std::memcpy(output, input, numSamples * sizeof(float));
}

float NoiseReductionProcessor::calculateReductionAmount(const AudioBuffer& processedBuffer) {
    // This is a simplified calculation - in practice you'd compare before/after
    // For now, return a placeholder value based on RMS level
    float rmsLevel = processedBuffer.getRMSLevel(0, 0, processedBuffer.getNumSamples());
    
    // Estimate reduction based on signal level (very rough approximation)
    if (rmsLevel < 0.01f) {
        return 20.0f;  // High reduction for very quiet signals
    } else if (rmsLevel < 0.1f) {
        return 10.0f;  // Medium reduction
    } else {
        return 5.0f;   // Low reduction for loud signals
    }
}

} // namespace core
} // namespace quiet