# QUIET - Algorithm Design Document

## 1. Overview

This document details the core algorithms used in QUIET for real-time noise cancellation. Each algorithm is designed for optimal performance, minimal latency, and lock-free operation where required.

## 2. Core Audio Processing Algorithms

### 2.1 Real-Time Audio Pipeline Algorithm

```pseudocode
algorithm RealtimeAudioPipeline:
    // Main audio processing pipeline with lock-free communication
    
    constants:
        BUFFER_SIZE = 256       // ~5.3ms at 48kHz
        PROCESS_PRIORITY = 95   // Near real-time priority
        
    state:
        inputBuffer: LockFreeRingBuffer
        outputBuffer: LockFreeRingBuffer
        processingEnabled: atomic<bool>
        dropoutCount: atomic<int>
        
    function processAudioCallback(input: float*, output: float*, frameCount: int):
        // This runs in the audio thread - must be lock-free
        startTime = getHighResolutionTime()
        
        // Try to write input to ring buffer
        if !inputBuffer.tryWrite(input, frameCount):
            dropoutCount.increment()
            // Continue processing even if we drop input
            
        // Process if we have enough data
        if outputBuffer.availableFrames() >= frameCount:
            outputBuffer.read(output, frameCount)
        else:
            // Output silence to prevent glitches
            fillWithSilence(output, frameCount)
            dropoutCount.increment()
            
        // Measure processing time
        elapsedTime = getHighResolutionTime() - startTime
        if elapsedTime > BUFFER_SIZE / 48000.0:
            // Log performance warning (lock-free)
            performanceWarnings.increment()
            
    function processingThread():
        // Separate thread for heavy processing
        setThreadPriority(PROCESS_PRIORITY)
        
        while processingEnabled.load():
            if inputBuffer.availableFrames() >= BUFFER_SIZE:
                // Read input data
                tempBuffer = allocaAudioBuffer(BUFFER_SIZE)
                inputBuffer.read(tempBuffer, BUFFER_SIZE)
                
                // Apply noise reduction
                processedBuffer = noiseProcessor.process(tempBuffer)
                
                // Write to output buffer
                if !outputBuffer.tryWrite(processedBuffer, BUFFER_SIZE):
                    // Output buffer full - skip this block
                    outputOverruns.increment()
            else:
                // No data available - yield CPU
                yieldThread()
```

### 2.2 Lock-Free Ring Buffer Algorithm

```pseudocode
ALGORITHM LockFreeRingBuffer:
    CONSTANTS:
        BUFFER_SIZE = 8192  // Power of 2 for efficient modulo
        CACHE_LINE_SIZE = 64
        
    DATA:
        buffer: Array[Float] aligned to CACHE_LINE_SIZE
        writePos: Atomic<Integer> = 0
        readPos: Atomic<Integer> = 0
        
    FUNCTION write(data: Array[Float], numSamples: Integer) -> Boolean:
        currentWrite = writePos.load(memory_order_acquire)
        currentRead = readPos.load(memory_order_acquire)
        
        // Calculate available space
        available = BUFFER_SIZE - (currentWrite - currentRead)
        IF available < numSamples:
            RETURN false  // Buffer full
        
        // Write data (may wrap around)
        writeIndex = currentWrite % BUFFER_SIZE
        firstPart = MIN(numSamples, BUFFER_SIZE - writeIndex)
        secondPart = numSamples - firstPart
        
        MEMCPY(buffer + writeIndex, data, firstPart * sizeof(Float))
        IF secondPart > 0:
            MEMCPY(buffer, data + firstPart, secondPart * sizeof(Float))
        
        // Update write position atomically
        writePos.store(currentWrite + numSamples, memory_order_release)
        RETURN true
    
    FUNCTION read(output: Array[Float], numSamples: Integer) -> Boolean:
        currentWrite = writePos.load(memory_order_acquire)
        currentRead = readPos.load(memory_order_acquire)
        
        // Check available data
        available = currentWrite - currentRead
        IF available < numSamples:
            RETURN false  // Not enough data
        
        // Read data (may wrap around)
        readIndex = currentRead % BUFFER_SIZE
        firstPart = MIN(numSamples, BUFFER_SIZE - readIndex)
        secondPart = numSamples - firstPart
        
        MEMCPY(output, buffer + readIndex, firstPart * sizeof(Float))
        IF secondPart > 0:
            MEMCPY(output + firstPart, buffer, secondPart * sizeof(Float))
        
        // Update read position atomically
        readPos.store(currentRead + numSamples, memory_order_release)
        RETURN true
```

