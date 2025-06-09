#include "quiet/ui/WaveformDisplay.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Example application demonstrating the WaveformDisplay component
 */
class WaveformDisplayExample : public juce::Component,
                              private juce::Timer {
public:
    WaveformDisplayExample() {
        // Configure waveform display settings
        quiet::ui::WaveformDisplay::WaveformSettings settings;
        settings.drawingMode = quiet::ui::WaveformDisplay::DrawingMode::Line;
        settings.channelMode = quiet::ui::WaveformDisplay::ChannelMode::Both;
        settings.inputWaveformColour = juce::Colours::cyan;
        settings.outputWaveformColour = juce::Colours::lightgreen;
        settings.showGrid = true;
        settings.showTimeMarkers = true;
        settings.refreshRate = 60;
        
        waveformDisplay.setSettings(settings);
        waveformDisplay.setSampleRate(48000.0);
        
        addAndMakeVisible(waveformDisplay);
        
        // Create zoom controls
        zoomInButton.setButtonText("+");
        zoomInButton.onClick = [this] {
            waveformDisplay.setZoomLevel(waveformDisplay.getZoomLevel() * 1.5f);
        };
        addAndMakeVisible(zoomInButton);
        
        zoomOutButton.setButtonText("-");
        zoomOutButton.onClick = [this] {
            waveformDisplay.setZoomLevel(waveformDisplay.getZoomLevel() / 1.5f);
        };
        addAndMakeVisible(zoomOutButton);
        
        zoomResetButton.setButtonText("Reset");
        zoomResetButton.onClick = [this] {
            waveformDisplay.setZoomLevel(1.0f);
            waveformDisplay.setTimeOffset(0.0f);
        };
        addAndMakeVisible(zoomResetButton);
        
        // Drawing mode selector
        drawingModeBox.addItem("Line", 1);
        drawingModeBox.addItem("Filled", 2);
        drawingModeBox.addItem("Dots", 3);
        drawingModeBox.setSelectedId(1);
        drawingModeBox.onChange = [this] {
            auto settings = waveformDisplay.getSettings();
            switch (drawingModeBox.getSelectedId()) {
                case 1: settings.drawingMode = quiet::ui::WaveformDisplay::DrawingMode::Line; break;
                case 2: settings.drawingMode = quiet::ui::WaveformDisplay::DrawingMode::Filled; break;
                case 3: settings.drawingMode = quiet::ui::WaveformDisplay::DrawingMode::Dots; break;
            }
            waveformDisplay.setSettings(settings);
        };
        addAndMakeVisible(drawingModeBox);
        
        // Channel mode selector
        channelModeBox.addItem("Input", 1);
        channelModeBox.addItem("Output", 2);
        channelModeBox.addItem("Both", 3);
        channelModeBox.setSelectedId(3);
        channelModeBox.onChange = [this] {
            auto settings = waveformDisplay.getSettings();
            switch (channelModeBox.getSelectedId()) {
                case 1: settings.channelMode = quiet::ui::WaveformDisplay::ChannelMode::Input; break;
                case 2: settings.channelMode = quiet::ui::WaveformDisplay::ChannelMode::Output; break;
                case 3: settings.channelMode = quiet::ui::WaveformDisplay::ChannelMode::Both; break;
            }
            waveformDisplay.setSettings(settings);
        };
        addAndMakeVisible(channelModeBox);
        
        // Start generating test audio
        startTimerHz(100); // 100Hz update rate for test signal
        
        setSize(800, 600);
    }
    
    void resized() override {
        auto bounds = getLocalBounds();
        
        // Control panel at top
        auto controlArea = bounds.removeFromTop(40);
        controlArea.reduce(10, 5);
        
        zoomInButton.setBounds(controlArea.removeFromLeft(40));
        controlArea.removeFromLeft(5);
        zoomOutButton.setBounds(controlArea.removeFromLeft(40));
        controlArea.removeFromLeft(5);
        zoomResetButton.setBounds(controlArea.removeFromLeft(60));
        controlArea.removeFromLeft(20);
        
        drawingModeBox.setBounds(controlArea.removeFromLeft(100));
        controlArea.removeFromLeft(10);
        channelModeBox.setBounds(controlArea.removeFromLeft(100));
        
        // Waveform display takes remaining space
        bounds.reduce(10, 10);
        waveformDisplay.setBounds(bounds);
    }
    
private:
    void timerCallback() override {
        // Generate test signals
        const int numSamples = 480; // 10ms at 48kHz
        std::vector<float> inputSamples(numSamples);
        std::vector<float> outputSamples(numSamples);
        
        const double sampleRate = 48000.0;
        const double frequency = 440.0; // A4 note
        
        for (int i = 0; i < numSamples; ++i) {
            const double time = (sampleIndex + i) / sampleRate;
            
            // Input: sine wave with some noise
            inputSamples[i] = static_cast<float>(
                0.7 * std::sin(2.0 * M_PI * frequency * time) +
                0.1 * (random.nextFloat() * 2.0f - 1.0f)
            );
            
            // Output: processed sine wave (simulated noise reduction)
            outputSamples[i] = static_cast<float>(
                0.6 * std::sin(2.0 * M_PI * frequency * time)
            );
        }
        
        // Push samples to waveform display
        waveformDisplay.pushInputBuffer(inputSamples.data(), numSamples);
        waveformDisplay.pushOutputBuffer(outputSamples.data(), numSamples);
        
        sampleIndex += numSamples;
    }
    
    quiet::ui::WaveformDisplay waveformDisplay;
    juce::TextButton zoomInButton, zoomOutButton, zoomResetButton;
    juce::ComboBox drawingModeBox, channelModeBox;
    
    juce::Random random;
    size_t sampleIndex = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplayExample)
};

// Main application
class WaveformDisplayApp : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Waveform Display Example"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    
    void initialise(const juce::String&) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }
    
    void shutdown() override {
        mainWindow = nullptr;
    }
    
private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                           juce::Desktop::getInstance().getDefaultLookAndFeel()
                               .findColour(juce::ResizableWindow::backgroundColourId),
                           DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new WaveformDisplayExample(), true);
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        
        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
        
    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
    
    std::unique_ptr<MainWindow> mainWindow;
};

// Create the application instance
START_JUCE_APPLICATION(WaveformDisplayApp)