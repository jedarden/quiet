/*
  ==============================================================================

    MainWindow.cpp
    Created: 2025
    Author:  QUIET Application

    Main application window implementation using JUCE framework.
    Provides the primary user interface for noise cancellation control.

  ==============================================================================
*/

#include "MainWindow.h"
#include "../core/AudioDeviceManager.h"
#include "../core/ConfigurationManager.h"
#include "../core/EventDispatcher.h"
#include <JuceHeader.h>

//==============================================================================
// Custom LookAndFeel for modern dark theme
//==============================================================================
class QuietLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuietLookAndFeel()
    {
        // Define color scheme
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff1e1e1e));
        setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2d2d2d));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffe0e0e0));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3d3d3d));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3d3d3d));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00ff00));
        setColour(juce::Slider::backgroundColourId, juce::Colour(0xff2d2d2d));
        setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ff00));
        setColour(juce::Slider::trackColourId, juce::Colour(0xff00ff00));
        
        // Set modern font
        setDefaultSansSerifTypefaceName("Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial");
    }
    
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto isOn = button.getToggleState();
        
        // Draw modern toggle switch
        const float toggleWidth = 60.0f;
        const float toggleHeight = 30.0f;
        const float toggleX = bounds.getCentreX() - toggleWidth * 0.5f;
        const float toggleY = bounds.getCentreY() - toggleHeight * 0.5f;
        
        juce::Rectangle<float> toggleBounds(toggleX, toggleY, toggleWidth, toggleHeight);
        
        // Background
        g.setColour(isOn ? juce::Colour(0xff00ff00).withAlpha(0.3f) : juce::Colour(0xff3d3d3d));
        g.fillRoundedRectangle(toggleBounds, toggleHeight * 0.5f);
        
        // Border
        g.setColour(isOn ? juce::Colour(0xff00ff00) : juce::Colour(0xff5d5d5d));
        g.drawRoundedRectangle(toggleBounds, toggleHeight * 0.5f, 2.0f);
        
        // Thumb
        const float thumbSize = toggleHeight - 6.0f;
        const float thumbX = isOn ? (toggleX + toggleWidth - thumbSize - 3.0f) : (toggleX + 3.0f);
        const float thumbY = toggleY + 3.0f;
        
        g.setColour(isOn ? juce::Colour(0xff00ff00) : juce::Colour(0xffe0e0e0));
        g.fillEllipse(thumbX, thumbY, thumbSize, thumbSize);
        
        // Draw label
        g.setColour(button.findColour(juce::Label::textColourId));
        g.setFont(14.0f);
        g.drawText(button.getButtonText(), bounds.reduced(toggleWidth + 10, 0),
                   juce::Justification::centredLeft, true);
    }
    
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height);
        
        // Background with subtle gradient
        g.setGradientFill(juce::ColourGradient(
            box.findColour(juce::ComboBox::backgroundColourId),
            0, 0,
            box.findColour(juce::ComboBox::backgroundColourId).darker(0.1f),
            0, height,
            false
        ));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
        
        // Arrow
        const float arrowSize = 8.0f;
        const float arrowX = width - arrowSize - 10.0f;
        const float arrowY = height * 0.5f - arrowSize * 0.25f;
        
        juce::Path arrow;
        arrow.addTriangle(arrowX, arrowY,
                         arrowX + arrowSize, arrowY,
                         arrowX + arrowSize * 0.5f, arrowY + arrowSize * 0.5f);
        
        g.setColour(box.findColour(juce::ComboBox::textColourId).withAlpha(0.7f));
        g.fillPath(arrow);
    }
};

//==============================================================================
// WaveformDisplay Component
//==============================================================================
class WaveformDisplay : public juce::Component, public juce::Timer
{
public:
    WaveformDisplay(const juce::String& title) : displayTitle(title)
    {
        audioBuffer.setSize(1, bufferSize);
        audioBuffer.clear();
        startTimerHz(30); // 30 FPS update rate
    }
    
