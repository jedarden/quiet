#include "quiet/ui/MainWindow.h"
#include "quiet/core/AudioDeviceManager.h"
#include "quiet/core/ConfigurationManager.h"
#include "quiet/core/EventDispatcher.h"
#include "quiet/core/NoiseReductionProcessor.h"
#include "quiet/core/VirtualDeviceRouter.h"
#include "quiet/ui/WaveformDisplay.h"
#include "quiet/ui/SpectrumAnalyzer.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

namespace quiet {
namespace ui {

//==============================================================================
// Main Content Component Implementation
//==============================================================================
// Settings Panel Component
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel(core::ConfigurationManager& configManager)
        : m_configManager(configManager)
    {
        // Buffer size
        addAndMakeVisible(m_bufferSizeLabel);
        m_bufferSizeLabel.setText("Buffer Size:", juce::dontSendNotification);
        
        addAndMakeVisible(m_bufferSizeCombo);
        m_bufferSizeCombo.addItem("64", 1);
        m_bufferSizeCombo.addItem("128", 2);
        m_bufferSizeCombo.addItem("256", 3);
        m_bufferSizeCombo.addItem("512", 4);
        m_bufferSizeCombo.addItem("1024", 5);
        m_bufferSizeCombo.setSelectedId(3);
        
        // Sample rate
        addAndMakeVisible(m_sampleRateLabel);
        m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
        
        addAndMakeVisible(m_sampleRateCombo);
        m_sampleRateCombo.addItem("44100 Hz", 1);
        m_sampleRateCombo.addItem("48000 Hz", 2);
        m_sampleRateCombo.addItem("96000 Hz", 3);
        m_sampleRateCombo.setSelectedId(2);
        
        // Auto-start toggle
        addAndMakeVisible(m_autoStartToggle);
        m_autoStartToggle.setButtonText("Start with system");
        m_autoStartToggle.setToggleState(m_configManager.getConfiguration().system.autoStart, 
                                         juce::dontSendNotification);
        
        // Minimize to tray
        addAndMakeVisible(m_minimizeToTrayToggle);
        m_minimizeToTrayToggle.setButtonText("Minimize to system tray");
        m_minimizeToTrayToggle.setToggleState(m_configManager.getConfiguration().ui.minimizeToTray,
                                               juce::dontSendNotification);
        
        // Check updates
        addAndMakeVisible(m_checkUpdatesToggle);
        m_checkUpdatesToggle.setButtonText("Check for updates");
        m_checkUpdatesToggle.setToggleState(m_configManager.getConfiguration().system.checkForUpdates,
                                           juce::dontSendNotification);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds().reduced(20);
        const int rowHeight = 30;
        const int spacing = 10;
        const int labelWidth = 120;
        
        // Title
        auto titleBounds = bounds.removeFromTop(40);
        // Buffer size
        auto row = bounds.removeFromTop(rowHeight);
        m_bufferSizeLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(spacing);
        m_bufferSizeCombo.setBounds(row.removeFromLeft(200));
        
        bounds.removeFromTop(spacing);
        
        // Sample rate
        row = bounds.removeFromTop(rowHeight);
        m_sampleRateLabel.setBounds(row.removeFromLeft(labelWidth));
        row.removeFromLeft(spacing);
        m_sampleRateCombo.setBounds(row.removeFromLeft(200));
        
        bounds.removeFromTop(spacing * 2);
        
        // Toggles
        m_autoStartToggle.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        m_minimizeToTrayToggle.setBounds(bounds.removeFromTop(rowHeight));
        bounds.removeFromTop(spacing);
        m_checkUpdatesToggle.setBounds(bounds.removeFromTop(rowHeight));
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(ThemeColors::background);
        
        g.setColour(ThemeColors::text);
        g.setFont(juce::Font(20.0f, juce::Font::bold));
        g.drawText("Settings", getLocalBounds().removeFromTop(40), 
                   juce::Justification::centred);
    }
    
private:
    core::ConfigurationManager& m_configManager;
    