### 2.3 RNNoise Integration Algorithm

```pseudocode
algorithm RNNoiseProcessor:
    // Integrates RNNoise with proper buffering and resampling
    
    constants:
        RNNOISE_SAMPLE_RATE = 48000
        RNNOISE_FRAME_SIZE = 480    // 10ms frames
        RNNOISE_CHANNELS = 1        // Mono processing
        
    state:
        denoiser: RNNoiseState*
        inputResampler: Resampler
        outputResampler: Resampler
        frameBuffer: float[RNNOISE_FRAME_SIZE]
        residualBuffer: CircularBuffer
        
    function initialize(inputSampleRate: int):
        denoiser = rnnoise_create()
        
        if inputSampleRate != RNNOISE_SAMPLE_RATE:
            inputResampler = Resampler(inputSampleRate, RNNOISE_SAMPLE_RATE)
            outputResampler = Resampler(RNNOISE_SAMPLE_RATE, inputSampleRate)
            
        residualBuffer = CircularBuffer(RNNOISE_FRAME_SIZE * 2)
        
    function process(input: AudioBuffer) -> AudioBuffer:
        // Handle resampling if needed
        workBuffer = input
        if inputResampler != null:
            workBuffer = inputResampler.process(input)
            
        // Add to residual buffer
        residualBuffer.write(workBuffer.data, workBuffer.frameCount)
        
        // Process all complete frames
        outputBuffer = AudioBuffer(input.frameCount)
        outputIndex = 0
        
        while residualBuffer.available() >= RNNOISE_FRAME_SIZE:
            // Extract one frame
            residualBuffer.read(frameBuffer, RNNOISE_FRAME_SIZE)
            
            // Convert to float and normalize
            floatFrame = new float[RNNOISE_FRAME_SIZE]
            for i = 0 to RNNOISE_FRAME_SIZE:
                floatFrame[i] = frameBuffer[i] / 32768.0
                
            // Apply RNNoise
            rnnoise_process_frame(denoiser, floatFrame, floatFrame)
            
            // Convert back to samples
            for i = 0 to RNNOISE_FRAME_SIZE:
                frameBuffer[i] = clamp(floatFrame[i] * 32768.0, -32768, 32767)
                
            // Add to output
            copyToOutput(frameBuffer, outputBuffer, outputIndex)
            outputIndex += RNNOISE_FRAME_SIZE
            
        // Resample back if needed
        if outputResampler != null:
            outputBuffer = outputResampler.process(outputBuffer)
            
        return outputBuffer
        
    function reset():
        rnnoise_destroy(denoiser)
        denoiser = rnnoise_create()
        residualBuffer.clear()
```

