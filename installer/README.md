# QUIET Installer Documentation

This directory contains all the necessary files and scripts to create production-ready installers for QUIET on Windows and macOS platforms.

## Directory Structure

```
installer/
├── windows/                  # Windows installer files
│   ├── quiet_installer.nsi  # NSIS installer script
│   ├── post_install.bat     # Post-installation configuration
│   ├── header.bmp          # Installer header graphic (needs to be created)
│   └── welcome.bmp         # Welcome page graphic (needs to be created)
├── macos/                   # macOS installer files
│   ├── Info.plist          # Application bundle information
│   ├── dmg_setup.scpt      # DMG customization script
│   ├── post_install.sh     # Post-installation configuration
│   └── dmg_background.png  # DMG background image (needs to be created)
└── scripts/                 # Cross-platform scripts
    ├── install_quiet.sh    # Universal installation script
    └── uninstall_quiet.sh  # Uninstallation script
```

## Building Installers

### Windows

1. **Prerequisites:**
   - Windows 10 or later (64-bit)
   - Visual Studio 2019 or later
   - CMake 3.20+
   - NSIS 3.0+ (https://nsis.sourceforge.io/)

2. **Build QUIET:**
   ```cmd
   cd C:\path\to\quiet
   mkdir build && cd build
   cmake .. -G "Visual Studio 16 2019" -A x64
   cmake --build . --config Release
   ```

3. **Create Installer:**
   ```cmd
   cpack -G NSIS -C Release
   ```

   Or manually:
   ```cmd
   cd installer\windows
   makensis quiet_installer.nsi
   ```

4. **Output:** `QUIET-1.0.0-Windows-Setup.exe`

### macOS

1. **Prerequisites:**
   - macOS 10.15 or later
   - Xcode Command Line Tools
   - CMake 3.20+
   - Apple Developer ID (for signing)

2. **Build QUIET:**
   ```bash
   cd /path/to/quiet
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   make -j$(sysctl -n hw.ncpu)
   ```

3. **Create Installer:**
   ```bash
   cpack -G DragNDrop
   ```

4. **Output:** `QUIET-1.0.0-Darwin-x86_64.dmg`

## Platform-Specific Features

### Windows Installer
- NSIS-based installer with modern UI
- Administrator privileges required
- Automatic VB-Cable download prompt
- Start Menu and Desktop shortcuts
- Windows Firewall exception
- Uninstaller with configuration cleanup option
- Support for both per-user and system-wide installation

### macOS Installer
- DMG disk image with drag-to-Applications interface
- Code signing and notarization support
- Automatic BlackHole download prompt
- Login item configuration
- Gatekeeper-friendly
- Universal binary support (Intel + Apple Silicon)

## Post-Installation Setup

### Windows
The installer includes `post_install.bat` which:
- Verifies VB-Cable installation
- Configures Windows audio settings
- Sets up firewall rules
- Optionally adds QUIET to startup

### macOS
The installer includes `post_install.sh` which:
- Verifies BlackHole installation
- Requests microphone permissions
- Creates aggregate audio device
- Optionally adds QUIET to login items

## Virtual Audio Device Requirements

### Windows - VB-Cable
- Download: https://vb-audio.com/Cable/
- Free for personal use
- Creates virtual audio input/output
- Required for routing processed audio

### macOS - BlackHole
- Download: https://existential.audio/blackhole/
- Open source (MIT license)
- Creates virtual audio interface
- Supports up to 16 channels

## Code Signing

### Windows
```cmd
signtool sign /t http://timestamp.digicert.com /f certificate.pfx /p password "QUIET-Setup.exe"
```

### macOS
```bash
# Sign the app
codesign --force --deep --sign "Developer ID Application: Name (TEAM)" QUIET.app

# Notarize
xcrun altool --notarize-app --primary-bundle-id "com.quietapp.quiet" \
  --username "email" --password "@keychain:AC_PASSWORD" --file QUIET.zip

# Staple
xcrun stapler staple QUIET.app
```

## Creating Installer Assets

### Icon Files
```bash
# Windows .ico
convert icon_256.png -define icon:auto-resize=256,128,64,48,32,16 icon.ico

# macOS .icns
iconutil -c icns icon.iconset
```

### Installer Graphics
- Windows: 150x57 header.bmp, 164x314 welcome.bmp (8-bit color)
- macOS: 500x400 dmg_background.png

## Testing Checklist

- [ ] Clean installation on fresh OS
- [ ] Upgrade from previous version
- [ ] Shortcuts created correctly
- [ ] Virtual device driver prompts work
- [ ] Application launches after installation
- [ ] Audio processing functions correctly
- [ ] Uninstaller removes all files
- [ ] Code signing validated
- [ ] No antivirus warnings

## Distribution

1. **GitHub Releases:**
   - Upload installers as release assets
   - Include SHA256 checksums
   - Provide installation instructions

2. **Website Downloads:**
   - Host on CDN for fast downloads
   - Provide mirror links
   - Display system requirements

3. **Auto-Update:**
   - Implement Sparkle (macOS) or WinSparkle (Windows)
   - Sign update feeds
   - Test update paths

## Troubleshooting

### Windows Issues
- **"Windows protected your PC"**: Need code signing certificate
- **Missing DLLs**: Include Visual C++ Redistributables
- **No virtual device**: Ensure VB-Cable installed correctly

### macOS Issues
- **"App is damaged"**: Notarization required
- **No microphone access**: Check System Preferences permissions
- **BlackHole not found**: Reinstall from official source

## Support

For installer-related issues:
- Check installer logs in temp directory
- Run with verbose output
- Contact: support@quietaudio.com