    juce::Label m_bufferSizeLabel, m_sampleRateLabel;
    juce::ComboBox m_bufferSizeCombo, m_sampleRateCombo;
    juce::ToggleButton m_autoStartToggle, m_minimizeToTrayToggle, m_checkUpdatesToggle;
};

//==============================================================================
// Main Content Component Implementation
//==============================================================================
class MainWindow::MainContentComponent : public juce::Component,
                                       public juce::Timer,
                                       public juce::Button::Listener,
                                       public juce::ComboBox::Listener,
                                       public juce::Slider::Listener
{
public:
    MainContentComponent(MainWindow& window)
        : parentWindow(window)
        , audioDeviceManager(window.getAudioDeviceManager())
        , configManager(window.getConfigurationManager())
        , eventDispatcher(window.getEventDispatcher())
    {
        // Device selection
        addAndMakeVisible(deviceLabel);
        deviceLabel.setText("Input Device:", juce::dontSendNotification);
        deviceLabel.setFont(juce::Font(14.0f));
        deviceLabel.setColour(juce::Label::textColourId, ThemeColors::text);
        
        addAndMakeVisible(deviceCombo);
        deviceCombo.addListener(this);
        updateDeviceList();
        
        // Main power button
        addAndMakeVisible(powerButton);
        powerButton.setButtonText("Enable Noise Reduction");
        powerButton.addListener(this);
        powerButton.setToggleState(false, juce::dontSendNotification);
        
        // Reduction level slider
        addAndMakeVisible(reductionLabel);
        reductionLabel.setText("Reduction Level:", juce::dontSendNotification);
        reductionLabel.setFont(juce::Font(14.0f));
        reductionLabel.setColour(juce::Label::textColourId, ThemeColors::text);
        
        addAndMakeVisible(reductionSlider);
        reductionSlider.setRange(0.0, 100.0, 1.0);
        reductionSlider.setValue(50.0);
        reductionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        reductionSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
        reductionSlider.addListener(this);
        reductionSlider.setTextValueSuffix("%");
        
        // Level meters
        addAndMakeVisible(inputLevelLabel);
        inputLevelLabel.setText("Input Level", juce::dontSendNotification);
        inputLevelLabel.setFont(juce::Font(12.0f));
        inputLevelLabel.setColour(juce::Label::textColourId, ThemeColors::textDim);
        
        addAndMakeVisible(inputLevelMeter);
        
        addAndMakeVisible(outputLevelLabel);
        outputLevelLabel.setText("Output Level", juce::dontSendNotification);
        outputLevelLabel.setFont(juce::Font(12.0f));
        outputLevelLabel.setColour(juce::Label::textColourId, ThemeColors::textDim);
        
        addAndMakeVisible(outputLevelMeter);
        
        // Status panel
        addAndMakeVisible(statusPanel);
        statusPanel.setColour(juce::Label::backgroundColourId, ThemeColors::panel);
        statusPanel.setColour(juce::Label::textColourId, ThemeColors::text);
        statusPanel.setFont(juce::Font(12.0f));
        updateStatus("Ready");
        
        // Advanced settings button
        addAndMakeVisible(settingsButton);
        settingsButton.setButtonText("Settings");
        settingsButton.addListener(this);
        
        // Initialize visualization components
        addAndMakeVisible(visualizationTabs);
        
        // Waveform tab
        waveformContainer = new juce::Component();
        inputWaveform = std::make_unique<WaveformDisplay>("Input", ThemeColors::accent);
        outputWaveform = std::make_unique<WaveformDisplay>("Output", ThemeColors::success);
        waveformContainer->addAndMakeVisible(inputWaveform.get());
        waveformContainer->addAndMakeVisible(outputWaveform.get());
        visualizationTabs.addTab("Waveform", ThemeColors::panel, waveformContainer, true);
        
        // Spectrum analyzer tab  
        spectrumAnalyzer = std::make_unique<SpectrumAnalyzer>(ThemeColors::accent);
        visualizationTabs.addTab("Spectrum", ThemeColors::panel, spectrumAnalyzer.get(), false);
        
        // Settings tab (created dynamically)
        settingsPanel = std::make_unique<SettingsPanel>(configManager);
        visualizationTabs.addTab("Settings", ThemeColors::panel, settingsPanel.get(), false);
        
        // Subscribe to events
        eventDispatcher.subscribe(EventType::AudioLevelChanged,
            [this](const Event& e) {
                if (e.data) {
                    float level = e.data->getValue<float>("level", 0.0f);
                    bool isInput = e.data->getValue<bool>("isInput", true);
                    
                    juce::MessageManager::callAsync([this, level, isInput]() {
                        if (isInput)
                            inputLevelMeter.updateLevel(level);
                        else
                            outputLevelMeter.updateLevel(level);
                    });
                }
            });
            
        eventDispatcher.subscribe(EventType::AudioDeviceChanged,
            [this](const Event&) {
                juce::MessageManager::callAsync([this]() {
                    updateDeviceList();
                });
            });
            
        eventDispatcher.subscribe(EventType::ProcessingStatsUpdated,
            [this](const Event& e) {
                if (e.data) {
                    float cpuUsage = e.data->getValue<float>("cpu_usage", 0.0f);
                    float latency = e.data->getValue<float>("latency", 0.0f);
                    float reduction = e.data->getValue<float>("reduction_level", 0.0f);
                    
                    juce::MessageManager::callAsync([this, cpuUsage, latency, reduction]() {
                        updateStats(cpuUsage, latency, reduction);
                    });
                }
            });
            
        eventDispatcher.subscribe(EventType::AudioBufferProcessed,
            [this](const Event& e) {
                if (e.data) {
                    bool isInput = e.data->getValue<bool>("isInput", true);
                    
                    // Get audio buffer data
                    if (e.data->hasValue("audioData")) {
                        auto audioData = e.data->getValue<std::shared_ptr<AudioBuffer>>("audioData");
                        if (audioData) {
                            juce::MessageManager::callAsync([this, audioData, isInput]() {
                                if (isInput) {
                                    inputWaveform->updateBuffer(*audioData);
                                    spectrumAnalyzer->updateSpectrum(*audioData);
                                } else {
                                    outputWaveform->updateBuffer(*audioData);
                                }
                            });
                        }
                    }
                }
            });
        
        // Start update timer
        startTimerHz(10);
    }
    
    ~MainContentComponent()
    {
        stopTimer();
        visualizationTabs.clearTabs();
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(ThemeColors::background);
        
        // Draw panels
        auto bounds = getLocalBounds();
        
        // Top panel
        auto topPanel = bounds.removeFromTop(100);
        g.setColour(ThemeColors::panel);
        g.fillRect(topPanel);
        g.setColour(ThemeColors::border);
        g.drawRect(topPanel, 1);
        
        // Draw logo/title
        g.setColour(ThemeColors::accent);
        g.setFont(juce::Font(32.0f, juce::Font::bold));
        g.drawText("QUIET", topPanel.removeFromLeft(150), juce::Justification::centred);
        
        g.setColour(ThemeColors::text);
        g.setFont(juce::Font(14.0f));
        g.drawText("AI-Powered Noise Reduction", topPanel, juce::Justification::centredLeft);
    }
    
    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromTop(100); // Skip header area
        
        auto content = bounds.reduced(20);
        
        // Device selection row
        auto row = content.removeFromTop(40);
        deviceLabel.setBounds(row.removeFromLeft(100));
        row.removeFromLeft(10);
        deviceCombo.setBounds(row.removeFromLeft(300));
        
        content.removeFromTop(20);
        
        // Power button
        auto powerRow = content.removeFromTop(60);
        powerButton.setBounds(powerRow.withSizeKeepingCentre(200, 50));
        
        content.removeFromTop(20);
        
        // Reduction level
        row = content.removeFromTop(40);
        reductionLabel.setBounds(row.removeFromLeft(120));
        row.removeFromLeft(10);
        reductionSlider.setBounds(row);
        
        content.removeFromTop(30);
        
        // Level meters
        auto metersArea = content.removeFromTop(100);
        auto leftMeter = metersArea.removeFromLeft(metersArea.getWidth() / 2).reduced(10);
        auto rightMeter = metersArea.reduced(10);
        
        inputLevelLabel.setBounds(leftMeter.removeFromTop(20));
        inputLevelMeter.setBounds(leftMeter);
        
        outputLevelLabel.setBounds(rightMeter.removeFromTop(20));
        outputLevelMeter.setBounds(rightMeter);
        
        // Visualization area (takes remaining space)
        auto visualizationArea = content.removeFromBottom(300);
        visualizationTabs.setBounds(visualizationArea);
        
        // Layout waveform container if visible
        if (waveformContainer && visualizationTabs.getCurrentTabIndex() == 0)
        {
            auto waveformBounds = waveformContainer->getLocalBounds();
            const int halfHeight = waveformBounds.getHeight() / 2;
            inputWaveform->setBounds(waveformBounds.removeFromTop(halfHeight).reduced(5));
            outputWaveform->setBounds(waveformBounds.reduced(5));
        }
        
        // Bottom area
        auto bottomArea = content.removeFromBottom(40);
        settingsButton.setBounds(bottomArea.removeFromRight(100).reduced(5));
        statusPanel.setBounds(bottomArea.reduced(5));
    }
    
