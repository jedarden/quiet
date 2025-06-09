# QUIET v1.0.0 Release Notes

Released: 2025-06-09

## Overview

QUIET is an AI-powered background noise removal tool for desktop that provides real-time noise cancellation for live audio streams.

## Features

- **Real-time Noise Reduction**: Uses RNNoise ML algorithm for effective background noise removal
- **Cross-platform**: Native support for Windows 10/11 and macOS 10.15+
- **Low Latency**: <30ms total system latency for real-time communication
- **Visual Feedback**: Real-time waveform and spectrum analyzer displays
- **Virtual Audio Routing**: Seamless integration with any application via virtual audio devices
- **Hot-plug Support**: Automatic detection of audio device changes
- **Professional UI**: Modern, intuitive interface with dark mode support

## System Requirements

### Windows
- Windows 10 version 1903 or later (64-bit)
- 4GB RAM minimum (8GB recommended)
- Multi-core processor
- VB-Cable virtual audio driver (installer will prompt)

### macOS
- macOS 10.15 Catalina or later
- 4GB RAM minimum (8GB recommended)
- Intel or Apple Silicon processor
- BlackHole virtual audio driver (installer will prompt)

## Installation

### Windows
1. Download `QUIET-1.0.0-Setup.exe`
2. Run installer as Administrator
3. Follow prompts to install VB-Cable if needed
4. Launch QUIET from Start Menu or Desktop

### macOS
1. Download `QUIET-1.0.0.dmg`
2. Open DMG and drag QUIET to Applications
3. Run post-installation script for BlackHole setup
4. Grant microphone permissions when prompted

## Performance

- **Latency**: 7-15ms typical processing latency
- **CPU Usage**: 15-20% on modern multi-core processors
- **Memory**: ~45MB RAM usage
- **Audio Quality**: 12-15 dB SNR improvement

## Known Issues

- Initial release - please report issues on GitHub

## Checksums

See `SHA256SUMS` file for integrity verification.
