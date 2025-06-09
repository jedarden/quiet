# Infrastructure Architecture - QUIET Application

## 1. Development Infrastructure

### 1.1 Development Environment Setup

```bash
#!/bin/bash
# setup-dev-environment.sh

echo "Setting up QUIET development environment..."

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
    echo "Detected macOS"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    OS="windows"
    echo "Detected Windows"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    echo "Detected Linux"
else
    echo "Unsupported OS: $OSTYPE"
    exit 1
fi

# Install dependencies
case $OS in
    macos)
        # Install Homebrew if not present
        if ! command -v brew &> /dev/null; then
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        # Install tools
        brew install cmake ninja ccache git-lfs
        brew install --cask visual-studio-code
        
        # Install audio tools
        brew install portaudio libsndfile
        
        # Install BlackHole for virtual audio
        brew install --cask blackhole-2ch
        ;;
        
    windows)
        # Check for chocolatey
        if ! command -v choco &> /dev/null; then
            echo "Please install Chocolatey first"
            exit 1
        fi
        
        # Install tools
        choco install cmake ninja git git-lfs vscode -y
        choco install visualstudio2022community -y
        
        # Install ASIO4ALL for low-latency audio
        choco install asio4all -y
        ;;
        
    linux)
        # Update package manager
        sudo apt-get update
        
        # Install build tools
        sudo apt-get install -y \
            build-essential cmake ninja-build ccache \
            git git-lfs curl wget
            
        # Install audio development packages
        sudo apt-get install -y \
            libasound2-dev libpulse-dev \
            libjack-jackd2-dev portaudio19-dev
            
        # Install JUCE dependencies
        sudo apt-get install -y \
            libfreetype6-dev libx11-dev libxinerama-dev \
            libxrandr-dev libxcursor-dev libxcomposite-dev \
            mesa-common-dev libasound2-dev freeglut3-dev \
            libcurl4-gnutls-dev libasound2-dev libgtk-3-dev \
            libwebkit2gtk-4.0-dev
        ;;
esac

# Setup Git LFS for binary assets
git lfs install
git lfs track "*.png" "*.jpg" "*.wav" "*.model"

# Create project structure
mkdir -p build/{debug,release,test}
mkdir -p tools/{scripts,configs}
mkdir -p docs/{api,guides}

# Setup pre-commit hooks
cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# Run clang-format on changed files
for file in $(git diff --cached --name-only | grep -E '\.(cpp|h|hpp)$'); do
    clang-format -i "$file"
    git add "$file"
done

# Run tests
cd build/test && ctest --output-on-failure
EOF
chmod +x .git/hooks/pre-commit

echo "Development environment setup complete!"
```

### 1.2 Build Configuration

```cmake
# cmake/BuildOptions.cmake
include(CMakeDependentOption)

# Build type options
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)

# Compiler options
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
option(WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers in debug builds" ON)
option(ENABLE_LTO "Enable Link Time Optimization" OFF)
option(ENABLE_PROFILING "Enable profiling support" OFF)

# Feature options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(BUILD_DOCUMENTATION "Build API documentation" OFF)
option(BUILD_INSTALLER "Build platform installer" OFF)

# Audio options
option(USE_NATIVE_AUDIO "Use native audio APIs instead of PortAudio" ON)
option(ENABLE_ASIO "Enable ASIO support on Windows" OFF)

# Platform-specific options
if(WIN32)
    option(USE_STATIC_RUNTIME "Use static MSVC runtime" OFF)
    option(ENABLE_CONSOLE "Enable console window in debug builds" ON)
elseif(APPLE)
    option(BUILD_UNIVERSAL "Build universal binary for Intel and ARM" ON)
    option(ENABLE_HARDENED_RUNTIME "Enable hardened runtime for notarization" ON)
endif()

# Setup compiler flags
function(setup_compiler_flags target)
    if(ENABLE_WARNINGS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
            if(WARNINGS_AS_ERRORS)
                target_compile_options(${target} PRIVATE /WX)
            endif()
        else()
            target_compile_options(${target} PRIVATE 
                -Wall -Wextra -Wpedantic
                -Wno-unused-parameter
                -Wno-missing-field-initializers
            )
            if(WARNINGS_AS_ERRORS)
                target_compile_options(${target} PRIVATE -Werror)
            endif()
        endif()
    endif()
    
    # Sanitizers
    if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(NOT MSVC)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    
    # LTO
    if(ENABLE_LTO)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
    
    # Profiling
    if(ENABLE_PROFILING)
        if(NOT MSVC)
            target_compile_options(${target} PRIVATE -pg)
            target_link_options(${target} PRIVATE -pg)
        endif()
    endif()
endfunction()
```

