#pragma once

#include <JuceHeader.h>
#include "quiet/core/AudioBuffer.h"
#include <array>

namespace quiet {
namespace ui {

/**
 * @brief Simple spectrum analyzer component
 * 
 * Displays frequency spectrum using FFT analysis with basic bar visualization.
 */
class SpectrumAnalyzer : public juce::Component, private juce::Timer
{
public:
    SpectrumAnalyzer(const juce::Colour& barColor = juce::Colours::cyan);
    ~SpectrumAnalyzer() override;
    
    // Update the spectrum with new audio data
    void updateSpectrum(const core::AudioBuffer& buffer);
    
    // Clear the display
    void clear();
    
    // Set bar color
    void setBarColor(const juce::Colour& color);
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    void timerCallback() override;
    void createWindow();
    
    static constexpr int m_fftOrder = 11;
    static constexpr int m_fftSize = 1 << m_fftOrder; // 2048
    
    juce::dsp::FFT m_forwardFFT;
    std::array<float, m_fftSize * 2> m_fftData;
    std::array<float, m_fftSize / 2> m_smoothedMagnitudes;
    std::array<float, m_fftSize> m_window;
    
    juce::Colour m_barColor;
    juce::CriticalSection m_fftLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace ui
} // namespace quiet