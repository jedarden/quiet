# QUIET - AI-Powered Background Noise Removal

QUIET is a desktop application that provides real-time background noise removal for audio input devices. It uses advanced machine learning algorithms to enhance speech quality while preserving natural voice characteristics.

## Features

- **Real-time noise reduction** using RNNoise ML algorithm
- **Cross-platform support** for Windows and macOS
- **Virtual audio device** integration for seamless use with any application
- **Visual audio monitoring** with waveform and spectrum displays
- **Low latency processing** (<30ms end-to-end)
- **Minimal resource usage** (<15% CPU on modern systems)

## System Requirements

### Windows
- Windows 10 or later (64-bit)
- 4GB RAM minimum
- Dual-core processor or better
- Audio input device (microphone)

### macOS
- macOS 10.15 (Catalina) or later
- 4GB RAM minimum
- Intel or Apple Silicon processor
- Audio input device (microphone)

## Installation

### Windows
1. Download the latest installer from [Releases](https://github.com/quiet-audio/quiet/releases)
2. Run the `.msi` installer as administrator
3. Install VB-Audio VB-Cable when prompted (required for virtual device)
4. Launch QUIET from the Start Menu

### macOS
1. Download the latest `.dmg` from [Releases](https://github.com/quiet-audio/quiet/releases)
2. Open the DMG and drag QUIET to Applications
3. Install BlackHole audio driver when prompted
4. Launch QUIET from Applications folder

## Quick Start

1. **Select your microphone** from the device dropdown
2. **Enable noise reduction** with the toggle button
3. **Set QUIET Virtual Mic** as your input device in communication apps (Discord, Zoom, etc.)
4. **Monitor the visualizations** to see noise reduction in action

## Building from Source

### Prerequisites

- CMake 3.20 or later
- C++17 compatible compiler
- Git

### Dependencies

The build system will automatically fetch:
- JUCE framework
- RNNoise library
- Google Test (for testing)

### Build Steps

```bash
git clone https://github.com/quiet-audio/quiet.git
cd quiet
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

## Usage

### Basic Operation

1. **Device Selection**: Choose your microphone from the dropdown menu
2. **Noise Reduction**: Toggle the main switch to enable/disable processing
3. **Level Adjustment**: Select Low/Medium/High reduction levels based on your environment
4. **Monitoring**: Watch the input/output waveforms to verify operation

### Integration with Applications

#### Discord
1. Go to Settings → Voice & Video
2. Select "QUIET Virtual Mic" as your input device
3. Adjust input sensitivity as needed

#### Zoom
1. Go to Settings → Audio
2. Select "QUIET Virtual Mic" from the microphone dropdown
3. Test your audio to verify quality

#### Slack
1. Go to Preferences → Audio & video
2. Choose "QUIET Virtual Mic" as your microphone
3. Test your setup in a huddle

### Performance Tips

- **Close unnecessary applications** to reduce CPU load
- **Use a USB headset** for best audio quality
- **Position microphone properly** about 6 inches from your mouth
- **Monitor CPU usage** in the status bar - should stay below 15%

## Troubleshooting

### Virtual Device Not Found

**Windows:**
- Ensure VB-Cable is properly installed
- Restart the application after VB-Cable installation
- Check Device Manager for VB-Audio devices

**macOS:**
- Verify BlackHole is installed in Audio MIDI Setup
- Grant microphone permissions in System Preferences → Security & Privacy
- Restart QUIET after installing BlackHole

### High CPU Usage

- Reduce the noise reduction level from High to Medium or Low
- Close other audio applications
- Check for system updates
- Ensure sufficient RAM is available

### Audio Quality Issues

- Check microphone positioning and quality
- Verify input levels are not clipping (red indicators)
- Try different noise reduction levels
- Ensure proper device drivers are installed

### No Audio Processing

- Verify the correct input device is selected
- Check that noise reduction is enabled (toggle button)
- Ensure the virtual device is working in other applications
- Restart the application

## Technical Details

### Architecture

QUIET uses a modular architecture with the following components:

- **Audio Engine**: Handles device I/O and real-time processing
- **Noise Processor**: RNNoise-based ML inference
- **Virtual Device**: Platform-specific audio routing
- **UI Framework**: JUCE-based cross-platform interface

### Performance Characteristics

- **Latency**: <30ms typical, <50ms maximum
- **CPU Usage**: 5-15% on modern quad-core processors
- **Memory**: <200MB typical usage
- **Audio Quality**: PESQ improvement of 0.4+ points

### Supported Audio Formats

- **Sample Rates**: 16kHz, 44.1kHz, 48kHz
- **Bit Depths**: 16-bit, 24-bit, 32-bit float
- **Channels**: Mono input (stereo input mixed to mono)

## Development

### Code Structure

```
src/
├── core/           # Core audio processing
├── ui/             # User interface components  
├── platform/       # Platform-specific code
└── utils/          # Utility functions

include/quiet/      # Public headers
tests/             # Unit and integration tests
resources/         # Icons, configurations
```

### Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Coding Standards

- Follow Google C++ Style Guide
- Use meaningful variable and function names
- Include unit tests for new functionality
- Document public APIs with Doxygen comments
- Ensure cross-platform compatibility

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

- **Documentation**: [Wiki](https://github.com/quiet-audio/quiet/wiki)
- **Issues**: [GitHub Issues](https://github.com/quiet-audio/quiet/issues)
- **Discussions**: [GitHub Discussions](https://github.com/quiet-audio/quiet/discussions)
- **Email**: support@quietaudio.com

## Acknowledgments

- [RNNoise](https://jmvalin.ca/demo/rnnoise/) by Jean-Marc Valin
- [JUCE Framework](https://juce.com/) by ROLI Ltd.
- [VB-Cable](https://vb-audio.com/Cable/) by VB-Audio Software
- [BlackHole](https://existential.audio/blackhole/) by Existential Audio Inc.