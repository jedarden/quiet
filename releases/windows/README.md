# Windows Installer Assets

This directory contains assets and configurations for the Windows installer.

## Required Files

### Graphics
- `header.bmp` - Header image for installer (150x57 pixels, 8-bit color)
- `welcome.bmp` - Welcome/finish page image (164x314 pixels, 8-bit color)

### Creating Graphics
You can create these files from PNG sources using ImageMagick:

```bash
# Create header image
convert -resize 150x57 -depth 8 -colors 256 header.png BMP3:header.bmp

# Create welcome image  
convert -resize 164x314 -depth 8 -colors 256 welcome.png BMP3:welcome.bmp
```

## Building the Installer

### Prerequisites
1. Install NSIS (Nullsoft Scriptable Install System)
   - Download from: https://nsis.sourceforge.io/
   - Add NSIS to your PATH

2. Build QUIET in Release mode:
   ```bash
   cd /path/to/quiet
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . --config Release
   ```

### Creating the Installer
1. From the build directory:
   ```bash
   cpack -G NSIS
   ```

2. Or manually with NSIS:
   ```bash
   cd installer/windows
   makensis quiet_installer.nsi
   ```

The installer will be created as `QUIET-1.0.0-Windows-Setup.exe`

## Code Signing

For production releases, the installer should be signed with a code signing certificate:

```bash
signtool sign /t http://timestamp.digicert.com /f certificate.pfx /p password "QUIET-1.0.0-Windows-Setup.exe"
```

## Testing

Always test the installer on a clean Windows system to ensure:
- All files are installed correctly
- Shortcuts are created
- VB-Cable prompt appears
- Uninstaller works properly
- No antivirus false positives