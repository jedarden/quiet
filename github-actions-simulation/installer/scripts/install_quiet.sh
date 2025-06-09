#!/bin/bash
# Cross-platform installation script for QUIET
# Supports macOS and Linux

set -e

QUIET_VERSION="1.0.0"
INSTALL_PREFIX="/usr/local"
BUILD_DIR="build"

echo ""
echo "============================================"
echo "QUIET Installation Script v${QUIET_VERSION}"
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

echo "Detected OS: $OS"

# Check prerequisites
echo ""
echo "Checking prerequisites..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is not installed"
    echo "Please install CMake 3.20 or later"
    exit 1
fi

# Check for C++ compiler
if ! command -v c++ &> /dev/null; then
    echo "Error: C++ compiler is not installed"
    if [[ "$OS" == "macos" ]]; then
        echo "Please install Xcode Command Line Tools:"
        echo "  xcode-select --install"
    else
        echo "Please install build-essential:"
        echo "  sudo apt-get install build-essential"
    fi
    exit 1
fi

# Check for Git
if ! command -v git &> /dev/null; then
    echo "Error: Git is not installed"
    exit 1
fi

echo "[OK] All prerequisites found"

# Clone repository if not already in it
if [ ! -f "CMakeLists.txt" ]; then
    echo ""
    echo "Cloning QUIET repository..."
    git clone https://github.com/quiet-audio/quiet.git
    cd quiet
fi

# Create build directory
echo ""
echo "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo ""
echo "Configuring build..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DBUILD_TESTS=OFF

# Build
echo ""
echo "Building QUIET..."
cmake --build . --config Release --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu)

# Run tests if available
if [ -f "CTestTestfile.cmake" ]; then
    echo ""
    echo "Running tests..."
    ctest --output-on-failure || true
fi

# Install
echo ""
echo "Installing QUIET..."
if [[ "$OS" == "macos" ]]; then
    # macOS installation
    echo "Creating application bundle..."
    cpack -G DragNDrop
    
    echo ""
    echo "============================================"
    echo "Installation Complete!"
    echo "============================================"
    echo ""
    echo "The installer has been created: QUIET-${QUIET_VERSION}-Darwin-*.dmg"
    echo ""
    echo "To install:"
    echo "1. Open the DMG file"
    echo "2. Drag QUIET to your Applications folder"
    echo "3. Run the post-installation script:"
    echo "   sudo /Applications/QUIET.app/Contents/Resources/post_install.sh"
    
elif [[ "$OS" == "linux" ]]; then
    # Linux installation
    sudo cmake --install .
    
    # Create desktop entry
    echo ""
    echo "Creating desktop entry..."
    cat > ~/.local/share/applications/quiet.desktop << EOF
[Desktop Entry]
Type=Application
Name=QUIET
Comment=AI-Powered Background Noise Removal
Exec=${INSTALL_PREFIX}/bin/Quiet
Icon=${INSTALL_PREFIX}/share/icons/quiet.png
Categories=AudioVideo;Audio;
Terminal=false
StartupNotify=true
EOF
    
    # Update desktop database
    update-desktop-database ~/.local/share/applications/ 2>/dev/null || true
    
    echo ""
    echo "============================================"
    echo "Installation Complete!"
    echo "============================================"
    echo ""
    echo "QUIET has been installed to: ${INSTALL_PREFIX}/bin/Quiet"
    echo ""
    echo "You can run QUIET from:"
    echo "- Terminal: quiet"
    echo "- Application menu"
    echo ""
    echo "Note: You may need to install PulseAudio virtual devices:"
    echo "  sudo apt-get install pulseaudio-module-null-sink"
fi

echo ""
echo "For usage instructions, see: ${INSTALL_PREFIX}/share/doc/quiet/README.md"
echo "Report issues at: https://github.com/quiet-audio/quiet/issues"
echo ""