```pseudocode
ALGORITHM NoiseReductionProcessor:
    CONSTANTS:
        RNNOISE_FRAME_SIZE = 480  // 10ms at 48kHz
        TARGET_SAMPLE_RATE = 48000
        
    DATA:
        denoiserState: RNNoiseState*
        inputResampler: Resampler
        outputResampler: Resampler
        frameBuffer: Array[Float, RNNOISE_FRAME_SIZE]
        overlapBuffer: Array[Float, RNNOISE_FRAME_SIZE/2]
        
    FUNCTION initialize(inputSampleRate: Integer):
        denoiserState = rnnoise_create(NULL)
        
        IF inputSampleRate != TARGET_SAMPLE_RATE:
            inputResampler = createResampler(inputSampleRate, TARGET_SAMPLE_RATE)
            outputResampler = createResampler(TARGET_SAMPLE_RATE, inputSampleRate)
        
        // Initialize overlap buffer for smooth transitions
        FILL(overlapBuffer, 0.0)
    
    FUNCTION processBlock(input: AudioBuffer, output: AudioBuffer):
        // Handle sample rate conversion if needed
        workingBuffer = input
        IF inputResampler != NULL:
            workingBuffer = inputResampler.process(input)
        
        numFrames = workingBuffer.size / RNNOISE_FRAME_SIZE
        
        FOR frameIdx = 0 TO numFrames:
            // Extract frame with overlap
            frameStart = frameIdx * RNNOISE_FRAME_SIZE
            
            // Copy frame data
            FOR i = 0 TO RNNOISE_FRAME_SIZE:
                frameBuffer[i] = workingBuffer[frameStart + i]
            
            // Apply windowing for smooth transitions
            applyWindow(frameBuffer, WINDOW_HANN)
            
            // Process with RNNoise
            rnnoise_process_frame(denoiserState, frameBuffer, frameBuffer)
            
            // Apply inverse window
            applyWindow(frameBuffer, WINDOW_HANN_INVERSE)
            
            // Overlap-add for smooth output
            FOR i = 0 TO RNNOISE_FRAME_SIZE/2:
                workingBuffer[frameStart + i] = 
                    frameBuffer[i] + overlapBuffer[i]
            
            // Save overlap for next frame
            FOR i = 0 TO RNNOISE_FRAME_SIZE/2:
                overlapBuffer[i] = frameBuffer[RNNOISE_FRAME_SIZE/2 + i]
            
            // Copy non-overlapped part
            FOR i = RNNOISE_FRAME_SIZE/2 TO RNNOISE_FRAME_SIZE:
                workingBuffer[frameStart + i] = frameBuffer[i]
        
        // Convert back to original sample rate if needed
        IF outputResampler != NULL:
            output = outputResampler.process(workingBuffer)
        ELSE:
            output = workingBuffer
```

### 2.4 Waveform Visualization Algorithm

```pseudocode
algorithm WaveformVisualizer:
    // Efficient waveform display with peak detection
    
    constants:
        DISPLAY_WIDTH = 800         // Pixels
        DISPLAY_TIME_WINDOW = 2.0   // Seconds
        UPDATE_RATE = 60            // FPS
        
    state:
        sampleBuffer: CircularBuffer
        peakBuffer: float[DISPLAY_WIDTH * 2]  // Min/max for each pixel
        displayMutex: Mutex
        lastUpdateTime: timestamp
        
    function pushAudioData(audio: AudioBuffer):
        // Add new audio data (called from audio thread)
        sampleBuffer.writeNonBlocking(audio.data, audio.frameCount)
        
    function updateDisplay():
        // Called from UI thread at UPDATE_RATE
        currentTime = getCurrentTime()
        if currentTime - lastUpdateTime < 1.0 / UPDATE_RATE:
            return  // Rate limit updates
            
        samplesPerPixel = (sampleRate * DISPLAY_TIME_WINDOW) / DISPLAY_WIDTH
        
        displayMutex.lock()
        
        for pixel = 0 to DISPLAY_WIDTH:
            sampleStart = pixel * samplesPerPixel
            sampleEnd = (pixel + 1) * samplesPerPixel
            
            // Find min/max in this pixel's sample range
            minValue = +1.0
            maxValue = -1.0
            
            for s = sampleStart to sampleEnd:
                sample = sampleBuffer.getSample(s)
                minValue = min(minValue, sample)
                maxValue = max(maxValue, sample)
                
            peakBuffer[pixel * 2] = minValue
            peakBuffer[pixel * 2 + 1] = maxValue
            
        displayMutex.unlock()
        lastUpdateTime = currentTime
        invalidateDisplay()
        
    function render(graphics: Graphics):
        displayMutex.lock()
        
        centerY = displayHeight / 2
        
        // Draw waveform using peak data
        graphics.beginPath()
        for pixel = 0 to DISPLAY_WIDTH:
            minY = centerY + peakBuffer[pixel * 2] * centerY
            maxY = centerY + peakBuffer[pixel * 2 + 1] * centerY
            
            if pixel == 0:
                graphics.moveTo(pixel, centerY)
                
            graphics.lineTo(pixel, minY)
            graphics.lineTo(pixel, maxY)
            
        graphics.stroke()
        
        displayMutex.unlock()
```

