# Audio Visualization Research for QUIET Project

## Executive Summary

This document provides in-depth research on audio visualization techniques suitable for the QUIET project's requirements of displaying real-time input and output waveforms for noise-reduced audio. The focus is on performance-optimized implementations that work within the JUCE framework.

## 1. Waveform Visualization Techniques

### 1.1 Basic Waveform Rendering

#### Sample-Based Display
The simplest approach plots audio samples directly:

```cpp
class BasicWaveform {
    std::vector<float> samples;
    
    void paint(Graphics& g) {
        Path wavePath;
        float xScale = getWidth() / float(samples.size());
        float yScale = getHeight() / 2.0f;
        
        wavePath.startNewSubPath(0, getHeight() / 2.0f);
        
        for (size_t i = 0; i < samples.size(); ++i) {
            float x = i * xScale;
            float y = (getHeight() / 2.0f) - (samples[i] * yScale);
            wavePath.lineTo(x, y);
        }
        
        g.strokePath(wavePath, PathStrokeType(2.0f));
    }
};
```

**Pros**: Simple implementation
**Cons**: Inefficient for large buffers, aliasing at high zoom levels

### 1.2 Peak-Based Rendering

#### Min/Max Peak Detection
More efficient approach using peak detection:

```cpp
class PeakWaveform {
    struct PeakPair {
        float min, max;
    };
    
    std::vector<PeakPair> peaks;
    
    void calculatePeaks(const float* audioData, int numSamples, int displayWidth) {
        peaks.resize(displayWidth);
        int samplesPerPixel = numSamples / displayWidth;
        
        for (int pixel = 0; pixel < displayWidth; ++pixel) {
            float minVal = 1.0f, maxVal = -1.0f;
            int startSample = pixel * samplesPerPixel;
            int endSample = std::min(startSample + samplesPerPixel, numSamples);
            
            for (int i = startSample; i < endSample; ++i) {
                minVal = std::min(minVal, audioData[i]);
                maxVal = std::max(maxVal, audioData[i]);
            }
            
            peaks[pixel] = {minVal, maxVal};
        }
    }
    
    void paint(Graphics& g) {
        float centerY = getHeight() / 2.0f;
        float yScale = getHeight() / 2.0f;
        
        for (int x = 0; x < peaks.size(); ++x) {
            float minY = centerY - (peaks[x].min * yScale);
            float maxY = centerY - (peaks[x].max * yScale);
            g.drawVerticalLine(x, minY, maxY);
        }
    }
};
```

**Pros**: Efficient rendering, preserves peaks
**Cons**: Can miss detail at low zoom levels

### 1.3 RMS-Based Rendering

#### Root Mean Square Visualization
Shows average energy levels:

```cpp
class RMSWaveform {
    std::vector<float> rmsValues;
    
    void calculateRMS(const float* audioData, int numSamples, int displayWidth) {
        rmsValues.resize(displayWidth);
        int samplesPerPixel = numSamples / displayWidth;
        
        for (int pixel = 0; pixel < displayWidth; ++pixel) {
            float sumSquares = 0.0f;
            int startSample = pixel * samplesPerPixel;
            int endSample = std::min(startSample + samplesPerPixel, numSamples);
            
            for (int i = startSample; i < endSample; ++i) {
                sumSquares += audioData[i] * audioData[i];
            }
            
            rmsValues[pixel] = std::sqrt(sumSquares / samplesPerPixel);
        }
    }
};
```

**Pros**: Shows energy distribution
**Cons**: Loses transient information

### 1.4 Advanced Ring Buffer Implementation

#### Lock-Free Circular Buffer for Real-Time Display
```cpp
template<typename T, size_t Size>
class LockFreeRingBuffer {
    std::array<std::atomic<T>, Size> buffer;
    std::atomic<size_t> writeIndex{0};
    std::atomic<size_t> readIndex{0};
    
public:
    bool push(T value) {
        size_t currentWrite = writeIndex.load();
        size_t nextWrite = (currentWrite + 1) % Size;
        
        if (nextWrite == readIndex.load()) {
            return false; // Buffer full
        }
        
        buffer[currentWrite].store(value);
        writeIndex.store(nextWrite);
        return true;
    }
    
    bool pop(T& value) {
        size_t currentRead = readIndex.load();
        
        if (currentRead == writeIndex.load()) {
            return false; // Buffer empty
        }
        
        value = buffer[currentRead].load();
        readIndex.store((currentRead + 1) % Size);
        return true;
    }
};
```

### 1.5 JUCE-Specific Implementation