    void paint(juce::Graphics& g) override
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
        g.drawText(displayTitle, bounds.removeFromTop(20), juce::Justification::centred);
        
        // Waveform
        const float width = static_cast<float>(bounds.getWidth());
        const float height = static_cast<float>(bounds.getHeight());
        const float midY = height * 0.5f;
        
        g.setColour(juce::Colour(0xff00ff00).withAlpha(0.8f));
        
        juce::Path waveform;
        waveform.startNewSubPath(0, midY);
        
        const int numSamples = audioBuffer.getNumSamples();
        const float* samples = audioBuffer.getReadPointer(0);
        
        for (int i = 0; i < width; ++i)
        {
            const int sampleIndex = static_cast<int>((i / width) * numSamples);
            const float sampleValue = samples[sampleIndex];
            const float y = midY - (sampleValue * midY * 0.8f);
            
            if (i == 0)
                waveform.startNewSubPath(i, y);
            else
                waveform.lineTo(i, y);
        }
        
        g.strokePath(waveform, juce::PathStrokeType(1.5f));
        
        // Center line
        g.setColour(juce::Colour(0xff3d3d3d));
        g.drawHorizontalLine(static_cast<int>(midY), 0, width);
    }
    
    void updateBuffer(const juce::AudioBuffer<float>& newBuffer)
    {
        const juce::ScopedLock sl(bufferLock);
        
        // Copy new samples
        const int samplesToAdd = newBuffer.getNumSamples();
        const int currentSamples = audioBuffer.getNumSamples();
        
        if (samplesToAdd >= bufferSize)
        {
            // Replace entire buffer
            audioBuffer.copyFrom(0, 0, newBuffer, 0, 0, bufferSize);
        }
        else
        {
            // Shift existing samples and add new ones
            audioBuffer.copyFrom(0, 0, audioBuffer, 0, samplesToAdd, currentSamples - samplesToAdd);
            audioBuffer.copyFrom(0, currentSamples - samplesToAdd, newBuffer, 0, 0, samplesToAdd);
        }
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    juce::String displayTitle;
    juce::AudioBuffer<float> audioBuffer;
    juce::CriticalSection bufferLock;
    static constexpr int bufferSize = 4096;
};

//==============================================================================
// SpectrumAnalyzer Component
//==============================================================================
class SpectrumAnalyzer : public juce::Component, public juce::Timer
{
public:
    SpectrumAnalyzer() : forwardFFT(fftOrder)
    {
        startTimerHz(30);
        zeromem(fftData, sizeof(fftData));
    }
    
    void paint(juce::Graphics& g) override
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
        const int numBins = fftSize / 2;
        const float binWidth = width / numBins;
        
        for (int i = 1; i < numBins; ++i)
        {
            const float magnitude = fftData[i];
            const float db = 20.0f * log10f(magnitude + 1e-6f);
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
    
    void updateSpectrum(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() < fftSize)
            return;
            
        // Copy audio data to FFT input
        const float* samples = buffer.getReadPointer(0);
        for (int i = 0; i < fftSize; ++i)
            fftData[i] = samples[i] * window[i];
        
        // Perform FFT
        forwardFFT.performRealOnlyForwardTransform(fftData);
        
        // Calculate magnitudes with smoothing
        for (int i = 0; i < fftSize / 2; ++i)
        {
            const float real = fftData[i * 2];
            const float imag = fftData[i * 2 + 1];
            const float magnitude = std::sqrt(real * real + imag * imag);
            
            // Smooth the spectrum
            smoothedMagnitudes[i] = smoothedMagnitudes[i] * 0.8f + magnitude * 0.2f;
            fftData[i] = smoothedMagnitudes[i];
        }
    }
    
    void timerCallback() override
    {
        repaint();
    }
    
private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    
    juce::dsp::FFT forwardFFT;
    float fftData[fftSize * 2];
    float smoothedMagnitudes[fftSize / 2] = {0};
    float window[fftSize];
    
