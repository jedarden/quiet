#include "quiet/core/NoiseReductionProcessor.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <stdexcept>
#include <cmath>

// Include the actual RNNoise library
extern "C" {
#include <rnnoise.h>
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
    
    // Initialize frame buffers for stereo processing
    m_leftChannelBuffer.resize(RNNOISE_FRAME_SIZE, 0.0f);
    m_rightChannelBuffer.resize(RNNOISE_FRAME_SIZE, 0.0f);
    m_inputQueue.reserve(RNNOISE_FRAME_SIZE * 2);
    m_outputQueue.reserve(RNNOISE_FRAME_SIZE * 2);
}

NoiseReductionProcessor::~NoiseReductionProcessor() {
    shutdown();
}

bool NoiseReductionProcessor::initialize(double sampleRate) {
    if (m_isInitialized) {
        return true;  // Already initialized
    }
    
    m_sampleRate = sampleRate;
    
    // Check if sample rate conversion is needed
    m_needsResampling = (sampleRate != RNNOISE_SAMPLE_RATE);
    if (m_needsResampling) {
        // Initialize resampler for RNNoise's expected 48kHz
        m_resampleRatio = RNNOISE_SAMPLE_RATE / sampleRate;
        m_resampleBuffer.resize(static_cast<size_t>(RNNOISE_FRAME_SIZE * std::max(1.0, m_resampleRatio) * 2));
    }
    
    // Initialize RNNoise
    if (!initializeRNNoise()) {
        return false;
    }
    
    // Allocate working buffers
    m_workingBuffer.resize(RNNOISE_FRAME_SIZE);
    m_tempBuffer.resize(RNNOISE_FRAME_SIZE);
    m_floatToShortBuffer.resize(RNNOISE_FRAME_SIZE);
    m_shortToFloatBuffer.resize(RNNOISE_FRAME_SIZE);
    
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
    
    // Add incoming samples to input queue
    m_inputQueue.insert(m_inputQueue.end(), data, data + numSamples);
    
    int outputIndex = 0;
    
    // Process complete frames from the queue
    while (m_inputQueue.size() >= RNNOISE_FRAME_SIZE) {
        // Extract a frame from the queue
        std::copy(m_inputQueue.begin(), m_inputQueue.begin() + RNNOISE_FRAME_SIZE,
                 m_workingBuffer.begin());
        
        // Handle sample rate conversion if needed
        if (m_needsResampling) {
            resampleFrame(m_workingBuffer.data(), m_tempBuffer.data(), RNNOISE_FRAME_SIZE, true);
            
            // Process the resampled frame
            if (!processFrame(m_tempBuffer.data(), RNNOISE_FRAME_SIZE)) {
                return false;
            }
            
            // Resample back to original rate
            resampleFrame(m_tempBuffer.data(), m_workingBuffer.data(), RNNOISE_FRAME_SIZE, false);
        } else {
            // Process the frame directly
            if (!processFrame(m_workingBuffer.data(), RNNOISE_FRAME_SIZE)) {
                return false;
            }
        }
        
        // Add processed frame to output queue
        m_outputQueue.insert(m_outputQueue.end(), m_workingBuffer.begin(), m_workingBuffer.end());
        
        // Remove processed samples from input queue
        m_inputQueue.erase(m_inputQueue.begin(), m_inputQueue.begin() + RNNOISE_FRAME_SIZE);
    }
    
    // Copy available output samples back to the buffer
    int samplesToOutput = std::min(static_cast<int>(m_outputQueue.size()), numSamples);
    if (samplesToOutput > 0) {
        std::copy(m_outputQueue.begin(), m_outputQueue.begin() + samplesToOutput, data);
        m_outputQueue.erase(m_outputQueue.begin(), m_outputQueue.begin() + samplesToOutput);
        
        // Fill any remaining samples with zeros (latency compensation)
        if (samplesToOutput < numSamples) {
            std::fill(data + samplesToOutput, data + numSamples, 0.0f);
        }
    }
    
    return true;
}

