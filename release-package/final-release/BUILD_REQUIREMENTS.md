# QUIET Build Requirements

## Minimum Requirements

### All Platforms
- CMake 3.20 or later
- C++17 compatible compiler
- Git

### Windows
- Visual Studio 2022 or later
- Windows SDK 10.0.17763.0 or later
- NSIS (for installer creation)

### macOS  
- Xcode 12 or later
- macOS 10.15 SDK or later
- Command Line Tools

### Linux
- GCC 9+ or Clang 10+
- ALSA development libraries
- X11 development libraries

## Dependencies (automatically downloaded)

- JUCE 8.0.0 - Audio application framework
- RNNoise - Noise suppression library
- Google Test 1.14.0 - Testing framework

## Build Instructions

```bash
# Clone repository
git clone https://github.com/jedarden/quiet.git
cd quiet

# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# Test
cd build && ctest

# Create installer
# Windows: makensis installer/windows/quiet_installer.nsi
# macOS: ./installer/macos/create_dmg.sh
```

## Virtual Audio Drivers

### Windows
- VB-Cable: https://vb-audio.com/Cable/
- Install before running QUIET

### macOS
- BlackHole: https://github.com/ExistentialAudio/BlackHole
- Install via brew: `brew install blackhole-2ch`

## Troubleshooting

If build fails:
1. Ensure all requirements are installed
2. Check CMakeFiles/CMakeError.log
3. Try building with: `cmake --build build --verbose`
4. See docs/technical_specification.md for details
