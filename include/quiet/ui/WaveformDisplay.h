#pragma once

#include <JuceHeader.h>
#include "quiet/core/AudioBuffer.h"

namespace quiet {
namespace ui {

/**
 * @brief Simple waveform display component
 * 
 * Displays audio waveforms with basic rendering for input/output visualization.
 */
class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay(const juce::String& title = "Waveform", 
                   const juce::Colour& waveColor = juce::Colours::cyan);
    ~WaveformDisplay() override;
    
    // Update the display with new audio data
    void updateBuffer(const core::AudioBuffer& buffer);
    
    // Clear the display
    void clear();
    
    // Set waveform color
    void setWaveformColor(const juce::Colour& color);
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    void timerCallback() override;
    
    juce::String m_title;
    juce::Colour m_waveformColor;
    juce::AudioBuffer<float> m_audioBuffer;
    juce::CriticalSection m_bufferLock;
    
    float m_currentLevel = 0.0f;
    static constexpr int m_bufferSize = 4096;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace ui
} // namespace quiet