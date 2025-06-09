/*
  ==============================================================================

    SpectrumAnalyzer.cpp
    Created: 2025
    Author:  Quiet Development Team

    Real-time frequency spectrum analyzer with FFT-based analysis,
    multiple visualization modes, and professional audio features.

  ==============================================================================
*/

#include "SpectrumAnalyzer.h"
#include <algorithm>
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer()
    : forwardFFT(fftOrder),
      window(fftSize),
      fifo(fftSize),
      fftData(fftSize * 2),
      scopeData(scopeSize),
      peakHoldData(scopeSize),
      waterfallData(waterfallHeight * scopeSize),
      smoothingFactor(0.8f),
      decayRate(0.95f),
      peakHoldTime(2000),
      minFrequency(20.0f),
      maxFrequency(20000.0f),
      minDecibels(-100.0f),
      maxDecibels(0.0f),
      currentSampleRate(48000.0),
      visualizationMode(VisualizationMode::Bars),
      windowType(WindowType::Hanning),
      isLogScale(true),
      showPeakHold(true),
      showGrid(true),
      showLabels(true),
      nextFFTBlockReady(false),
      waterfallPosition(0)
{
    // Initialize window function
    updateWindowFunction();
    
    // Initialize peak hold timers
    peakHoldTimers.resize(scopeSize, 0);
    
    // Set up OpenGL context if available
    setOpaque(true);
    
    // Start timer for decay animations
    startTimerHz(60);
}

SpectrumAnalyzer::~SpectrumAnalyzer()
{
    shutdownOpenGL();
}

void SpectrumAnalyzer::pushNextSampleIntoFifo(float sample) noexcept
{
    if (fifo.getFreeSpace() >= 1)
    {
        fifo.write(&sample, 1);
        
        if (fifo.getNumReady() >= fftSize)
        {
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            fifo.read(fftData.data(), fftSize);
            nextFFTBlockReady = true;
        }
    }
}

void SpectrumAnalyzer::processAudioBuffer(const juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float mixedSample = 0.0f;
        
        // Mix all channels to mono
        for (int channel = 0; channel < numChannels; ++channel)
        {
            mixedSample += buffer.getSample(channel, sample);
        }
        
        mixedSample /= static_cast<float>(numChannels);
        pushNextSampleIntoFifo(mixedSample);
    }
}

void SpectrumAnalyzer::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
}

void SpectrumAnalyzer::setFFTSize(FFTSize size)
{
    switch (size)
    {
        case FFTSize::Size512:
            fftOrder = 9;
            break;
        case FFTSize::Size1024:
            fftOrder = 10;
            break;
        case FFTSize::Size2048:
            fftOrder = 11;
            break;
        case FFTSize::Size4096:
            fftOrder = 12;
            break;
        case FFTSize::Size8192:
            fftOrder = 13;
            break;
    }
    
    fftSize = 1 << fftOrder;
    forwardFFT = juce::dsp::FFT(fftOrder);
    window.resize(fftSize);
    fifo.setSize(fftSize);
    fftData.resize(fftSize * 2);
    
    updateWindowFunction();
}

void SpectrumAnalyzer::setWindowType(WindowType type)
{
    windowType = type;
    updateWindowFunction();
}

void SpectrumAnalyzer::setVisualizationMode(VisualizationMode mode)
{
    visualizationMode = mode;
    repaint();
}

void SpectrumAnalyzer::setLogScale(bool useLogScale)
{
    isLogScale = useLogScale;
    repaint();
}

void SpectrumAnalyzer::setPeakHold(bool enabled)
{
    showPeakHold = enabled;
    if (!enabled)
    {
        // Clear peak hold data
        std::fill(peakHoldData.begin(), peakHoldData.end(), minDecibels);
        std::fill(peakHoldTimers.begin(), peakHoldTimers.end(), 0);
    }
    repaint();
}

void SpectrumAnalyzer::setSmoothing(float factor)
{
    smoothingFactor = juce::jlimit(0.0f, 0.99f, factor);
}

void SpectrumAnalyzer::setFrequencyRange(float minFreq, float maxFreq)
{
    minFrequency = minFreq;
    maxFrequency = maxFreq;
    repaint();
}

