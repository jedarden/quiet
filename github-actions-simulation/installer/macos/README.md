# macOS Installer Assets

This directory contains assets and configurations for the macOS installer.

## Required Files

### Graphics
- `dmg_background.png` - Background image for DMG window (500x400 pixels)
- `.background/dmg_background.png` - Same image in hidden folder for DMG

### Creating the DMG Background
The background should be 500x400 pixels and include:
- QUIET logo
- Arrow pointing from app icon to Applications folder
- Installation instructions

## Building the Installer

### Prerequisites
1. Xcode Command Line Tools
2. Valid Apple Developer ID (for code signing)

### Build Steps
1. Build QUIET in Release mode:
   ```bash
   cd /path/to/quiet
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

2. Create the installer:
   ```bash
   cpack -G DragNDrop
   ```

This creates `QUIET-1.0.0-Darwin-x86_64.dmg`

## Code Signing

### Sign the Application
```bash
codesign --force --deep --sign "Developer ID Application: Your Name (TEAM_ID)" \
  --options runtime \
  --entitlements entitlements.plist \
  QUIET.app
```

### Create Entitlements File
Create `entitlements.plist`:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.device.audio-input</key>
    <true/>
    <key>com.apple.security.app-sandbox</key>
    <false/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
</dict>
</plist>
```

### Notarize the Application
```bash
# Create ZIP for notarization
ditto -c -k --keepParent QUIET.app QUIET.zip

# Submit for notarization
xcrun altool --notarize-app \
  --primary-bundle-id "com.quietapp.quiet" \
  --username "your@email.com" \
  --password "@keychain:AC_PASSWORD" \
  --file QUIET.zip

# Wait for notarization
xcrun altool --notarization-info <RequestUUID> \
  --username "your@email.com" \
  --password "@keychain:AC_PASSWORD"

# Staple the notarization
xcrun stapler staple QUIET.app
```

### Sign the DMG
```bash
codesign --force --sign "Developer ID Application: Your Name (TEAM_ID)" QUIET-1.0.0-Darwin-x86_64.dmg
```

## Creating a Custom DMG

For a professional appearance:

1. Create DMG with custom settings:
   ```bash
   hdiutil create -volname "QUIET" -srcfolder QUIET.app -ov -format UDRW QUIET_temp.dmg
   hdiutil attach QUIET_temp.dmg
   ```

2. Customize the DMG window:
   - Set background image
   - Position icons
   - Create Applications symlink
   - Run the `dmg_setup.scpt` AppleScript

3. Convert to final DMG:
   ```bash
   hdiutil convert QUIET_temp.dmg -format UDZO -o QUIET-1.0.0.dmg
   ```

## Universal Binary

To support both Intel and Apple Silicon:

```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
```

## Testing

Test on multiple macOS versions:
- macOS 10.15 (Catalina) - minimum supported
- macOS 11 (Big Sur)
- macOS 12 (Monterey)
- macOS 13 (Ventura)
- macOS 14 (Sonoma)

Verify:
- Gatekeeper approval
- Microphone permissions
- BlackHole integration
- Proper app bundle structure