    void buttonClicked(juce::Button* button) override
    {
        if (button == &powerButton)
        {
            bool enabled = powerButton.getToggleState();
            
            // Update UI state
            powerButton.setButtonText(enabled ? "Disable Noise Reduction" : "Enable Noise Reduction");
            powerButton.setColour(juce::TextButton::buttonColourId, 
                                enabled ? ThemeColors::accent : ThemeColors::panel);
            
            // Notify processor
            eventDispatcher.publish(EventType::NoiseReductionToggled,
                EventDataFactory::createProcessingStatsData(0, 0, enabled ? 1.0f : 0.0f));
            
            updateStatus(enabled ? "Processing..." : "Ready");
        }
        else if (button == &settingsButton)
        {
            visualizationTabs.setCurrentTabIndex(2); // Switch to settings tab
        }
    }
    
    void comboBoxChanged(juce::ComboBox* combo) override
    {
        if (combo == &deviceCombo)
        {
            int selectedId = deviceCombo.getSelectedId();
            if (selectedId > 0)
            {
                auto devices = audioDeviceManager.getAvailableInputDevices();
                if (selectedId <= devices.size())
                {
                    const auto& device = devices[selectedId - 1];
                    audioDeviceManager.selectInputDevice(device.id);
                    updateStatus("Device: " + device.name);
                }
            }
        }
    }
    
