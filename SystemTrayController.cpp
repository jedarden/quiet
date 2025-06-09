/*
  ==============================================================================

    SystemTrayController.cpp
    Created: 6 Sep 2025
    Author:  QUIET Development Team
    
    Manages system tray icon functionality including:
    - Cross-platform tray icon display
    - Context menu with app controls
    - Notification bubbles
    - Minimize to tray
    - Dynamic status updates
    - Custom icon animations

  ==============================================================================
*/

#include "SystemTrayController.h"
#include "MainWindow.h"
#include "AudioEngine.h"
#include "NoiseReductionProcessor.h"
#include "ConfigurationManager.h"
#include "EventDispatcher.h"
#include <JuceHeader.h>

//==============================================================================
// TrayIconAnimator - Handles animated tray icon updates
//==============================================================================
class TrayIconAnimator : public Timer
{
public:
    TrayIconAnimator(SystemTrayIconComponent* icon) 
        : trayIcon(icon), currentFrame(0), isAnimating(false)
    {
        // Load animation frames
        loadAnimationFrames();
    }
    
    void startAnimation(AnimationType type)
    {
        animationType = type;
        currentFrame = 0;
        isAnimating = true;
        
        // Different frame rates for different animations
        int frameRate = (type == AnimationType::Processing) ? 100 : 50;
        startTimer(frameRate);
    }
    
    void stopAnimation()
    {
        isAnimating = false;
        stopTimer();
        
        // Reset to default icon
        if (trayIcon != nullptr)
        {
            trayIcon->setIconImage(defaultIcon);
        }
    }
    
    void timerCallback() override
    {
        if (!isAnimating || frames.empty()) return;
        
        // Update to next frame
        currentFrame = (currentFrame + 1) % frames.size();
        
        if (trayIcon != nullptr)
        {
            trayIcon->setIconImage(frames[currentFrame]);
        }
    }
    
    enum class AnimationType
    {
        Processing,
        Connecting,
        Error
    };
    
private:
    void loadAnimationFrames()
    {
        // Load default icon
        defaultIcon = ImageCache::getFromMemory(BinaryData::tray_icon_default_png,
                                               BinaryData::tray_icon_default_pngSize);
        
        // Load animation frames based on platform
        #if JUCE_WINDOWS
            int iconSize = 16;
        #elif JUCE_MAC
            int iconSize = 22;
        #else
            int iconSize = 24;
        #endif
        
        // Create processing animation frames (pulsing effect)
        for (int i = 0; i < 8; ++i)
        {
            float alpha = 0.5f + 0.5f * std::sin(i * MathConstants<float>::pi / 4);
            Image frame = defaultIcon.createCopy();
            frame.multiplyAllAlphas(alpha);
            frames.add(frame.rescaled(iconSize, iconSize, Graphics::highResamplingQuality));
        }
    }
    
    SystemTrayIconComponent* trayIcon;
    Array<Image> frames;
    Image defaultIcon;
    int currentFrame;
    bool isAnimating;
    AnimationType animationType;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrayIconAnimator)
};

//==============================================================================
// SystemTrayController Implementation
//==============================================================================
SystemTrayController::SystemTrayController(MainWindow* window, AudioEngine* engine)
    : mainWindow(window), 
      audioEngine(engine),
      lastNoiseReductionState(false),
      minimizeToTray(false)
{
    // Initialize system tray icon
    trayIcon = std::make_unique<SystemTrayIconComponent>();
    
    // Create animator
    animator = std::make_unique<TrayIconAnimator>(trayIcon.get());
    
    // Load configuration
    auto config = ConfigurationManager::getInstance();
    minimizeToTray = config->getSetting("ui.minimize_to_tray", false);
    showNotifications = config->getSetting("ui.show_notifications", true);
    
    // Set initial icon and tooltip
    updateTrayIcon();
    updateTooltip();
    
    // Register event listeners
    registerEventListeners();
    
    // Platform-specific initialization
    initializePlatformSpecific();
}

