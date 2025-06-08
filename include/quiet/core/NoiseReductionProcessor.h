#pragma once

#include <rnnoise.h>
#include <memory>
#include <atomic>
#include <vector>
#include "AudioBuffer.h"
#include "EventDispatcher.h"

namespace quiet {
namespace core {

/**
 * @brief Configuration for noise reduction processing
 */
struct NoiseReductionConfig {
    enum class Level {
        Low,
        Medium,
        High
    };
    
    Level level = Level::Medium;
    bool enabled = true;
    float threshold = 0.5f;  // VAD threshold (0.0-1.0)
    bool adaptiveMode = true;
};

/**
 * @brief Statistics about noise reduction performance
 */
struct NoiseReductionStats {
    float reductionLevel = 0.0f;  // Current reduction in dB
    float averageReduction = 0.0f;  // Average reduction over time
    float voiceProbability = 0.0f;  // Voice activity detection probability
    uint64_t framesProcessed = 0;
    uint64_t totalProcessingTime = 0;  // Microseconds
};

/**
 * @brief High-performance real-time noise reduction processor
 * 
 * This class implements ML-based noise reduction using RNNoise:
 * - Real-time processing with <10ms latency
 * - Adaptive noise reduction levels
 * - Voice activity detection
 * - Performance monitoring and statistics
 */
class NoiseReductionProcessor {
public:
    NoiseReductionProcessor(EventDispatcher& eventDispatcher);
    ~NoiseReductionProcessor();

    // Initialization
    bool initialize(double sampleRate = 48000.0);
    void shutdown();
    bool isInitialized() const;

    // Configuration
    void setConfig(const NoiseReductionConfig& config);
    NoiseReductionConfig getConfig() const;
    
    void setEnabled(bool enabled);
    bool isEnabled() const;
    
    void setLevel(NoiseReductionConfig::Level level);
    NoiseReductionConfig::Level getLevel() const;

    // Processing
    bool process(AudioBuffer& buffer);
    bool processInPlace(float* samples, int numSamples);
    
    // Statistics
    NoiseReductionStats getStats() const;
    void resetStats();

    // Performance monitoring
    float getCpuUsage() const;
    float getLatency() const;

private:
    // Internal processing methods
    bool processFrame(float* frame, int frameSize);
    void updateStats(float reductionDb, float voiceProb, uint64_t processingTime);
    void applyReductionLevel(float* frame, int frameSize, float reduction);
    
    // RNNoise management
    bool initializeRNNoise();
    void cleanupRNNoise();
    
    // Audio format conversion
    void convertToRNNoiseFormat(const float* input, float* output, int numSamples);
    void convertFromRNNoiseFormat(const float* input, float* output, int numSamples);
    
    // Member variables
    EventDispatcher& m_eventDispatcher;
    
    // RNNoise state
    DenoiseState* m_rnnoise{nullptr};
    
    // Configuration
    NoiseReductionConfig m_config;
    std::atomic<bool> m_enabled{true};
    double m_sampleRate{48000.0};
    
    // Processing buffers
    static constexpr int RNNOISE_FRAME_SIZE = 480;  // 10ms at 48kHz
    std::vector<float> m_workingBuffer;
    std::vector<float> m_tempBuffer;
    
    // Statistics
    mutable std::mutex m_statsMutex;
    NoiseReductionStats m_stats;
    
    // Performance monitoring
    std::atomic<float> m_cpuUsage{0.0f};
    std::atomic<float> m_latency{0.0f};
    uint64_t m_lastProcessingTime{0};
    
    // Overlap-add state for frame-based processing
    std::vector<float> m_overlapBuffer;
    int m_overlapSize{0};
    
    bool m_isInitialized{false};
};

} // namespace core
} // namespace quiet