    void sliderValueChanged(juce::Slider* slider) override
    {
        if (slider == &reductionSlider)
        {
            float level = static_cast<float>(reductionSlider.getValue() / 100.0);
            
            // Map to reduction levels
            NoiseReductionConfig::Level configLevel;
            if (level < 0.33f)
                configLevel = NoiseReductionConfig::Level::Low;
            else if (level < 0.66f)
                configLevel = NoiseReductionConfig::Level::Medium;
            else
                configLevel = NoiseReductionConfig::Level::High;
            
            auto eventData = std::make_shared<EventData>();
            eventData->setValue("level", static_cast<int>(configLevel));
            eventDispatcher.publish(EventType::NoiseReductionLevelChanged, eventData);
        }
    }
    
    void timerCallback() override
    {
        // Update any real-time displays if needed
        repaint();
    }
    
    void toggleNoiseReduction()
    {
        powerButton.setToggleState(!powerButton.getToggleState(), juce::sendNotification);
    }
    
    void showSettings()
    {
        visualizationTabs.setCurrentTabIndex(2); // Switch to settings tab
    }
    
private:
    MainWindow& parentWindow;
    core::AudioDeviceManager& audioDeviceManager;
    core::ConfigurationManager& configManager;
    core::EventDispatcher& eventDispatcher;
    
