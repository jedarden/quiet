# QUIET - System Architecture Pseudocode

## 1. Main Application Controller

```pseudocode
class QuietApplication:
    audioEngine: AudioEngine
    uiManager: UIManager
    settingsManager: SettingsManager
    virtualDeviceManager: VirtualDeviceManager
    
    function initialize():
        // Initialize subsystems
        settingsManager = new SettingsManager()
        settingsManager.loadUserPreferences()
        
        // Platform-specific virtual device setup
        if platform == WINDOWS:
            virtualDeviceManager = new WASAPIVirtualDevice()
        else if platform == MACOS:
            virtualDeviceManager = new CoreAudioVirtualDevice()
        
        virtualDeviceManager.createVirtualMicrophone("QUIET Virtual Mic")
        
        // Initialize audio engine
        audioEngine = new AudioEngine(settingsManager)
        audioEngine.setOutputDevice(virtualDeviceManager.getDeviceId())
        
        // Initialize UI
        uiManager = new UIManager()
        uiManager.onDeviceSelected = audioEngine.setInputDevice
        uiManager.onToggleDenoiser = audioEngine.toggleProcessing
        uiManager.show()
        
    function run():
        while not shouldExit:
            // Main event loop
            uiManager.processEvents()
            
            // Update visualizations
            if audioEngine.isProcessing():
                inputData = audioEngine.getInputVisualizationData()
                outputData = audioEngine.getOutputVisualizationData()
                uiManager.updateVisualizations(inputData, outputData)
                
            sleep(16) // ~60 FPS update rate
```

## 2. Audio Engine Architecture

```pseudocode
class AudioEngine:
    captureThread: Thread
    processingThread: Thread
    outputThread: Thread
    
    inputRingBuffer: LockFreeRingBuffer
    outputRingBuffer: LockFreeRingBuffer
    
    noiseCanceller: NoiseCancellationModule
    deviceManager: AudioDeviceManager
    
    isProcessingEnabled: AtomicBool
    
    function initialize(settings):
        // Setup audio subsystem
        deviceManager = new AudioDeviceManager()
        deviceManager.initialize()
        
        // Setup ML model
        noiseCanceller = new NoiseCancellationModule()
        noiseCanceller.loadModel("models/dccrn_quantized.onnx")
        
        // Setup buffers (10ms frames at 48kHz = 480 samples)
        inputRingBuffer = new LockFreeRingBuffer(48000) // 1 second buffer
        outputRingBuffer = new LockFreeRingBuffer(48000)
        
        // Start threads
        captureThread = new Thread(captureAudioLoop)
        processingThread = new Thread(processAudioLoop)
        outputThread = new Thread(outputAudioLoop)
        
    function captureAudioLoop():
        inputDevice = deviceManager.getInputDevice()
        
        while running:
            // Capture audio frame
            audioFrame = inputDevice.readFrame(480) // 10ms at 48kHz
            
            // Write to ring buffer
            if not inputRingBuffer.write(audioFrame):
                logWarning("Input buffer overflow")
                
    function processAudioLoop():
        resampler16k = new Resampler(48000, 16000)
        resampler48k = new Resampler(16000, 48000)
        
        while running:
            // Check if we have enough samples
            if inputRingBuffer.availableRead() >= 480:
                // Read 10ms frame
                frame48k = inputRingBuffer.read(480)
                
                if isProcessingEnabled:
                    // Downsample to 16kHz for processing
                    frame16k = resampler16k.process(frame48k)
                    
                    // Apply noise cancellation
                    processed16k = noiseCanceller.process(frame16k)
                    
                    // Upsample back to 48kHz
                    processed48k = resampler48k.process(processed16k)
                    
                    outputRingBuffer.write(processed48k)
                else:
                    // Passthrough mode
                    outputRingBuffer.write(frame48k)
            else:
                sleep(1) // Wait for more data
                
    function outputAudioLoop():
        outputDevice = virtualDeviceManager.getOutputStream()
        
        while running:
            if outputRingBuffer.availableRead() >= 480:
                frame = outputRingBuffer.read(480)
                outputDevice.write(frame)
            else:
                // Output silence to prevent dropout
                outputDevice.write(silence(480))
```

## 3. Noise Cancellation Module

```pseudocode
class NoiseCancellationModule:
    onnxSession: ONNXRuntimeSession
    
    // DCCRN parameters
    frameSize: int = 512
    hopSize: int = 160  // 10ms at 16kHz
    
    // Buffers for overlap-add
    inputBuffer: CircularBuffer
    outputBuffer: CircularBuffer
    
    // STFT state
    fftProcessor: FFTProcessor
    window: Array<float>
    
    function loadModel(modelPath):
        onnxSession = new ONNXRuntimeSession(modelPath)
        onnxSession.setOptimizationLevel(ORT_ENABLE_ALL)
        onnxSession.setExecutionMode(ORT_SEQUENTIAL)
        
        // Initialize STFT
        fftProcessor = new FFTProcessor(frameSize)
        window = createHannWindow(frameSize)
        
        inputBuffer = new CircularBuffer(frameSize)
        outputBuffer = new CircularBuffer(frameSize)
        
    function process(audioFrame):
        // Add new samples to input buffer
        inputBuffer.push(audioFrame)
        
        processedSamples = []
        
        // Process with overlap
        while inputBuffer.available() >= frameSize:
            // Extract frame
            frame = inputBuffer.read(frameSize)
            
            // Apply window
            windowedFrame = frame * window
            
            // STFT
            spectrum = fftProcessor.forward(windowedFrame)
            magnitude = abs(spectrum)
            phase = angle(spectrum)
            
            // Prepare input for neural network
            // DCCRN expects [batch, channels, freq_bins, time_frames]
            nnInput = prepareNetworkInput(magnitude, phase)
            
            // Run inference
            nnOutput = onnxSession.run({
                "input": nnInput
            })
            
            // Extract enhanced magnitude and phase
            enhancedMag = nnOutput["magnitude"]
            enhancedPhase = nnOutput["phase"]
            
            // Reconstruct complex spectrum
            enhancedSpectrum = enhancedMag * exp(j * enhancedPhase)
            
            // iSTFT
            enhancedFrame = fftProcessor.inverse(enhancedSpectrum)
            
            // Apply window and overlap-add
            enhancedFrame *= window
            outputBuffer.overlapAdd(enhancedFrame, hopSize)
            
            // Collect processed samples
            processedSamples.append(outputBuffer.read(hopSize))
            
            // Hop to next frame
            inputBuffer.advance(hopSize)
            
        return concatenate(processedSamples)
```

