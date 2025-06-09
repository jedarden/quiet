# Test Strategy - QUIET Application

## 1. Testing Philosophy

### TDD London School Approach
- **Test First**: Write tests before implementation
- **Outside-In**: Start from user-facing features, work inward
- **Mock Dependencies**: Isolate units under test
- **Behavior Focus**: Test behavior, not implementation
- **Fast Feedback**: Tests must run quickly

## 2. Test Architecture

### 2.1 Test Pyramid
```
         /\
        /  \    E2E Tests (5%)
       /____\   - User workflows
      /      \  - System integration
     /________\ Integration Tests (15%)
    /          \ - Component interaction
   /____________\- Platform-specific
  /              \ Unit Tests (80%)
 /________________\- Business logic
                   - Algorithms
                   - Data structures
```

### 2.2 Test Categories

#### Unit Tests
- **Scope**: Individual classes and functions
- **Speed**: <1ms per test
- **Dependencies**: All mocked
- **Coverage Target**: 80%+

#### Integration Tests
- **Scope**: Component interactions
- **Speed**: <100ms per test
- **Dependencies**: Real where necessary
- **Focus**: Audio pipeline, device integration

#### End-to-End Tests
- **Scope**: Complete user workflows
- **Speed**: <5s per test
- **Dependencies**: Full system
- **Focus**: Critical paths only

## 3. Unit Test Strategy

### 3.1 Audio Processing Tests

```cpp
// Test: Noise reduction improves SNR
TEST(NoiseReductionTest, ImprovesSNR) {
    // Arrange
    auto processor = createTestProcessor();
    auto testSignal = generateSineWave(1000.0f, 48000);
    auto noise = generateWhiteNoise(0.1f, 48000);
    auto noisySignal = mixSignals(testSignal, noise);
    
    // Act
    auto processed = processor->process(noisySignal);
    
    // Assert
    float snrBefore = calculateSNR(testSignal, noisySignal);
    float snrAfter = calculateSNR(testSignal, processed);
    EXPECT_GT(snrAfter - snrBefore, 10.0f); // >10dB improvement
}

// Test: Processing latency within bounds
TEST(NoiseReductionTest, MeetsLatencyRequirement) {
    // Arrange
    auto processor = createTestProcessor();
    auto testBuffer = createTestBuffer(256, 48000);
    
    // Act
    auto start = getHighResTime();
    processor->process(testBuffer);
    auto elapsed = getHighResTime() - start;
    
    // Assert
    EXPECT_LT(elapsed, 30.0); // <30ms
}

// Test: Handles edge cases gracefully
TEST(NoiseReductionTest, HandlesEmptyBuffer) {
    // Arrange
    auto processor = createTestProcessor();
    auto emptyBuffer = AudioBuffer(2, 0);
    
    // Act & Assert
    EXPECT_NO_THROW(processor->process(emptyBuffer));
}
```

### 3.2 Device Management Tests

```cpp
// Test: Device enumeration
TEST(AudioDeviceTest, EnumeratesDevices) {
    // Arrange
    auto mockDriver = std::make_unique<MockAudioDriver>();
    mockDriver->addDevice("Device1", "Microphone 1");
    mockDriver->addDevice("Device2", "Microphone 2");
    
    AudioDeviceManager manager(std::move(mockDriver));
    
    // Act
    auto devices = manager.getAvailableDevices();
    
    // Assert
    EXPECT_EQ(devices.size(), 2);
    EXPECT_EQ(devices[0].name, "Microphone 1");
    EXPECT_EQ(devices[1].name, "Microphone 2");
}

// Test: Device selection persistence
TEST(AudioDeviceTest, PersistsDeviceSelection) {
    // Arrange
    auto mockConfig = std::make_unique<MockConfigStore>();
    AudioDeviceManager manager(nullptr, std::move(mockConfig));
    
    // Act
    manager.selectDevice("device123");
    
    // Assert
    EXPECT_CALL(*mockConfig, saveString("audio.device_id", "device123"));
}

// Test: Hot-plug detection
TEST(AudioDeviceTest, DetectsHotPlug) {
    // Arrange
    auto mockDriver = std::make_unique<MockAudioDriver>();
    auto mockListener = std::make_unique<MockDeviceListener>();
    
    AudioDeviceManager manager(std::move(mockDriver));
    manager.addListener(mockListener.get());
    
    // Act
    mockDriver->simulateDeviceAdded("NewDevice", "USB Mic");
    
    // Assert
    EXPECT_CALL(*mockListener, onDeviceAdded(_));
}
```

