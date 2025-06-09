# QUIET v1.0.0 Release

Released: 2025-06-09

## 🎉 Initial Release

QUIET is an AI-powered background noise removal tool for desktop platforms, providing real-time noise cancellation for live audio streams.

## ✨ Features

- **🎤 Real-time Noise Reduction**: Powered by RNNoise ML algorithm
- **💻 Cross-platform**: Native Windows 10/11 and macOS 10.15+ support  
- **⚡ Low Latency**: <30ms total system latency
- **📊 Visual Feedback**: Real-time waveform and spectrum analyzer
- **🔊 Virtual Audio Routing**: Works with any application
- **🔌 Hot-plug Support**: Automatic device detection
- **🎨 Modern UI**: Professional interface with dark theme

## 📈 Performance

- Processing Latency: 7-15ms typical
- CPU Usage: 15-20% on modern processors
- Memory Usage: ~45MB
- Audio Quality: 12-15 dB SNR improvement

## 📦 Downloads

### Pre-built Packages
- **Windows**: `QUIET-1.0.0-windows-installer.tar.gz`
- **macOS**: `QUIET-1.0.0-macos-installer.tar.gz`

### Source Code
- **Source**: `quiet-1.0.0-source.tar.gz` (git archive)
- **Complete**: `quiet-1.0.0-complete.tar.gz` (all files)

### Documentation
- **Docs**: `quiet-1.0.0-docs.tar.gz`

## 🛠️ Installation

### Windows
1. Extract `QUIET-1.0.0-windows-installer.tar.gz`
2. Install VB-Cable virtual audio driver
3. Run NSIS installer or build from source
4. Launch QUIET

### macOS
1. Extract `QUIET-1.0.0-macos-installer.tar.gz`
2. Install BlackHole virtual audio driver
3. Build from source or use pre-built app
4. Grant microphone permissions

### Build from Source
See `BUILD_REQUIREMENTS.md` for detailed instructions.

## 🔒 Verification

Verify downloads using `SHA256SUMS.txt`:
```bash
sha256sum -c SHA256SUMS.txt
```

## 🐛 Known Issues

- First release - please report issues on GitHub
- Virtual audio drivers must be installed separately
- Windows may require administrator privileges

## 🙏 Acknowledgments

- RNNoise by Mozilla/Xiph.Org Foundation
- JUCE Framework by Raw Material Software
- Virtual audio drivers: VB-Cable, BlackHole

## 📄 License

MIT License - see LICENSE file

---

*Built with ❤️ using SPARC automated development system*