    void createWindow()
    {
        // Hann window
        for (int i = 0; i < fftSize; ++i)
            window[i] = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1));
    }
};

//==============================================================================
// LevelMeter Component
//==============================================================================
class LevelMeter : public juce::Component, public juce::Timer
{
public:
    LevelMeter()
    {
        startTimerHz(24); // 24 FPS for smooth animation
    }
    
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(4);
        
        // Background
        g.setColour(juce::Colour(0xff2d2d2d));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);
        
        // Level bar
        const float levelWidth = bounds.getWidth() * currentLevel;
        auto levelBounds = bounds.withWidth(static_cast<int>(levelWidth));
        
        // Color based on level
        juce::Colour levelColor;
        if (currentLevel < 0.6f)
            levelColor = juce::Colour(0xff00ff00); // Green
        else if (currentLevel < 0.9f)
            levelColor = juce::Colour(0xffffff00); // Yellow
        else
            levelColor = juce::Colour(0xffff0000); // Red
        
        g.setColour(levelColor);
        g.fillRoundedRectangle(levelBounds.toFloat(), 4.0f);
        
        // Peak indicator
        if (peakLevel > 0.0f)
        {
            const float peakX = bounds.getX() + bounds.getWidth() * peakLevel - 2;
            g.setColour(juce::Colour(0xffffffff));
            g.fillRect(static_cast<int>(peakX), bounds.getY(), 2, bounds.getHeight());
        }
        
        // dB markings
        g.setColour(juce::Colour(0xff808080));
        g.setFont(10.0f);
        
        const float dbValue = 20.0f * log10f(currentLevel + 1e-6f);
        g.drawText(juce::String(dbValue, 1) + " dB", 
                  bounds.getRight() + 5, bounds.getY(), 50, bounds.getHeight(),
                  juce::Justification::centredLeft);
    }
    
    void updateLevel(float newLevel)
    {
        // Smooth the level changes
        targetLevel = juce::jlimit(0.0f, 1.0f, newLevel);
        
        // Update peak with decay
        if (newLevel > peakLevel)
            peakLevel = newLevel;
    }
    
    void timerCallback() override
    {
        // Smooth animation
        currentLevel = currentLevel * 0.7f + targetLevel * 0.3f;
        
        // Peak decay
        peakLevel *= 0.99f;
        if (peakLevel < 0.001f)
            peakLevel = 0.0f;
            
        repaint();
    }
    
private:
    float currentLevel = 0.0f;
    float targetLevel = 0.0f;
    float peakLevel = 0.0f;
};

//==============================================================================
// SettingsPanel Component
//==============================================================================
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel(ConfigurationManager& config) : configManager(config)
    {
        // Buffer size selector
        addAndMakeVisible(bufferSizeLabel);
        bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
        bufferSizeLabel.setJustificationType(juce::Justification::right);
        
        addAndMakeVisible(bufferSizeCombo);
        bufferSizeCombo.addItem("64", 1);
        bufferSizeCombo.addItem("128", 2);
        bufferSizeCombo.addItem("256", 3);
        bufferSizeCombo.addItem("512", 4);
        bufferSizeCombo.addItem("1024", 5);
        bufferSizeCombo.setSelectedId(3); // Default 256
        
        // Sample rate selector
        addAndMakeVisible(sampleRateLabel);
        sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
        sampleRateLabel.setJustificationType(juce::Justification::right);
        
        addAndMakeVisible(sampleRateCombo);
        sampleRateCombo.addItem("44100 Hz", 1);
        sampleRateCombo.addItem("48000 Hz", 2);
        sampleRateCombo.addItem("96000 Hz", 3);
        sampleRateCombo.setSelectedId(2); // Default 48000
        
        // Auto-start toggle
        addAndMakeVisible(autoStartToggle);
        autoStartToggle.setButtonText("Start with system");
        
        // Minimize to tray toggle
        addAndMakeVisible(minimizeToTrayToggle);
        minimizeToTrayToggle.setButtonText("Minimize to system tray");
        
        // Check updates toggle
        addAndMakeVisible(checkUpdatesToggle);
        checkUpdatesToggle.setButtonText("Check for updates");
        
        loadSettings();
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        const int rowHeight = 30;
        const int spacing = 10;
        const int labelWidth = 100;
        
        // Buffer size
        auto row = bounds.removeFromTop(rowHeight);
        bufferSizeLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(spacing);
        bufferSizeCombo.setBounds(row);
        
        bounds.removeFromTop(spacing);
        
        // Sample rate
        row = bounds.removeFromTop(rowHeight);
        sampleRateLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(spacing);
        sampleRateCombo.setBounds(row);
        
        bounds.removeFromTop(spacing * 2);
        
        // Toggles
        autoStartToggle.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        minimizeToTrayToggle.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        checkUpdatesToggle.setBounds(bounds.removeFromTop(rowHeight));
    }
    