### 1.3 Dependency Management

```cmake
# cmake/Dependencies.cmake
include(FetchContent)

# Set download and build options
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# JUCE
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
    GIT_SHALLOW    TRUE
)

# RNNoise with custom patches
FetchContent_Declare(
    RNNoise
    GIT_REPOSITORY https://github.com/quiet-audio/rnnoise.git  # Our fork
    GIT_TAG        quiet-1.0
    GIT_SHALLOW    TRUE
)

# Google Test for testing
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

# Google Benchmark for performance testing
if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
        GIT_SHALLOW    TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
endif()

# Tracy Profiler for performance analysis
if(ENABLE_PROFILING)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.10
        GIT_SHALLOW    TRUE
    )
endif()

# Platform-specific dependencies
if(WIN32 AND USE_NATIVE_AUDIO)
    # Windows Audio Session API headers
    find_package(WindowsSDK REQUIRED)
endif()

if(APPLE AND USE_NATIVE_AUDIO)
    # Core Audio frameworks
    find_library(COREAUDIO CoreAudio REQUIRED)
    find_library(AUDIOUNIT AudioUnit REQUIRED)
    find_library(COREMIDI CoreMIDI REQUIRED)
endif()

# Make dependencies available
FetchContent_MakeAvailable(JUCE RNNoise)
if(BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()
if(BUILD_BENCHMARKS)
    FetchContent_MakeAvailable(googlebenchmark)
endif()
if(ENABLE_PROFILING)
    FetchContent_MakeAvailable(tracy)
endif()
```

## 2. CI/CD Infrastructure

### 2.1 GitHub Actions Workflow

```yaml
# .github/workflows/ci.yml
name: CI Build and Test

on:
  push:
    branches: [ main, develop, 'release/*' ]
  pull_request:
    branches: [ main ]
  workflow_dispatch:

env:
  CMAKE_BUILD_PARALLEL_LEVEL: 4
  CTEST_PARALLEL_LEVEL: 4

jobs:
  build-matrix:
    strategy:
      fail-fast: false
      matrix:
        include:
          # Windows builds
          - os: windows-latest
            name: Windows-x64-MSVC
            cmake_args: -G "Visual Studio 17 2022" -A x64
            artifact_name: quiet-windows-x64
            
          # macOS builds
          - os: macos-13
            name: macOS-x64
            cmake_args: -G Ninja -DCMAKE_OSX_ARCHITECTURES=x86_64
            artifact_name: quiet-macos-x64
            
          - os: macos-14
            name: macOS-arm64
            cmake_args: -G Ninja -DCMAKE_OSX_ARCHITECTURES=arm64
            artifact_name: quiet-macos-arm64
            
          # Linux builds
          - os: ubuntu-22.04
            name: Linux-x64-GCC
            cmake_args: -G Ninja
            artifact_name: quiet-linux-x64
            
    runs-on: ${{ matrix.os }}
    name: ${{ matrix.name }}
    
    steps:
    - name: Checkout
      uses: actions/checkout@v4
      with:
        submodules: recursive
        lfs: true
        
    - name: Setup Build Environment
      uses: ./.github/actions/setup-build-env
      with:
        os: ${{ matrix.os }}
        
    - name: Cache Dependencies
      uses: actions/cache@v3
      with:
        path: |
          build/_deps
          ~/.cache/ccache
          ~/Library/Caches/ccache
        key: ${{ runner.os }}-deps-${{ hashFiles('cmake/Dependencies.cmake') }}
        restore-keys: |
          ${{ runner.os }}-deps-
          
    - name: Configure
      run: |
        cmake -B build -S . \
          ${{ matrix.cmake_args }} \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTS=ON \
          -DBUILD_BENCHMARKS=ON \
          -DENABLE_WARNINGS=ON
          
    - name: Build
      run: cmake --build build --config Release
      
    - name: Test
      run: |
        cd build
        ctest -C Release --output-on-failure --verbose
        
    - name: Benchmark
      if: matrix.os != 'windows-latest'  # Windows benchmarks are flaky in CI
      run: |
        cd build
        ./benchmarks/audio_benchmarks --benchmark_out=benchmark_results.json
        
    - name: Package
      run: |
        cd build
        cpack -C Release
        
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: ${{ matrix.artifact_name }}
        path: |
          build/*.dmg
          build/*.msi
          build/*.deb
          build/*.AppImage
          build/benchmark_results.json
        retention-days: 7

  code-quality:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: clang-format Check
      uses: jidicula/clang-format-action@v4.11.0
      with:
        clang-format-version: '16'
        check-path: 'src'
        
    - name: clang-tidy
      run: |
        cmake -B build -S . -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        run-clang-tidy -p build
        
    - name: cppcheck
      run: |
        cppcheck --enable=all --error-exitcode=1 \
          --inline-suppr --suppress=missingInclude \
          -I include src
          
    - name: CodeQL Analysis
      uses: github/codeql-action/analyze@v2
      with:
        languages: cpp

  coverage:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Configure with Coverage
      run: |
        cmake -B build -S . \
          -DCMAKE_BUILD_TYPE=Debug \
          -DENABLE_COVERAGE=ON \
          -DBUILD_TESTS=ON
          
    - name: Build and Test
      run: |
        cmake --build build
        cd build && ctest
        
    - name: Generate Coverage Report
      run: |
        cd build
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/test/*' --output-file coverage.info
        lcov --list coverage.info
        
    - name: Upload Coverage
      uses: codecov/codecov-action@v3
      with:
        file: build/coverage.info
        fail_ci_if_error: true
```