### 3.3 Visualization Tests

```cpp
// Test: Waveform display updates
TEST(WaveformTest, UpdatesWithAudioData) {
    // Arrange
    WaveformDisplay display(800, 200);
    auto testBuffer = generateSineWave(440.0f, 48000, 0.1f);
    
    // Act
    display.updateBuffer(testBuffer);
    
    // Assert
    auto displayData = display.getDisplayBuffer();
    EXPECT_EQ(displayData.size(), 800); // One sample per pixel
    EXPECT_GT(displayData[400], 0.0f); // Peak in middle
}

// Test: Spectrum analyzer FFT
TEST(SpectrumTest, CalculatesCorrectFrequencies) {
    // Arrange
    SpectrumAnalyzer analyzer(2048);
    auto testSignal = generateSineWave(1000.0f, 48000, 1.0f);
    
    // Act
    analyzer.process(testSignal);
    auto spectrum = analyzer.getMagnitudeSpectrum();
    
    // Assert
    int expectedBin = 1000 * 2048 / 48000; // ~43
    float peakMagnitude = spectrum[expectedBin];
    
    // Verify 1kHz peak
    for (int i = 0; i < spectrum.size(); ++i) {
        if (abs(i - expectedBin) > 5) {
            EXPECT_LT(spectrum[i], peakMagnitude - 20.0f);
        }
    }
}
```

## 4. Integration Test Strategy

### 4.1 Audio Pipeline Tests

```cpp
// Test: End-to-end audio flow
TEST_F(AudioPipelineTest, ProcessesAudioEndToEnd) {
    // Setup
    auto app = createTestApplication();
    auto mockInput = createMockAudioInput();
    auto mockOutput = createMockAudioOutput();
    
    app->setAudioDevices(mockInput, mockOutput);
    
    // Generate test signal
    auto testSignal = generateTestSignal();
    mockInput->queueAudioData(testSignal);
    
    // Process
    app->enableNoiseReduction(true);
    waitForProcessing();
    
    // Verify
    auto outputData = mockOutput->getCapturedData();
    ASSERT_EQ(outputData.size(), testSignal.size());
    
    // Check noise reduction applied
    float inputNoise = calculateNoiseLevel(testSignal);
    float outputNoise = calculateNoiseLevel(outputData);
    EXPECT_LT(outputNoise, inputNoise * 0.5f);
}

// Test: Virtual device routing
TEST_F(VirtualDeviceTest, RoutesToVirtualDevice) {
    // Platform-specific setup
    #ifdef _WIN32
        auto virtualDevice = createVBCableMock();
    #else
        auto virtualDevice = createBlackHoleMock();
    #endif
    
    VirtualDeviceRouter router;
    router.initialize(virtualDevice);
    
    // Send audio
    auto testAudio = generateTestAudio();
    router.routeAudio(testAudio);
    
    // Verify
    auto captured = virtualDevice->getCapturedAudio();
    EXPECT_EQ(captured.size(), testAudio.size());
    EXPECT_FLOAT_ARRAY_EQ(captured, testAudio, 0.001f);
}
```

### 4.2 UI Integration Tests

```cpp
// Test: Device selection updates audio engine
TEST_F(UIIntegrationTest, DeviceSelectionUpdatesEngine) {
    // Setup
    auto app = createTestApplication();
    auto ui = app->getMainWindow();
    
    // Simulate device selection
    ui->selectDevice("TestDevice123");
    
    // Verify
    auto currentDevice = app->getAudioEngine()->getCurrentDevice();
    EXPECT_EQ(currentDevice.id, "TestDevice123");
}

// Test: Enable toggle affects processing
TEST_F(UIIntegrationTest, ToggleEnablesProcessing) {
    // Setup
    auto app = createTestApplication();
    auto ui = app->getMainWindow();
    
    // Initially disabled
    EXPECT_FALSE(app->isNoiseReductionEnabled());
    
    // Toggle on
    ui->clickEnableToggle();
    
    // Verify
    EXPECT_TRUE(app->isNoiseReductionEnabled());
}
```

## 5. Performance Test Strategy

### 5.1 Latency Tests