private:
    ConfigurationManager& configManager;
    
    juce::Label bufferSizeLabel, sampleRateLabel;
    juce::ComboBox bufferSizeCombo, sampleRateCombo;
    juce::ToggleButton autoStartToggle, minimizeToTrayToggle, checkUpdatesToggle;
    
    void loadSettings()
    {
        // Load settings from config manager
        auto bufferSize = configManager.getSetting("audio.buffer_size");
        auto sampleRate = configManager.getSetting("audio.sample_rate");
        auto autoStart = configManager.getSetting("system.auto_start");
        auto minimizeToTray = configManager.getSetting("ui.minimize_to_tray");
        auto checkUpdates = configManager.getSetting("system.check_updates");
        
        // Apply settings to UI
        // Implementation would map values to combo box selections
    }
};

//==============================================================================
// MainWindow Implementation
//==============================================================================
MainWindow::MainWindow(juce::String name, AudioDeviceManager& audioDev, 
                       ConfigurationManager& config, EventDispatcher& events)
    : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::allButtons),
      audioDeviceManager(audioDev),
      configManager(config),
      eventDispatcher(events)
{
    // Set custom look and feel
    lookAndFeel = std::make_unique<QuietLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());
    
    // Create main content component
    mainComponent = std::make_unique<MainContentComponent>(*this);
    setContentOwned(mainComponent.get(), true);
    
    // Configure window
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(600, 400, 1200, 800);
    
    // Center on screen
    centreWithSize(800, 600);
    
    // Register for events
    eventDispatcher.addListener(this);
    
    // Setup keyboard shortcuts
    addKeyListener(this);
    
    // Make visible
    setVisible(true);
    
    // Initialize system tray if supported
    initializeSystemTray();
}

MainWindow::~MainWindow()
{
    eventDispatcher.removeListener(this);
    setLookAndFeel(nullptr);
}

void MainWindow::closeButtonPressed()
{
    if (configManager.getSetting("ui.minimize_to_tray"))
    {
        setVisible(false);
        if (systemTrayIcon)
            systemTrayIcon->setIconTooltip("QUIET - Click to restore");
    }
    else
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
}

bool MainWindow::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    // Keyboard shortcuts
    if (key.getModifiers().isCommandDown())
    {
        switch (key.getKeyCode())
        {
            case 'Q':
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
                return true;
                
            case 'M':
                setVisible(false);
                return true;
                
            case 'T':
                // Toggle noise reduction
                if (mainComponent)
                    mainComponent->toggleNoiseReduction();
                return true;
                
            case ',':
                // Show settings
                if (mainComponent)
                    mainComponent->showSettings();
                return true;
                
            default:
                break;
        }
    }
    
    return false;
}

void MainWindow::onAudioEvent(AudioEvent event, const EventData& data)
{
    // Handle audio events on the message thread
    juce::MessageManager::callAsync([this, event, data]()
    {
        if (mainComponent)
            mainComponent->handleAudioEvent(event, data);
    });
}

