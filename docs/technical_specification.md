# QUIET - Technical Specification

## 1. System Architecture

### 1.1 High-Level Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                      Desktop Application                      │
├─────────────────────────┬───────────────────────────────────┤
│      UI Layer          │         Audio Engine              │
│  ┌─────────────────┐  │  ┌─────────────────────────────┐ │
│  │   Qt/Electron   │  │  │   Audio Capture Thread      │ │
│  │   GUI Controls  │  │  │   ┌─────────────────────┐   │ │
│  │   Waveform Viz  │  │  │   │  Noise Cancellation │   │ │
│  └────────┬────────┘  │  │   │  (DCCRN/PercepNet)  │   │ │
│           │           │  │   └─────────┬───────────┘   │ │
│  ┌────────┴────────┐  │  │   ┌─────────┴───────────┐   │ │
│  │  Settings Mgr   │  │  │   │  Audio Router       │   │ │
│  │  State Manager  │  │  │   │  Virtual Device     │   │ │
│  └─────────────────┘  │  │   └─────────────────────┘   │ │
└───────────────────────┴─────────────────────────────────┘
                                    │
                        ┌───────────┴────────────┐
                        │   OS Audio Subsystem   │
                        │  (Core Audio/WASAPI)   │
                        └────────────────────────┘
```

### 1.2 Technology Stack

**Core Technologies:**
- **Language**: C++ (performance-critical audio processing)
- **UI Framework**: Electron + React (cross-platform) OR Qt 6
- **Audio Framework**: JUCE 7 (audio processing and plugin support)
- **ML Runtime**: ONNX Runtime (cross-platform inference)
- **Build System**: CMake
- **Package Manager**: vcpkg/Conan

**Platform-Specific:**
- **Windows**: WASAPI, VB-Cable for virtual device
- **macOS**: Core Audio, BlackHole for virtual device
- **Virtual Audio**: Platform-specific kernel drivers

## 2. Component Design

### 2.1 Audio Engine Components

**AudioCaptureModule**
- Responsibility: Capture audio from selected microphone
- Interface: `startCapture()`, `stopCapture()`, `setDevice(deviceId)`
- Threading: Dedicated capture thread with ring buffer
- Sample rate: 16kHz (processing), 44.1/48kHz (output)

**NoiseCancellationModule**
- Responsibility: Apply ML-based noise suppression
- Algorithm: DCCRN (Deep Complex Convolution Recurrent Network)
- Interface: `processFrame(inputBuffer, outputBuffer)`
- Latency: <10ms processing time per frame
- Frame size: 160 samples (10ms at 16kHz)

**AudioRouterModule**
- Responsibility: Route processed audio to virtual device
- Interface: `routeAudio(processedBuffer)`
- Implementation: Platform-specific audio APIs
- Buffer management: Triple buffering for smooth playback

**VisualizationModule**
- Responsibility: Provide audio data for UI visualization
- Interface: `getWaveformData()`, `getSpectrumData()`
- Update rate: 30-60 FPS
- Data format: Downsampled for display efficiency

### 2.2 UI Components

**MainWindow**
- Framework: Electron (HTML/CSS/JS) or Qt QML
- Layout: Single window with control panel and visualizers
- State management: Redux (Electron) or Qt property bindings

**AudioVisualizer**
- Technology: Canvas 2D (Electron) or QCustomPlot (Qt)
- Features: Dual waveform display, level meters
- Performance: GPU-accelerated rendering where available

**DeviceSelector**
- Implementation: Dropdown populated from audio subsystem
- Updates: Real-time device detection
- Validation: Check device availability before selection

### 2.3 ML Model Integration

**Model Architecture**: DCCRN
- Input: 16kHz mono audio frames
- Processing: Complex-valued neural network
- Output: Enhanced speech signal
- Model size: ~3.7MB (quantized)
- Inference: ONNX Runtime with CPU optimization

**Model Pipeline**:
1. Preprocessing: STFT with 512-point FFT
2. Feature extraction: Magnitude and phase
3. Network inference: Complex convolution layers
4. Postprocessing: iSTFT reconstruction
5. Overlap-add for continuous stream

## 3. Data Flow Architecture

```
Microphone Input (48kHz)
        ↓
    Resampler (→16kHz)
        ↓
    Ring Buffer
        ↓
    Frame Extraction (10ms)
        ↓
    STFT Transform
        ↓
    DCCRN Inference
        ↓
    iSTFT Transform
        ↓
    Overlap-Add Buffer
        ↓
    Resampler (→48kHz)
        ↓
    Virtual Audio Device
        ↓
    Communication Apps
```

## 4. Threading Model

**Main Thread**: UI updates and user interaction
**Audio Capture Thread**: High-priority audio input
**Processing Thread**: ML inference and DSP
**Audio Output Thread**: Virtual device writing
**Visualization Thread**: Waveform data preparation

Thread communication via lock-free ring buffers.

## 5. Performance Requirements

- **CPU Usage**: <10% on modern processors
- **Memory**: <200MB RAM
- **Latency**: <20ms end-to-end
- **Sample rates**: 16/44.1/48 kHz support
- **Bit depth**: 16/24-bit audio

## 6. Build Configuration

### Windows Build:
```cmake
cmake -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_WASAPI=ON \
  -DBUILD_VST=OFF
```

### macOS Build:
```cmake
cmake -G "Xcode" \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_COREAUDIO=ON \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
```

## 7. Testing Architecture

- **Unit Tests**: Google Test for C++ components
- **Integration Tests**: Audio pipeline validation
- **Performance Tests**: Latency and CPU benchmarks
- **Platform Tests**: Windows 10/11, macOS 10.15+

## 8. Security Considerations

- Code signing for Windows/macOS distribution
- Sandboxing where applicable
- No network connectivity required
- Local processing only
- Audio data never leaves device

## 9. Deployment

**Windows**: 
- MSI installer with virtual driver
- Code-signed executable
- Auto-update mechanism

**macOS**:
- DMG with app bundle
- Notarized for Gatekeeper
- Separate BlackHole installer

## 10. Future Extensibility

- Plugin architecture for alternative algorithms
- Multi-microphone support
- Custom training for specific noise types
- Cloud-based model updates
- Mobile companion apps