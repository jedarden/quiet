# QUIET Build Instructions

## Prerequisites

### All Platforms
- CMake 3.20 or later
- Git
- C++17 compatible compiler

### Windows
- Visual Studio 2022 with C++ Desktop workload
- NSIS (for installer creation)

### macOS
- Xcode Command Line Tools
- Homebrew (recommended for dependencies)

### Linux
- GCC 9+ or Clang 10+
- Development packages for audio libraries

## Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/jedarden/quiet.git
   cd quiet
   ```

2. Configure the build:
   ```bash
   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
   ```

3. Build the application:
   ```bash
   cmake --build build --config Release
   ```

4. Run tests:
   ```bash
   cd build && ctest
   ```

5. Create installer:
   - Windows: `makensis installer/windows/quiet_installer.nsi`
   - macOS: `./installer/macos/create_dmg.sh`

## Dependencies

The build process will automatically download:
- JUCE 8.0.0 (audio framework)
- RNNoise (noise reduction library)
- Google Test (testing framework)

## Troubleshooting

If you encounter build issues:
1. Ensure all prerequisites are installed
2. Check that CMake version is 3.20+
3. On Windows, run from Developer Command Prompt
4. See docs/technical_specification.md for detailed requirements