    // UI Components
    juce::Label deviceLabel;
    juce::ComboBox deviceCombo;
    juce::ToggleButton powerButton;
    juce::Label reductionLabel;
    juce::Slider reductionSlider;
    juce::Label inputLevelLabel, outputLevelLabel;
    AnimatedLevelMeter inputLevelMeter, outputLevelMeter;
    juce::Label statusPanel;
    juce::TextButton settingsButton;
    
    // Visualization components
    juce::TabbedComponent visualizationTabs{juce::TabbedButtonBar::TabsAtTop};
    juce::Component* waveformContainer = nullptr;
    std::unique_ptr<WaveformDisplay> inputWaveform, outputWaveform;
    std::unique_ptr<SpectrumAnalyzer> spectrumAnalyzer;
    std::unique_ptr<SettingsPanel> settingsPanel;
    
    // Stats display
    float currentCpuUsage = 0.0f;
    float currentLatency = 0.0f;
    float currentReduction = 0.0f;
    
    void updateDeviceList()
    {
        deviceCombo.clear();
        
        auto devices = audioDeviceManager.getAvailableInputDevices();
        int selectedId = 0;
        auto currentDevice = audioDeviceManager.getCurrentInputDevice();
        
        for (size_t i = 0; i < devices.size(); ++i)
        {
            deviceCombo.addItem(devices[i].name, static_cast<int>(i + 1));
            if (devices[i].id == currentDevice.id)
                selectedId = static_cast<int>(i + 1);
        }
        
        if (selectedId > 0)
            deviceCombo.setSelectedId(selectedId);
    }
    
    void updateStatus(const juce::String& message)
    {
        statusPanel.setText(message, juce::dontSendNotification);
    }
    
    void updateStats(float cpu, float latency, float reduction)
    {
        currentCpuUsage = cpu;
        currentLatency = latency;
        currentReduction = reduction;
        
        juce::String status;
        status << "CPU: " << juce::String(cpu, 1) << "% | ";
        status << "Latency: " << juce::String(latency, 1) << "ms | ";
        status << "Reduction: " << juce::String(reduction, 1) << "dB";
        
        updateStatus(status);
    }
    
};

//==============================================================================
// AnimatedLevelMeter Implementation
//==============================================================================
AnimatedLevelMeter::AnimatedLevelMeter()
{
    startTimerHz(30);
}

AnimatedLevelMeter::~AnimatedLevelMeter()
{
    stopTimer();
}

void AnimatedLevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(ThemeColors::panel);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Level bar
    float levelWidth = bounds.getWidth() * currentLevel;
    auto levelBounds = bounds.withWidth(levelWidth);
    
    // Gradient based on level
    juce::ColourGradient gradient;
    if (currentLevel < 0.6f)
    {
        gradient = juce::ColourGradient(ThemeColors::accent.darker(0.2f), 
                                       bounds.getX(), bounds.getY(),
                                       ThemeColors::accent,
                                       bounds.getRight(), bounds.getY(), false);
    }
    else if (currentLevel < 0.9f)
    {
        gradient = juce::ColourGradient(ThemeColors::accent,
                                       bounds.getX(), bounds.getY(),
                                       ThemeColors::warning,
                                       bounds.getRight(), bounds.getY(), false);
    }
    else
    {
        gradient = juce::ColourGradient(ThemeColors::warning,
                                       bounds.getX(), bounds.getY(),
                                       ThemeColors::error,
                                       bounds.getRight(), bounds.getY(), false);
    }
    
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(levelBounds, 4.0f);
    
    // Peak indicator
    if (peakLevel > 0.01f)
    {
        float peakX = bounds.getX() + bounds.getWidth() * peakLevel - 2.0f;
        g.setColour(juce::Colours::white);
        g.fillRect(peakX, bounds.getY(), 2.0f, bounds.getHeight());
    }
    
    // Border
    g.setColour(ThemeColors::border);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void AnimatedLevelMeter::timerCallback()
{
    // Smooth animation
    currentLevel = currentLevel * 0.8f + targetLevel * 0.2f;
    
    // Peak hold and decay
    if (peakHoldTime > 0)
    {
        peakHoldTime -= 1.0f / 30.0f; // Decay based on timer frequency
    }
    else
    {
        peakLevel *= 0.95f; // Slow decay
    }
    
    repaint();
}

