#!/bin/bash
# Uninstallation script for QUIET

echo ""
echo "============================================"
echo "QUIET Uninstaller"
echo "============================================"
echo ""

# Detect operating system
OS="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
else
    echo "Error: Unsupported operating system: $OSTYPE"
    exit 1
fi

echo "This will remove QUIET from your system."
read -p "Are you sure you want to continue? (y/n) " -n 1 -r
echo ""
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

if [[ "$OS" == "macos" ]]; then
    echo ""
    echo "Removing QUIET from macOS..."
    
    # Remove application
    if [ -d "/Applications/QUIET.app" ]; then
        echo "Removing application..."
        sudo rm -rf "/Applications/QUIET.app"
    fi
    
    # Remove login item
    echo "Removing login item..."
    osascript -e 'tell application "System Events" to delete login item "QUIET"' 2>/dev/null || true
    
    # Remove preferences
    echo "Removing preferences..."
    rm -rf ~/Library/Preferences/com.quietapp.quiet.plist
    rm -rf ~/Library/Application\ Support/QUIET
    rm -rf ~/Library/Caches/com.quietapp.quiet
    
    # Remove aggregate audio device
    echo "Note: Please manually remove 'QUIET Virtual Mic' from Audio MIDI Setup if it exists."
    
elif [[ "$OS" == "linux" ]]; then
    echo ""
    echo "Removing QUIET from Linux..."
    
    # Check if installed system-wide
    if [ -f "/usr/local/bin/Quiet" ]; then
        echo "Removing system installation..."
        sudo rm -f /usr/local/bin/Quiet
        sudo rm -rf /usr/local/share/quiet
        sudo rm -f /usr/local/share/applications/quiet.desktop
        sudo rm -f /usr/local/share/icons/quiet.png
    fi
    
    # Remove user files
    echo "Removing user configuration..."
    rm -rf ~/.config/quiet
    rm -rf ~/.local/share/quiet
    rm -f ~/.local/share/applications/quiet.desktop
    
    # Update desktop database
    update-desktop-database ~/.local/share/applications/ 2>/dev/null || true
fi

# Ask about configuration files
echo ""
read -p "Do you want to remove all QUIET configuration and preference files? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [[ "$OS" == "macos" ]]; then
        rm -rf ~/Library/Application\ Support/QUIET
        rm -rf ~/Library/Preferences/com.quietapp.quiet.*
    else
        rm -rf ~/.config/quiet
        rm -rf ~/.quiet
    fi
    echo "Configuration files removed."
fi

echo ""
echo "============================================"
echo "Uninstallation Complete"
echo "============================================"
echo ""
echo "QUIET has been removed from your system."
echo ""
echo "Note: Virtual audio devices (VB-Cable/BlackHole) were not removed"
echo "as they may be used by other applications."
echo ""