void SpectrumAnalyzer::setDecibelRange(float minDb, float maxDb)
{
    minDecibels = minDb;
    maxDecibels = maxDb;
    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1e1e1e));
    
    const auto bounds = getLocalBounds().toFloat();
    const auto responseArea = getResponseArea(bounds);
    
    // Draw grid if enabled
    if (showGrid)
    {
        drawGrid(g, responseArea);
    }
    
    // Draw spectrum based on visualization mode
    switch (visualizationMode)
    {
        case VisualizationMode::Bars:
            drawBars(g, responseArea);
            break;
        case VisualizationMode::Line:
            drawLine(g, responseArea);
            break;
        case VisualizationMode::Waterfall:
            drawWaterfall(g, responseArea);
            break;
    }
    
    // Draw peak hold if enabled
    if (showPeakHold && visualizationMode != VisualizationMode::Waterfall)
    {
        drawPeakHold(g, responseArea);
    }
    
    // Draw labels if enabled
    if (showLabels)
    {
        drawLabels(g, bounds);
    }
    
    // Draw border
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRect(responseArea, 1.0f);
}

void SpectrumAnalyzer::timerCallback()
{
    if (nextFFTBlockReady)
    {
        processFFTData();
        nextFFTBlockReady = false;
        repaint();
    }
    
    // Update peak hold decay
    if (showPeakHold)
    {
        updatePeakHoldDecay();
    }
}

void SpectrumAnalyzer::updateWindowFunction()
{
    switch (windowType)
    {
        case WindowType::Rectangular:
            std::fill(window.begin(), window.end(), 1.0f);
            break;
            
        case WindowType::Hanning:
            for (int i = 0; i < fftSize; ++i)
            {
                window[i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1));
            }
            break;
            
        case WindowType::Hamming:
            for (int i = 0; i < fftSize; ++i)
            {
                window[i] = 0.54f - 0.46f * std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1));
            }
            break;
            
        case WindowType::Blackman:
            for (int i = 0; i < fftSize; ++i)
            {
                const float phase = 2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1);
                window[i] = 0.42f - 0.5f * std::cos(phase) + 0.08f * std::cos(2.0f * phase);
            }
            break;
            
        case WindowType::BlackmanHarris:
            for (int i = 0; i < fftSize; ++i)
            {
                const float phase = 2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1);
                window[i] = 0.35875f - 0.48829f * std::cos(phase) + 
                           0.14128f * std::cos(2.0f * phase) - 0.01168f * std::cos(3.0f * phase);
            }
            break;
    }
}

void SpectrumAnalyzer::processFFTData()
{
    // Apply window function
    for (int i = 0; i < fftSize; ++i)
    {
        fftData[i] *= window[i];
    }
    
    // Perform FFT
    forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
    
    // Process magnitude spectrum
    const float binWidth = currentSampleRate / fftSize;
    const int numBins = fftSize / 2;
    
    for (int i = 0; i < scopeSize; ++i)
    {
        float frequency;
        
        if (isLogScale)
        {
            // Logarithmic frequency mapping
            const float logMin = std::log10(minFrequency);
            const float logMax = std::log10(maxFrequency);
            const float logFreq = logMin + (logMax - logMin) * i / scopeSize;
            frequency = std::pow(10.0f, logFreq);
        }
        else
        {
            // Linear frequency mapping
            frequency = minFrequency + (maxFrequency - minFrequency) * i / scopeSize;
        }
        
        // Find the bin for this frequency
        const int bin = static_cast<int>(frequency / binWidth);
        
        if (bin < numBins)
        {
            // Calculate magnitude in dB
            const float magnitude = fftData[bin];
            const float magnitudeDb = 20.0f * std::log10(magnitude + 1e-6f);
            
            // Apply smoothing
            const float smoothedValue = smoothingFactor * scopeData[i] + 
                                       (1.0f - smoothingFactor) * magnitudeDb;
            
            scopeData[i] = juce::jlimit(minDecibels, maxDecibels, smoothedValue);
            
            // Update peak hold
            if (showPeakHold && scopeData[i] > peakHoldData[i])
            {
                peakHoldData[i] = scopeData[i];
                peakHoldTimers[i] = peakHoldTime;
            }
        }
    }
    
    // Update waterfall data
    if (visualizationMode == VisualizationMode::Waterfall)
    {
        updateWaterfallData();
    }
}

void SpectrumAnalyzer::updatePeakHoldDecay()
{
    for (int i = 0; i < scopeSize; ++i)
    {
        if (peakHoldTimers[i] > 0)
        {
            peakHoldTimers[i] -= 16; // Approximately 60 Hz timer
        }
        else
        {
            // Apply decay
            peakHoldData[i] *= decayRate;
            if (peakHoldData[i] < minDecibels)
            {
                peakHoldData[i] = minDecibels;
            }
        }
    }
}