SystemTrayController::~SystemTrayController()
{
    // Save settings
    auto config = ConfigurationManager::getInstance();
    config->setSetting("ui.minimize_to_tray", minimizeToTray);
    config->setSetting("ui.show_notifications", showNotifications);
    
    // Unregister event listeners
    EventDispatcher::getInstance()->removeListener(this);
}

//==============================================================================
// Public Methods
//==============================================================================
void SystemTrayController::showTrayIcon(bool shouldShow)
{
    if (shouldShow)
    {
        createContextMenu();
        trayIcon->setVisible(true);
    }
    else
    {
        trayIcon->setVisible(false);
    }
}

void SystemTrayController::showNotification(const String& title, const String& message, NotificationType type)
{
    if (!showNotifications) return;
    
    // Platform-specific notification display
    #if JUCE_WINDOWS
        // Windows balloon notification
        trayIcon->showInfoBubble(title, message);
    #elif JUCE_MAC
        // macOS notification center
        showMacOSNotification(title, message, type);
    #else
        // Linux notification (libnotify)
        showLinuxNotification(title, message, type);
    #endif
    
    // Also update tooltip with latest status
    updateTooltip();
}

void SystemTrayController::setMinimizeToTray(bool shouldMinimize)
{
    minimizeToTray = shouldMinimize;
    updateContextMenu();
}

void SystemTrayController::handleMinimize()
{
    if (minimizeToTray && mainWindow != nullptr)
    {
        mainWindow->setVisible(false);
        
        if (showNotifications)
        {
            showNotification("QUIET", "Application minimized to system tray", NotificationType::Info);
        }
    }
}

void SystemTrayController::restoreWindow()
{
    if (mainWindow != nullptr)
    {
        mainWindow->setVisible(true);
        mainWindow->toFront(true);
        
        #if JUCE_WINDOWS
            // Restore from minimized state on Windows
            if (mainWindow->getPeer() != nullptr)
            {
                mainWindow->getPeer()->setMinimised(false);
            }
        #endif
    }
}

//==============================================================================
// Private Methods
//==============================================================================
void SystemTrayController::updateTrayIcon()
{
    if (audioEngine == nullptr) return;
    
    bool isProcessingEnabled = audioEngine->getNoiseReductionProcessor()->isEnabled();
    bool isConnected = audioEngine->getVirtualDeviceRouter()->isConnected();
    
    // Update icon based on state
    if (!isConnected)
    {
        // Show disconnected icon
        setTrayIcon(IconType::Disconnected);
        animator->stopAnimation();
    }
    else if (isProcessingEnabled)
    {
        // Show active icon with animation
        setTrayIcon(IconType::Active);
        animator->startAnimation(TrayIconAnimator::AnimationType::Processing);
    }
    else
    {
        // Show idle icon
        setTrayIcon(IconType::Idle);
        animator->stopAnimation();
    }
    
    // Check if state changed for notifications
    if (isProcessingEnabled != lastNoiseReductionState)
    {
        lastNoiseReductionState = isProcessingEnabled;
        
        if (showNotifications)
        {
            String status = isProcessingEnabled ? "enabled" : "disabled";
            showNotification("Noise Reduction", "Noise reduction " + status, NotificationType::Info);
        }
    }
}

