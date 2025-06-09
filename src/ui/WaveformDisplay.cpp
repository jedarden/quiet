#include "quiet/ui/WaveformDisplay.h"
#include <algorithm>
#include <cmath>

namespace quiet {
namespace ui {

WaveformDisplay::WaveformDisplay(const juce::String& title, const juce::Colour& waveColor)
    : m_title(title), m_waveformColor(waveColor)
{
    m_audioBuffer.setSize(2, m_bufferSize);
    m_audioBuffer.clear();
    startTimerHz(30); // 30 FPS update rate
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Background
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    // Border
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(bounds, 1);
    
    // Title
    g.setColour(juce::Colour(0xff808080));
    g.setFont(12.0f);
    auto titleBounds = bounds.removeFromTop(20);
    g.drawText(m_title, titleBounds, juce::Justification::centred);
    
    // Waveform area
    const float width = static_cast<float>(bounds.getWidth());
    const float height = static_cast<float>(bounds.getHeight());
    const float midY = height * 0.5f;
    
    // Draw center line
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawHorizontalLine(static_cast<int>(bounds.getY() + midY), 
                        bounds.getX(), bounds.getRight());
    
    // Draw waveform
    g.setColour(m_waveformColor.withAlpha(0.8f));
    
    juce::Path waveform;
    const juce::ScopedLock sl(m_bufferLock);
    
    if (m_audioBuffer.getNumSamples() > 0)
    {
        const float* samples = m_audioBuffer.getReadPointer(0);
        const int numSamples = m_audioBuffer.getNumSamples();
        const float samplesPerPixel = numSamples / width;
        
        waveform.startNewSubPath(bounds.getX(), bounds.getY() + midY);
        
        for (int x = 0; x < width; ++x)
        {
            // Find min/max values for this pixel column
            int startSample = static_cast<int>(x * samplesPerPixel);
            int endSample = std::min(static_cast<int>((x + 1) * samplesPerPixel), numSamples);
            
            float minVal = 0.0f;
            float maxVal = 0.0f;
            
            for (int i = startSample; i < endSample; ++i)
            {
                float sample = samples[i];
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
            }
            
            // Scale and draw
            float yMin = bounds.getY() + midY - (maxVal * midY * 0.8f);
            float yMax = bounds.getY() + midY - (minVal * midY * 0.8f);
            
            if (x == 0)
            {
                waveform.startNewSubPath(bounds.getX() + x, yMin);
            }
            else
            {
                waveform.lineTo(bounds.getX() + x, yMin);
            }
            
            if (yMax != yMin)
            {
                waveform.lineTo(bounds.getX() + x, yMax);
            }
        }
        
        g.strokePath(waveform, juce::PathStrokeType(1.5f));
    }
    
    // Draw level meter at bottom
    if (m_currentLevel > 0.0f)
    {
        auto levelBounds = bounds.removeFromBottom(4).reduced(2, 0);
        
        // Background
        g.setColour(juce::Colour(0xff2d2d2d));
        g.fillRect(levelBounds);
        
        // Level bar
        float levelWidth = levelBounds.getWidth() * m_currentLevel;
        auto levelBar = levelBounds.withWidth(levelWidth);
        
        // Color based on level
        if (m_currentLevel < 0.6f)
            g.setColour(juce::Colour(0xff00ff00));
        else if (m_currentLevel < 0.9f)
            g.setColour(juce::Colour(0xffffff00));
        else
            g.setColour(juce::Colour(0xffff0000));
            
        g.fillRect(levelBar);
    }
}

void WaveformDisplay::resized()
{
    // Nothing special needed
}

void WaveformDisplay::updateBuffer(const AudioBuffer& buffer)
{
    const juce::ScopedLock sl(m_bufferLock);
    
    // Calculate RMS level
    m_currentLevel = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    
    // Copy buffer data for display
    int samplesToUse = std::min(buffer.getNumSamples(), m_bufferSize);
    
    // Simple downsampling if needed
    if (buffer.getNumSamples() > m_bufferSize)
    {
        float downsampleRatio = static_cast<float>(buffer.getNumSamples()) / m_bufferSize;
        
        for (int i = 0; i < m_bufferSize; ++i)
        {
            int sourceIndex = static_cast<int>(i * downsampleRatio);
            m_audioBuffer.setSample(0, i, buffer.getSample(0, sourceIndex));
        }
    }
    else
    {
        // Shift existing samples and add new ones
        int samplesToShift = m_bufferSize - samplesToUse;
        
        // Shift existing samples to the left
        for (int i = 0; i < samplesToShift; ++i)
        {
            m_audioBuffer.setSample(0, i, m_audioBuffer.getSample(0, i + samplesToUse));
        }
        
        // Add new samples
        for (int i = 0; i < samplesToUse; ++i)
        {
            m_audioBuffer.setSample(0, samplesToShift + i, buffer.getSample(0, i));
        }
    }
}

void WaveformDisplay::timerCallback()
{
    repaint();
}

void WaveformDisplay::clear()
{
    const juce::ScopedLock sl(m_bufferLock);
    m_audioBuffer.clear();
    m_currentLevel = 0.0f;
    repaint();
}

void WaveformDisplay::setWaveformColor(const juce::Colour& color)
{
    m_waveformColor = color;
    repaint();
}

} // namespace ui
} // namespace quiet