```cpp
class LatencyTest : public PerformanceTest {
    void runLatencyProfile() {
        auto processor = createProcessor();
        std::vector<double> latencies;
        
        // Warm up
        for (int i = 0; i < 100; ++i) {
            auto buffer = createTestBuffer(256);
            processor->process(buffer);
        }
        
        // Measure
        for (int i = 0; i < 1000; ++i) {
            auto buffer = createTestBuffer(256);
            
            auto start = getHighResTime();
            processor->process(buffer);
            auto elapsed = getHighResTime() - start;
            
            latencies.push_back(elapsed);
        }
        
        // Analyze
        auto stats = calculateStats(latencies);
        EXPECT_LT(stats.p95, 20.0); // 95th percentile < 20ms
        EXPECT_LT(stats.p99, 30.0); // 99th percentile < 30ms
        EXPECT_LT(stats.max, 50.0); // Max < 50ms
    }
};
```

### 5.2 CPU Usage Tests

```cpp
class CPUUsageTest : public PerformanceTest {
    void runCPUProfile() {
        auto app = createApplication();
        app->enableNoiseReduction(true);
        
        // Monitor CPU for 60 seconds
        CPUMonitor monitor;
        monitor.start();
        
        // Simulate real usage
        simulateAudioStream(60.0); // 60 seconds
        
        auto cpuStats = monitor.getStats();
        
        // Verify
        EXPECT_LT(cpuStats.average, 15.0); // <15% average
        EXPECT_LT(cpuStats.peak, 25.0);    // <25% peak
    }
};
```

### 5.3 Memory Tests

```cpp
class MemoryTest : public PerformanceTest {
    void runMemoryProfile() {
        // Baseline
        auto baseline = getCurrentMemoryUsage();
        
        // Create application
        auto app = createApplication();
        auto afterInit = getCurrentMemoryUsage();
        
        // Run for extended period
        simulateUsage(3600); // 1 hour
        auto afterRun = getCurrentMemoryUsage();
        
        // Verify
        auto initMemory = afterInit - baseline;
        auto leakage = afterRun - afterInit;
        
        EXPECT_LT(initMemory, 200 * 1024 * 1024); // <200MB
        EXPECT_LT(leakage, 10 * 1024 * 1024);     // <10MB leak
    }
};
```

## 6. Platform-Specific Tests

### 6.1 Windows Tests

```cpp
#ifdef _WIN32
TEST(WindowsPlatformTest, WASAPIDeviceEnumeration) {
    WASAPIDeviceEnumerator enumerator;
    auto devices = enumerator.getDevices();
    
    ASSERT_FALSE(devices.empty());
    
    for (const auto& device : devices) {
        EXPECT_FALSE(device.id.empty());
        EXPECT_FALSE(device.name.empty());
        EXPECT_GT(device.sampleRate, 0);
    }
}

TEST(WindowsPlatformTest, VBCableIntegration) {
    if (!VBCableDriver::isInstalled()) {
        GTEST_SKIP() << "VB-Cable not installed";
    }
    
    VBCableIntegration vbcable;
    ASSERT_TRUE(vbcable.initialize());
    
    auto testAudio = generateTestAudio();
    EXPECT_TRUE(vbcable.sendAudio(testAudio));
}
#endif
```

### 6.2 macOS Tests

```cpp
#ifdef __APPLE__
TEST(MacPlatformTest, CoreAudioDeviceEnumeration) {
    CoreAudioDeviceEnumerator enumerator;
    auto devices = enumerator.getDevices();
    
    ASSERT_FALSE(devices.empty());
    
    // Should have at least built-in mic
    auto builtIn = std::find_if(devices.begin(), devices.end(),
        [](const auto& d) { return d.name.find("Built-in") != std::string::npos; });
    
    EXPECT_NE(builtIn, devices.end());
}

TEST(MacPlatformTest, BlackHoleIntegration) {
    if (!BlackHoleDriver::isInstalled()) {
        GTEST_SKIP() << "BlackHole not installed";
    }
    
    BlackHoleIntegration blackhole;
    ASSERT_TRUE(blackhole.initialize());
    
    auto testAudio = generateTestAudio();
    EXPECT_TRUE(blackhole.sendAudio(testAudio));
}
#endif
```

## 7. Test Utilities

### 7.1 Test Signal Generation