void SpectrumAnalyzer::updateWaterfallData()
{
    // Shift waterfall data down
    std::copy(waterfallData.begin() + scopeSize, waterfallData.end(), waterfallData.begin());
    
    // Add new line at the top
    const int topLineStart = (waterfallHeight - 1) * scopeSize;
    for (int i = 0; i < scopeSize; ++i)
    {
        waterfallData[topLineStart + i] = scopeData[i];
    }
}

juce::Rectangle<float> SpectrumAnalyzer::getResponseArea(juce::Rectangle<float> bounds) const
{
    bounds.removeFromTop(4);
    bounds.removeFromBottom(showLabels ? 20 : 4);
    bounds.removeFromLeft(showLabels ? 50 : 4);
    bounds.removeFromRight(4);
    
    return bounds;
}

void SpectrumAnalyzer::drawGrid(juce::Graphics& g, juce::Rectangle<float> responseArea)
{
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    
    // Frequency grid lines
    const std::vector<float> frequencies = {100, 1000, 10000};
    for (auto freq : frequencies)
    {
        if (freq >= minFrequency && freq <= maxFrequency)
        {
            const float x = frequencyToX(freq, responseArea);
            g.drawVerticalLine(static_cast<int>(x), responseArea.getY(), responseArea.getBottom());
        }
    }
    
    // Decibel grid lines
    for (float db = -80; db <= 0; db += 20)
    {
        if (db >= minDecibels && db <= maxDecibels)
        {
            const float y = decibelToY(db, responseArea);
            g.drawHorizontalLine(static_cast<int>(y), responseArea.getX(), responseArea.getRight());
        }
    }
}

void SpectrumAnalyzer::drawBars(juce::Graphics& g, juce::Rectangle<float> responseArea)
{
    const float barWidth = responseArea.getWidth() / scopeSize;
    
    for (int i = 0; i < scopeSize; ++i)
    {
        const float x = responseArea.getX() + i * barWidth;
        const float barHeight = decibelToHeight(scopeData[i], responseArea);
        const float y = responseArea.getBottom() - barHeight;
        
        // Color based on level
        const float normalizedLevel = (scopeData[i] - minDecibels) / (maxDecibels - minDecibels);
        const auto color = getColorForLevel(normalizedLevel);
        
        g.setColour(color);
        g.fillRect(x, y, barWidth - 1.0f, barHeight);
    }
}

void SpectrumAnalyzer::drawLine(juce::Graphics& g, juce::Rectangle<float> responseArea)
{
    juce::Path spectrumPath;
    
    const float xIncrement = responseArea.getWidth() / scopeSize;
    
    for (int i = 0; i < scopeSize; ++i)
    {
        const float x = responseArea.getX() + i * xIncrement;
        const float y = decibelToY(scopeData[i], responseArea);
        
        if (i == 0)
            spectrumPath.startNewSubPath(x, y);
        else
            spectrumPath.lineTo(x, y);
    }
    
    // Fill area under curve
    g.setColour(juce::Colours::cyan.withAlpha(0.2f));
    juce::Path fillPath = spectrumPath;
    fillPath.lineTo(responseArea.getRight(), responseArea.getBottom());
    fillPath.lineTo(responseArea.getX(), responseArea.getBottom());
    fillPath.closeSubPath();
    g.fillPath(fillPath);
    
    // Draw line
    g.setColour(juce::Colours::cyan);
    g.strokePath(spectrumPath, juce::PathStrokeType(2.0f));
}

void SpectrumAnalyzer::drawWaterfall(juce::Graphics& g, juce::Rectangle<float> responseArea)
{
    const float rowHeight = responseArea.getHeight() / waterfallHeight;
    const float colWidth = responseArea.getWidth() / scopeSize;
    
    for (int row = 0; row < waterfallHeight; ++row)
    {
        const float y = responseArea.getY() + row * rowHeight;
        
        for (int col = 0; col < scopeSize; ++col)
        {
            const float x = responseArea.getX() + col * colWidth;
            const int dataIndex = row * scopeSize + col;
            
            const float normalizedLevel = (waterfallData[dataIndex] - minDecibels) / 
                                        (maxDecibels - minDecibels);
            const auto color = getColorForLevel(normalizedLevel);
            
            g.setColour(color);
            g.fillRect(x, y, colWidth, rowHeight);
        }
    }
}