### 2.2 Release Pipeline

```yaml
# .github/workflows/release.yml
name: Release Build

on:
  push:
    tags:
      - 'v*'
  workflow_dispatch:
    inputs:
      version:
        description: 'Release version'
        required: true
        default: '1.0.0'

jobs:
  create-release:
    runs-on: ubuntu-latest
    outputs:
      upload_url: ${{ steps.create_release.outputs.upload_url }}
      version: ${{ steps.version.outputs.version }}
    steps:
    - name: Determine Version
      id: version
      run: |
        if [[ "${{ github.event_name }}" == "push" ]]; then
          VERSION=${GITHUB_REF#refs/tags/v}
        else
          VERSION=${{ github.event.inputs.version }}
        fi
        echo "version=$VERSION" >> $GITHUB_OUTPUT
        
    - name: Create Release
      id: create_release
      uses: actions/create-release@v1
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
      with:
        tag_name: v${{ steps.version.outputs.version }}
        release_name: QUIET v${{ steps.version.outputs.version }}
        draft: true
        prerelease: false

  build-release:
    needs: create-release
    strategy:
      matrix:
        include:
          - os: windows-latest
            name: Windows
            ext: .msi
          - os: macos-latest
            name: macOS
            ext: .dmg
          - os: ubuntu-latest
            name: Linux
            ext: .AppImage
            
    runs-on: ${{ matrix.os }}
    
    steps:
    - uses: actions/checkout@v4
    
    - name: Setup Signing
      uses: ./.github/actions/setup-signing
      with:
        os: ${{ matrix.os }}
        certificate: ${{ secrets.SIGNING_CERTIFICATE }}
        password: ${{ secrets.SIGNING_PASSWORD }}
        
    - name: Build Release
      run: |
        cmake -B build -S . \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_INSTALLER=ON \
          -DQUIET_VERSION=${{ needs.create-release.outputs.version }}
        cmake --build build --config Release
        cd build && cpack -C Release
        
    - name: Sign Binary
      run: |
        if [[ "${{ matrix.os }}" == "windows-latest" ]]; then
          signtool sign /f cert.pfx /p ${{ secrets.SIGNING_PASSWORD }} \
            /tr http://timestamp.digicert.com /td sha256 /fd sha256 \
            build/QUIET-*.msi
        elif [[ "${{ matrix.os }}" == "macos-latest" ]]; then
          codesign --deep --force --verify --verbose \
            --sign "${{ secrets.APPLE_DEVELOPER_ID }}" \
            --options runtime \
            build/QUIET-*.dmg
        fi
        
    - name: Notarize (macOS)
      if: matrix.os == 'macos-latest'
      run: |
        xcrun notarytool submit build/QUIET-*.dmg \
          --apple-id ${{ secrets.APPLE_ID }} \
          --password ${{ secrets.APPLE_PASSWORD }} \
          --team-id ${{ secrets.APPLE_TEAM_ID }} \
          --wait
          
    - name: Upload Release Asset
      uses: actions/upload-release-asset@v1
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
      with:
        upload_url: ${{ needs.create-release.outputs.upload_url }}
        asset_path: build/QUIET-${{ needs.create-release.outputs.version }}${{ matrix.ext }}
        asset_name: QUIET-${{ needs.create-release.outputs.version }}-${{ matrix.name }}${{ matrix.ext }}
        asset_content_type: application/octet-stream
```