bool NoiseReductionProcessor::processFrame(float* frame, int frameSize) {
    if (!m_rnnoise || frameSize != RNNOISE_FRAME_SIZE) {
        return false;
    }
    
    // Store pre-processed RMS for statistics
    float preRMS = calculateRMS(frame, frameSize);
    
    // Convert float samples to short for RNNoise
    convertFloatToShort(frame, m_floatToShortBuffer.data(), frameSize);
    
    // Apply RNNoise processing
    float voiceProb = rnnoise_process_frame(m_rnnoise, 
                                           m_floatToShortBuffer.data(), 
                                           m_floatToShortBuffer.data());
    
    // Convert back to float
    convertShortToFloat(m_floatToShortBuffer.data(), frame, frameSize);
    
    // Apply additional processing based on configuration
    applyReductionLevel(frame, frameSize, voiceProb);
    
    // Update VAD state
    updateVADState(voiceProb);
    
    // Calculate post-processed RMS for statistics
    float postRMS = calculateRMS(frame, frameSize);
    float reductionDb = 20.0f * log10f(std::max(preRMS / std::max(postRMS, 1e-10f), 1e-10f));
    
    // Store statistics for this frame
    m_lastVoiceProb = voiceProb;
    m_lastReductionDb = reductionDb;
    
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
        // Create RNNoise instances for each channel if processing stereo
        m_rnnoise = rnnoise_create(nullptr);  // Use default model
        if (!m_rnnoise) {
            return false;
        }
        
        // Create second instance for stereo processing
        m_rnnoiseRight = rnnoise_create(nullptr);
        if (!m_rnnoiseRight) {
            rnnoise_destroy(m_rnnoise);
            m_rnnoise = nullptr;
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

void NoiseReductionProcessor::cleanupRNNoise() {
    if (m_rnnoise) {
        rnnoise_destroy(m_rnnoise);
        m_rnnoise = nullptr;
    }
    if (m_rnnoiseRight) {
        rnnoise_destroy(m_rnnoiseRight);
        m_rnnoiseRight = nullptr;
    }
}

void NoiseReductionProcessor::convertFloatToShort(const float* input, short* output, int numSamples) {
    // Convert float [-1.0, 1.0] to short [-32768, 32767]
    constexpr float SCALE = 32767.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        float sample = input[i] * SCALE;
        // Clamp to prevent overflow
        sample = std::max(-32768.0f, std::min(32767.0f, sample));
        output[i] = static_cast<short>(sample);
    }
}

void NoiseReductionProcessor::convertShortToFloat(const short* input, float* output, int numSamples) {
    // Convert short [-32768, 32767] to float [-1.0, 1.0]
    constexpr float INV_SCALE = 1.0f / 32768.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        output[i] = static_cast<float>(input[i]) * INV_SCALE;
    }
}

void NoiseReductionProcessor::resampleFrame(const float* input, float* output, int frameSize, bool upsample) {
    // Simple linear interpolation resampling
    if (upsample && m_resampleRatio > 1.0) {
        // Upsample to 48kHz
        int outputSize = static_cast<int>(frameSize * m_resampleRatio);
        for (int i = 0; i < outputSize; ++i) {
            float srcIndex = i / m_resampleRatio;
            int srcIdx = static_cast<int>(srcIndex);
            float frac = srcIndex - srcIdx;
            
            if (srcIdx < frameSize - 1) {
                output[i] = input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac;
            } else {
                output[i] = input[frameSize - 1];
            }
        }
    } else if (!upsample && m_resampleRatio > 1.0) {
        // Downsample from 48kHz
        for (int i = 0; i < frameSize; ++i) {
            float srcIndex = i * m_resampleRatio;
            int srcIdx = static_cast<int>(srcIndex);
            float frac = srcIndex - srcIdx;
            
            int inputSize = static_cast<int>(frameSize * m_resampleRatio);
            if (srcIdx < inputSize - 1) {
                output[i] = input[srcIdx] * (1.0f - frac) + input[srcIdx + 1] * frac;
            } else {
                output[i] = input[inputSize - 1];
            }
        }
    } else {
        // No resampling needed
        std::memcpy(output, input, frameSize * sizeof(float));
    }
}

float NoiseReductionProcessor::calculateRMS(const float* samples, int numSamples) {
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        sum += samples[i] * samples[i];
    }
    return std::sqrt(sum / numSamples);
}

void NoiseReductionProcessor::updateVADState(float voiceProb) {
    // Update VAD history for adaptive mode
    m_vadHistory.push_back(voiceProb);
    if (m_vadHistory.size() > VAD_HISTORY_SIZE) {
        m_vadHistory.erase(m_vadHistory.begin());
    }
    
    // Calculate average VAD probability
    float avgVAD = 0.0f;
    for (float prob : m_vadHistory) {
        avgVAD += prob;
    }
    avgVAD /= m_vadHistory.size();
    
    // Update voice detected state with hysteresis
    if (avgVAD > m_config.threshold + 0.1f) {
        m_voiceDetected = true;
    } else if (avgVAD < m_config.threshold - 0.1f) {
        m_voiceDetected = false;
    }
}