### 2.5 Spectrum Analyzer FFT Algorithm

```pseudocode
ALGORITHM AdaptiveQualityController:
    CONSTANTS:
        CPU_TARGET = 50.0  // Target 50% CPU usage
        QUALITY_LEVELS = ["Low", "Medium", "High"]
        CHECK_INTERVAL = 1000  // Check every second
        
    DATA:
        currentQuality: Integer = 2  // Start at High
        cpuMonitor: CPUMonitor
        lastCheckTime: Timestamp
        smoothedCPU: Float = 0.0
        
    FUNCTION update():
        currentTime = getCurrentTime()
        IF currentTime - lastCheckTime < CHECK_INTERVAL:
            RETURN  // Not time to check yet
        
        // Get current CPU usage with smoothing
        instantCPU = cpuMonitor.getCurrentUsage()
        smoothedCPU = 0.7 * smoothedCPU + 0.3 * instantCPU
        
        // Determine quality adjustment
        IF smoothedCPU > CPU_TARGET + 15:
            // CPU too high, reduce quality
            IF currentQuality > 0:
                currentQuality -= 1
                applyQualitySettings(currentQuality)
                notifyUser("Reduced quality to " + QUALITY_LEVELS[currentQuality])
        
        ELSE IF smoothedCPU < CPU_TARGET - 15:
            // CPU has headroom, increase quality
            IF currentQuality < 2:
                currentQuality += 1
                applyQualitySettings(currentQuality)
                notifyUser("Increased quality to " + QUALITY_LEVELS[currentQuality])
        
        lastCheckTime = currentTime
    
    FUNCTION applyQualitySettings(level: Integer):
        SWITCH level:
            CASE 0:  // Low
                NoiseReduction.setProcessingDepth(BASIC)
                Visualizations.setFrameRate(15)
                AudioBuffer.setSize(512)
                
            CASE 1:  // Medium
                NoiseReduction.setProcessingDepth(STANDARD)
                Visualizations.setFrameRate(30)
                AudioBuffer.setSize(256)
                
            CASE 2:  // High
                NoiseReduction.setProcessingDepth(MAXIMUM)
                Visualizations.setFrameRate(60)
                AudioBuffer.setSize(128)
```

### 1.4 Spectrum Analyzer FFT Algorithm

