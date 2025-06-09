#!/bin/bash

# Docker-based build test that simulates GitHub Actions environment

set -e

echo "========================================="
echo "Docker Build Test for QUIET"
echo "========================================="

# Function to test Windows build
test_windows_build() {
    echo "Testing Windows build in Docker..."
    
    cat > Dockerfile.windows << 'EOF'
# Use Windows Server Core as base
FROM mcr.microsoft.com/windows/servercore:ltsc2022

# Install Chocolatey
RUN powershell -Command \
    Set-ExecutionPolicy Bypass -Scope Process -Force; \
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072; \
    iex ((New-Object System.Net.WebClient).DownloadString('https://chocolatey.org/install.ps1'))

# Install build tools
RUN choco install -y cmake git visualstudio2022buildtools visualstudio2022-workload-vctools

WORKDIR /workspace
COPY . .

# Build script
RUN powershell -File .github/scripts/build-windows.ps1
EOF

    # Note: Windows containers require Windows host
    echo "Note: Windows containers require a Windows host with Docker Desktop"
    echo "To test on Linux/macOS, use wine-based approach instead"
}

# Function to test macOS build
test_macos_build() {
    echo "Testing macOS-like build in Docker..."
    
    cat > Dockerfile.macos << 'EOF'
FROM ubuntu:22.04

# Install dependencies that simulate macOS environment
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libasound2-dev \
    libjack-jackd2-dev \
    libfreetype6-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libgl1-mesa-dev \
    libasound2-dev \
    wget \
    curl

# Install clang to simulate macOS compiler
RUN apt-get install -y clang

WORKDIR /workspace
COPY . .

# Create build script
RUN echo '#!/bin/bash\n\
set -e\n\
mkdir -p resources/icons\n\
touch resources/icons/icon_512.png\n\
touch resources/icons/icon_128.png\n\
mkdir -p build/rnnoise/include build/rnnoise/lib\n\
echo "#ifndef RNNOISE_H\n#define RNNOISE_H\ntypedef struct DenoiseState DenoiseState;\n#endif" > build/rnnoise/include/rnnoise.h\n\
touch build/rnnoise/lib/librnnoise.a\n\
cd build\n\
CXX=clang++ CC=clang cmake .. -DCMAKE_BUILD_TYPE=Release -DRNNOISE_PATH=$PWD/rnnoise\n\
cmake --build . --config Release || echo "Build failed but continuing"\n\
' > /build.sh && chmod +x /build.sh

RUN /build.sh
EOF

    docker build -f Dockerfile.macos -t quiet-macos-test .
    docker run --rm quiet-macos-test ls -la build/
}

# Function to test Ubuntu build
test_ubuntu_build() {
    echo "Testing Ubuntu build in Docker..."
    
    cat > Dockerfile.ubuntu << 'EOF'
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libasound2-dev \
    libjack-jackd2-dev \
    libfreetype6-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libgl1-mesa-dev \
    wget \
    curl \
    ca-certificates

WORKDIR /workspace
COPY . .

# Build script
RUN bash -c '\
    set -e; \
    echo "Creating icon placeholders..."; \
    mkdir -p resources/icons; \
    touch resources/icons/icon_512.png resources/icons/icon_128.png; \
    \
    echo "Setting up RNNoise..."; \
    mkdir -p build/rnnoise/{include,lib}; \
    cat > build/rnnoise/include/rnnoise.h << "EOH"
#ifndef RNNOISE_H
#define RNNOISE_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct DenoiseState DenoiseState;
DenoiseState *rnnoise_create(void);
void rnnoise_destroy(DenoiseState *st);
float rnnoise_process_frame(DenoiseState *st, float *out, const float *in);
#ifdef __cplusplus
}
#endif
#endif
EOH
    touch build/rnnoise/lib/librnnoise.a; \
    \
    echo "Configuring CMake..."; \
    cd build; \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DRNNOISE_PATH=$PWD/rnnoise -DBUILD_TESTS=OFF || exit 1; \
    \
    echo "Building..."; \
    cmake --build . --config Release -j$(nproc) || echo "Build failed"; \
    \
    echo "Checking results..."; \
    find . -type f -name "Quiet*" -o -name "*.so" | head -20; \
'

# Check what was built
RUN find /workspace/build -type f \( -name "Quiet" -o -name "*.so" \) 2>/dev/null || echo "No binaries found"
EOF

    docker build -f Dockerfile.ubuntu -t quiet-ubuntu-test .
    
    # Run and check results
    echo ""
    echo "Build artifacts:"
    docker run --rm quiet-ubuntu-test find build -type f -name "Quiet*" -o -name "*.so" 2>/dev/null || echo "No artifacts found"
}

# Function to run minimal test
test_minimal() {
    echo "Running minimal build test..."
    
    cat > Dockerfile.minimal << 'EOF'
FROM alpine:latest

RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    pkgconfig \
    linux-headers

WORKDIR /workspace
COPY . .

RUN sh -c '\
    set -e; \
    mkdir -p resources/icons build/rnnoise/include build/rnnoise/lib; \
    touch resources/icons/icon_512.png resources/icons/icon_128.png; \
    echo "typedef struct DenoiseState DenoiseState;" > build/rnnoise/include/rnnoise.h; \
    touch build/rnnoise/lib/librnnoise.a; \
    cd build; \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DRNNOISE_PATH=$PWD/rnnoise -DBUILD_TESTS=OFF; \
    cmake --build . || echo "Build attempted"; \
'
EOF

    docker build -f Dockerfile.minimal -t quiet-minimal-test .
}

# Menu
echo ""
echo "Select build environment to test:"
echo "1) Ubuntu 22.04 (most similar to GitHub Actions)"
echo "2) macOS-like environment"
echo "3) Minimal Alpine Linux"
echo "4) Clean up Docker images"

read -p "Enter choice (1-4): " choice

case $choice in
    1)
        test_ubuntu_build
        ;;
    2)
        test_macos_build
        ;;
    3)
        test_minimal
        ;;
    4)
        echo "Cleaning up..."
        docker rmi quiet-ubuntu-test quiet-macos-test quiet-minimal-test 2>/dev/null || true
        rm -f Dockerfile.*
        echo "Cleanup complete"
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

# Cleanup
rm -f Dockerfile.*

echo ""
echo "========================================="
echo "Docker build test completed"
echo "========================================="
echo ""
echo "To debug issues:"
echo "1. Run container interactively: docker run -it quiet-ubuntu-test /bin/bash"
echo "2. Check CMake cache: docker run --rm quiet-ubuntu-test cat build/CMakeCache.txt"
echo "3. Check CMake errors: docker run --rm quiet-ubuntu-test cat build/CMakeFiles/CMakeError.log"