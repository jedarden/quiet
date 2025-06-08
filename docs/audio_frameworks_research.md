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
**Overview**: The most widely used framework for audio application and plug-in development. Open source C++ codebase for creating standalone software and audio plugins. JUCE 8 was released in June 2024 with significant new features.

**Pros:**
- Comprehensive framework with GUI capabilities
- Supports VST, VST3, AU, AUv3, AAX and LV2 plug-ins
- Excellent documentation and community support
- Cross-platform: Windows, macOS, Linux, iOS, Android
- Built-in support for multiple audio backends (ASIO, WASAPI, CoreAudio, JACK)
- JUCE 8 includes WebView UI for web-based interfaces
- New Direct2D renderer for Windows with hardware acceleration
- Includes AAX SDK for Pro Tools development
- New animation framework in JUCE 8

**Cons:**
- Cannot create virtual audio devices (system-level drivers)
- Licensing considerations for commercial closed-source projects
- Heavier framework compared to alternatives
- Requires C++ expertise

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
**Overview**: Modern, lightweight abstraction over various sound drivers with focus on robustness and low-level control. Performs no buffering or processing, exposing raw power of underlying backend.

**Pros:**
- Clean, well-documented API
- Excellent error handling and robustness
- Built-in channel layout support
- Device monitoring and disconnect events
- Exposes raw backend power (including raw/exclusive device access)
- Pure C implementation (no C++ mixed in)
- Better device management than PortAudio
- Supports both raw and shared device access
- Designed for lowest possible latency

**Cons:**
- No virtual audio device creation
- No plugin support
- Less adoption than PortAudio
- No ASIO support (relies on WASAPI for Windows)

**Best for:** Applications requiring robust, low-level audio I/O with minimal latency

### JACK Audio Connection Kit
**Overview**: Professional sound server for real-time, low-latency audio routing between applications.

**Pros:**
- Designed for professional audio work
- Extremely low latency
- Cross-platform (Linux, macOS, Windows, FreeBSD)
- Acts as virtual audio patch bay
- Lock-free client graph access
- Allows inter-application audio routing
- Most popular Linux and Open Source alternative to Virtual Audio Cable

**Cons:**
- Requires JACK server running
- More complex setup for end users
- Not as seamless on Windows as on Linux/macOS

**Best for:** Professional audio routing and inter-application audio connections

### miniaudio
**Overview**: Single file audio playback and capture library written in C with no external dependencies, released into public domain. Actively maintained with updates in 2024.

**Pros:**
- Single header file implementation - extremely easy to integrate
- No external dependencies
- Public domain license
- Cross-platform support
- Both low-level and high-level APIs
- Built-in decoders for WAV, FLAC, and MP3
- Node graph system for mixing and effects
- Resource management for sound files
- Data conversion and resampling
- Basic effects and filters
- Low-latency shared mode support via IAudioClient3 on Windows 10+

**Cons:**
- Higher latency on Windows compared to macOS (83ms Win10, 93ms Win11 vs 1ms macOS)
- No virtual audio device creation
- Limited to built-in audio formats for encoding (WAV only)

**Best for:** Games, simple audio applications, or projects requiring minimal dependencies

### RtAudio
**Overview**: Set of C++ classes providing common API for realtime audio I/O across platforms.

**Pros:**
- C++ API (object-oriented design)
- Supports ASIO backend on Windows
- Cross-platform support
- Simplifies audio hardware interaction
- Used by OpenFrameworks

**Cons:**
- Limited documentation
- C++ only (no C API)
- No virtual audio device creation
- Less focus on lowest possible latency

**Best for:** C++ applications requiring cross-platform audio with ASIO support

## Virtual Audio Device Solutions

### Windows

#### Virtual Audio Cable (VAC) by Eugene Muzychenko
- Professional virtual audio device solution
- Simulates multi-line audio adapter with loopback
- Very low latency in well-tuned systems
- Bitperfect audio transfer (no quality loss)
- Supports up to 256 virtual cables in paid version
- Works with all Windows audio APIs

#### VB-Audio Solutions
**VB-Cable**
- Free virtual audio cable driver
- Simple to use and widely adopted
- Single virtual cable in free version
- Best Windows alternative to Virtual Audio Cable