```pseudocode
ALGORITHM SpectrumAnalyzer:
    CONSTANTS:
        FFT_SIZE = 2048
        WINDOW_SIZE = 2048
        HOP_SIZE = 512
        SMOOTHING_FACTOR = 0.8
        
    DATA:
        fftProcessor: FFT
        window: Array[Float, WINDOW_SIZE]
        magnitudeSpectrum: Array[Float, FFT_SIZE/2]
        smoothedSpectrum: Array[Float, FFT_SIZE/2]
        
    FUNCTION initialize():
        fftProcessor = createFFT(FFT_SIZE)
        
        // Generate Hann window
        FOR i = 0 TO WINDOW_SIZE:
            window[i] = 0.5 * (1 - cos(2 * PI * i / (WINDOW_SIZE - 1)))
        
        // Initialize smoothed spectrum
        FILL(smoothedSpectrum, -100.0)  // Start at -100dB
    
    FUNCTION processAudioBlock(audioData: Array[Float]):
        // Apply window to reduce spectral leakage
        windowedData = Array[Float, FFT_SIZE]
        FOR i = 0 TO WINDOW_SIZE:
            windowedData[i] = audioData[i] * window[i]
        
        // Zero-pad if necessary
        FOR i = WINDOW_SIZE TO FFT_SIZE:
            windowedData[i] = 0.0
        
        // Perform FFT
        complexSpectrum = fftProcessor.forward(windowedData)
        
        // Calculate magnitude spectrum
        FOR bin = 0 TO FFT_SIZE/2:
            real = complexSpectrum[bin].real
            imag = complexSpectrum[bin].imag
            magnitude = sqrt(real*real + imag*imag)
            
            // Convert to dB
            magnitudeDB = 20 * log10(magnitude + 1e-10)
            magnitudeSpectrum[bin] = magnitudeDB
        
        // Apply smoothing
        FOR bin = 0 TO FFT_SIZE/2:
            smoothedSpectrum[bin] = SMOOTHING_FACTOR * smoothedSpectrum[bin] + 
                                   (1 - SMOOTHING_FACTOR) * magnitudeSpectrum[bin]
    
    FUNCTION getFrequencyForBin(bin: Integer, sampleRate: Float) -> Float:
        RETURN bin * sampleRate / FFT_SIZE
    
    FUNCTION getBinForFrequency(frequency: Float, sampleRate: Float) -> Integer:
        RETURN round(frequency * FFT_SIZE / sampleRate)
```

### 1.5 Device Hot-Plug Detection Algorithm

```pseudocode
ALGORITHM DeviceHotPlugDetector:
    DATA:
        knownDevices: Map<String, DeviceInfo>
        pollInterval: Integer = 500  // Poll every 500ms
        lastPollTime: Timestamp
        callbacks: List<Function>
        
    FUNCTION initialize():
        // Get initial device list
        devices = enumerateAudioDevices()
        FOR device IN devices:
            knownDevices[device.id] = device
        
        // Start polling thread
        startPollingThread()
    
    FUNCTION pollForChanges():
        WHILE running:
            currentDevices = enumerateAudioDevices()
            
            // Check for removed devices
            FOR knownId IN knownDevices.keys():
                IF knownId NOT IN currentDevices:
                    device = knownDevices[knownId]
                    knownDevices.remove(knownId)
                    notifyDeviceRemoved(device)
            
            // Check for new devices
            FOR device IN currentDevices:
                IF device.id NOT IN knownDevices:
                    knownDevices[device.id] = device
                    notifyDeviceAdded(device)
            
            sleep(pollInterval)
    
    FUNCTION notifyDeviceAdded(device: DeviceInfo):
        FOR callback IN callbacks:
            callback(DeviceEvent.ADDED, device)
        
        // If it was the previously selected device, auto-reconnect
        IF device.id == savedDeviceId:
            AudioDeviceManager.selectDevice(device.id)
            showNotification("Device reconnected: " + device.name)
    
    FUNCTION notifyDeviceRemoved(device: DeviceInfo):
        FOR callback IN callbacks:
            callback(DeviceEvent.REMOVED, device)
        
        // If it was the current device, switch to default
        IF device.id == currentDeviceId:
            defaultDevice = getDefaultAudioDevice()
            AudioDeviceManager.selectDevice(defaultDevice.id)
            showNotification("Device disconnected, switched to: " + defaultDevice.name)
```

## 2. Data Processing Algorithms

### 2.1 Audio Resampling Algorithm