void SystemTrayController::updateTooltip()
{
    if (audioEngine == nullptr) return;
    
    String tooltip = "QUIET - AI Noise Cancellation\n";
    
    // Add status information
    bool isProcessingEnabled = audioEngine->getNoiseReductionProcessor()->isEnabled();
    bool isConnected = audioEngine->getVirtualDeviceRouter()->isConnected();
    
    if (!isConnected)
    {
        tooltip += "Status: Disconnected";
    }
    else if (isProcessingEnabled)
    {
        float reductionLevel = audioEngine->getNoiseReductionProcessor()->getReductionLevel();
        tooltip += "Status: Active\n";
        tooltip += "Reduction: " + String(reductionLevel, 1) + " dB";
    }
    else
    {
        tooltip += "Status: Idle";
    }
    
    // Add current device info
    String currentDevice = audioEngine->getAudioDeviceManager()->getCurrentDeviceName();
    if (currentDevice.isNotEmpty())
    {
        tooltip += "\nInput: " + currentDevice;
    }
    
    trayIcon->setTooltip(tooltip);
}

void SystemTrayController::createContextMenu()
{
    PopupMenu menu;
    
    // Application name header
    menu.addItem(1, "QUIET - AI Noise Cancellation", false, false);
    menu.addSeparator();
    
    // Noise reduction toggle
    bool isProcessingEnabled = audioEngine->getNoiseReductionProcessor()->isEnabled();
    menu.addItem(2, "Enable Noise Reduction", true, isProcessingEnabled);
    
    menu.addSeparator();
    
    // Device selection submenu
    PopupMenu deviceMenu;
    auto devices = audioEngine->getAudioDeviceManager()->getAvailableDevices();
    String currentDevice = audioEngine->getAudioDeviceManager()->getCurrentDeviceId();
    
    int deviceIndex = 100;
    for (const auto& device : devices)
    {
        bool isSelected = (device.id == currentDevice);
        deviceMenu.addItem(deviceIndex++, device.name, true, isSelected);
    }
    
    menu.addSubMenu("Input Device", deviceMenu);
    
    menu.addSeparator();
    
    // Settings
    menu.addItem(3, "Show Window", mainWindow != nullptr && !mainWindow->isVisible());
    menu.addItem(4, "Minimize to Tray", true, minimizeToTray);
    menu.addItem(5, "Show Notifications", true, showNotifications);
    menu.addItem(6, "Start with System", true, isStartupEnabled());
    
    menu.addSeparator();
    
    // About and Exit
    menu.addItem(7, "About QUIET...");
    menu.addItem(8, "Exit");
    
    // Set the menu
    trayIcon->setContextMenu(&menu);
}

void SystemTrayController::updateContextMenu()
{
    createContextMenu();
}

void SystemTrayController::handleMenuSelection(int menuItemID)
{
    switch (menuItemID)
    {
        case 2: // Toggle noise reduction
            if (audioEngine != nullptr)
            {
                auto processor = audioEngine->getNoiseReductionProcessor();
                processor->setEnabled(!processor->isEnabled());
                updateTrayIcon();
                updateTooltip();
            }
            break;
            
        case 3: // Show window
            restoreWindow();
            break;
            
        case 4: // Toggle minimize to tray
            minimizeToTray = !minimizeToTray;
            updateContextMenu();
            break;
            
        case 5: // Toggle notifications
            showNotifications = !showNotifications;
            updateContextMenu();
            break;
            
        case 6: // Toggle startup
            setStartupEnabled(!isStartupEnabled());
            updateContextMenu();
            break;
            
        case 7: // About
            showAboutDialog();
            break;
            
        case 8: // Exit
            JUCEApplication::getInstance()->systemRequestedQuit();
            break;
            
        default:
            // Handle device selection (IDs 100+)
            if (menuItemID >= 100 && menuItemID < 200)
            {
                int deviceIndex = menuItemID - 100;
                auto devices = audioEngine->getAudioDeviceManager()->getAvailableDevices();
                
                if (deviceIndex < devices.size())
                {
                    audioEngine->getAudioDeviceManager()->selectDevice(devices[deviceIndex].id);
                    updateTooltip();
                    updateContextMenu();
                }
            }
            break;
    }
}

