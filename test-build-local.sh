#!/bin/bash

# QUIET Local Build Testing Script
# Simulates GitHub Actions build process locally

set -e

echo "========================================="
echo "QUIET Local Build Test"
echo "========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Detect platform
PLATFORM=""
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    PLATFORM="windows"
else
    echo -e "${RED}Unknown platform: $OSTYPE${NC}"
    exit 1
fi

echo "Detected platform: $PLATFORM"

# Function to check command availability
check_command() {
    if command -v $1 &> /dev/null; then
        echo -e "${GREEN}✓ $1 found${NC}"
        return 0
    else
        echo -e "${RED}✗ $1 not found${NC}"
        return 1
    fi
}

# Function to create icon placeholders
create_icons() {
    echo -e "${YELLOW}Creating icon placeholders...${NC}"
    mkdir -p resources/icons
    
    if [ ! -f "resources/icons/icon_512.png" ]; then
        # Create a simple PNG using ImageMagick if available
        if command -v convert &> /dev/null; then
            convert -size 512x512 xc:gray -fill white -pointsize 200 \
                -draw "text 200,300 'Q'" resources/icons/icon_512.png
            echo -e "${GREEN}Created icon_512.png${NC}"
        else
            # Create empty file as fallback
            touch resources/icons/icon_512.png
            echo -e "${YELLOW}Created placeholder icon_512.png${NC}"
        fi
    fi
    
    if [ ! -f "resources/icons/icon_128.png" ]; then
        if command -v convert &> /dev/null; then
            convert -size 128x128 xc:gray -fill white -pointsize 50 \
                -draw "text 50,75 'Q'" resources/icons/icon_128.png
            echo -e "${GREEN}Created icon_128.png${NC}"
        else
            touch resources/icons/icon_128.png
            echo -e "${YELLOW}Created placeholder icon_128.png${NC}"
        fi
    fi
}

# Function to setup RNNoise
setup_rnnoise() {
    echo -e "${YELLOW}Setting up RNNoise...${NC}"
    
    RNNOISE_DIR="build/rnnoise"
    mkdir -p $RNNOISE_DIR/{include,lib}
    
    # Create RNNoise header
    cat > $RNNOISE_DIR/include/rnnoise.h << 'EOF'
#ifndef RNNOISE_H
#define RNNOISE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DenoiseState DenoiseState;

DenoiseState *rnnoise_create(void);
void rnnoise_destroy(DenoiseState *st);
float rnnoise_process_frame(DenoiseState *st, float *out, const float *in);
int rnnoise_get_size(void);
int rnnoise_init(DenoiseState *st);

#ifdef __cplusplus
}
#endif

#endif
EOF
    
    # Create dummy library file
    if [[ "$PLATFORM" == "windows" ]]; then
        touch $RNNOISE_DIR/lib/rnnoise.lib
    else
        touch $RNNOISE_DIR/lib/librnnoise.a
    fi
    
    echo -e "${GREEN}RNNoise setup complete${NC}"
}

# Check dependencies
echo -e "\n${YELLOW}Checking dependencies...${NC}"
check_command cmake
check_command git

if [[ "$PLATFORM" == "windows" ]]; then
    check_command msbuild || check_command cl
elif [[ "$PLATFORM" == "macos" ]]; then
    check_command clang++
    check_command brew
else
    check_command g++
    check_command make
fi

# Create necessary directories
echo -e "\n${YELLOW}Creating build directories...${NC}"
mkdir -p build

# Create icons
create_icons

# Setup RNNoise
setup_rnnoise

# Configure CMake
echo -e "\n${YELLOW}Configuring CMake...${NC}"
cd build

if [[ "$PLATFORM" == "windows" ]]; then
    cmake -G "Visual Studio 17 2022" \
        -DCMAKE_BUILD_TYPE=Release \
        -DRNNOISE_PATH="$PWD/rnnoise" \
        -DBUILD_TESTS=OFF \
        .. || {
        echo -e "${RED}CMake configuration failed${NC}"
        exit 1
    }
else
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DRNNOISE_PATH="$PWD/rnnoise" \
        -DBUILD_TESTS=OFF \
        .. || {
        echo -e "${RED}CMake configuration failed${NC}"
        exit 1
    }
fi

echo -e "${GREEN}CMake configuration successful${NC}"

# Try to build
echo -e "\n${YELLOW}Attempting build...${NC}"
if cmake --build . --config Release -j4; then
    echo -e "${GREEN}Build successful!${NC}"
else
    echo -e "${RED}Build failed${NC}"
    echo "Checking for partial build artifacts..."
    
    # List what was created
    find . -name "*.exe" -o -name "*.app" -o -name "*.so" -o -name "*.dylib" 2>/dev/null || true
fi

cd ..

# Summary
echo -e "\n========================================="
echo "Build Test Summary"
echo "========================================="
echo "Platform: $PLATFORM"
echo "Build directory: build/"

if [ -f "build/Quiet" ] || [ -f "build/Release/Quiet.exe" ] || [ -d "build/Quiet.app" ]; then
    echo -e "${GREEN}Executable found${NC}"
else
    echo -e "${YELLOW}No executable found (build may have failed)${NC}"
fi

echo -e "\nTo debug build issues:"
echo "1. Check build/CMakeFiles/CMakeError.log"
echo "2. Check build/CMakeFiles/CMakeOutput.log"
echo "3. Run with verbose: cmake --build build --verbose"