float NoiseReductionProcessor::calculateReductionAmount(const AudioBuffer& processedBuffer) {
    // Return the actual reduction calculated during processing
    return m_lastReductionDb;
}

bool NoiseReductionProcessor::processStereoBuffer(AudioBuffer& stereoBuffer) {
    if (stereoBuffer.getNumChannels() != 2) {
        return false;
    }
    
    const int numSamples = stereoBuffer.getNumSamples();
    float* leftData = stereoBuffer.getWritePointer(0);
    float* rightData = stereoBuffer.getWritePointer(1);
    
    // Add incoming samples to input queues
    m_leftInputQueue.insert(m_leftInputQueue.end(), leftData, leftData + numSamples);
    m_rightInputQueue.insert(m_rightInputQueue.end(), rightData, rightData + numSamples);
    
    // Process complete frames from both channels
    while (m_leftInputQueue.size() >= RNNOISE_FRAME_SIZE && 
           m_rightInputQueue.size() >= RNNOISE_FRAME_SIZE) {
        
        // Extract frames from both channels
        std::copy(m_leftInputQueue.begin(), m_leftInputQueue.begin() + RNNOISE_FRAME_SIZE,
                 m_leftChannelBuffer.begin());
        std::copy(m_rightInputQueue.begin(), m_rightInputQueue.begin() + RNNOISE_FRAME_SIZE,
                 m_rightChannelBuffer.begin());
        
        // Process left channel
        if (!processFrameStereo(m_leftChannelBuffer.data(), m_rnnoise, RNNOISE_FRAME_SIZE)) {
            return false;
        }
        
        // Process right channel
        if (!processFrameStereo(m_rightChannelBuffer.data(), m_rnnoiseRight, RNNOISE_FRAME_SIZE)) {
            return false;
        }
        
        // Add processed frames to output queues
        m_leftOutputQueue.insert(m_leftOutputQueue.end(), 
                                m_leftChannelBuffer.begin(), m_leftChannelBuffer.end());
        m_rightOutputQueue.insert(m_rightOutputQueue.end(), 
                                 m_rightChannelBuffer.begin(), m_rightChannelBuffer.end());
        
        // Remove processed samples from input queues
        m_leftInputQueue.erase(m_leftInputQueue.begin(), 
                              m_leftInputQueue.begin() + RNNOISE_FRAME_SIZE);
        m_rightInputQueue.erase(m_rightInputQueue.begin(), 
                               m_rightInputQueue.begin() + RNNOISE_FRAME_SIZE);
    }
    
    // Copy available output samples back to the buffer
    int samplesToOutput = std::min(static_cast<int>(m_leftOutputQueue.size()), numSamples);
    if (samplesToOutput > 0) {
        std::copy(m_leftOutputQueue.begin(), m_leftOutputQueue.begin() + samplesToOutput, leftData);
        std::copy(m_rightOutputQueue.begin(), m_rightOutputQueue.begin() + samplesToOutput, rightData);
        
        m_leftOutputQueue.erase(m_leftOutputQueue.begin(), 
                               m_leftOutputQueue.begin() + samplesToOutput);
        m_rightOutputQueue.erase(m_rightOutputQueue.begin(), 
                                m_rightOutputQueue.begin() + samplesToOutput);
        
        // Fill any remaining samples with zeros
        if (samplesToOutput < numSamples) {
            std::fill(leftData + samplesToOutput, leftData + numSamples, 0.0f);
            std::fill(rightData + samplesToOutput, rightData + numSamples, 0.0f);
        }
    }
    
    return true;
}

bool NoiseReductionProcessor::processFrameStereo(float* frame, DenoiseState* state, int frameSize) {
    if (!state || frameSize != RNNOISE_FRAME_SIZE) {
        return false;
    }
    
    // Convert and process similar to mono, but with specific state
    std::vector<short> tempShortBuffer(frameSize);
    convertFloatToShort(frame, tempShortBuffer.data(), frameSize);
    
    float voiceProb = rnnoise_process_frame(state, tempShortBuffer.data(), tempShortBuffer.data());
    
    convertShortToFloat(tempShortBuffer.data(), frame, frameSize);
    applyReductionLevel(frame, frameSize, voiceProb);
    
    return true;
}

} // namespace core
} // namespace quiet