```cpp
class TestSignalGenerator {
public:
    static AudioBuffer generateSineWave(float frequency, int sampleRate, 
                                       float duration = 1.0f, float amplitude = 1.0f) {
        int numSamples = static_cast<int>(duration * sampleRate);
        AudioBuffer buffer(1, numSamples);
        
        for (int i = 0; i < numSamples; ++i) {
            float t = static_cast<float>(i) / sampleRate;
            buffer.setSample(0, i, amplitude * std::sin(2.0f * M_PI * frequency * t));
        }
        
        return buffer;
    }
    
    static AudioBuffer generateWhiteNoise(float amplitude, int numSamples) {
        AudioBuffer buffer(1, numSamples);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);
        
        for (int i = 0; i < numSamples; ++i) {
            buffer.setSample(0, i, dist(gen));
        }
        
        return buffer;
    }
    
    static AudioBuffer generateSpeechLikeSignal(int sampleRate, float duration) {
        // Combination of formants typical in speech
        auto f1 = generateSineWave(700.0f, sampleRate, duration, 0.5f);
        auto f2 = generateSineWave(1220.0f, sampleRate, duration, 0.3f);
        auto f3 = generateSineWave(2600.0f, sampleRate, duration, 0.2f);
        
        return mixSignals({f1, f2, f3});
    }
};
```

### 7.2 Mock Objects

```cpp
class MockAudioDevice : public IAudioDevice {
private:
    std::queue<AudioBuffer> inputQueue;
    std::vector<AudioBuffer> outputCapture;
    
public:
    void queueInput(const AudioBuffer& buffer) {
        inputQueue.push(buffer);
    }
    
    AudioBuffer readInput() override {
        if (!inputQueue.empty()) {
            auto buffer = inputQueue.front();
            inputQueue.pop();
            return buffer;
        }
        return AudioBuffer(2, 256); // Empty buffer
    }
    
    void writeOutput(const AudioBuffer& buffer) override {
        outputCapture.push_back(buffer);
    }
    
    std::vector<AudioBuffer> getCapturedOutput() const {
        return outputCapture;
    }
};
```

## 8. Test Execution Strategy

### 8.1 Continuous Integration

```yaml
# CI Test Pipeline
test-pipeline:
  stages:
    - unit-tests:
        parallel: true
        timeout: 5m
        command: ctest -L "Unit" --output-on-failure
        
    - integration-tests:
        parallel: false
        timeout: 15m
        command: ctest -L "Integration" --output-on-failure
        
    - performance-tests:
        parallel: false
        timeout: 30m
        command: ctest -L "Performance" --output-on-failure
        environment:
          - PERFORMANCE_BASELINE: true
          
    - coverage-report:
        command: |
          lcov --capture --directory . --output-file coverage.info
          lcov --remove coverage.info '/usr/*' --output-file coverage.info
          lcov --list coverage.info
        minimum-coverage: 80%
```

### 8.2 Test Organization

```
tests/
├── unit/
│   ├── audio/
│   │   ├── AudioBufferTest.cpp
│   │   ├── NoiseReductionTest.cpp
│   │   └── ResamplerTest.cpp
│   ├── devices/
│   │   ├── DeviceManagerTest.cpp
│   │   └── VirtualDeviceTest.cpp
│   └── ui/
│       ├── WaveformTest.cpp
│       └── SpectrumTest.cpp
├── integration/
│   ├── AudioPipelineTest.cpp
│   ├── DeviceIntegrationTest.cpp
│   └── UIIntegrationTest.cpp
├── performance/
│   ├── LatencyTest.cpp
│   ├── CPUUsageTest.cpp
│   └── MemoryTest.cpp
└── utils/
    ├── TestSignalGenerator.h
    ├── MockObjects.h
    └── TestUtilities.h
```

## 9. Test Metrics and Reporting

### 9.1 Key Metrics
- **Code Coverage**: Minimum 80% line coverage
- **Test Execution Time**: <30 seconds for unit tests
- **Test Stability**: 0% flakiness tolerance
- **Performance Regression**: <5% tolerance

### 9.2 Test Reports
- Daily test execution summary
- Weekly performance trend analysis
- Coverage reports with uncovered code highlighting
- Failed test analysis with root cause

## Conclusion

This comprehensive test strategy ensures QUIET meets all quality requirements through systematic testing at every level. The TDD approach guarantees testable design, while the multi-layered test architecture provides confidence in both functionality and performance.