# Desktop Audio Frameworks Research

## Overview

This document provides comprehensive research on desktop audio frameworks for OSX and Windows that support real-time audio capture, processing, virtual audio device creation, and low-latency streaming for integration with communication apps like Discord and Zoom.

## Key Requirements
1. Real-time audio capture and processing
2. Virtual audio device creation (for routing processed audio to Discord, Zoom, etc.)
3. Low-latency audio streaming
4. Cross-platform compatibility

## Audio Frameworks Comparison

### JUCE
**Overview**: The most widely used framework for audio application and plug-in development. Open source C++ codebase for creating standalone software and audio plugins.

**Pros:**
- Comprehensive framework with GUI capabilities
- Supports VST, VST3, AU, AUv3, AAX and LV2 plug-ins
- Excellent documentation and community support
- Cross-platform: Windows, macOS, Linux, iOS, Android
- Built-in support for multiple audio backends (ASIO, WASAPI, CoreAudio, JACK)

**Cons:**
- Cannot create virtual audio devices (system-level drivers)
- Licensing considerations for commercial closed-source projects
- Heavier framework compared to alternatives

**Best for:** Audio applications and plugins that work with existing audio devices

### PortAudio
**Overview**: Cross-platform, open-source audio I/O library with a simple C API for recording/playing sound using callbacks.

**Pros:**
- Mature, battle-tested solution
- Wide platform support
- Low-level control
- Supports ASIO on Windows for low latency

**Cons:**
- C API (not C++)
- No virtual audio device creation capability
- No plugin support
- More complex/lower level than alternatives
- Less robust error handling compared to modern alternatives

**Best for:** Simple audio I/O in standalone applications

### libsoundio
**Overview**: Modern, lightweight abstraction over various sound drivers with focus on robustness and low-level control.

**Pros:**
- Clean, well-documented API
- Excellent error handling and robustness
- Built-in channel layout support
- Device monitoring and disconnect events
- Exposes raw backend power
- Pure C implementation
- Better device management than PortAudio

**Cons:**
- No virtual audio device creation
- No plugin support
- Less adoption than PortAudio

**Best for:** Applications requiring robust, low-level audio I/O with good device handling

### JACK Audio Connection Kit
**Overview**: Professional sound server for real-time, low-latency audio routing between applications.

**Pros:**
- Designed for professional audio work
- Extremely low latency
- Cross-platform (Linux, macOS, Windows, FreeBSD)
- Acts as virtual audio patch bay
- Lock-free client graph access
- Allows inter-application audio routing

**Cons:**
- Requires JACK server running
- More complex setup for end users
- Not as seamless on Windows as on Linux/macOS

**Best for:** Professional audio routing and inter-application audio connections

## Virtual Audio Device Solutions

### Windows

#### VB-Cable
- Virtual audio device working as virtual audio cable
- Simple to use and widely adopted
- Free version available
- Routes audio between applications

#### Synchronous Audio Router (SAR)
- Low latency application audio routing
- Synchronous with hardware audio interface
- Uses WaveRT for audio transport
- Doesn't impact DAW latency

#### Virtual Audio Capture Grabber Device
- Free, open-source solution
- Uses Windows loopback adapter interface
- Captures outgoing audio as DirectShow device
- For Windows Vista+

### macOS

#### BlackHole
- Modern macOS virtual audio loopback driver
- Zero additional latency
- Open source
- Widely supported

#### Loopback (by Rogue Amoeba)
- Commercial solution
- Create virtual devices with up to 64 channels
- Excellent integration with macOS
- Professional features

#### Soundflower (Legacy)
- Original virtual audio device for macOS
- Less maintained, being replaced by BlackHole

## Platform-Specific Development

### Windows Core Audio API (WASAPI)
- Native Windows audio API introduced in Vista
- 32-bit floating point audio engine
- User-mode audio processing for stability
- Required for creating Windows virtual audio drivers
- Supports exclusive mode for low latency

### macOS Core Audio HAL Plugins
- Required for creating macOS virtual audio devices
- Must be installed in `/Library/Audio/Plug-Ins/HAL/`
- Operates in sandboxed environment
- Real-time constraints apply
- AudioServerPlugin interface required

## Integration with Communication Apps

### Common Integration Pattern
1. Create virtual audio device (using platform-specific solution)
2. Set app output to virtual device
3. Process audio in your application
4. Route processed audio to communication app input

### Example Setup with Discord/Zoom:
1. Install virtual audio cable (VB-Cable, BlackHole, etc.)
2. Configure Discord/Zoom to use virtual cable as input
3. Route processed audio from your app to virtual cable
4. Use monitoring device for local playback

### OBS Integration
- Use Application Audio Capture (OBS 28+)
- No third-party virtual cables needed for basic routing
- Set monitoring device to virtual cable
- Configure sources to "monitor and output"

## Recommendations

### For Cross-Platform Audio Applications:
1. **JUCE** - If you need comprehensive features and plugin support
2. **libsoundio** - For lightweight, robust audio I/O
3. **JACK** - For professional inter-application routing

### For Virtual Audio Routing:
1. **Windows**: VB-Cable + WASAPI-based application
2. **macOS**: BlackHole + Core Audio-based application

### For Production Use:
- Combine audio framework (JUCE/libsoundio) with platform-specific virtual audio solution
- Consider JACK for professional setups requiring complex routing
- Use ASIO drivers on Windows for lowest latency
- Leverage Core Audio on macOS for optimal performance

## Key Considerations

1. **Latency**: ASIO (Windows) and Core Audio (macOS) provide lowest latency
2. **Stability**: User-mode audio processing more stable than kernel-mode
3. **Compatibility**: Virtual audio devices may require user installation
4. **Licensing**: Check framework licenses for commercial use
5. **Maintenance**: Choose actively maintained solutions

## Conclusion

For a production-ready solution that meets all requirements:
- Use JUCE or libsoundio for the audio processing application
- Integrate with platform-specific virtual audio drivers (VB-Cable/BlackHole)
- Consider JACK for professional users needing complex routing
- Implement platform-specific optimizations (ASIO on Windows, Core Audio on macOS)