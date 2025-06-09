# SpectrumAnalyzer Component

A professional real-time frequency spectrum analyzer component for the QUIET noise cancellation application, built with JUCE framework.

## Features

### Core Functionality
- **Real-time FFT Analysis**: Processes audio input using Fast Fourier Transform for frequency domain visualization
- **Multiple FFT Sizes**: Supports 512, 1024, 2048, 4096, and 8192 sample FFT sizes
- **Window Functions**: Implements Rectangular, Hanning, Hamming, Blackman, and Blackman-Harris window functions
- **Logarithmic/Linear Scaling**: Toggle between logarithmic and linear frequency scaling
- **Peak Hold**: Visual peak level indicators with configurable hold time and decay rate
- **Smoothing**: Adjustable spectrum smoothing for cleaner visualization

### Visualization Modes
1. **Bars Mode**: Traditional frequency bars with color-coded amplitude levels
2. **Line Mode**: Smooth line graph with filled area underneath
3. **Waterfall Mode**: Scrolling spectrogram showing frequency content over time

### Visual Features
- **Frequency Grid**: Optional grid lines for frequency reference
- **Amplitude Grid**: Decibel reference lines
- **Frequency Labels**: Automatic labeling of major frequency points (20Hz - 20kHz)
- **Amplitude Labels**: Decibel scale labels
- **Color Gradient**: Amplitude-based coloring from blue (low) through cyan, green, yellow to red (high)

### Performance Optimizations
- **Thread-Safe Audio Processing**: Lock-free FIFO buffer for real-time audio
- **OpenGL Support**: Optional hardware-accelerated rendering
- **Efficient FFT Processing**: Optimized for low-latency operation
- **Smart Repaint**: Only redraws when new FFT data is available

## Usage

### Basic Integration

```cpp
// Create spectrum analyzer
SpectrumAnalyzer spectrum;

// Configure settings
spectrum.setFFTSize(SpectrumAnalyzer::FFTSize::Size2048);
spectrum.setWindowType(SpectrumAnalyzer::WindowType::Hanning);
spectrum.setVisualizationMode(SpectrumAnalyzer::VisualizationMode::Bars);
spectrum.setSampleRate(48000.0);

// In your audio callback
void processBlock(const AudioBuffer<float>& buffer)
{
    spectrum.processAudioBuffer(buffer);
}
```

### Configuration Options

#### FFT Settings
- `setFFTSize()`: Choose FFT resolution (higher = more frequency detail, more CPU)
- `setWindowType()`: Select windowing function for spectral leakage reduction
- `setSampleRate()`: Must be called when sample rate changes

#### Display Settings
- `setVisualizationMode()`: Switch between Bars, Line, or Waterfall display
- `setLogScale()`: Toggle logarithmic frequency scaling
- `setPeakHold()`: Enable/disable peak hold indicators
- `setSmoothing()`: Adjust visualization smoothing (0.0 - 0.99)
- `setFrequencyRange()`: Set min/max displayed frequencies
- `setDecibelRange()`: Set amplitude display range

#### Visual Options
- `setShowGrid()`: Toggle frequency/amplitude grid
- `setShowLabels()`: Toggle axis labels

## Technical Details

### FFT Processing
- Uses JUCE's optimized FFT implementation
- Applies selected window function before FFT
- Converts complex FFT output to magnitude spectrum
- Calculates amplitude in decibels (20 * log10)

### Thread Safety
- Audio processing uses lock-free FIFO buffer
- FFT processing happens on timer callback (60 Hz)
- No locks or allocations in audio thread

### Memory Usage
- Approximate memory per instance:
  - FFT Size 2048: ~100KB
  - FFT Size 4096: ~200KB
  - FFT Size 8192: ~400KB
  - Waterfall mode adds: width * 100 * 4 bytes

### CPU Usage
- Typical usage (2048 FFT, 60fps): 2-5% of single core
- OpenGL rendering can reduce CPU usage
- Waterfall mode uses slightly more CPU

## Customization

### Color Schemes
Modify `getColorForLevel()` method to implement custom color gradients:

```cpp
juce::Colour getColorForLevel(float normalizedLevel) const
{
    // Your custom color mapping
    return juce::Colour::fromHSV(hue, saturation, brightness, 1.0f);
}
```

### Additional Visualization Modes
Extend the `VisualizationMode` enum and implement new drawing methods:

```cpp
enum class VisualizationMode
{
    Bars,
    Line,
    Waterfall,
    YourCustomMode  // Add new mode
};

void drawYourCustomMode(Graphics& g, Rectangle<float> area)
{
    // Your visualization implementation
}
```

## Performance Tips

1. **FFT Size**: Use the smallest FFT size that provides adequate frequency resolution
2. **Frame Rate**: The component updates at 60 Hz by default; reduce if needed
3. **OpenGL**: Enable OpenGL rendering for better performance with large displays
4. **Smoothing**: Higher smoothing values reduce CPU usage slightly
5. **Waterfall Height**: Smaller waterfall heights use less memory and CPU

## Known Limitations

1. Maximum FFT size is 8192 (can be extended if needed)
2. Waterfall mode history is limited to 100 lines (configurable)
3. Single-channel (mono) analysis only (stereo is mixed to mono)
4. No built-in frequency band analysis (can be added)

## Future Enhancements

- Stereo spectrum analysis
- Frequency band power meters
- A-weighting and other perceptual weightings
- Spectrum averaging modes
- Export spectrum data
- Configurable color schemes
- 3D spectrum visualization