#### Using JUCE's AudioVisualiserComponent
```cpp
class QUIETWaveformDisplay : public AudioVisualiserComponent {
    static constexpr int bufferSize = 1024;
    static constexpr int samplesPerBlock = 256;
    
public:
    QUIETWaveformDisplay() 
        : AudioVisualiserComponent(2) // 2 channels
    {
        setBufferSize(bufferSize);
        setSamplesPerBlock(samplesPerBlock);
        setRepaintRate(30); // 30 FPS
        
        // Custom colors
        setColours(Colours::black, Colours::green);
    }
    
    void pushBuffer(const AudioBuffer<float>& bufferToPush) {
        pushBuffer(bufferToPush.getArrayOfReadPointers(), 
                   bufferToPush.getNumSamples(), 
                   bufferToPush.getNumChannels());
    }
};
```

## 2. Spectrum Analysis Visualization

### 2.1 FFT-Based Spectrum Analyzer

#### Basic FFT Implementation
```cpp
class SpectrumAnalyzer : public Component, private Timer {
    dsp::FFT forwardFFT;
    dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData;
    std::array<float, fftSize / 2> spectrum;
    
    static constexpr int fftOrder = 11; // 2048 samples
    static constexpr int fftSize = 1 << fftOrder;
    
public:
    SpectrumAnalyzer() 
        : forwardFFT(fftOrder),
          window(fftSize, dsp::WindowingFunction<float>::hann)
    {
        startTimerHz(30);
    }
    
    void processBlock(const float* audioData, int numSamples) {
        if (numSamples >= fftSize) {
            // Copy and window the data
            std::copy(audioData, audioData + fftSize, fftData.begin());
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            
            // Perform FFT
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());
            
            // Calculate magnitude spectrum
            for (int i = 0; i < fftSize / 2; ++i) {
                auto magnitude = std::abs(std::complex<float>(
                    fftData[i * 2], fftData[i * 2 + 1]));
                
                // Convert to dB with smoothing
                auto db = 20.0f * std::log10(magnitude + 1e-6f);
                spectrum[i] = spectrum[i] * 0.7f + db * 0.3f; // Smooth
            }
        }
    }
    
    void paint(Graphics& g) override {
        auto bounds = getLocalBounds();
        
        Path spectrumPath;
        auto width = bounds.getWidth();
        auto height = bounds.getHeight();
        
        for (int i = 1; i < fftSize / 2; ++i) {
            // Logarithmic frequency scale
            auto freq = (float)i / fftSize * 44100.0f; // Assuming 44.1kHz
            auto x = std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f) * width;
            
            // Scale magnitude
            auto y = jmap(spectrum[i], -100.0f, 0.0f, 
                         (float)height, 0.0f);
            
            if (i == 1)
                spectrumPath.startNewSubPath(x, y);
            else
                spectrumPath.lineTo(x, y);
        }
        
        g.setColour(Colours::cyan);
        g.strokePath(spectrumPath, PathStrokeType(2.0f));
    }
};
```

### 2.2 Frequency Band Analysis

#### Octave Band Analyzer
```cpp
class OctaveBandAnalyzer {
    struct Band {
        float centerFreq;
        float lowerFreq;
        float upperFreq;
        float magnitude;
    };
    
    std::vector<Band> bands;
    
    void initializeBands() {
        // Standard octave bands
        const float centerFreqs[] = {
            31.5f, 63.0f, 125.0f, 250.0f, 500.0f,
            1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
        };
        
        for (auto freq : centerFreqs) {
            Band band;
            band.centerFreq = freq;
            band.lowerFreq = freq / std::sqrt(2.0f);
            band.upperFreq = freq * std::sqrt(2.0f);
            band.magnitude = 0.0f;
            bands.push_back(band);
        }
    }
    
    void updateBands(const float* spectrum, int spectrumSize, float sampleRate) {
        float binWidth = sampleRate / (2.0f * spectrumSize);
        
        for (auto& band : bands) {
            int startBin = std::max(1, (int)(band.lowerFreq / binWidth));
            int endBin = std::min(spectrumSize - 1, (int)(band.upperFreq / binWidth));
            
            float sum = 0.0f;
            for (int i = startBin; i <= endBin; ++i) {
                sum += spectrum[i] * spectrum[i];
            }
            
            band.magnitude = std::sqrt(sum / (endBin - startBin + 1));
        }
    }
};
```

### 2.3 Waterfall Display

