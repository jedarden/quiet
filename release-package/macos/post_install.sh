#!/bin/bash
# QUIET Post-Installation Script for macOS
# This script helps set up the virtual audio device and permissions

echo ""
echo "============================================"
echo "QUIET Post-Installation Configuration"
echo "============================================"
echo ""

# Check if running with proper permissions
if [ "$EUID" -ne 0 ]; then 
    echo "This script requires administrator privileges."
    echo "Please run with: sudo $0"
    exit 1
fi

# Function to check if BlackHole is installed
check_blackhole() {
    if [ -d "/Library/Audio/Plug-Ins/HAL/BlackHole.driver" ] || [ -d "/Library/Audio/Plug-Ins/HAL/BlackHole2ch.driver" ]; then
        return 0
    else
        return 1
    fi
}

# Check for BlackHole audio driver
echo "Checking for BlackHole audio driver..."
if check_blackhole; then
    echo "[OK] BlackHole audio driver is installed"
else
    echo "[WARNING] BlackHole audio driver not found"
    echo ""
    echo "QUIET requires BlackHole for virtual device functionality."
    echo "Please download and install it from: https://existential.audio/blackhole/"
    echo ""
    read -p "Would you like to open the download page now? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        open "https://existential.audio/blackhole/"
        echo ""
        echo "Please install BlackHole and run this script again."
        exit 1
    fi
fi

# Grant microphone permissions
echo ""
echo "Configuring microphone permissions..."
# This will trigger the permission dialog when QUIET first runs
defaults write com.quietapp.quiet NSMicrophoneUsageDescription -string "QUIET needs access to your microphone to process and remove background noise."

# Configure audio MIDI setup for aggregate device
echo ""
echo "Configuring audio devices..."
cat > /tmp/quiet_audio_setup.py << 'EOF'
#!/usr/bin/env python3
import subprocess
import json
import sys

def run_command(cmd):
    """Run a shell command and return the output."""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    return result.stdout.strip()

def create_aggregate_device():
    """Create an aggregate audio device for QUIET."""
    print("Creating QUIET aggregate audio device...")
    
    # Check if aggregate device already exists
    devices = run_command("system_profiler SPAudioDataType -json")
    if "QUIET Virtual Mic" in devices:
        print("QUIET Virtual Mic already exists")
        return
    
    # Create aggregate device using Audio MIDI Setup scripting
    script = '''
    tell application "Audio MIDI Setup"
        activate
        delay 1
        
        -- Create new aggregate device
        make new aggregate device with properties {name:"QUIET Virtual Mic"}
        
        -- Add BlackHole as subdevice
        tell aggregate device "QUIET Virtual Mic"
            make new subdevice with properties {device:"BlackHole 2ch"}
        end tell
    end tell
    '''
    
    subprocess.run(['osascript', '-e', script])
    print("Aggregate device created successfully")

try:
    create_aggregate_device()
except Exception as e:
    print(f"Note: Could not automatically create aggregate device: {e}")
    print("You may need to manually create it in Audio MIDI Setup")
EOF

python3 /tmp/quiet_audio_setup.py
rm -f /tmp/quiet_audio_setup.py

# Set up login item (optional)
echo ""
read -p "Would you like QUIET to start automatically at login? (y/n) " -n 1 -r
echo ""
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Adding QUIET to login items..."
    osascript -e 'tell application "System Events" to make login item at end with properties {path:"/Applications/QUIET.app", hidden:false}'
fi

# Configure privacy settings reminder
echo ""
echo "============================================"
echo "IMPORTANT: Manual Configuration Required"
echo "============================================"
echo ""
echo "1. When you first run QUIET, you'll be asked to grant microphone access."
echo "   Please click 'Allow' when prompted."
echo ""
echo "2. To use QUIET with your applications:"
echo "   - Open System Preferences > Sound"
echo "   - Set your microphone as the input device"
echo "   - In apps (Discord, Zoom, etc.), select 'QUIET Virtual Mic' as input"
echo ""
echo "3. For best results:"
echo "   - Use Audio MIDI Setup to configure audio devices"
echo "   - Set sample rate to 48000 Hz for all devices"
echo ""
read -p "Press any key to open System Preferences > Sound... " -n 1 -r
echo ""
open "x-apple.systempreferences:com.apple.preference.sound"

echo ""
echo "============================================"
echo "Configuration Complete!"
echo "============================================"
echo ""
echo "You can now run QUIET from:"
echo "- Applications folder"
echo "- Launchpad"
echo "- Spotlight (press Cmd+Space and type 'QUIET')"
echo ""