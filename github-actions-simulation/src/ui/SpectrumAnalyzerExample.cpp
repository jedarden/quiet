/*
  ==============================================================================

    SpectrumAnalyzerExample.cpp
    Created: 2025
    Author:  Quiet Development Team

    Example usage of the SpectrumAnalyzer component in the QUIET application.

  ==============================================================================
*/

#include "SpectrumAnalyzer.h"

/**
 * Example of how to integrate the SpectrumAnalyzer into your main window
 */
class MainAudioWindow : public juce::Component,
                       public juce::AudioIODeviceCallback
{
public:
    MainAudioWindow()
    {
        // Add spectrum analyzer to the window
        addAndMakeVisible(spectrumAnalyzer);
        
        // Configure spectrum analyzer
        spectrumAnalyzer.setFFTSize(SpectrumAnalyzer::FFTSize::Size2048);
        spectrumAnalyzer.setWindowType(SpectrumAnalyzer::WindowType::Hanning);
        spectrumAnalyzer.setVisualizationMode(SpectrumAnalyzer::VisualizationMode::Bars);
        spectrumAnalyzer.setLogScale(true);
        spectrumAnalyzer.setPeakHold(true);
        spectrumAnalyzer.setSmoothing(0.8f);
        spectrumAnalyzer.setFrequencyRange(20.0f, 20000.0f);
        spectrumAnalyzer.setDecibelRange(-100.0f, 0.0f);
        
        // Create control panel
        createControls();
        
        setSize(800, 600);
    }
    
    ~MainAudioWindow() override
    {
        // Clean up audio device
        audioDeviceManager.removeAudioCallback(this);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        
        // Control panel at top
        controlPanel.setBounds(bounds.removeFromTop(100));
        
        // Spectrum analyzer takes remaining space
        spectrumAnalyzer.setBounds(bounds.reduced(10));
    }
    
    //==============================================================================
    // Audio callbacks
    void audioDeviceIOCallback(const float** inputChannelData,
                              int numInputChannels,
                              float** outputChannelData,
                              int numOutputChannels,
                              int numSamples) override
    {
        // Create a temporary buffer for the spectrum analyzer
        juce::AudioBuffer<float> buffer(numInputChannels, numSamples);
        
        // Copy input data to buffer
        for (int channel = 0; channel < numInputChannels; ++channel)
        {
            if (inputChannelData[channel] != nullptr)
            {
                buffer.copyFrom(channel, 0, inputChannelData[channel], numSamples);
            }
        }
        
        // Send to spectrum analyzer
        spectrumAnalyzer.processAudioBuffer(buffer);
        
        // Clear output (or process audio as needed)
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            if (outputChannelData[channel] != nullptr)
            {
                juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
            }
        }
    }
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        spectrumAnalyzer.setSampleRate(device->getCurrentSampleRate());
    }
    
    void audioDeviceStopped() override {}
    
