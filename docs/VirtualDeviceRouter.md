# VirtualDeviceRouter Component

## Overview

The `VirtualDeviceRouter` is a critical component of the QUIET application that handles routing processed audio to virtual audio devices. This allows other applications (Discord, Zoom, Teams, etc.) to receive the noise-reduced audio stream.

## Features

- **Cross-Platform Support**: Works with VB-Cable on Windows and BlackHole on macOS
- **Automatic Device Detection**: Scans and identifies installed virtual audio devices
- **Hot-Plug Detection**: Monitors for device connections/disconnections in real-time
- **Format Conversion**: Handles sample rate and channel conversion as needed
- **Performance Monitoring**: Tracks latency, dropped buffers, and output levels
- **Thread-Safe Operation**: Designed for real-time audio processing
- **Error Recovery**: Automatic reconnection attempts on device disconnection

## Architecture

### Class Structure

```cpp
class VirtualDeviceRouter {
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Device Management
    std::vector<VirtualDeviceInfo> getAvailableVirtualDevices();
    bool selectVirtualDevice(const std::string& deviceId);
    
    // Audio Routing
    bool startRouting();
    void stopRouting();
    bool routeAudioBuffer(const AudioBuffer& buffer);
    
    // Monitoring
    float getOutputLevel() const;
    uint64_t getBuffersRouted() const;
    double getAverageLatency() const;
};
```

### Platform Implementation

The router uses platform-specific implementations:

- **Windows**: `WindowsVirtualDeviceImpl` - Uses WASAPI for VB-Cable integration
- **macOS**: `MacOSVirtualDeviceImpl` - Uses Core Audio for BlackHole integration

## Usage

### Basic Setup

```cpp
// Create event dispatcher
EventDispatcher eventDispatcher;
eventDispatcher.start();

// Create router
VirtualDeviceRouter router(eventDispatcher);

// Initialize
if (!router.initialize()) {
    // Handle initialization failure
}

// Get available devices
auto devices = router.getAvailableVirtualDevices();

// Select a device
if (!devices.empty()) {
    router.selectVirtualDevice(devices[0].id);
}

// Start routing
router.startRouting();

// Route audio buffers
AudioBuffer buffer(2, 256, 48000.0);
// ... fill buffer with audio data ...
router.routeAudioBuffer(buffer);
```

### Error Handling

```cpp
// Set error callback
router.setErrorCallback([](const std::string& message, int errorCode) {
    std::cerr << "Error: " << message << " (code: " << errorCode << ")\n";
});

// Set device change callback
router.setDeviceChangeCallback([](const VirtualDeviceInfo& device) {
    if (!device.isConnected) {
        // Handle device disconnection
    }
});
```

### Performance Monitoring

```cpp
// Get routing statistics
std::cout << "Buffers routed: " << router.getBuffersRouted() << "\n";
std::cout << "Dropped buffers: " << router.getDroppedBuffers() << "\n";
std::cout << "Average latency: " << router.getAverageLatency() << "ms\n";
std::cout << "Output level: " << router.getOutputLevel() << "\n";
```

## Virtual Device Installation

### Windows (VB-Cable)

1. Download VB-Cable from https://vb-audio.com/Cable/
2. Extract the ZIP file
3. Run VBCABLE_Setup_x64.exe as administrator
4. Follow installation prompts
5. Restart computer
6. Device appears as "CABLE Input"

### macOS (BlackHole)

1. Download BlackHole from https://existential.audio/blackhole/
2. Choose 2ch (stereo) or 16ch (multi-channel) version
3. Open the PKG installer
4. Follow installation prompts
5. Grant necessary permissions
6. Device appears as "BlackHole 2ch" or "BlackHole 16ch"

## Integration with QUIET

The VirtualDeviceRouter integrates with other QUIET components:

1. **AudioProcessor** → Produces processed audio buffers
2. **VirtualDeviceRouter** → Routes buffers to virtual device
3. **EventDispatcher** → Notifies about device changes and errors
4. **ConfigurationManager** → Saves selected device preferences

## Thread Safety

The router is designed for real-time audio processing:

- Lock-free audio routing path
- Mutex-protected device management
- Atomic statistics tracking
- Separate hot-plug detection thread

## Error Codes

| Code | Description |
|------|-------------|
| -1   | Platform not supported |
| -2   | Virtual device not found |
| -3   | Failed to open device |
| -4   | No device connected |
| -5   | Device disconnected during routing |
| -6   | Device removed (hot-plug) |

## Testing

Unit tests are provided in `VirtualDeviceRouterTest.cpp`:

- Device detection tests
- Configuration tests
- Error handling tests
- Callback tests
- Performance monitoring tests

## Future Enhancements

- Linux support (JACK/PulseAudio)
- Multiple simultaneous virtual devices
- Advanced format conversion (resampling)
- Latency compensation
- Device-specific optimizations