void MainWindow::initializeSystemTray()
{
    if (juce::SystemTrayIconComponent::isSystemTraySupported())
    {
        systemTrayIcon = std::make_unique<juce::SystemTrayIconComponent>();
        
        // Load icon (would need actual icon resource)
        // systemTrayIcon->setIconImage(iconImage);
        
        systemTrayIcon->setIconTooltip("QUIET - Noise Cancellation");
        
        // Setup menu
        juce::PopupMenu trayMenu;
        trayMenu.addItem(1, "Show Window");
        trayMenu.addItem(2, "Toggle Noise Reduction");
        trayMenu.addSeparator();
        trayMenu.addItem(3, "Settings");
        trayMenu.addSeparator();
        trayMenu.addItem(4, "Quit");
        
        systemTrayIcon->setMouseClickCallback(
            [this](const juce::MouseEvent&, const juce::Time&)
            {
                setVisible(true);
                toFront(true);
            }
        );
    }
}

//==============================================================================
// MainContentComponent Implementation
//==============================================================================
class MainWindow::MainContentComponent : public juce::Component
{
public:
    MainContentComponent(MainWindow& parent)
        : parentWindow(parent),
          settingsPanel(parent.configManager)
    {
        // Header
        addAndMakeVisible(logoLabel);
        logoLabel.setText("QUIET", juce::dontSendNotification);
        logoLabel.setFont(juce::Font(32.0f, juce::Font::bold));
        logoLabel.setJustificationType(juce::Justification::centred);
        
        addAndMakeVisible(statusLabel);
        statusLabel.setText("Ready", juce::dontSendNotification);
        statusLabel.setJustificationType(juce::Justification::centred);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff808080));
        
        // Device selection
        addAndMakeVisible(deviceLabel);
        deviceLabel.setText("Input Device:", juce::dontSendNotification);
        deviceLabel.setJustificationType(juce::Justification::right);
        
        addAndMakeVisible(deviceComboBox);
        updateDeviceList();
        deviceComboBox.onChange = [this]() { onDeviceChanged(); };
        
        // Main controls
        addAndMakeVisible(enableToggle);
        enableToggle.setButtonText("Enable Noise Reduction");
        enableToggle.setToggleState(true, juce::dontSendNotification);
        enableToggle.onClick = [this]() { onToggleClicked(); };
        
        addAndMakeVisible(reductionLevelSlider);
        reductionLevelSlider.setRange(0.0, 100.0, 1.0);
        reductionLevelSlider.setValue(50.0);
        reductionLevelSlider.setTextValueSuffix("%");
        reductionLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        reductionLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
        
        addAndMakeVisible(reductionLevelLabel);
        reductionLevelLabel.setText("Reduction Level:", juce::dontSendNotification);
        reductionLevelLabel.setJustificationType(juce::Justification::right);
        
        // Level meter
        addAndMakeVisible(levelMeter);
        
        // Visualizations
        addAndMakeVisible(visualizationTabs);
        
        // Waveform tab
        waveformContainer = new juce::Component();
        inputWaveform = std::make_unique<WaveformDisplay>("Input");
        outputWaveform = std::make_unique<WaveformDisplay>("Output");
        waveformContainer->addAndMakeVisible(inputWaveform.get());
        waveformContainer->addAndMakeVisible(outputWaveform.get());
        visualizationTabs.addTab("Waveform", juce::Colour(0xff2d2d2d), waveformContainer, true);
        
        // Spectrum tab
        spectrumAnalyzer = std::make_unique<SpectrumAnalyzer>();
        visualizationTabs.addTab("Spectrum", juce::Colour(0xff2d2d2d), spectrumAnalyzer.get(), false);
        
        // Settings tab
        visualizationTabs.addTab("Settings", juce::Colour(0xff2d2d2d), &settingsPanel, false);
        
        // Reduction indicator
        addAndMakeVisible(reductionIndicator);
        reductionIndicator.setText("Reduction: 0 dB", juce::dontSendNotification);
        reductionIndicator.setJustificationType(juce::Justification::centred);
        reductionIndicator.setColour(juce::Label::textColourId, juce::Colour(0xff00ff00));
        
        // Settings button
        addAndMakeVisible(settingsButton);
        settingsButton.setButtonText("Settings");
        settingsButton.onClick = [this]() { showSettings(); };
        
        // Set component tooltips
        deviceComboBox.setTooltip("Select your microphone input device");
        enableToggle.setTooltip("Toggle noise reduction on/off (Cmd+T)");
        reductionLevelSlider.setTooltip("Adjust the intensity of noise reduction");
        settingsButton.setTooltip("Open settings panel (Cmd+,)");
    }
    
    ~MainContentComponent()
    {
        visualizationTabs.clearTabs();
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        
        // Header section
        auto headerBounds = bounds.removeFromTop(80);
        logoLabel.setBounds(headerBounds.removeFromTop(50));
        statusLabel.setBounds(headerBounds);
        
        // Device selection
        auto deviceBounds = bounds.removeFromTop(40).reduced(20, 5);
        deviceLabel.setBounds(deviceBounds.removeFromLeft(100));
        deviceBounds.removeFromLeft(10);
        deviceComboBox.setBounds(deviceBounds.removeFromLeft(300));
        deviceBounds.removeFromLeft(20);
        settingsButton.setBounds(deviceBounds.removeFromRight(80));
        
        // Main controls
        auto controlBounds = bounds.removeFromTop(100).reduced(20, 10);
        
        // Enable toggle centered
        auto toggleBounds = controlBounds.removeFromTop(40);
        const int toggleWidth = 200;
        enableToggle.setBounds(toggleBounds.withSizeKeepingCentre(toggleWidth, 40));
        
        // Reduction level slider
        auto sliderBounds = controlBounds.removeFromTop(40);
        reductionLevelLabel.setBounds(sliderBounds.removeFromLeft(120));
        sliderBounds.removeFromLeft(10);
        reductionLevelSlider.setBounds(sliderBounds);
        
        // Level meter and reduction indicator
        auto meterBounds = bounds.removeFromTop(40).reduced(20, 5);
        levelMeter.setBounds(meterBounds.removeFromLeft(meterBounds.getWidth() * 0.7f));
        meterBounds.removeFromLeft(10);
        reductionIndicator.setBounds(meterBounds);
        
        // Visualizations take remaining space
        visualizationTabs.setBounds(bounds.reduced(10));
        
        // Layout waveform container
        if (waveformContainer)
        {
            auto waveformBounds = waveformContainer->getLocalBounds();
            const int halfHeight = waveformBounds.getHeight() / 2;
            inputWaveform->setBounds(waveformBounds.removeFromTop(halfHeight).reduced(5));
            outputWaveform->setBounds(waveformBounds.reduced(5));
        }
    }
    
    void handleAudioEvent(AudioEvent event, const EventData& data)
    {
        switch (event)
        {
            case AudioEvent::DeviceChanged:
                updateDeviceList();
                updateStatus("Device changed");
                break;
                
            case AudioEvent::ProcessingToggled:
                enableToggle.setToggleState(data.enabled, juce::dontSendNotification);
                updateStatus(data.enabled ? "Noise reduction enabled" : "Noise reduction disabled");
                break;
                
            case AudioEvent::BufferProcessed:
                if (data.bufferType == BufferType::Input)
                {
                    inputWaveform->updateBuffer(data.buffer);
                    levelMeter.updateLevel(data.buffer.getMagnitude(0, data.buffer.getNumSamples()));
                    spectrumAnalyzer->updateSpectrum(data.buffer);
                }
                else if (data.bufferType == BufferType::Output)
                {
                    outputWaveform->updateBuffer(data.buffer);
                }
                
                if (data.reductionLevel > 0.0f)
                {
                    reductionIndicator.setText("Reduction: " + 
                        juce::String(data.reductionLevel, 1) + " dB", 
                        juce::dontSendNotification);
                }
                break;
                
            case AudioEvent::ErrorOccurred:
                updateStatus("Error: " + data.errorMessage);
                break;
        }
    }
    
    void toggleNoiseReduction()
    {
        enableToggle.setToggleState(!enableToggle.getToggleState(), juce::sendNotification);
    }
    
    void showSettings()
    {
        visualizationTabs.setCurrentTabIndex(2); // Settings tab
    }
    
