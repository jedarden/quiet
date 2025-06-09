/*
  ==============================================================================

    SystemTrayController.h
    Created: 6 Sep 2025
    Author:  QUIET Development Team
    
    System tray controller header - manages tray icon, context menu,
    notifications, and minimize-to-tray functionality

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>

// Forward declarations
class MainWindow;
class AudioEngine;
class TrayIconAnimator;

//==============================================================================
/**
    Manages the system tray icon and associated functionality.
    
    This class handles:
    - Creating and managing the system tray icon
    - Displaying context menus with app controls
    - Showing notification bubbles
    - Minimize to tray functionality
    - Dynamic icon updates based on app state
    - Platform-specific tray behaviors
*/
class SystemTrayController : public Component,
                            private Timer
{
public:
    //==============================================================================
    /** Constructor */
    SystemTrayController(MainWindow* mainWindow, AudioEngine* audioEngine);
    
    /** Destructor */
    ~SystemTrayController() override;
    
    //==============================================================================
    /** Shows or hides the system tray icon */
    void showTrayIcon(bool shouldShow);
    
    /** Notification types for different message styles */
    enum class NotificationType
    {
        Info,
        Warning,
        Error,
        Success
    };
    
    /** Shows a notification bubble/toast */
    void showNotification(const String& title, 
                         const String& message, 
                         NotificationType type = NotificationType::Info);
    
    /** Sets whether the app should minimize to tray */
    void setMinimizeToTray(bool shouldMinimize);
    
    /** Handles window minimize event */
    void handleMinimize();
    
    /** Restores the main window from tray */
    void restoreWindow();
    
    /** Gets whether minimize to tray is enabled */
    bool isMinimizeToTrayEnabled() const { return minimizeToTray; }
    
    /** Updates the tray icon based on current state */
    void updateTrayIcon();
    
    /** Updates the tooltip text */
    void updateTooltip();
    
private:
    //==============================================================================
    /** Icon types for different states */
    enum class IconType
    {
        Idle,
        Active,
        Disconnected,
        Error
    };
    
    /** Creates the context menu */
    void createContextMenu();
    
    /** Updates the context menu items */
    void updateContextMenu();
    
    /** Handles menu item selection */
    void handleMenuSelection(int menuItemID);
    
    /** Sets the tray icon image */
    void setTrayIcon(IconType type);
    
    /** Registers event listeners for state updates */
    void registerEventListeners();
    
    /** Shows the about dialog */
    void showAboutDialog();
    
    /** Timer callback for periodic updates */
    void timerCallback() override;
    
    //==============================================================================
    // Platform-specific methods
    
    /** Initializes platform-specific features */
    void initializePlatformSpecific();
    
    /** Handles tray icon click (behavior varies by platform) */
    void handleTrayIconClick();
    
    /** Checks if app is set to start with system */
    bool isStartupEnabled();
    
    /** Sets whether app should start with system */
    void setStartupEnabled(bool enabled);
    
#if JUCE_WINDOWS
    /** Sets up Windows jump list */
    void setupWindowsJumpList();
#endif
    
#if JUCE_MAC
    /** Shows macOS notification using native API */
    void showMacOSNotification(const String& title, 
                              const String& message, 
                              NotificationType type);
    
    /** Requests notification permissions on macOS */
    void requestMacOSNotificationPermissions();
    
    /** Checks if app is in macOS login items */
    bool checkMacOSLoginItem();
    
    /** Adds/removes app from macOS login items */
    void setMacOSLoginItem(bool enabled);
#endif
    
#if JUCE_LINUX
    /** Shows Linux notification using libnotify */
    void showLinuxNotification(const String& title, 
                              const String& message, 
                              NotificationType type);
#endif
    
    //==============================================================================
    // Member variables
    
    /** The system tray icon component */
    std::unique_ptr<SystemTrayIconComponent> trayIcon;
    
    /** Icon animator for animated states */
    std::unique_ptr<TrayIconAnimator> animator;
    
    /** Reference to main window */
    MainWindow* mainWindow;
    
    /** Reference to audio engine */
    AudioEngine* audioEngine;
    
    /** Current minimize to tray setting */
    bool minimizeToTray;
    
    /** Whether to show notifications */
    bool showNotifications;
    
    /** Last known noise reduction state */
    bool lastNoiseReductionState;
    
    /** Menu IDs */
    enum MenuIDs
    {
        MenuHeader = 1,
        MenuToggleNoiseReduction = 2,
        MenuShowWindow = 3,
        MenuMinimizeToTray = 4,
        MenuShowNotifications = 5,
        MenuStartWithSystem = 6,
        MenuAbout = 7,
        MenuExit = 8,
        MenuDeviceStart = 100  // Device IDs start at 100
    };
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemTrayController)
};

//==============================================================================
/**
    Helper class for managing tray icon resources
*/
class TrayIconResources
{
public:
    /** Gets the icon for a specific state and platform */
    static Image getIcon(SystemTrayController::IconType type, int size = 0);
    
    /** Gets the default icon size for the current platform */
    static int getDefaultIconSize();
    
    /** Loads custom icon set from file */
    static bool loadCustomIcons(const File& iconDirectory);
    
private:
    static HashMap<String, Image> customIcons;
};

//==============================================================================
/**
    Platform-specific tray utilities
*/
class TrayPlatformUtilities
{
public:
    /** Gets the recommended tray icon position */
    static Point<int> getRecommendedPosition();
    
    /** Checks if the system supports tray icons */
    static bool isSystemTraySupported();
    
    /** Gets platform-specific tray behavior flags */
    struct TrayBehavior
    {
        bool singleClickShowsMenu;
        bool supportsAnimatedIcons;
        bool supportsColorIcons;
        bool supportsBalloonNotifications;
        int maxTooltipLength;
    };
    
    static TrayBehavior getPlatformBehavior();
};