## 3. Deployment Infrastructure

### 3.1 Update Server

```python
# deployment/update_server/app.py
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import boto3
from typing import Optional
import hashlib
import json

app = FastAPI(title="QUIET Update Server")

class UpdateChannel:
    STABLE = "stable"
    BETA = "beta"
    NIGHTLY = "nightly"

class UpdateInfo(BaseModel):
    version: str
    channel: str
    platform: str
    download_url: str
    size: int
    sha256: str
    release_notes: str
    minimum_version: Optional[str] = None
    
class UpdateRequest(BaseModel):
    current_version: str
    channel: str = UpdateChannel.STABLE
    platform: str
    
s3_client = boto3.client('s3')
BUCKET_NAME = "quiet-releases"

@app.post("/check-update")
async def check_update(request: UpdateRequest) -> Optional[UpdateInfo]:
    """Check if an update is available for the given version/platform"""
    
    # Get latest version for channel
    latest = get_latest_version(request.channel, request.platform)
    
    if not latest or version_compare(request.current_version, latest.version) >= 0:
        return None
        
    # Check if update is allowed
    if latest.minimum_version and \
       version_compare(request.current_version, latest.minimum_version) < 0:
        raise HTTPException(
            status_code=400,
            detail=f"Direct update not supported. Please download full installer."
        )
    
    # Generate signed download URL
    latest.download_url = s3_client.generate_presigned_url(
        'get_object',
        Params={
            'Bucket': BUCKET_NAME,
            'Key': f"{request.channel}/{request.platform}/{latest.version}/QUIET-{latest.version}.{get_ext(request.platform)}"
        },
        ExpiresIn=3600  # 1 hour
    )
    
    return latest

@app.get("/release-notes/{version}")
async def get_release_notes(version: str) -> dict:
    """Get release notes for a specific version"""
    try:
        response = s3_client.get_object(
            Bucket=BUCKET_NAME,
            Key=f"release-notes/{version}.md"
        )
        return {
            "version": version,
            "content": response['Body'].read().decode('utf-8')
        }
    except s3_client.exceptions.NoSuchKey:
        raise HTTPException(status_code=404, detail="Release notes not found")

def get_latest_version(channel: str, platform: str) -> Optional[UpdateInfo]:
    """Get the latest version info from S3"""
    try:
        response = s3_client.get_object(
            Bucket=BUCKET_NAME,
            Key=f"metadata/{channel}/{platform}/latest.json"
        )
        data = json.loads(response['Body'].read())
        return UpdateInfo(**data)
    except:
        return None
        
def version_compare(v1: str, v2: str) -> int:
    """Compare version strings"""
    from packaging import version
    return -1 if version.parse(v1) < version.parse(v2) else \
            1 if version.parse(v1) > version.parse(v2) else 0

def get_ext(platform: str) -> str:
    """Get file extension for platform"""
    return {
        "windows": "msi",
        "macos": "dmg", 
        "linux": "AppImage"
    }.get(platform, "")
```

### 3.2 Infrastructure as Code

```terraform
# deployment/terraform/main.tf
terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

# S3 bucket for releases
resource "aws_s3_bucket" "releases" {
  bucket = "quiet-releases"
  
  tags = {
    Name        = "QUIET Release Storage"
    Environment = "production"
  }
}

resource "aws_s3_bucket_versioning" "releases" {
  bucket = aws_s3_bucket.releases.id
  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_public_access_block" "releases" {
  bucket = aws_s3_bucket.releases.id

  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

# CloudFront distribution
resource "aws_cloudfront_distribution" "cdn" {
  enabled = true
  comment = "QUIET Release CDN"
  
  origin {
    domain_name = aws_s3_bucket.releases.bucket_regional_domain_name
    origin_id   = "S3-quiet-releases"
    
    s3_origin_config {
      origin_access_identity = aws_cloudfront_origin_access_identity.releases.cloudfront_access_identity_path
    }
  }
  
  default_cache_behavior {
    allowed_methods  = ["GET", "HEAD"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "S3-quiet-releases"
    
    forwarded_values {
      query_string = false
      cookies {
        forward = "none"
      }
    }
    
    viewer_protocol_policy = "redirect-to-https"
    min_ttl                = 0
    default_ttl            = 3600
    max_ttl                = 86400
  }
  
  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }
  
  viewer_certificate {
    cloudfront_default_certificate = true
  }
}

# Update server on ECS
resource "aws_ecs_cluster" "main" {
  name = "quiet-cluster"
}

resource "aws_ecs_task_definition" "update_server" {
  family                   = "quiet-update-server"
  network_mode            = "awsvpc"
  requires_compatibilities = ["FARGATE"]
  cpu                     = "256"
  memory                  = "512"
  
  container_definitions = jsonencode([
    {
      name  = "update-server"
      image = "${aws_ecr_repository.update_server.repository_url}:latest"
      
      portMappings = [
        {
          containerPort = 8000
          protocol      = "tcp"
        }
      ]
      
      environment = [
        {
          name  = "BUCKET_NAME"
          value = aws_s3_bucket.releases.id
        }
      ]
      
      logConfiguration = {
        logDriver = "awslogs"
        options = {
          "awslogs-group"         = aws_cloudwatch_log_group.update_server.name
          "awslogs-region"        = var.aws_region
          "awslogs-stream-prefix" = "ecs"
        }
      }
    }
  ])
}
```