**VoiceMeeter**
- Audio mixer application with virtual audio devices
- Free version includes 2 virtual inputs and outputs
- Built-in effects and routing capabilities
- Popular for streaming setups

**VB-Audio Matrix**
- Professional audio framework
- Connect multiple ASIO devices, Windows devices, DAWs
- Channel-by-channel routing
- Support for multiple computers

#### Synchronous Audio Router (SAR)
- Low latency application audio routing
- Synchronous with hardware audio interface
- Uses WaveRT for audio transport
- Doesn't impact DAW latency
- Dynamic creation of unlimited virtual devices
- Last updated in 2018 (v0.13.1) - limited maintenance

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

## Additional Audio Frameworks

### XT-Audio
**Overview**: Open source low-latency audio library focusing on simplicity and performance
- Exposes exclusive and shared device access separately
- Better API support than some alternatives
- Cross-platform support
- Focus on low-latency performance

### SoLoud
**Overview**: Easy to use, free, portable c/c++ audio engine for games
- Simple API design
- Built-in effects and filters
- 3D audio support
- No external dependencies
- Not designed for lowest latency

### FMOD
**Overview**: Professional game audio engine (commercial)
- Industry standard for game audio
- Comprehensive feature set
- Excellent performance
- Cross-platform support
- Commercial licensing required

## Audio Processing Libraries

### DSP and Noise Cancellation Libraries

#### RNNoise
**Overview**: Neural network-based noise suppression library by Jean-Marc Valin (2018)
- Uses recurrent neural network for real-time noise suppression
- Processes 10ms frames at 48 kHz
- Extracts 42 features from 22 frequency bands
- Low computational requirements
- Open source (BSD license)

#### BRIL Noise Reduction
**Overview**: High-quality blind noise reduction algorithm
- Takes noisy audio/voice signal as input
- Estimates and reduces noise without distorting audio
- No reference signal required
- Real-time capable

#### WebRTC Audio Processing
**Overview**: Google's audio processing module from WebRTC
- Acoustic echo cancellation (AEC)
- Noise suppression (NS)
- Automatic gain control (AGC)
- Voice activity detection (VAD)
- Cross-platform support
- Well-tested in production

#### Speex DSP
**Overview**: Open-source speech processing library
- Acoustic echo cancellation
- Noise suppression
- Automatic gain control
- Resampler
- Jitter buffer
- Lightweight and efficient

### Platform-Specific DSP Solutions

#### Windows - dsPIC30F Libraries
- Acoustic Echo Cancellation Library
- Uses adaptive FIR filter with NLMS algorithm
- Non-Linear Processor (NLP) for residual echo
- Voice activity and double-talk detection

#### macOS - Core Audio DSP
- Built-in Audio Units for effects
- vDSP framework for optimized DSP
- Accelerate framework for SIMD operations

## Recommendations

### For Cross-Platform Audio Applications:
1. **JUCE** - If you need comprehensive features and plugin support
2. **miniaudio** - For simple integration with minimal dependencies
3. **libsoundio** - For lightweight, robust audio I/O with lowest latency
4. **JACK** - For professional inter-application routing

### For Virtual Audio Routing:
1. **Windows**: VB-Audio solutions + WASAPI-based application
2. **macOS**: BlackHole + Core Audio-based application

### For Audio Processing:
1. **WebRTC Audio Processing** - For proven echo cancellation and noise suppression
2. **RNNoise** - For modern ML-based noise suppression
3. **Speex DSP** - For lightweight traditional DSP processing

### For Production Use:
- Combine audio framework (JUCE/libsoundio/miniaudio) with platform-specific virtual audio solution
- Integrate WebRTC or RNNoise for noise cancellation
- Consider JACK for professional setups requiring complex routing
- Use ASIO drivers on Windows for lowest latency
- Leverage Core Audio on macOS for optimal performance

## Key Considerations

1. **Latency**: 
   - ASIO (Windows) and Core Audio (macOS) provide lowest latency
   - Windows 10+ supports low-latency shared mode via IAudioClient3
   - Default Windows configuration not optimized for low latency
   - libsoundio offers raw device access for minimal latency

