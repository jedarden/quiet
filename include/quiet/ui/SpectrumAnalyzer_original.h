/*
  ==============================================================================

    SpectrumAnalyzer.h
    Created: 2025
    Author:  Quiet Development Team

    Real-time frequency spectrum analyzer component with FFT-based analysis,
    multiple visualization modes, and professional audio features.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>

class SpectrumAnalyzer : public juce::Component,
                        public juce::Timer,
                        public juce::OpenGLRenderer
{
public:
    //==============================================================================
    // FFT Size options
    enum class FFTSize
    {
        Size512 = 512,
        Size1024 = 1024,
        Size2048 = 2048,
        Size4096 = 4096,
        Size8192 = 8192
    };
    
    // Window function types
    enum class WindowType
    {
        Rectangular,
        Hanning,
        Hamming,
        Blackman,
        BlackmanHarris
    };
    
    // Visualization modes
    enum class VisualizationMode
    {
        Bars,
        Line,
        Waterfall
    };
    
    //==============================================================================
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override;
    
    //==============================================================================
    // Audio processing
    void pushNextSampleIntoFifo(float sample) noexcept;
    void processAudioBuffer(const juce::AudioBuffer<float>& buffer);
    void setSampleRate(double sampleRate);
    
    //==============================================================================
    // Configuration
    void setFFTSize(FFTSize size);
    void setWindowType(WindowType type);
    void setVisualizationMode(VisualizationMode mode);
    void setLogScale(bool useLogScale);
    void setPeakHold(bool enabled);
    void setSmoothing(float factor); // 0.0 to 0.99
    void setFrequencyRange(float minFreq, float maxFreq);
    void setDecibelRange(float minDb, float maxDb);
    
    //==============================================================================
    // UI Options
    void setShowGrid(bool show) { showGrid = show; repaint(); }
    void setShowLabels(bool show) { showLabels = show; repaint(); }
    
    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override { /* Handle resize if needed */ }
    
    //==============================================================================
    // Timer callback
    void timerCallback() override;
    
    //==============================================================================
    // OpenGL callbacks (optional for performance)
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    
private:
    //==============================================================================
    // FFT processing
    static constexpr int fftOrder = 11; // 2048 samples by default
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int scopeSize = 512; // Number of points to display
    static constexpr int waterfallHeight = 100; // Height of waterfall display
    
    juce::dsp::FFT forwardFFT;
    std::vector<float> window;
    
    juce::AudioBuffer<float> fifo;
    std::vector<float> fftData;
    std::vector<float> scopeData;
    std::vector<float> peakHoldData;
    std::vector<int> peakHoldTimers;
    std::vector<float> waterfallData;
    
    //==============================================================================
    // Configuration parameters
    float smoothingFactor;
    float decayRate;
    int peakHoldTime; // in milliseconds
    float minFrequency;
    float maxFrequency;
    float minDecibels;
    float maxDecibels;
    double currentSampleRate;
    
    VisualizationMode visualizationMode;
    WindowType windowType;
    bool isLogScale;
    bool showPeakHold;
    bool showGrid;
    bool showLabels;
    
    std::atomic<bool> nextFFTBlockReady;
    int waterfallPosition;
    
    //==============================================================================
    // Helper methods
    void updateWindowFunction();
    void processFFTData();
    void updatePeakHoldDecay();
    void updateWaterfallData();
    
    //==============================================================================
    // Drawing methods
    juce::Rectangle<float> getResponseArea(juce::Rectangle<float> bounds) const;
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> responseArea);
    void drawBars(juce::Graphics& g, juce::Rectangle<float> responseArea);
    void drawLine(juce::Graphics& g, juce::Rectangle<float> responseArea);
    void drawWaterfall(juce::Graphics& g, juce::Rectangle<float> responseArea);
    void drawPeakHold(juce::Graphics& g, juce::Rectangle<float> responseArea);
    void drawLabels(juce::Graphics& g, juce::Rectangle<float> bounds);
    
    //==============================================================================
    // Utility methods
    float frequencyToX(float frequency, juce::Rectangle<float> responseArea) const;
    float decibelToY(float db, juce::Rectangle<float> responseArea) const;
    float decibelToHeight(float db, juce::Rectangle<float> responseArea) const;
    juce::Colour getColorForLevel(float normalizedLevel) const;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};