void AnimatedLevelMeter::setLevel(float newLevel)
{
    targetLevel = juce::jlimit(0.0f, 1.0f, newLevel);
    
    if (newLevel > peakLevel)
    {
        peakLevel = newLevel;
        peakHoldTime = 2.0f; // Hold for 2 seconds
    }
}

void AnimatedLevelMeter::setPeakLevel(float peak)
{
    peakLevel = juce::jlimit(0.0f, 1.0f, peak);
    peakHoldTime = 2.0f;
}

//==============================================================================
// MainWindow Implementation
//==============================================================================
MainWindow::MainWindow(const juce::String& name,
                      core::AudioDeviceManager& audioManager,
                      core::ConfigurationManager& configManager,
                      core::EventDispatcher& eventDispatcher)
    : juce::DocumentWindow(name, ThemeColors::background, 
                          juce::DocumentWindow::allButtons)
    , audioDeviceManager(audioManager)
    , configManager(configManager)
    , eventDispatcher(eventDispatcher)
{
    // Set custom look and feel
    lookAndFeel = std::make_unique<juce::LookAndFeel_V4>();
    lookAndFeel->setColour(juce::ResizableWindow::backgroundColourId, ThemeColors::background);
    lookAndFeel->setColour(juce::DocumentWindow::textColourId, ThemeColors::text);
    setLookAndFeel(lookAndFeel.get());
    
    // Create and set content
    mainComponent = std::make_unique<MainContentComponent>(*this);
    setContentOwned(mainComponent.release(), true);
    
    // Configure window
    setUsingNativeTitleBar(true);
    setResizable(true, true);
    setResizeLimits(600, 500, 1200, 800);
    centreWithSize(800, 600);
    
    // Make visible
    setVisible(true);
    
    // Initialize system tray
    initializeSystemTray();
}

MainWindow::~MainWindow()
{
    setLookAndFeel(nullptr);
}

void MainWindow::closeButtonPressed()
{
    if (configManager.getConfiguration().ui.minimizeToTray)
    {
        setVisible(false);
    }
    else
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
}

void MainWindow::initializeSystemTray()
{
#if JUCE_MAC || JUCE_WINDOWS
    systemTrayIcon = std::make_unique<juce::SystemTrayIconComponent>();
    
    // Create tray icon image
    juce::Image trayImage(juce::Image::ARGB, 16, 16, true);
    juce::Graphics g(trayImage);
    g.fillAll(juce::Colours::transparentBlack);
    g.setColour(ThemeColors::accent);
    g.fillEllipse(2, 2, 12, 12);
    
    systemTrayIcon->setImage(trayImage);
    systemTrayIcon->setTooltip("QUIET - Noise Reduction");
    
    // Mouse handler for tray icon
    systemTrayIcon->onMouseDown = [this]() {
        setVisible(!isVisible());
        if (isVisible())
            toFront(true);
    };
#endif
}

std::unique_ptr<MainWindow> MainWindow::create(
    core::AudioDeviceManager& audioManager,
    core::ConfigurationManager& configManager,
    core::EventDispatcher& eventDispatcher)
{
    return std::make_unique<MainWindow>("QUIET", audioManager, 
                                       configManager, eventDispatcher);
}

} // namespace ui
} // namespace quiet