## 4. UI Manager

```pseudocode
class UIManager:
    mainWindow: MainWindow
    deviceSelector: DeviceSelector
    visualizer: AudioVisualizer
    controls: ControlPanel
    
    // Callbacks
    onDeviceSelected: Function
    onToggleDenoiser: Function
    
    function initialize():
        mainWindow = new MainWindow("QUIET - Noise Cancellation")
        
        // Create UI components
        deviceSelector = new DeviceSelector()
        deviceSelector.onChange = handleDeviceSelection
        
        controls = new ControlPanel()
        controls.addToggleButton("Enable Denoiser", handleToggleDenoiser)
        controls.addSlider("Sensitivity", 0, 100, 70)
        
        visualizer = new AudioVisualizer()
        visualizer.setChannels(["Input", "Output"])
        
        // Layout
        layout = new VerticalLayout()
        layout.add(deviceSelector)
        layout.add(controls)
        layout.add(visualizer)
        
        mainWindow.setLayout(layout)
        
    function updateVisualizations(inputData, outputData):
        // Update waveforms at 30-60 FPS
        visualizer.updateChannel(0, inputData.waveform)
        visualizer.updateChannel(1, outputData.waveform)
        
        // Update level meters
        controls.updateLevel("input", inputData.rms)
        controls.updateLevel("output", outputData.rms)
        
        // Update performance metrics
        controls.updateMetric("CPU", getCPUUsage())
        controls.updateMetric("Latency", getProcessingLatency())
```

## 5. Virtual Device Manager

```pseudocode
class VirtualDeviceManager:
    deviceHandle: VirtualAudioDevice
    
    function createVirtualMicrophone(name):
        if platform == WINDOWS:
            // Use Windows Audio Session API
            deviceHandle = new WASAPIVirtualDevice()
            deviceHandle.initialize({
                name: name,
                sampleRate: 48000,
                channels: 1,
                format: FLOAT32
            })
        else if platform == MACOS:
            // Use Core Audio HAL plugin
            deviceHandle = new CoreAudioVirtualDevice()
            deviceHandle.initialize({
                name: name,
                sampleRate: 48000,
                channels: 1,
                manufacturer: "QUIET"
            })
            
    function getOutputStream():
        return deviceHandle.getOutputStream()
        
    function destroy():
        deviceHandle.unregister()
```

## 6. Test Strategy Pseudocode

```pseudocode
class AudioPipelineTest:
    function testEndToEndLatency():
        // Generate test tone
        testSignal = generateSineWave(1000, 0.1) // 1kHz, 100ms
        
        // Measure processing time
        startTime = getCurrentTime()
        
        // Process through pipeline
        processed = audioEngine.processTestSignal(testSignal)
        
        endTime = getCurrentTime()
        latency = endTime - startTime
        
        assert(latency < 20) // Must be under 20ms
        
    function testNoiseReduction():
        // Load test samples
        cleanSpeech = loadAudio("test/clean_speech.wav")
        noisySpeech = loadAudio("test/noisy_speech.wav")
        
        // Process noisy speech
        processed = noiseCanceller.process(noisySpeech)
        
        // Calculate metrics
        snrImprovement = calculateSNR(processed) - calculateSNR(noisySpeech)
        pesqScore = calculatePESQ(cleanSpeech, processed)
        
        assert(snrImprovement > 10) // At least 10dB improvement
        assert(pesqScore > 3.0) // Good perceptual quality
        
    function testCPUUsage():
        // Start CPU monitoring
        cpuMonitor = new CPUMonitor()
        cpuMonitor.start()
        
        // Run processing for 60 seconds
        audioEngine.startProcessing()
        sleep(60000)
        audioEngine.stopProcessing()
        
        avgCPU = cpuMonitor.getAverageCPUUsage()
        assert(avgCPU < 10) // Less than 10% CPU usage
```

## Key Design Decisions

1. **Lock-free Ring Buffers**: Prevent audio glitches from thread contention
2. **Triple Buffering**: Smooth visualization without impacting audio
3. **Overlap-Add Processing**: Seamless frame-based processing
4. **Platform Abstraction**: Clean separation of platform-specific code
5. **ONNX Runtime**: Cross-platform ML inference
6. **Modular Architecture**: Easy to swap noise cancellation algorithms

## Performance Optimizations

1. **SIMD Operations**: Use AVX2/NEON for DSP operations
2. **Memory Pool**: Pre-allocate audio buffers to avoid allocation in audio thread
3. **CPU Affinity**: Pin audio threads to specific cores
4. **Compiler Optimizations**: -O3 with profile-guided optimization
5. **Quantized Models**: INT8 quantization for faster inference