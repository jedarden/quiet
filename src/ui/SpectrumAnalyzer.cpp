#include "quiet/ui/SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace quiet {
namespace ui {

SpectrumAnalyzer::SpectrumAnalyzer(const juce::Colour& barColor)
    : m_barColor(barColor), m_forwardFFT(m_fftOrder)
{
    std::fill(m_fftData.begin(), m_fftData.end(), 0.0f);
    std::fill(m_smoothedMagnitudes.begin(), m_smoothedMagnitudes.end(), 0.0f);
    
    // Create Hann window
    createWindow();
    
    startTimerHz(30); // 30 FPS update rate
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    stopTimer();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Background
    g.fillAll(juce::Colour(0xff1a1a1a));
    
    // Border
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(bounds, 1);
    
    const float width = static_cast<float>(bounds.getWidth());
    const float height = static_cast<float>(bounds.getHeight());
    
    // Draw spectrum bars
    const int numBins = m_fftSize / 2;
    const float binWidth = width / numBins;
    
    for (int i = 1; i < numBins; ++i)
    {
        const float magnitude = m_smoothedMagnitudes[i];
        const float db = 20.0f * std::log10(magnitude + 1e-6f);
        const float normalizedDb = juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f);
        const float barHeight = normalizedDb * height;
        
        // Color gradient based on frequency
        const float hue = i / static_cast<float>(numBins) * 0.3f; // Green to yellow
        g.setColour(juce::Colour::fromHSV(hue, 0.8f, 0.9f, 0.8f));
        
        const float x = i * binWidth;
        g.fillRect(x, height - barHeight, binWidth - 1.0f, barHeight);
    }
    
    // Draw frequency grid
    g.setColour(juce::Colour(0xff3d3d3d).withAlpha(0.5f));
    for (int freq = 1000; freq < 20000; freq += 1000)
    {
        const float x = (freq / 24000.0f) * width;
        g.drawVerticalLine(static_cast<int>(x), 0, height);
        
        if (freq % 5000 == 0)
        {
            g.setColour(juce::Colour(0xff808080));
            g.setFont(10.0f);
            g.drawText(juce::String(freq / 1000) + "k", 
                      static_cast<int>(x - 15), height - 20, 30, 20,
                      juce::Justification::centred);
            g.setColour(juce::Colour(0xff3d3d3d).withAlpha(0.5f));
        }
    }
}

void SpectrumAnalyzer::resized()
{
    // Nothing special needed
}

void SpectrumAnalyzer::updateSpectrum(const AudioBuffer& buffer)
{
    if (buffer.getNumSamples() < m_fftSize)
        return;
    
    const juce::ScopedLock sl(m_fftLock);
    
    // Copy and window the audio data
    const float* samples = buffer.getReadPointer(0);
    for (int i = 0; i < m_fftSize; ++i)
    {
        m_fftData[i] = samples[i] * m_window[i];
    }
    
    // Perform FFT
    m_forwardFFT.performRealOnlyForwardTransform(m_fftData.data());
    
    // Calculate magnitudes with smoothing
    for (int i = 0; i < m_fftSize / 2; ++i)
    {
        const float real = m_fftData[i * 2];
        const float imag = m_fftData[i * 2 + 1];
        const float magnitude = std::sqrt(real * real + imag * imag);
        
        // Smooth the spectrum
        m_smoothedMagnitudes[i] = m_smoothedMagnitudes[i] * 0.8f + magnitude * 0.2f;
    }
}

void SpectrumAnalyzer::timerCallback()
{
    repaint();
}

void SpectrumAnalyzer::clear()
{
    const juce::ScopedLock sl(m_fftLock);
    std::fill(m_smoothedMagnitudes.begin(), m_smoothedMagnitudes.end(), 0.0f);
    repaint();
}

void SpectrumAnalyzer::setBarColor(const juce::Colour& color)
{
    m_barColor = color;
    repaint();
}

void SpectrumAnalyzer::createWindow()
{
    // Hann window
    for (int i = 0; i < m_fftSize; ++i)
    {
        m_window[i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * i / (m_fftSize - 1));
    }
}

} // namespace ui
} // namespace quiet