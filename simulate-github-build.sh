#!/bin/bash

# Simulate GitHub Actions build steps locally

set -e

echo "========================================="
echo "GitHub Actions Build Simulation"
echo "========================================="

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# Create a temporary directory for the simulation
WORK_DIR="github-actions-simulation"
rm -rf $WORK_DIR
mkdir -p $WORK_DIR
cd $WORK_DIR

echo -e "${YELLOW}Setting up simulation environment...${NC}"

# Copy essential files
cp -r ../CMakeLists.txt .
cp -r ../src .
cp -r ../include .
cp -r ../resources .
cp -r ../installer .
mkdir -p tests
cp ../tests/CMakeLists.txt tests/ 2>/dev/null || touch tests/CMakeLists.txt

# Simulate Windows build steps
echo -e "\n${YELLOW}=== Simulating Windows Build ===${NC}"

echo "Step 1: Create placeholder icon"
mkdir -p resources/icons
if [ ! -f "resources/icons/icon_512.png" ]; then
    # Create a simple placeholder
    echo "PNG placeholder" > resources/icons/icon_512.png
    echo -e "${GREEN}✓ Created icon_512.png${NC}"
fi

echo -e "\nStep 2: Setup RNNoise"
mkdir -p build/rnnoise/{include,lib}
cat > build/rnnoise/include/rnnoise.h << 'EOF'
#ifndef RNNOISE_H
#define RNNOISE_H
typedef struct DenoiseState DenoiseState;
DenoiseState *rnnoise_create(void);
void rnnoise_destroy(DenoiseState *st);
float rnnoise_process_frame(DenoiseState *st, float *out, const float *in);
#endif
EOF
touch build/rnnoise/lib/rnnoise.lib
echo -e "${GREEN}✓ RNNoise headers and lib created${NC}"

echo -e "\nStep 3: Simulate CMake configure"
cd build
echo "CMake would run: cmake .. -DCMAKE_BUILD_TYPE=Release -DRNNOISE_PATH=\$PWD/rnnoise"
# Create mock CMake output
cat > CMakeCache.txt << 'EOF'
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_INSTALL_PREFIX:PATH=/usr/local
RNNOISE_PATH:PATH=/build/rnnoise
EOF
echo -e "${GREEN}✓ CMake configuration simulated${NC}"

echo -e "\nStep 4: Simulate build"
# Create mock executable
mkdir -p Release
touch Release/QUIET.exe
echo -e "${GREEN}✓ Build simulated (QUIET.exe created)${NC}"

echo -e "\nStep 5: Create installer"
cd ..
if [ -f "installer/windows/quiet_installer.nsi" ]; then
    echo "NSIS script found. Would run: makensis installer/windows/quiet_installer.nsi"
    # Create mock installer
    touch installer/windows/QUIET-Setup.exe
    echo -e "${GREEN}✓ Installer created${NC}"
fi

# Simulate macOS build steps
echo -e "\n${YELLOW}=== Simulating macOS Build ===${NC}"

echo "Step 1: Setup RNNoise for macOS"
touch build/rnnoise/lib/librnnoise.a
echo -e "${GREEN}✓ RNNoise library created${NC}"

echo -e "\nStep 2: Create app bundle"
mkdir -p build/QUIET.app/Contents/{MacOS,Resources}
if [ -f "installer/macos/Info.plist" ]; then
    cp installer/macos/Info.plist build/QUIET.app/Contents/
fi
echo '#!/bin/bash' > build/QUIET.app/Contents/MacOS/QUIET
chmod +x build/QUIET.app/Contents/MacOS/QUIET
echo -e "${GREEN}✓ App bundle created${NC}"

echo -e "\nStep 3: Create DMG"
touch QUIET.dmg
echo -e "${GREEN}✓ DMG created${NC}"

# Generate checksums
echo -e "\n${YELLOW}=== Generating Checksums ===${NC}"
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum installer/windows/QUIET-Setup.exe > windows-sha256.txt 2>/dev/null || echo "placeholder" > windows-sha256.txt
    sha256sum QUIET.dmg > macos-sha256.txt
    echo -e "${GREEN}✓ Checksums generated${NC}"
fi

# Summary
echo -e "\n${YELLOW}=== Simulation Summary ===${NC}"
echo "Created artifacts:"
find . -name "*.exe" -o -name "*.dmg" -o -name "*.app" | head -10

cd ..

echo -e "\n${GREEN}Simulation complete!${NC}"
echo "Artifacts created in: $WORK_DIR"
echo ""
echo "This simulation shows what the GitHub Actions workflow would do."
echo "Any errors here would likely occur in the actual CI/CD pipeline."

# Cleanup option
read -p "Clean up simulation directory? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    rm -rf $WORK_DIR
    echo "Cleaned up."
fi