void SystemTrayController::setTrayIcon(IconType type)
{
    Image icon;
    
    switch (type)
    {
        case IconType::Idle:
            icon = ImageCache::getFromMemory(BinaryData::tray_icon_idle_png,
                                            BinaryData::tray_icon_idle_pngSize);
            break;
            
        case IconType::Active:
            icon = ImageCache::getFromMemory(BinaryData::tray_icon_active_png,
                                            BinaryData::tray_icon_active_pngSize);
            break;
            
        case IconType::Disconnected:
            icon = ImageCache::getFromMemory(BinaryData::tray_icon_disconnected_png,
                                            BinaryData::tray_icon_disconnected_pngSize);
            break;
            
        case IconType::Error:
            icon = ImageCache::getFromMemory(BinaryData::tray_icon_error_png,
                                            BinaryData::tray_icon_error_pngSize);
            break;
    }
    
    // Resize icon based on platform
    #if JUCE_WINDOWS
        icon = icon.rescaled(16, 16, Graphics::highResamplingQuality);
    #elif JUCE_MAC
        icon = icon.rescaled(22, 22, Graphics::highResamplingQuality);
    #else
        icon = icon.rescaled(24, 24, Graphics::highResamplingQuality);
    #endif
    
    trayIcon->setIconImage(icon);
}

void SystemTrayController::registerEventListeners()
{
    auto dispatcher = EventDispatcher::getInstance();
    
    // Audio events
    dispatcher->addListener(AudioEvent::DEVICE_CHANGED, [this](const EventData& data) {
        updateTooltip();
        updateContextMenu();
    });
    
    dispatcher->addListener(AudioEvent::PROCESSING_TOGGLED, [this](const EventData& data) {
        updateTrayIcon();
        updateTooltip();
    });
    
    dispatcher->addListener(AudioEvent::VIRTUAL_DEVICE_CONNECTED, [this](const EventData& data) {
        updateTrayIcon();
        updateTooltip();
        
        if (showNotifications)
        {
            showNotification("Virtual Device", "Connected to virtual audio device", NotificationType::Success);
        }
    });
    
    dispatcher->addListener(AudioEvent::VIRTUAL_DEVICE_DISCONNECTED, [this](const EventData& data) {
        updateTrayIcon();
        updateTooltip();
        
        if (showNotifications)
        {
            showNotification("Virtual Device", "Virtual audio device disconnected", NotificationType::Warning);
        }
    });
    
    dispatcher->addListener(AudioEvent::ERROR_OCCURRED, [this](const EventData& data) {
        setTrayIcon(IconType::Error);
        
        if (showNotifications && data.message.isNotEmpty())
        {
            showNotification("Error", data.message, NotificationType::Error);
        }
    });
}

void SystemTrayController::showAboutDialog()
{
    String message = "QUIET - AI Noise Cancellation\n\n";
    message += "Version: 1.0.0\n";
    message += "Built with JUCE and RNNoise\n\n";
    message += "Intelligent noise reduction for clear communication";
    
    AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                                   "About QUIET",
                                   message,
                                   "OK");
}

//==============================================================================
// Platform-specific implementations
//==============================================================================
void SystemTrayController::initializePlatformSpecific()
{
    #if JUCE_WINDOWS
        // Windows-specific initialization
        // Set up jump list for Windows 7+
        if (SystemStats::getOperatingSystemType() >= SystemStats::Windows7)
        {
            setupWindowsJumpList();
        }
    #elif JUCE_MAC
        // macOS-specific initialization
        // Request notification permissions
        requestMacOSNotificationPermissions();
    #endif
    
    // Set up tray icon callbacks
    trayIcon->onMouseDown = [this]() {
        handleTrayIconClick();
    };
    
    trayIcon->onMouseDoubleClick = [this]() {
        restoreWindow();
    };
    
    trayIcon->onContextMenuItemSelected = [this](int itemId) {
        handleMenuSelection(itemId);
    };
}

