/*
  ==============================================================================

    MainWindow.h
    Created: 2025
    Author:  QUIET Application

    Main application window header using JUCE framework.
    Defines the primary user interface for noise cancellation control.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include "../interfaces/IAudioEventListener.h"

// Forward declarations
class AudioDeviceManager;
class ConfigurationManager;
class EventDispatcher;

//==============================================================================
/**
    Main application window class.
    
    Provides the complete user interface for the QUIET noise cancellation app,
    including audio device selection, noise reduction controls, visualizations,
    and system tray integration.
*/
class MainWindow : public juce::DocumentWindow,
                   public IAudioEventListener,
                   private juce::KeyListener
{
public:
    //==============================================================================
    /** Constructor */
    MainWindow(juce::String name, AudioDeviceManager& audioDeviceManager,
               ConfigurationManager& configManager, EventDispatcher& eventDispatcher);
    
    /** Destructor */
    ~MainWindow() override;
    
    //==============================================================================
    /** Factory method to create the main window */
    static std::unique_ptr<MainWindow> create(AudioDeviceManager& audioDeviceManager,
                                              ConfigurationManager& configManager,
                                              EventDispatcher& eventDispatcher);
    
    //==============================================================================
    /** DocumentWindow overrides */
    void closeButtonPressed() override;
    
    /** KeyListener overrides */
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    
    /** IAudioEventListener implementation */
    void onAudioEvent(AudioEvent event, const EventData& data) override;
    
    //==============================================================================
    /** Window state management */
    void restoreWindowState();
    void saveWindowState();
    
    //==============================================================================
    /** Get the audio device manager reference */
    AudioDeviceManager& getAudioDeviceManager() { return audioDeviceManager; }
    
    /** Get the configuration manager reference */
    ConfigurationManager& getConfigurationManager() { return configManager; }
    
    /** Get the event dispatcher reference */
    EventDispatcher& getEventDispatcher() { return eventDispatcher; }
    
private:
    //==============================================================================
    /** Internal component class that contains all UI elements */
    class MainContentComponent;
    
    //==============================================================================
    /** References to core system components */
    AudioDeviceManager& audioDeviceManager;
    ConfigurationManager& configManager;
    EventDispatcher& eventDispatcher;
    
    /** UI components */
    std::unique_ptr<MainContentComponent> mainComponent;
    std::unique_ptr<juce::LookAndFeel> lookAndFeel;
    std::unique_ptr<juce::SystemTrayIconComponent> systemTrayIcon;
    
    //==============================================================================
    /** Initialize system tray functionality */
    void initializeSystemTray();
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

//==============================================================================
/**
    Helper classes for specific UI components
*/

/** Custom toggle button for modern appearance */
class ModernToggleButton : public juce::ToggleButton
{
public:
    ModernToggleButton() {}
    
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModernToggleButton)
};

/** Animated level meter component */
class AnimatedLevelMeter : public juce::Component, public juce::Timer
{
public:
    AnimatedLevelMeter();
    ~AnimatedLevelMeter() override;
    
    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    
    void setLevel(float newLevel);
    void setPeakLevel(float peak);
    
private:
    float currentLevel = 0.0f;
    float targetLevel = 0.0f;
    float peakLevel = 0.0f;
    float peakHoldTime = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnimatedLevelMeter)
};

/** Tooltip window with custom styling */
class CustomTooltipWindow : public juce::TooltipWindow
{
public:
    CustomTooltipWindow() : TooltipWindow(nullptr, 700) {}
    
    void paint(juce::Graphics& g) override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomTooltipWindow)
};

//==============================================================================
/**
    Keyboard shortcut definitions
*/
namespace KeyboardShortcuts
{
    const juce::KeyPress toggleNoiseReduction('T', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress showSettings(',', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress minimizeWindow('M', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress quitApplication('Q', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress nextDevice(']', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress previousDevice('[', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress increaseReduction('+', juce::ModifierKeys::commandModifier, 0);
    const juce::KeyPress decreaseReduction('-', juce::ModifierKeys::commandModifier, 0);
}

//==============================================================================
/**
    Animation helper class for smooth UI transitions
*/
class UIAnimator
{
public:
    /** Animate a component property */
    static void animateComponent(juce::Component* component, 
                                const juce::Rectangle<int>& finalBounds,
                                int durationMs = 200);
    
    /** Fade in/out a component */
    static void fadeComponent(juce::Component* component, 
                             float finalAlpha,
                             int durationMs = 150);
    
    /** Animate color change */
    static void animateColor(juce::Component* component,
                            int colorId,
                            const juce::Colour& finalColor,
                            int durationMs = 200);
};

//==============================================================================
/**
    Theme colors for the application
*/
namespace ThemeColors
{
    const juce::Colour background(0xff1e1e1e);
    const juce::Colour panel(0xff2d2d2d);
    const juce::Colour border(0xff3d3d3d);
    const juce::Colour text(0xffe0e0e0);
    const juce::Colour textDim(0xff808080);
    const juce::Colour accent(0xff00ff00);
    const juce::Colour accentDim(0xff00cc00);
    const juce::Colour warning(0xffffff00);
    const juce::Colour error(0xffff0000);
    const juce::Colour success(0xff00ff00);
    
    /** Get color with hover state */
    inline juce::Colour getHoverColor(const juce::Colour& base)
    {
        return base.brighter(0.2f);
    }
    
    /** Get color with pressed state */
    inline juce::Colour getPressedColor(const juce::Colour& base)
    {
        return base.darker(0.2f);
    }
}