private:
    void createControls()
    {
        addAndMakeVisible(controlPanel);
        
        // FFT Size selector
        addAndMakeVisible(fftSizeLabel);
        fftSizeLabel.setText("FFT Size:", juce::dontSendNotification);
        fftSizeLabel.attachToComponent(&fftSizeCombo, true);
        
        addAndMakeVisible(fftSizeCombo);
        fftSizeCombo.addItem("512", 1);
        fftSizeCombo.addItem("1024", 2);
        fftSizeCombo.addItem("2048", 3);
        fftSizeCombo.addItem("4096", 4);
        fftSizeCombo.addItem("8192", 5);
        fftSizeCombo.setSelectedId(3); // 2048 default
        fftSizeCombo.onChange = [this]()
        {
            switch (fftSizeCombo.getSelectedId())
            {
                case 1: spectrumAnalyzer.setFFTSize(SpectrumAnalyzer::FFTSize::Size512); break;
                case 2: spectrumAnalyzer.setFFTSize(SpectrumAnalyzer::FFTSize::Size1024); break;
                case 3: spectrumAnalyzer.setFFTSize(SpectrumAnalyzer::FFTSize::Size2048); break;
                case 4: spectrumAnalyzer.setFFTSize(SpectrumAnalyzer::FFTSize::Size4096); break;
                case 5: spectrumAnalyzer.setFFTSize(SpectrumAnalyzer::FFTSize::Size8192); break;
            }
        };
        
        // Window type selector
        addAndMakeVisible(windowTypeLabel);
        windowTypeLabel.setText("Window:", juce::dontSendNotification);
        windowTypeLabel.attachToComponent(&windowTypeCombo, true);
        
        addAndMakeVisible(windowTypeCombo);
        windowTypeCombo.addItem("Rectangular", 1);
        windowTypeCombo.addItem("Hanning", 2);
        windowTypeCombo.addItem("Hamming", 3);
        windowTypeCombo.addItem("Blackman", 4);
        windowTypeCombo.addItem("Blackman-Harris", 5);
        windowTypeCombo.setSelectedId(2); // Hanning default
        windowTypeCombo.onChange = [this]()
        {
            switch (windowTypeCombo.getSelectedId())
            {
                case 1: spectrumAnalyzer.setWindowType(SpectrumAnalyzer::WindowType::Rectangular); break;
                case 2: spectrumAnalyzer.setWindowType(SpectrumAnalyzer::WindowType::Hanning); break;
                case 3: spectrumAnalyzer.setWindowType(SpectrumAnalyzer::WindowType::Hamming); break;
                case 4: spectrumAnalyzer.setWindowType(SpectrumAnalyzer::WindowType::Blackman); break;
                case 5: spectrumAnalyzer.setWindowType(SpectrumAnalyzer::WindowType::BlackmanHarris); break;
            }
        };
        
        // Visualization mode selector
        addAndMakeVisible(modeLabel);
        modeLabel.setText("Mode:", juce::dontSendNotification);
        modeLabel.attachToComponent(&modeCombo, true);
        
        addAndMakeVisible(modeCombo);
        modeCombo.addItem("Bars", 1);
        modeCombo.addItem("Line", 2);
        modeCombo.addItem("Waterfall", 3);
        modeCombo.setSelectedId(1); // Bars default
        modeCombo.onChange = [this]()
        {
            switch (modeCombo.getSelectedId())
            {
                case 1: spectrumAnalyzer.setVisualizationMode(SpectrumAnalyzer::VisualizationMode::Bars); break;
                case 2: spectrumAnalyzer.setVisualizationMode(SpectrumAnalyzer::VisualizationMode::Line); break;
                case 3: spectrumAnalyzer.setVisualizationMode(SpectrumAnalyzer::VisualizationMode::Waterfall); break;
            }
        };
        
        // Toggle buttons
        addAndMakeVisible(logScaleButton);
        logScaleButton.setButtonText("Log Scale");
        logScaleButton.setToggleState(true, juce::dontSendNotification);
        logScaleButton.onClick = [this]()
        {
            spectrumAnalyzer.setLogScale(logScaleButton.getToggleState());
        };
        
        addAndMakeVisible(peakHoldButton);
        peakHoldButton.setButtonText("Peak Hold");
        peakHoldButton.setToggleState(true, juce::dontSendNotification);
        peakHoldButton.onClick = [this]()
        {
            spectrumAnalyzer.setPeakHold(peakHoldButton.getToggleState());
        };
        
        addAndMakeVisible(showGridButton);
        showGridButton.setButtonText("Show Grid");
        showGridButton.setToggleState(true, juce::dontSendNotification);
        showGridButton.onClick = [this]()
        {
            spectrumAnalyzer.setShowGrid(showGridButton.getToggleState());
        };
        
        // Smoothing slider
        addAndMakeVisible(smoothingLabel);
        smoothingLabel.setText("Smoothing:", juce::dontSendNotification);
        smoothingLabel.attachToComponent(&smoothingSlider, true);
        
        addAndMakeVisible(smoothingSlider);
        smoothingSlider.setRange(0.0, 0.99, 0.01);
        smoothingSlider.setValue(0.8);
        smoothingSlider.onValueChange = [this]()
        {
            spectrumAnalyzer.setSmoothing(static_cast<float>(smoothingSlider.getValue()));
        };
        
        // Layout controls
        controlPanel.setLayout([this](juce::Rectangle<int> bounds)
        {
            auto area = bounds.reduced(10);
            const int rowHeight = 24;
            const int spacing = 5;
            const int labelWidth = 80;
            
            // First row
            auto row = area.removeFromTop(rowHeight);
            fftSizeCombo.setBounds(row.removeFromLeft(120).withTrimmedLeft(labelWidth));
            row.removeFromLeft(spacing);
            windowTypeCombo.setBounds(row.removeFromLeft(140).withTrimmedLeft(labelWidth));
            row.removeFromLeft(spacing);
            modeCombo.setBounds(row.removeFromLeft(120).withTrimmedLeft(labelWidth));
            
            area.removeFromTop(spacing);
            
            // Second row
            row = area.removeFromTop(rowHeight);
            logScaleButton.setBounds(row.removeFromLeft(100));
            row.removeFromLeft(spacing);
            peakHoldButton.setBounds(row.removeFromLeft(100));
            row.removeFromLeft(spacing);
            showGridButton.setBounds(row.removeFromLeft(100));
            
            area.removeFromTop(spacing);
            
            // Third row
            row = area.removeFromTop(rowHeight);
            smoothingSlider.setBounds(row.withTrimmedLeft(labelWidth));
        });
    }
    
    //==============================================================================
    // Components
    SpectrumAnalyzer spectrumAnalyzer;
    juce::AudioDeviceManager audioDeviceManager;
    
    juce::Component controlPanel;
    juce::Label fftSizeLabel, windowTypeLabel, modeLabel, smoothingLabel;
    juce::ComboBox fftSizeCombo, windowTypeCombo, modeCombo;
    juce::ToggleButton logScaleButton, peakHoldButton, showGridButton;
    juce::Slider smoothingSlider;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainAudioWindow)
};

/**
 * Example of using the SpectrumAnalyzer in a plugin or audio processor
 */
class AudioProcessorWithSpectrum : public juce::AudioProcessor
{
public:
    AudioProcessorWithSpectrum()
        : AudioProcessor(BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        spectrumAnalyzer.setSampleRate(sampleRate);
    }
    
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        // Send audio to spectrum analyzer
        spectrumAnalyzer.processAudioBuffer(buffer);
        
        // Process audio as needed...
    }
    
    // Other AudioProcessor methods...
    
    SpectrumAnalyzer& getSpectrumAnalyzer() { return spectrumAnalyzer; }
    
private:
    SpectrumAnalyzer spectrumAnalyzer;
};