#### 3D Spectrum History
```cpp
class WaterfallDisplay : public Component, public OpenGLRenderer {
    std::deque<std::vector<float>> spectrumHistory;
    static constexpr int historySize = 100;
    
    void addSpectrum(const std::vector<float>& spectrum) {
        spectrumHistory.push_back(spectrum);
        if (spectrumHistory.size() > historySize) {
            spectrumHistory.pop_front();
        }
    }
    
    void renderOpenGL() override {
        // Set up 3D perspective
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, getWidth() / (double)getHeight(), 0.1, 100.0);
        
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -50.0f);
        glRotatef(30.0f, 1.0f, 0.0f, 0.0f);
        
        // Draw waterfall
        for (int t = 0; t < spectrumHistory.size(); ++t) {
            glBegin(GL_LINE_STRIP);
            
            for (int f = 0; f < spectrumHistory[t].size(); ++f) {
                float x = (f / float(spectrumHistory[t].size()) - 0.5f) * 40.0f;
                float y = spectrumHistory[t][f] * 10.0f;
                float z = (t / float(historySize) - 0.5f) * 40.0f;
                
                // Color based on magnitude
                float intensity = spectrumHistory[t][f] / 100.0f;
                glColor3f(intensity, intensity * 0.5f, 1.0f - intensity);
                
                glVertex3f(x, y, z);
            }
            
            glEnd();
        }
    }
};
```

## 3. Performance Optimization Strategies

### 3.1 Multi-Threading Architecture

```cpp
class VisualizationManager {
    std::thread renderThread;
    std::atomic<bool> shouldExit{false};
    
    // Triple buffering for smooth updates
    struct VisualizationData {
        std::vector<float> waveform;
        std::vector<float> spectrum;
        std::atomic<bool> ready{false};
    };
    
    std::array<VisualizationData, 3> buffers;
    std::atomic<int> writeIndex{0};
    std::atomic<int> readIndex{1};
    std::atomic<int> processIndex{2};
    
    void audioThreadUpdate(const AudioBuffer<float>& audio) {
        auto& buffer = buffers[writeIndex.load()];
        
        // Process audio data
        processWaveform(audio, buffer.waveform);
        processSpectrum(audio, buffer.spectrum);
        
        buffer.ready = true;
        
        // Swap buffers atomically
        int expected = processIndex.load();
        writeIndex.compare_exchange_strong(expected, writeIndex.load());
        processIndex.store(expected);
    }
    
    void renderThreadFunc() {
        while (!shouldExit) {
            if (buffers[processIndex.load()].ready) {
                // Swap read and process buffers
                int temp = readIndex.load();
                readIndex.store(processIndex.load());
                processIndex.store(temp);
                
                buffers[processIndex.load()].ready = false;
                
                // Trigger repaint on message thread
                MessageManager::callAsync([this] {
                    repaint();
                });
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }
};
```

### 3.2 GPU Acceleration with OpenGL

```cpp
class GPUAcceleratedWaveform : public Component, private OpenGLRenderer {
    OpenGLContext openGLContext;
    std::unique_ptr<OpenGLShaderProgram> shader;
    
    const char* vertexShader = R"(
        attribute vec2 position;
        uniform mat4 projectionMatrix;
        
        void main() {
            gl_Position = projectionMatrix * vec4(position, 0.0, 1.0);
        }
    )";
    
    const char* fragmentShader = R"(
        uniform vec4 colour;
        
        void main() {
            gl_FragColor = colour;
        }
    )";
    
    void initialise() override {
        shader.reset(new OpenGLShaderProgram(openGLContext));
        
        if (!shader->addVertexShader(vertexShader) ||
            !shader->addFragmentShader(fragmentShader) ||
            !shader->link()) {
            shader.reset();
        }
    }
    
    void renderOpenGL() override {
        if (!shader) return;
        
        shader->use();
        
        // Upload waveform data as vertex buffer
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, 
                     waveformPoints.size() * sizeof(Point<float>),
                     waveformPoints.data(), GL_DYNAMIC_DRAW);
        
        // Draw
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glDrawArrays(GL_LINE_STRIP, 0, waveformPoints.size());
        
        glDeleteBuffers(1, &vbo);
    }
};
```

### 3.3 Memory Pool Optimization

```cpp
class AudioVisualizationPool {
    struct Block {
        alignas(16) std::array<float, 4096> data;
        std::atomic<bool> inUse{false};
    };
    
    std::array<Block, 16> pool;
    
public:
    float* acquire(size_t size) {
        for (auto& block : pool) {
            bool expected = false;
            if (block.inUse.compare_exchange_strong(expected, true)) {
                return block.data.data();
            }
        }
        return nullptr; // Pool exhausted
    }
    
    void release(float* ptr) {
        for (auto& block : pool) {
            if (block.data.data() == ptr) {
                block.inUse = false;
                return;
            }
        }
    }
};
```

## 4. Real-Time Visualization Best Practices

### 4.1 Decoupling Audio and UI Threads