```pseudocode
ALGORITHM AudioResampler:
    CONSTANTS:
        FILTER_LENGTH = 64
        
    DATA:
        inputRate: Float
        outputRate: Float
        resampleRatio: Float
        filterCoeffs: Array[Float, FILTER_LENGTH]
        
    FUNCTION initialize(inRate: Float, outRate: Float):
        inputRate = inRate
        outputRate = outRate
        resampleRatio = outputRate / inputRate
        
        // Design lowpass filter (Kaiser window)
        cutoffFreq = MIN(inputRate, outputRate) * 0.45
        designLowpassFilter(filterCoeffs, cutoffFreq, inputRate)
    
    FUNCTION process(input: Array[Float]) -> Array[Float]:
        inputLength = input.length
        outputLength = ceil(inputLength * resampleRatio)
        output = Array[Float, outputLength]
        
        FOR outIdx = 0 TO outputLength:
            // Calculate corresponding input position
            inPos = outIdx / resampleRatio
            inIdx = floor(inPos)
            fraction = inPos - inIdx
            
            // Apply interpolation filter
            sum = 0.0
            FOR k = 0 TO FILTER_LENGTH:
                sampleIdx = inIdx - FILTER_LENGTH/2 + k
                
                // Handle boundaries
                IF sampleIdx < 0:
                    sampleIdx = 0
                ELSE IF sampleIdx >= inputLength:
                    sampleIdx = inputLength - 1
                
                // Interpolate filter coefficient
                filterPos = k - fraction
                coeff = sincInterpolation(filterPos) * filterCoeffs[k]
                
                sum += input[sampleIdx] * coeff
            
            output[outIdx] = sum
        
        RETURN output
```

### 2.2 Voice Activity Detection Algorithm

```pseudocode
ALGORITHM VoiceActivityDetector:
    CONSTANTS:
        FRAME_SIZE = 320  // 20ms at 16kHz
        ENERGY_THRESHOLD = 0.01
        ZCR_THRESHOLD = 0.5
        HANGOVER_TIME = 200  // ms
        
    DATA:
        energyHistory: CircularBuffer[10]
        zcrHistory: CircularBuffer[10]
        isSpeech: Boolean = false
        hangoverCounter: Integer = 0
        
    FUNCTION detectSpeech(frame: Array[Float]) -> Boolean:
        // Calculate frame energy
        energy = 0.0
        FOR sample IN frame:
            energy += sample * sample
        energy = energy / FRAME_SIZE
        
        // Calculate zero-crossing rate
        zcr = 0
        FOR i = 1 TO FRAME_SIZE:
            IF sign(frame[i]) != sign(frame[i-1]):
                zcr += 1
        zcr = zcr / FRAME_SIZE
        
        // Update histories
        energyHistory.push(energy)
        zcrHistory.push(zcr)
        
        // Calculate adaptive thresholds
        energyMean = mean(energyHistory)
        energyStd = stddev(energyHistory)
        adaptiveEnergyThreshold = energyMean + 2 * energyStd
        
        // Speech detection logic
        IF energy > adaptiveEnergyThreshold AND zcr < ZCR_THRESHOLD:
            isSpeech = true
            hangoverCounter = HANGOVER_TIME / 20  // Reset hangover
        ELSE IF isSpeech AND hangoverCounter > 0:
            hangoverCounter -= 1
            // Keep speech state during hangover
        ELSE:
            isSpeech = false
        
        RETURN isSpeech
```

## 3. Optimization Algorithms

### 3.1 SIMD-Optimized Buffer Operations

```pseudocode
ALGORITHM SIMDBufferOps:
    FUNCTION copyBufferSIMD(src: Float*, dst: Float*, count: Integer):
        // Process 8 floats at a time with AVX
        simdCount = count / 8
        remainder = count % 8
        
        FOR i = 0 TO simdCount:
            // Load 8 floats
            vec = _mm256_loadu_ps(src + i * 8)
            // Store 8 floats
            _mm256_storeu_ps(dst + i * 8, vec)
        
        // Handle remaining samples
        FOR i = simdCount * 8 TO count:
            dst[i] = src[i]
    
    FUNCTION applyGainSIMD(buffer: Float*, count: Integer, gain: Float):
        gainVec = _mm256_set1_ps(gain)
        simdCount = count / 8
        
        FOR i = 0 TO simdCount:
            samples = _mm256_loadu_ps(buffer + i * 8)
            samples = _mm256_mul_ps(samples, gainVec)
            _mm256_storeu_ps(buffer + i * 8, samples)
        
        // Handle remainder
        FOR i = simdCount * 8 TO count:
            buffer[i] *= gain
    
    FUNCTION calculateRMSSIMD(buffer: Float*, count: Integer) -> Float:
        sumVec = _mm256_setzero_ps()
        simdCount = count / 8
        
        FOR i = 0 TO simdCount:
            samples = _mm256_loadu_ps(buffer + i * 8)
            samples = _mm256_mul_ps(samples, samples)  // Square
            sumVec = _mm256_add_ps(sumVec, samples)
        
        // Horizontal sum
        sum = horizontalSum(sumVec)
        
        // Add remainder
        FOR i = simdCount * 8 TO count:
            sum += buffer[i] * buffer[i]
        
        RETURN sqrt(sum / count)
```