### 3.3 Monitoring and Analytics

```yaml
# deployment/monitoring/prometheus.yml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'update-server'
    static_configs:
      - targets: ['update-server:8000']
    
  - job_name: 'node-exporter'
    static_configs:
      - targets: ['node-exporter:9100']

rule_files:
  - '/etc/prometheus/alerts.yml'

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

---
# deployment/monitoring/alerts.yml
groups:
  - name: quiet_alerts
    interval: 30s
    rules:
      - alert: HighErrorRate
        expr: rate(http_requests_total{status=~"5.."}[5m]) > 0.1
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate detected"
          description: "Error rate is {{ $value }} errors per second"
          
      - alert: UpdateServerDown
        expr: up{job="update-server"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Update server is down"
          
      - alert: HighDownloadLatency
        expr: histogram_quantile(0.95, http_request_duration_seconds_bucket) > 2
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "High download latency"
          description: "95th percentile latency is {{ $value }}s"
```

## 4. Security Infrastructure

### 4.1 Code Signing Setup

```bash
#!/bin/bash
# tools/setup-signing.sh

case "$1" in
    windows)
        # Download Windows SDK for signtool
        curl -L -o winsdk.exe https://go.microsoft.com/fwlink/?linkid=2173743
        ./winsdk.exe /features OptionId.SigningTools /quiet
        
        # Import certificate
        certutil -f -p "$CERT_PASSWORD" -importpfx "$CERT_FILE"
        ;;
        
    macos)
        # Import certificate to keychain
        security create-keychain -p "$KEYCHAIN_PASSWORD" build.keychain
        security default-keychain -s build.keychain
        security unlock-keychain -p "$KEYCHAIN_PASSWORD" build.keychain
        
        security import "$CERT_FILE" -k build.keychain -P "$CERT_PASSWORD" -T /usr/bin/codesign
        security set-key-partition-list -S apple-tool:,apple: -s -k "$KEYCHAIN_PASSWORD" build.keychain
        
        # Verify certificate
        security find-identity -v -p codesigning
        ;;
        
    linux)
        # For Linux AppImage signing
        wget -q https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
        chmod +x appimagetool-x86_64.AppImage
        
        # GPG key for signing
        echo "$GPG_PRIVATE_KEY" | gpg --import
        ;;
esac
```

### 4.2 Security Scanning

```yaml
# .github/workflows/security.yml
name: Security Scan

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 0 * * *'  # Daily

jobs:
  dependency-scan:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Run Trivy vulnerability scanner
      uses: aquasecurity/trivy-action@master
      with:
        scan-type: 'fs'
        scan-ref: '.'
        format: 'sarif'
        output: 'trivy-results.sarif'
        
    - name: Upload Trivy scan results
      uses: github/codeql-action/upload-sarif@v2
      with:
        sarif_file: 'trivy-results.sarif'
        
  static-analysis:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Initialize CodeQL
      uses: github/codeql-action/init@v2
      with:
        languages: cpp
        
    - name: Build
      run: |
        cmake -B build -S .
        cmake --build build
        
    - name: Perform CodeQL Analysis
      uses: github/codeql-action/analyze@v2
      
  license-check:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4
    
    - name: Check licenses
      uses: fossa-contrib/fossa-action@v2
      with:
        api-key: ${{ secrets.FOSSA_API_KEY }}
```

## Conclusion

This infrastructure architecture provides:
- Automated development environment setup
- Comprehensive CI/CD pipelines
- Secure release distribution
- Monitoring and analytics
- Security scanning and compliance

The infrastructure supports the entire software development lifecycle from development to production deployment.