1. **Never allocate memory** in audio callback
2. **Use lock-free data structures** for communication
3. **Batch updates** to reduce overhead
4. **Use fixed-size buffers** to avoid allocation

### 4.2 Frame Rate Management

```cpp
class AdaptiveFrameRate : public Timer {
    std::chrono::high_resolution_clock::time_point lastFrame;
    double targetFrameTime = 1.0 / 60.0; // 60 FPS target
    double currentFrameTime = 0.0;
    
    void timerCallback() override {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(now - lastFrame).count();
        
        if (elapsed < targetFrameTime * 0.9) {
            // Running too fast, reduce frame rate
            stopTimer();
            startTimer(int(targetFrameTime * 1100)); // Add 10% margin
        } else if (elapsed > targetFrameTime * 1.5) {
            // Running too slow, reduce quality or skip frames
            // Implement quality reduction logic
        }
        
        lastFrame = now;
        repaint();
    }
};
```

### 4.3 Quality Scaling

```cpp
enum class QualityLevel {
    Low,    // Basic waveform, 15 FPS
    Medium, // Peak waveform, 30 FPS
    High,   // Anti-aliased, 60 FPS
    Ultra   // GPU accelerated, 60+ FPS
};

class AdaptiveQualityRenderer {
    QualityLevel currentQuality = QualityLevel::High;
    
    void adjustQuality(double cpuUsage) {
        if (cpuUsage > 80.0 && currentQuality != QualityLevel::Low) {
            currentQuality = static_cast<QualityLevel>(
                static_cast<int>(currentQuality) - 1);
        } else if (cpuUsage < 50.0 && currentQuality != QualityLevel::Ultra) {
            currentQuality = static_cast<QualityLevel>(
                static_cast<int>(currentQuality) + 1);
        }
    }
};
```

## 5. Implementation Recommendations for QUIET

### 5.1 Recommended Architecture

```cpp
class QUIETVisualizationSystem {
    // Dual waveform displays (input/output)
    class DualWaveformDisplay : public Component {
        QUIETWaveformDisplay inputWaveform;
        QUIETWaveformDisplay outputWaveform;
        
        void resized() override {
            auto bounds = getLocalBounds();
            inputWaveform.setBounds(bounds.removeFromTop(bounds.getHeight() / 2));
            outputWaveform.setBounds(bounds);
        }
    };
    
    // Combined spectrum analyzer
    class ComparativeSpectrumAnalyzer : public Component {
        SpectrumAnalyzer inputSpectrum;
        SpectrumAnalyzer outputSpectrum;
        
        void paint(Graphics& g) override {
            // Draw both spectrums with transparency
            g.setColour(Colours::red.withAlpha(0.7f));
            inputSpectrum.drawSpectrum(g, getLocalBounds());
            
            g.setColour(Colours::green.withAlpha(0.7f));
            outputSpectrum.drawSpectrum(g, getLocalBounds());
        }
    };
};
```

### 5.2 Performance Targets

- **Waveform Update Rate**: 30-60 FPS
- **Spectrum Update Rate**: 20-30 FPS
- **Latency**: < 50ms visual delay
- **CPU Usage**: < 5% for visualization
- **Memory Usage**: < 50MB for buffers

### 5.3 JUCE Integration Example

```cpp
class QUIETMainComponent : public AudioAppComponent {
    DualWaveformDisplay waveforms;
    ComparativeSpectrumAnalyzer spectrum;
    AudioVisualizationPool memoryPool;
    
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override {
        // Initialize visualization components
        waveforms.setSampleRate(sampleRate);
        spectrum.setSampleRate(sampleRate);
    }
    
    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override {
        // Process audio with noise reduction
        // ...
        
        // Update visualizations (non-blocking)
        if (auto* buffer = memoryPool.acquire(bufferToFill.numSamples)) {
            // Copy data for visualization
            std::memcpy(buffer, bufferToFill.buffer->getReadPointer(0), 
                       bufferToFill.numSamples * sizeof(float));
            
            // Post to visualization thread
            MessageManager::callAsync([this, buffer, numSamples = bufferToFill.numSamples] {
                waveforms.pushBuffer(buffer, numSamples);
                spectrum.processBlock(buffer, numSamples);
                memoryPool.release(buffer);
            });
        }
    }
};
```

## Conclusion

For the QUIET project, the recommended approach combines:

1. **JUCE's built-in AudioVisualiserComponent** for basic waveform display
2. **Custom FFT-based spectrum analyzer** for frequency visualization
3. **Lock-free ring buffers** for thread-safe communication
4. **Adaptive quality rendering** to maintain performance
5. **GPU acceleration** for high-quality visualization modes

This approach provides professional-quality visualization while maintaining the low-latency requirements essential for real-time audio processing.