private:
    MainWindow& parentWindow;
    
    // UI Components
    juce::Label logoLabel, statusLabel;
    juce::Label deviceLabel, reductionLevelLabel;
    juce::ComboBox deviceComboBox;
    juce::ToggleButton enableToggle;
    juce::Slider reductionLevelSlider;
    juce::TextButton settingsButton;
    LevelMeter levelMeter;
    juce::Label reductionIndicator;
    
    // Visualizations
    juce::TabbedComponent visualizationTabs{juce::TabbedButtonBar::TabsAtTop};
    juce::Component* waveformContainer = nullptr;
    std::unique_ptr<WaveformDisplay> inputWaveform, outputWaveform;
    std::unique_ptr<SpectrumAnalyzer> spectrumAnalyzer;
    SettingsPanel settingsPanel;
    
    void updateDeviceList()
    {
        deviceComboBox.clear();
        
        auto devices = parentWindow.audioDeviceManager.getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
        {
            deviceComboBox.addItem(devices[i].name, i + 1);
        }
        
        // Select current device
        auto currentDevice = parentWindow.audioDeviceManager.getCurrentDevice();
        for (int i = 0; i < devices.size(); ++i)
        {
            if (devices[i].id == currentDevice.id)
            {
                deviceComboBox.setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }
    }
    
    void onDeviceChanged()
    {
        int selectedId = deviceComboBox.getSelectedId() - 1;
        if (selectedId >= 0)
        {
            auto devices = parentWindow.audioDeviceManager.getAvailableDevices();
            if (selectedId < devices.size())
            {
                parentWindow.audioDeviceManager.selectDevice(devices[selectedId].id);
            }
        }
    }
    
    void onToggleClicked()
    {
        bool enabled = enableToggle.getToggleState();
        parentWindow.eventDispatcher.dispatch(AudioEvent::ProcessingToggled, 
                                             EventData{enabled});
        
        // Animate button color change
        enableToggle.setColour(juce::ToggleButton::textColourId, 
                              enabled ? juce::Colour(0xff00ff00) : juce::Colour(0xffe0e0e0));
    }
    
    void updateStatus(const juce::String& message)
    {
        statusLabel.setText(message, juce::dontSendNotification);
        
        // Fade out status message after 3 seconds
        juce::Timer::callAfterDelay(3000, [this]()
        {
            statusLabel.setText("Ready", juce::dontSendNotification);
        });
    }
};

