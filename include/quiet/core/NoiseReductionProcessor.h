#pragma once

#include <memory>
#include <atomic>
#include <vector>
#include <mutex>
#include "AudioBuffer.h"
#include "EventDispatcher.h"

// Forward declaration for RNNoise
extern "C" {
    typedef struct DenoiseState DenoiseState;
}

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
    bool processMonoBuffer(AudioBuffer& monoBuffer);
    bool processStereoBuffer(AudioBuffer& stereoBuffer);
    bool processFrame(float* frame, int frameSize);
    bool processFrameStereo(float* frame, DenoiseState* state, int frameSize);
    void updateStats(float reductionDb, float voiceProb, uint64_t processingTime);
    void applyReductionLevel(float* frame, int frameSize, float voiceProb);
    
    // RNNoise management
    bool initializeRNNoise();
    void cleanupRNNoise();
    
    // Audio format conversion
    void convertFloatToShort(const float* input, short* output, int numSamples);
    void convertShortToFloat(const short* input, float* output, int numSamples);
    void resampleFrame(const float* input, float* output, int frameSize, bool upsample);
    
    // Helper methods
    float calculateRMS(const float* samples, int numSamples);
    float calculateReductionAmount(const AudioBuffer& processedBuffer);
    void updateVADState(float voiceProb);
    
    // Member variables
    EventDispatcher& m_eventDispatcher;
    
    // RNNoise state
    DenoiseState* m_rnnoise{nullptr};
    DenoiseState* m_rnnoiseRight{nullptr};  // For stereo processing
    
    // Configuration
    NoiseReductionConfig m_config;
    std::atomic<bool> m_enabled{true};
    double m_sampleRate{48000.0};
    
    // Processing constants
    static constexpr int RNNOISE_FRAME_SIZE = 480;  // 10ms at 48kHz
    static constexpr int RNNOISE_SAMPLE_RATE = 48000;
    static constexpr int VAD_HISTORY_SIZE = 10;
    
    // Processing buffers
    std::vector<float> m_workingBuffer;
    std::vector<float> m_tempBuffer;
    std::vector<short> m_floatToShortBuffer;
    std::vector<short> m_shortToFloatBuffer;
    
    // Frame buffering for stereo
    std::vector<float> m_leftChannelBuffer;
    std::vector<float> m_rightChannelBuffer;
    
    // Input/output queues for frame-based processing
    std::vector<float> m_inputQueue;
    std::vector<float> m_outputQueue;
    std::vector<float> m_leftInputQueue;
    std::vector<float> m_rightInputQueue;
    std::vector<float> m_leftOutputQueue;
    std::vector<float> m_rightOutputQueue;
    
    // Resampling support
    bool m_needsResampling{false};
    double m_resampleRatio{1.0};
    std::vector<float> m_resampleBuffer;
    
    // VAD state
    std::vector<float> m_vadHistory;
    bool m_voiceDetected{false};
    float m_lastVoiceProb{0.0f};
    float m_lastReductionDb{0.0f};
    
    // Statistics
    mutable std::mutex m_statsMutex;
    NoiseReductionStats m_stats;
    
    // Performance monitoring
    std::atomic<float> m_cpuUsage{0.0f};
    std::atomic<float> m_latency{0.0f};
    uint64_t m_lastProcessingTime{0};
    
    bool m_isInitialized{false};
};

} // namespace core
} // namespace quiet