#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "../core/EventDispatcher.h"
#include "../core/AudioDeviceManager.h"
#include "../core/NoiseReductionProcessor.h"

namespace quiet {
namespace ui {

class WaveformDisplay;
class SpectrumAnalyzer;
class DeviceSelector;
class ControlPanel;

/**
 * @brief Main application window
 * 
 * The primary user interface for the QUIET application, providing:
 * - Device selection and configuration
 * - Noise reduction controls
 * - Real-time audio visualization
 * - System status and performance monitoring
 */
class MainWindow : public juce::DocumentWindow,
                   private juce::Timer {
public:
    MainWindow(const juce::String& name,
               core::EventDispatcher& eventDispatcher,
               core::AudioDeviceManager& audioManager,
               core::NoiseReductionProcessor& processor);
    
    ~MainWindow() override;

    // DocumentWindow overrides
    void closeButtonPressed() override;
    void moved() override;
    void resized() override;

    // Window management
    void showWindow();
    void hideWindow();
    void minimizeToTray();
    void restoreFromTray();
    
    // Configuration
    void saveWindowState();
    void restoreWindowState();

private:
    // Timer callback for UI updates
    void timerCallback() override;
    
    // Event handling
    void handleAudioEvent(const core::Event& event);
    void onDeviceChanged();
    void onAudioLevelChanged(float level);
    void onProcessingToggled(bool enabled);
    void onProcessingStatsUpdated(float cpuUsage, float latency, float reduction);
    
    // UI setup
    void createComponents();
    void setupLayout();
    void setupStyling();
    void setupEventListeners();
    
    // Component callbacks
    void onDeviceSelected(const std::string& deviceId);
    void onNoiseReductionToggled();
    void onReductionLevelChanged(core::NoiseReductionConfig::Level level);
    void onSettingsButtonClicked();
    void onAboutButtonClicked();
    
    // UI updates
    void updateDeviceList();
    void updateProcessingState();
    void updateAudioVisualization();
    void updatePerformanceMetrics();
    
    // Member variables
    core::EventDispatcher& m_eventDispatcher;
    core::AudioDeviceManager& m_audioManager;
    core::NoiseReductionProcessor& m_processor;
    
    core::EventDispatcher::ListenerHandle m_eventHandle;
    
    // UI Components
    std::unique_ptr<DeviceSelector> m_deviceSelector;
    std::unique_ptr<ControlPanel> m_controlPanel;
    std::unique_ptr<WaveformDisplay> m_inputWaveform;
    std::unique_ptr<WaveformDisplay> m_outputWaveform;
    std::unique_ptr<SpectrumAnalyzer> m_spectrumAnalyzer;
    
    // Layout components
    std::unique_ptr<juce::Component> m_contentComponent;
    std::unique_ptr<juce::TabbedComponent> m_visualizationTabs;
    
    // Status components
    std::unique_ptr<juce::Label> m_statusLabel;
    std::unique_ptr<juce::Label> m_cpuLabel;
    std::unique_ptr<juce::Label> m_latencyLabel;
    std::unique_ptr<juce::Label> m_reductionLabel;
    
    // Window state
    juce::Rectangle<int> m_savedBounds;
    bool m_isMinimized{false};
    bool m_closeToTray{true};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

/**
 * @brief Control panel component
 * 
 * Contains the main controls for noise reduction:
 * - Enable/disable toggle
 * - Reduction level selection
 * - Input level meter
 * - Settings access
 */
class ControlPanel : public juce::Component {
public:
    using ToggleCallback = std::function<void()>;
    using LevelCallback = std::function<void(core::NoiseReductionConfig::Level)>;
    using ButtonCallback = std::function<void()>;

    ControlPanel();
    ~ControlPanel() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Configuration
    void setEnabled(bool enabled);
    void setReductionLevel(core::NoiseReductionConfig::Level level);
    void setInputLevel(float level);
    void setProcessingStats(float cpuUsage, float latency, float reduction);

    // Callbacks
    void setToggleCallback(ToggleCallback callback);
    void setLevelCallback(LevelCallback callback);
    void setSettingsCallback(ButtonCallback callback);

private:
    void setupComponents();
    void updateToggleButton();
    void updateLevelMeter();
    void updateStatsDisplay();

    // Components
    std::unique_ptr<juce::ToggleButton> m_enableButton;
    std::unique_ptr<juce::ComboBox> m_levelComboBox;
    std::unique_ptr<juce::Slider> m_inputLevelMeter;
    std::unique_ptr<juce::TextButton> m_settingsButton;
    
    // Status labels
    std::unique_ptr<juce::Label> m_cpuLabel;
    std::unique_ptr<juce::Label> m_latencyLabel;
    std::unique_ptr<juce::Label> m_reductionLabel;

    // Callbacks
    ToggleCallback m_toggleCallback;
    LevelCallback m_levelCallback;
    ButtonCallback m_settingsCallback;

    // State
    bool m_enabled{true};
    core::NoiseReductionConfig::Level m_level{core::NoiseReductionConfig::Level::Medium};
    float m_inputLevel{0.0f};
    float m_cpuUsage{0.0f};
    float m_latency{0.0f};
    float m_reductionLevel{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlPanel)
};

} // namespace ui
} // namespace quiet