void SystemTrayController::handleTrayIconClick()
{
    #if JUCE_WINDOWS
        // On Windows, single click shows menu
        createContextMenu();
    #elif JUCE_MAC
        // On macOS, single click toggles window
        if (mainWindow != nullptr)
        {
            if (mainWindow->isVisible())
                mainWindow->setVisible(false);
            else
                restoreWindow();
        }
    #else
        // On Linux, single click shows menu
        createContextMenu();
    #endif
}

#if JUCE_WINDOWS
void SystemTrayController::setupWindowsJumpList()
{
    // Windows jump list implementation
    // This would use Windows API to create jump list items
}
#endif

#if JUCE_MAC
void SystemTrayController::showMacOSNotification(const String& title, const String& message, NotificationType type)
{
    // Use NSUserNotification for macOS notifications
    // This would use Objective-C++ to display native notifications
}

void SystemTrayController::requestMacOSNotificationPermissions()
{
    // Request notification permissions on macOS
    // This would use Objective-C++ to request permissions
}
#endif

#if JUCE_LINUX
void SystemTrayController::showLinuxNotification(const String& title, const String& message, NotificationType type)
{
    // Use libnotify for Linux notifications
    String iconName;
    switch (type)
    {
        case NotificationType::Info:
            iconName = "dialog-information";
            break;
        case NotificationType::Warning:
            iconName = "dialog-warning";
            break;
        case NotificationType::Error:
            iconName = "dialog-error";
            break;
        case NotificationType::Success:
            iconName = "dialog-information";
            break;
    }
    
    // Execute notify-send command
    String command = "notify-send";
    command += " -i " + iconName;
    command += " \"" + title + "\"";
    command += " \"" + message + "\"";
    
    system(command.toRawUTF8());
}
#endif

bool SystemTrayController::isStartupEnabled()
{
    #if JUCE_WINDOWS
        // Check Windows registry for startup entry
        WindowsRegistry::getValue("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\QUIET", "");
        return WindowsRegistry::valueExists("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\QUIET");
    #elif JUCE_MAC
        // Check macOS login items
        return checkMacOSLoginItem();
    #else
        // Check Linux autostart
        File autostartFile("~/.config/autostart/quiet.desktop");
        return autostartFile.exists();
    #endif
}

void SystemTrayController::setStartupEnabled(bool enabled)
{
    #if JUCE_WINDOWS
        if (enabled)
        {
            // Add to Windows startup
            String appPath = File::getSpecialLocation(File::currentExecutableFile).getFullPathName();
            WindowsRegistry::setValue("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\QUIET", appPath);
        }
        else
        {
            // Remove from Windows startup
            WindowsRegistry::deleteValue("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\QUIET");
        }
    #elif JUCE_MAC
        // Add/remove from macOS login items
        setMacOSLoginItem(enabled);
    #else
        // Linux autostart using .desktop file
        File autostartDir("~/.config/autostart");
        autostartDir.createDirectory();
        
        File autostartFile = autostartDir.getChildFile("quiet.desktop");
        
        if (enabled)
        {
            String desktopEntry = "[Desktop Entry]\n";
            desktopEntry += "Type=Application\n";
            desktopEntry += "Name=QUIET\n";
            desktopEntry += "Comment=AI Noise Cancellation\n";
            desktopEntry += "Exec=" + File::getSpecialLocation(File::currentExecutableFile).getFullPathName() + "\n";
            desktopEntry += "Hidden=false\n";
            desktopEntry += "X-GNOME-Autostart-enabled=true\n";
            
            autostartFile.replaceWithText(desktopEntry);
        }
        else
        {
            autostartFile.deleteFile();
        }
    #endif
}

#if JUCE_MAC
bool SystemTrayController::checkMacOSLoginItem()
{
    // This would use Objective-C++ to check login items
    return false;
}

void SystemTrayController::setMacOSLoginItem(bool enabled)
{
    // This would use Objective-C++ to add/remove login items
}
#endif