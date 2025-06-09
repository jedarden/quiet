# QUIET v1.0.0 - Initial Release

## Overview

QUIET is an AI-powered background noise removal tool that provides real-time noise cancellation for live audio streams on desktop platforms.

## Key Features

- 🎤 **Real-time Noise Reduction**: Powered by RNNoise ML algorithm
- 🖥️ **Cross-platform**: Native Windows 10/11 and macOS 10.15+ support
- ⚡ **Low Latency**: <30ms total system latency
- 📊 **Visual Feedback**: Real-time waveform and spectrum analyzer
- 🔊 **Virtual Audio Routing**: Works with any application
- 🔌 **Hot-plug Support**: Automatic device detection
- 🎨 **Modern UI**: Professional interface with visualization

## Performance Metrics

- **Processing Latency**: 7-15ms typical
- **CPU Usage**: 15-20% average
- **Memory Usage**: ~45MB
- **Audio Quality**: 12-15 dB SNR improvement

## Installation

### Option 1: Build from Source
See `BUILD_INSTRUCTIONS.md` for detailed build steps.

### Option 2: Pre-built Installers
*Note: Pre-built installers will be available in future releases.*

For now, please build from source using the provided installer kits:
- Windows: `quiet-1.0.0-windows-installer-kit.tar.gz`
- macOS: `quiet-1.0.0-macos-installer-kit.tar.gz`

## System Requirements

### Windows
- Windows 10 version 1903+ (64-bit)
- 4GB RAM minimum
- VB-Cable virtual audio driver

### macOS  
- macOS 10.15 Catalina+
- 4GB RAM minimum
- BlackHole virtual audio driver

## Documentation

Complete documentation is available in `quiet-1.0.0-docs.tar.gz`, including:
- Technical specifications
- Architecture documentation
- Performance validation reports
- API documentation

## Checksums

Verify file integrity using the SHA256 checksums in `SHA256SUMS.txt`.

## License

MIT License - See LICENSE file for details.

## Acknowledgments

- RNNoise by Mozilla/Xiph.Org
- JUCE Framework by Raw Material Software
- Virtual audio drivers: VB-Cable (Windows), BlackHole (macOS)