2. **Stability**: 
   - User-mode audio processing more stable than kernel-mode
   - Virtual audio devices run in kernel space
   - Consider crash isolation between components

3. **Compatibility**: 
   - Virtual audio devices require user installation
   - Some solutions need driver signing on Windows
   - Consider deployment complexity for end users

4. **Licensing**: 
   - JUCE: GPL or commercial license
   - miniaudio: Public domain
   - libsoundio: MIT license
   - Check all dependencies for commercial use

5. **Maintenance**: 
   - JUCE: Very active development (2024 release)
   - miniaudio: Active maintenance
   - SAR: Limited maintenance since 2018
   - Choose solutions with active communities

## Framework Comparison Table

| Framework | Language | Latency | Virtual Device | Cross-Platform | License | Best Use Case |
|-----------|----------|---------|----------------|----------------|---------|---------------|
| JUCE | C++ | Medium | No | Yes | GPL/Commercial | Full-featured audio apps |
| PortAudio | C | Medium | No | Yes | MIT | Simple audio I/O |
| libsoundio | C | Very Low | No | Yes | MIT | Low-latency audio I/O |
| miniaudio | C | Low-Medium | No | Yes | Public Domain | Games, simple apps |
| RtAudio | C++ | Medium | No | Yes | MIT | C++ audio apps |
| JACK | C | Very Low | Virtual routing | Yes | GPL/LGPL | Pro audio routing |

## Conclusion

For a production-ready solution that meets all requirements:

1. **Audio Framework Selection**:
   - **JUCE 8** for comprehensive features and plugin support
   - **libsoundio** for minimal latency requirements
   - **miniaudio** for simple integration with no dependencies

2. **Virtual Audio Routing**:
   - **Windows**: VB-Audio suite (VB-Cable, VoiceMeeter) or VAC
   - **macOS**: BlackHole for modern systems
   - **Cross-platform**: JACK for professional routing

3. **Audio Processing**:
   - **WebRTC Audio Processing** for proven AEC/NS
   - **RNNoise** for ML-based noise suppression
   - Platform-specific DSP frameworks for optimization

4. **Integration Pattern**:
   - Use audio framework for capture/playback
   - Route through virtual audio device
   - Apply DSP processing in real-time
   - Output to communication apps via virtual cable

5. **Performance Optimization**:
   - Use ASIO on Windows when available
   - Leverage Core Audio HAL on macOS
   - Consider exclusive/raw device access
   - Optimize OS settings for audio workloads

## Implementation Examples

### Basic Audio Capture with miniaudio
```c
// Simple example of audio capture
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    // Process audio frames here
    // pInput contains captured audio
    // frameCount is number of PCM frames
}

int main() {
    ma_device device;
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 48000;
    config.dataCallback = data_callback;
    
    ma_device_init(NULL, &config, &device);
    ma_device_start(&device);
    // ... keep running ...
    ma_device_uninit(&device);
}
```

### Virtual Audio Routing Pattern
```
[Microphone] -> [Your App] -> [Processing] -> [Virtual Cable Out]
                                                        |
[Discord/Zoom] <-- [Virtual Cable In] <-----------------+
```

### Platform-Specific Setup

**Windows Setup:**
1. Install VB-Cable or Virtual Audio Cable
2. Set your app output to "CABLE Input" 
3. Set Discord/Zoom input to "CABLE Output"
4. Use WASAPI exclusive mode for lower latency

**macOS Setup:**
1. Install BlackHole 2ch/16ch
2. Create aggregate device if needed
3. Route app output to BlackHole
4. Set communication app input to BlackHole

## Resources and References

- [JUCE Framework](https://juce.com/)
- [miniaudio Documentation](https://miniaud.io/)
- [libsoundio GitHub](https://github.com/andrewrk/libsoundio)
- [WebRTC Audio Processing](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_processing/)
- [RNNoise Project](https://github.com/xiph/rnnoise)
- [VB-Audio Software](https://vb-audio.com/)
- [BlackHole Driver](https://github.com/ExistentialAudio/BlackHole)