void SpectrumAnalyzer::drawPeakHold(juce::Graphics& g, juce::Rectangle<float> responseArea)
{
    g.setColour(juce::Colours::yellow.withAlpha(0.8f));
    
    const float xIncrement = responseArea.getWidth() / scopeSize;
    
    for (int i = 0; i < scopeSize; ++i)
    {
        const float x = responseArea.getX() + i * xIncrement;
        const float y = decibelToY(peakHoldData[i], responseArea);
        
        g.fillRect(x, y - 1, xIncrement - 1, 2.0f);
    }
}

void SpectrumAnalyzer::drawLabels(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(10.0f);
    
    const auto responseArea = getResponseArea(bounds);
    
    // Frequency labels
    const std::vector<float> freqLabels = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    
    for (auto freq : freqLabels)
    {
        if (freq >= minFrequency && freq <= maxFrequency)
        {
            const float x = frequencyToX(freq, responseArea);
            
            juce::String label;
            if (freq >= 1000)
                label = juce::String(freq / 1000) + "k";
            else
                label = juce::String(static_cast<int>(freq));
            
            g.drawText(label, static_cast<int>(x - 20), static_cast<int>(responseArea.getBottom() + 2),
                      40, 18, juce::Justification::centred);
        }
    }
    
    // Decibel labels
    for (float db = minDecibels; db <= maxDecibels; db += 20)
    {
        const float y = decibelToY(db, responseArea);
        
        g.drawText(juce::String(static_cast<int>(db)) + " dB",
                  2, static_cast<int>(y - 9), 45, 18,
                  juce::Justification::right);
    }
    
    // Axis labels
    g.setFont(12.0f);
    g.drawText("Frequency (Hz)", responseArea.getCentreX() - 50, bounds.getBottom() - 20,
              100, 20, juce::Justification::centred);
}

float SpectrumAnalyzer::frequencyToX(float frequency, juce::Rectangle<float> responseArea) const
{
    float normalizedFreq;
    
    if (isLogScale)
    {
        const float logMin = std::log10(minFrequency);
        const float logMax = std::log10(maxFrequency);
        const float logFreq = std::log10(frequency);
        normalizedFreq = (logFreq - logMin) / (logMax - logMin);
    }
    else
    {
        normalizedFreq = (frequency - minFrequency) / (maxFrequency - minFrequency);
    }
    
    return responseArea.getX() + normalizedFreq * responseArea.getWidth();
}

float SpectrumAnalyzer::decibelToY(float db, juce::Rectangle<float> responseArea) const
{
    const float normalizedDb = 1.0f - (db - minDecibels) / (maxDecibels - minDecibels);
    return responseArea.getY() + normalizedDb * responseArea.getHeight();
}

float SpectrumAnalyzer::decibelToHeight(float db, juce::Rectangle<float> responseArea) const
{
    const float normalizedDb = (db - minDecibels) / (maxDecibels - minDecibels);
    return normalizedDb * responseArea.getHeight();
}

juce::Colour SpectrumAnalyzer::getColorForLevel(float normalizedLevel) const
{
    // Create a gradient from dark blue through cyan, green, yellow to red
    if (normalizedLevel < 0.25f)
    {
        // Dark blue to cyan
        const float t = normalizedLevel * 4.0f;
        return juce::Colour::fromHSV(0.55f, 1.0f - t * 0.3f, 0.3f + t * 0.4f, 1.0f);
    }
    else if (normalizedLevel < 0.5f)
    {
        // Cyan to green
        const float t = (normalizedLevel - 0.25f) * 4.0f;
        return juce::Colour::fromHSV(0.55f - t * 0.22f, 0.7f, 0.7f + t * 0.2f, 1.0f);
    }
    else if (normalizedLevel < 0.75f)
    {
        // Green to yellow
        const float t = (normalizedLevel - 0.5f) * 4.0f;
        return juce::Colour::fromHSV(0.33f - t * 0.16f, 0.7f - t * 0.1f, 0.9f, 1.0f);
    }
    else
    {
        // Yellow to red
        const float t = (normalizedLevel - 0.75f) * 4.0f;
        return juce::Colour::fromHSV(0.17f - t * 0.17f, 0.6f + t * 0.4f, 0.9f + t * 0.1f, 1.0f);
    }
}

// OpenGL rendering methods (optional, for performance)
void SpectrumAnalyzer::newOpenGLContextCreated()
{
    // Initialize OpenGL resources if needed
}

void SpectrumAnalyzer::renderOpenGL()
{
    // Implement OpenGL rendering for better performance if needed
}

void SpectrumAnalyzer::openGLContextClosing()
{
    // Clean up OpenGL resources
}