## 4. Test Strategy Algorithms

### 4.1 Automated Test Generation

```pseudocode
ALGORITHM TestSignalGenerator:
    FUNCTION generateTestSignal(type: String, params: Dict) -> AudioBuffer:
        SWITCH type:
            CASE "sine":
                frequency = params["frequency"]
                amplitude = params["amplitude"]
                duration = params["duration"]
                sampleRate = params["sampleRate"]
                
                numSamples = duration * sampleRate
                buffer = AudioBuffer(1, numSamples)
                
                FOR i = 0 TO numSamples:
                    t = i / sampleRate
                    buffer[i] = amplitude * sin(2 * PI * frequency * t)
                
                RETURN buffer
            
            CASE "white_noise":
                amplitude = params["amplitude"]
                numSamples = params["numSamples"]
                
                buffer = AudioBuffer(1, numSamples)
                FOR i = 0 TO numSamples:
                    buffer[i] = amplitude * (random() * 2 - 1)
                
                RETURN buffer
            
            CASE "speech_with_noise":
                speechFile = params["speechFile"]
                noiseLevel = params["noiseLevel"]
                
                speech = loadAudioFile(speechFile)
                noise = generateWhiteNoise(speech.length, noiseLevel)
                
                combined = AudioBuffer(1, speech.length)
                FOR i = 0 TO speech.length:
                    combined[i] = speech[i] + noise[i]
                
                RETURN combined
```

### 4.2 Performance Profiling Algorithm

```pseudocode
ALGORITHM PerformanceProfiler:
    DATA:
        metrics: Map<String, List<Float>>
        timers: Map<String, Timestamp>
        
    FUNCTION startTimer(name: String):
        timers[name] = getCurrentHighResTime()
    
    FUNCTION stopTimer(name: String):
        IF name IN timers:
            elapsed = getCurrentHighResTime() - timers[name]
            
            IF name NOT IN metrics:
                metrics[name] = []
            
            metrics[name].append(elapsed)
            timers.remove(name)
    
    FUNCTION getStatistics(name: String) -> Dict:
        IF name NOT IN metrics:
            RETURN NULL
        
        values = metrics[name]
        RETURN {
            "count": values.length,
            "mean": mean(values),
            "median": median(values),
            "min": min(values),
            "max": max(values),
            "stddev": stddev(values),
            "p95": percentile(values, 95),
            "p99": percentile(values, 99)
        }
    
    FUNCTION generateReport() -> String:
        report = "Performance Profile Report\n"
        report += "========================\n\n"
        
        FOR metricName IN metrics.keys():
            stats = getStatistics(metricName)
            report += metricName + ":\n"
            report += "  Mean: " + stats["mean"] + "ms\n"
            report += "  P95: " + stats["p95"] + "ms\n"
            report += "  P99: " + stats["p99"] + "ms\n"
            report += "\n"
        
        RETURN report
```

## Conclusion

These algorithms form the core of the QUIET application's audio processing pipeline. They are designed for:
- Real-time performance (<30ms latency)
- Lock-free operation in audio threads
- Efficient CPU usage through SIMD optimization
- Robust error handling and recovery
- Easy testing and profiling

Each algorithm has been carefully designed to meet the strict requirements of real-time audio processing while maintaining high quality noise reduction.