//==============================================================================
// Window positioning and state management
//==============================================================================
void MainWindow::restoreWindowState()
{
    // Restore window position and size from config
    auto x = configManager.getSetting("ui.window_position.x", 100);
    auto y = configManager.getSetting("ui.window_position.y", 100);
    auto width = configManager.getSetting("ui.window_size.width", 800);
    auto height = configManager.getSetting("ui.window_size.height", 600);
    
    setBounds(x, y, width, height);
}

void MainWindow::saveWindowState()
{
    // Save window position and size to config
    auto bounds = getBounds();
    configManager.setSetting("ui.window_position.x", bounds.getX());
    configManager.setSetting("ui.window_position.y", bounds.getY());
    configManager.setSetting("ui.window_size.width", bounds.getWidth());
    configManager.setSetting("ui.window_size.height", bounds.getHeight());
}

//==============================================================================
// Static factory method
//==============================================================================
std::unique_ptr<MainWindow> MainWindow::create(AudioDeviceManager& audioDeviceManager,
                                                ConfigurationManager& configManager,
                                                EventDispatcher& eventDispatcher)
{
    auto window = std::make_unique<MainWindow>("QUIET - Noise Cancellation",
                                               audioDeviceManager,
                                               configManager,
                                               eventDispatcher);
    window->restoreWindowState();
    return window;
}