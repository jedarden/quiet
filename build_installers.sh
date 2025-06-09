#!/bin/bash
# Build script for creating QUIET installers on all platforms

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="${SCRIPT_DIR}/build"
VERSION=$(grep "project(Quiet VERSION" CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')

echo ""
echo "============================================"
echo "QUIET Installer Build Script v${VERSION}"
echo "============================================"
echo ""

# Detect platform
OS="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
fi

echo "Building installers for: $OS"
echo "Version: $VERSION"

# Function to build on Windows
build_windows() {
    echo ""
    echo "Building Windows installer..."
    
    # Create build directory
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure with CMake
    echo "Configuring..."
    cmake .. -G "Visual Studio 16 2019" -A x64 \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=OFF
    
    # Build
    echo "Building..."
    cmake --build . --config Release --parallel
    
    # Create installer
    echo "Creating installer..."
    cpack -G NSIS -C Release
    
    echo ""
    echo "Windows installer created: ${BUILD_DIR}/QUIET-${VERSION}-Windows-*.exe"
}

# Function to build on macOS
build_macos() {
    echo ""
    echo "Building macOS installer..."
    
    # Create build directory
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure with CMake
    echo "Configuring..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
        -DBUILD_TESTS=OFF
    
    # Build
    echo "Building..."
    cmake --build . --config Release --parallel $(sysctl -n hw.ncpu)
    
    # Create installer
    echo "Creating DMG..."
    cpack -G DragNDrop
    
    # Sign if certificate is available
    if [ -n "$MACOS_CERTIFICATE_NAME" ]; then
        echo "Signing application..."
        codesign --force --deep --sign "$MACOS_CERTIFICATE_NAME" \
            --options runtime \
            _CPack_Packages/Darwin/DragNDrop/QUIET.app
        
        echo "Creating signed DMG..."
        cpack -G DragNDrop
        
        # Sign the DMG
        DMG_FILE=$(ls QUIET-${VERSION}-Darwin-*.dmg | head -1)
        codesign --force --sign "$MACOS_CERTIFICATE_NAME" "$DMG_FILE"
        
        echo "Signed installer: ${BUILD_DIR}/${DMG_FILE}"
    else
        echo "No code signing certificate found. Installer will not be signed."
        echo "Set MACOS_CERTIFICATE_NAME environment variable to enable signing."
    fi
    
    echo ""
    echo "macOS installer created: ${BUILD_DIR}/QUIET-${VERSION}-Darwin-*.dmg"
}

# Function to build on Linux
build_linux() {
    echo ""
    echo "Building Linux packages..."
    
    # Create build directory
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    
    # Configure with CMake
    echo "Configuring..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_TESTS=OFF
    
    # Build
    echo "Building..."
    cmake --build . --config Release --parallel $(nproc)
    
    # Create packages
    echo "Creating DEB package..."
    cpack -G DEB
    
    echo "Creating RPM package..."
    cpack -G RPM
    
    echo "Creating TGZ package..."
    cpack -G TGZ
    
    echo ""
    echo "Linux packages created:"
    echo "  DEB: ${BUILD_DIR}/QUIET-${VERSION}-Linux-*.deb"
    echo "  RPM: ${BUILD_DIR}/QUIET-${VERSION}-Linux-*.rpm"
    echo "  TGZ: ${BUILD_DIR}/QUIET-${VERSION}-Linux-*.tar.gz"
}

# Main build logic
case "$OS" in
    windows)
        build_windows
        ;;
    macos)
        build_macos
        ;;
    linux)
        build_linux
        ;;
    *)
        echo "Error: Unsupported platform: $OS"
        exit 1
        ;;
esac

# Create checksums
echo ""
echo "Creating checksums..."
cd "${BUILD_DIR}"
sha256sum QUIET-${VERSION}-* > QUIET-${VERSION}-checksums.txt || shasum -a 256 QUIET-${VERSION}-* > QUIET-${VERSION}-checksums.txt

echo ""
echo "============================================"
echo "Build Complete!"
echo "============================================"
echo ""
echo "Installers created in: ${BUILD_DIR}"
echo "Checksums: ${BUILD_DIR}/QUIET-${VERSION}-checksums.txt"
echo ""
echo "Next steps:"
echo "1. Test the installer on a clean system"
echo "2. Sign the installer (if not already done)"
echo "3. Upload to GitHub Releases or distribution server"
echo ""