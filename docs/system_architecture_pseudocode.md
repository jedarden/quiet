# QUIET - System Architecture Pseudocode

## 1. High-Level Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        QUIET Application                         │
├─────────────────────────────────────────────────────────────────┤
│                          UI Layer                                │
│  ┌─────────────┐  ┌────────────────┐  ┌─────────────────────┐  │
│  │ Main Window │  │ System Tray    │  │ Visualizations      │  │
│  │ Controls    │  │ Integration    │  │ (Waveform/Spectrum) │  │
│  └─────────────┘  └────────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                      Core Processing Layer                       │
│  ┌─────────────┐  ┌────────────────┐  ┌─────────────────────┐  │
│  │Audio Device │  │Noise Reduction │  │ Virtual Device      │  │
│  │Manager      │  │Processor       │  │ Router              │  │
│  └─────────────┘  └────────────────┘  └─────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                    Platform Abstraction Layer                    │
│  ┌─────────────┐  ┌────────────────┐  ┌─────────────────────┐  │
│  │Windows Audio│  │macOS Audio     │  │ Configuration       │  │
│  │(WASAPI)     │  │(Core Audio)    │  │ Manager             │  │
│  └─────────────┘  └────────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

## 2. Core Components Pseudocode

### 2.1 Main Application Controller

```pseudocode
class QuietApplication:
    // Singleton application controller
    
    private:
        audioDeviceManager: AudioDeviceManager
        noiseProcessor: NoiseReductionProcessor
        virtualRouter: VirtualDeviceRouter
        mainWindow: MainWindow
        systemTray: SystemTrayController
        config: ConfigurationManager
        eventDispatcher: EventDispatcher
        
    function initialize():
        // Initialize configuration
        config = ConfigurationManager.load()
        
        // Setup event system
        eventDispatcher = new EventDispatcher()
        
        // Initialize audio subsystem
        audioDeviceManager = new AudioDeviceManager(eventDispatcher)
        noiseProcessor = new NoiseReductionProcessor(config.reductionLevel)
        virtualRouter = new VirtualDeviceRouter()
        
        // Initialize UI
        mainWindow = new MainWindow(eventDispatcher)
        systemTray = new SystemTrayController(eventDispatcher)
        
        // Connect components
        connectAudioPipeline()
        setupEventHandlers()
        
        // Start processing
        if config.processingEnabled:
            startProcessing()
            
    function connectAudioPipeline():
        // Setup audio flow: Input -> Processor -> Virtual Output
        audioDeviceManager.onAudioData = (buffer) =>
            processedBuffer = noiseProcessor.process(buffer)
            virtualRouter.sendToVirtualDevice(processedBuffer)
            mainWindow.updateVisualizations(buffer, processedBuffer)
            
    function setupEventHandlers():
        eventDispatcher.on(DeviceChanged, handleDeviceChange)
        eventDispatcher.on(ProcessingToggled, handleProcessingToggle)
        eventDispatcher.on(ReductionLevelChanged, handleReductionChange)
        
    function startProcessing():
        audioDeviceManager.startCapture(config.selectedDevice)
        virtualRouter.startRouting()
        noiseProcessor.setEnabled(true)
        
    function stopProcessing():
        noiseProcessor.setEnabled(false)
        audioDeviceManager.stopCapture()
        virtualRouter.stopRouting()
```

### 2.2 Audio Device Manager

```pseudocode
class AudioDeviceManager:
    // Manages audio input devices and capture
    
    private:
        devices: List<AudioDevice>
        currentDevice: AudioDevice
        audioCallback: Function
        captureThread: Thread
        isCapturing: boolean
        bufferQueue: LockFreeQueue<AudioBuffer>
        
    function initialize(eventDispatcher):
        this.eventDispatcher = eventDispatcher
        enumerateDevices()
        setupHotplugDetection()
        
    function enumerateDevices():
        // Platform-specific device enumeration
        if PLATFORM == Windows:
            devices = WindowsAudioAPI.getInputDevices()
        else if PLATFORM == macOS:
            devices = CoreAudioAPI.getInputDevices()
            
        // Filter out virtual devices to avoid feedback
        devices = devices.filter(d => !d.isVirtual)
        
    function selectDevice(deviceId: string):
        device = devices.find(d => d.id == deviceId)
        if device == null:
            throw DeviceNotFoundException(deviceId)
            
        if isCapturing:
            stopCapture()
            
        currentDevice = device
        
        if wasCapturing:
            startCapture(device)
            
    function startCapture(device: AudioDevice):
        // Configure audio format
        format = AudioFormat {
            sampleRate: 48000,  // Required for RNNoise
            channels: 1,        // Mono for processing
            bitDepth: 16,       // 16-bit PCM
            bufferSize: 256     // ~5.3ms at 48kHz
        }
        
        // Start capture thread
        isCapturing = true
        captureThread = new Thread(captureLoop, device, format)
        captureThread.setPriority(REALTIME_PRIORITY)
        captureThread.start()
        
    function captureLoop(device: AudioDevice, format: AudioFormat):
        // Real-time audio capture loop
        audioInterface = createAudioInterface(device, format)
        
        while isCapturing:
            buffer = audioInterface.readBuffer()
            
            if buffer.isValid():
                // Process in callback (lock-free)
                if audioCallback != null:
                    audioCallback(buffer)
                    
                // Also queue for visualization (non-blocking)
                bufferQueue.tryPush(buffer)
            else:
                handleBufferError(buffer.error)
                
    function handleDeviceHotplug(event: DeviceEvent):
        oldDevices = devices
        enumerateDevices()
        
        if event.type == DeviceDisconnected:
            if currentDevice.id == event.deviceId:
                // Fall back to default device
                defaultDevice = getDefaultDevice()
                selectDevice(defaultDevice.id)
                eventDispatcher.emit(DeviceDisconnected, event)
                
        eventDispatcher.emit(DeviceListChanged, devices)
```

### 2.3 Noise Reduction Processor

```pseudocode
class NoiseReductionProcessor:
    // Wraps RNNoise algorithm with audio processing pipeline
    
    private:
        rnnoiseState: RNNoiseState
        reductionLevel: float  // 0.0 to 1.0
        enabled: boolean
        inputResampler: Resampler
        outputResampler: Resampler
        processingBuffer: float[480]  // RNNoise frame size
        
    function initialize(level: float):
        // Initialize RNNoise
        rnnoiseState = rnnoise_create()
        reductionLevel = level
        enabled = false
        
        // Setup resamplers for format conversion
        inputResampler = new Resampler(ANY_RATE, 48000)
        outputResampler = new Resampler(48000, ANY_RATE)
        
    function process(inputBuffer: AudioBuffer) -> AudioBuffer:
        if !enabled:
            return inputBuffer  // Passthrough
            
        // Convert to 48kHz mono if needed
        monoBuffer = convertToMono(inputBuffer)
        resampledBuffer = inputResampler.process(monoBuffer)
        
        // Process in RNNoise frame sizes (480 samples = 10ms)
        outputBuffer = new AudioBuffer(resampledBuffer.size)
        
        for i = 0; i < resampledBuffer.size; i += 480:
            // Extract frame
            frame = resampledBuffer.getFrame(i, 480)
            
            // Apply RNNoise
            rnnoise_process_frame(rnnoiseState, processingBuffer, frame)
            
            // Apply reduction level scaling
            for j = 0; j < 480; j++:
                scaledSample = frame[j] * (1 - reductionLevel) + 
                              processingBuffer[j] * reductionLevel
                outputBuffer[i + j] = scaledSample
                
        // Resample back to original rate if needed
        if inputBuffer.sampleRate != 48000:
            outputBuffer = outputResampler.process(outputBuffer)
            
        // Convert back to original channel count
        if inputBuffer.channels > 1:
            outputBuffer = duplicateToChannels(outputBuffer, inputBuffer.channels)
            
        return outputBuffer
        
    function setReductionLevel(level: float):
        reductionLevel = clamp(level, 0.0, 1.0)
        
    function getNoiseReductionDB() -> float:
        // Estimate noise reduction in dB
        if !enabled:
            return 0.0
            
        // RNNoise typically achieves 20-30dB reduction
        return reductionLevel * 25.0  // Approximate
```

### 2.4 Virtual Device Router

```
CLASS VirtualDeviceRouter:
    PRIVATE:
        virtual_device: VirtualAudioDevice
        platform_handler: PlatformAudioHandler
        
    PUBLIC:
        FUNCTION initialize():
            IF platform == "Windows":
                platform_handler = WindowsVirtualAudioHandler()
                virtual_device = find_vb_cable_device()
            ELSE IF platform == "macOS":
                platform_handler = MacOSVirtualAudioHandler()
                virtual_device = find_blackhole_device()
            END IF
            
            IF virtual_device == NULL:
                show_virtual_device_setup_dialog()
            END IF
        END FUNCTION
        
        FUNCTION write_buffer(audio_buffer: AudioBuffer):
            IF virtual_device.is_connected():
                // Convert format if necessary
                converted = convert_audio_format(
                    audio_buffer,
                    virtual_device.format,
                    virtual_device.sample_rate
                )
                
                // Write to virtual device
                virtual_device.write(converted)
            ELSE:
                // Attempt reconnection
                reconnect_virtual_device()
            END IF
        END FUNCTION
        
        FUNCTION convert_audio_format(buffer: AudioBuffer, target_format: Format, target_rate: Integer):
            converted = buffer
            
            IF buffer.sample_rate != target_rate:
                converted = resample(converted, target_rate)
            END IF
            
            IF buffer.bit_depth != target_format.bit_depth:
                converted = convert_bit_depth(converted, target_format.bit_depth)
            END IF
            
            RETURN converted
        END FUNCTION
END CLASS
```

### 2.4 User Interface Components

```
CLASS MainWindow:
    PRIVATE:
        device_dropdown: ComboBox
        enable_toggle: ToggleButton
        level_meter: LevelMeter
        waveform_input: WaveformDisplay
        waveform_output: WaveformDisplay
        spectrum_analyzer: SpectrumAnalyzer
        reduction_indicator: Label
        
    PUBLIC:
        FUNCTION initialize():
            create_ui_layout()
            populate_device_list()
            register_event_listeners()
            apply_theme()
        END FUNCTION
        
        FUNCTION create_ui_layout():
            // Header section
            header = create_panel()
            header.add(create_logo())
            header.add(create_title("QUIET"))
            
            // Device selection
            device_section = create_panel()
            device_section.add(create_label("Input Device:"))
            device_dropdown = create_combo_box()
            device_section.add(device_dropdown)
            
            // Main control
            control_section = create_panel()
            enable_toggle = create_toggle_button("Enable Noise Reduction")
            enable_toggle.set_size(200, 60)
            control_section.add(enable_toggle)
            
            // Visualization section
            viz_section = create_tabbed_panel()
            
            // Waveform tab
            waveform_tab = create_panel()
            waveform_input = create_waveform_display("Input")
            waveform_output = create_waveform_display("Output")
            waveform_tab.add_split(waveform_input, waveform_output)
            
            // Spectrum tab
            spectrum_tab = create_panel()
            spectrum_analyzer = create_spectrum_analyzer()
            spectrum_tab.add(spectrum_analyzer)
            
            viz_section.add_tab("Waveform", waveform_tab)
            viz_section.add_tab("Spectrum", spectrum_tab)
            
            // Status section
            status_section = create_panel()
            level_meter = create_level_meter()
            reduction_indicator = create_label("Reduction: 0 dB")
            status_section.add(level_meter)
            status_section.add(reduction_indicator)
            
            // Layout assembly
            main_layout = create_vertical_layout()
            main_layout.add(header)
            main_layout.add(device_section)
            main_layout.add(control_section)
            main_layout.add(viz_section)
            main_layout.add(status_section)
            
            set_content(main_layout)
        END FUNCTION
        
        FUNCTION on_device_selected(device_id: String):
            AudioDeviceManager.select_device(device_id)
            update_ui_state()
        END FUNCTION
        
        FUNCTION on_toggle_clicked():
            new_state = !enable_toggle.is_checked()
            NoiseReductionProcessor.set_enabled(new_state)
            update_toggle_appearance(new_state)
        END FUNCTION
        
        FUNCTION on_audio_event(event: AudioEvent, data: EventData):
            SWITCH event:
                CASE AudioEvent.INPUT_BUFFER_READY:
                    waveform_input.update_buffer(data.buffer)
                    level_meter.update_level(data.buffer.get_peak_level())
                    spectrum_analyzer.update_input(data.buffer)
                    
                CASE AudioEvent.OUTPUT_BUFFER_READY:
                    waveform_output.update_buffer(data.buffer)
                    spectrum_analyzer.update_output(data.buffer)
                    
                CASE AudioEvent.PROCESSING_TOGGLED:
                    update_toggle_appearance(data.enabled)
                    
                CASE AudioEvent.DEVICE_CHANGED:
                    update_device_selection()
            END SWITCH
        END FUNCTION
END CLASS
```

### 2.5 Visualization Components
```
CLASS WaveformDisplay:
    PRIVATE:
        audio_buffer: CircularBuffer
        display_buffer: Array<Float>
        display_width: Integer = 800
        display_height: Integer = 200
        
    PUBLIC:
        FUNCTION update_buffer(new_audio: AudioBuffer):
            audio_buffer.write(new_audio)
            
            // Downsample for display
            samples_per_pixel = audio_buffer.size / display_width
            
            FOR x = 0 TO display_width:
                sample_index = x * samples_per_pixel
                
                // Find min/max in this pixel's range
                min_val = +1.0
                max_val = -1.0
                
                FOR i = sample_index TO sample_index + samples_per_pixel:
                    sample = audio_buffer[i]
                    min_val = MIN(min_val, sample)
                    max_val = MAX(max_val, sample)
                END FOR
                
                display_buffer[x] = (max_val - min_val) / 2
            END FOR
            
            repaint()
        END FUNCTION
        
        FUNCTION paint(graphics: Graphics):
            graphics.fill_background(Colors.DARK_GRAY)
            
            // Draw waveform
            graphics.set_color(Colors.GREEN)
            
            FOR x = 0 TO display_width - 1:
                y_center = display_height / 2
                amplitude = display_buffer[x] * display_height / 2
                
                graphics.draw_line(
                    x, y_center - amplitude,
                    x, y_center + amplitude
                )
            END FOR
            
            // Draw center line
            graphics.set_color(Colors.GRAY)
            graphics.draw_line(0, display_height/2, display_width, display_height/2)
        END FUNCTION
END CLASS

CLASS SpectrumAnalyzer:
    PRIVATE:
        fft_size: Integer = 2048
        fft_processor: FFTProcessor
        magnitude_buffer: Array<Float>
        smoothing_factor: Float = 0.8
        
    PUBLIC:
        FUNCTION update_input(audio_buffer: AudioBuffer):
            // Perform FFT
            fft_result = fft_processor.forward(audio_buffer, fft_size)
            
            // Convert to magnitude spectrum
            FOR bin = 0 TO fft_size/2:
                magnitude = calculate_magnitude(fft_result[bin])
                magnitude_db = 20 * log10(magnitude)
                
                // Apply smoothing
                magnitude_buffer[bin] = smoothing_factor * magnitude_buffer[bin] + 
                                       (1 - smoothing_factor) * magnitude_db
            END FOR
            
            repaint()
        END FUNCTION
        
        FUNCTION paint(graphics: Graphics):
            graphics.fill_background(Colors.BLACK)
            
            num_bins = fft_size / 2
            bin_width = display_width / num_bins
            
            // Draw frequency bins
            FOR bin = 0 TO num_bins:
                // Convert magnitude to pixel height
                db_value = magnitude_buffer[bin]
                normalized = (db_value + 100) / 100  // Normalize -100dB to 0dB
                bar_height = normalized * display_height
                
                // Color based on frequency
                color = get_frequency_color(bin, num_bins)
                graphics.set_color(color)
                
                // Draw bar
                x = bin * bin_width
                graphics.fill_rectangle(x, display_height - bar_height, bin_width - 1, bar_height)
            END FOR
            
            // Draw frequency labels
            draw_frequency_grid(graphics)
        END FUNCTION
END CLASS
```

### 2.6 NoiseReductionProcessor Implementation
```
CLASS NoiseReductionProcessor:
    PRIVATE:
        rnnoise_state: RNNoiseState
        processing_enabled: Boolean = true
        reduction_level: Float = 0.0
        frame_size: Integer = 480  // RNNoise frame size
        sample_rate: Integer = 48000
        resampler: AudioResampler
        
    PUBLIC:
        FUNCTION initialize():
            rnnoise_state = rnnoise_create()
            resampler = create_resampler(input_rate, 48000)
        END FUNCTION
        
        FUNCTION process(input_buffer: AudioBuffer):
            IF NOT processing_enabled:
                RETURN input_buffer
            END IF
            
            // Resample to 48kHz if needed
            IF input_buffer.sample_rate != 48000:
                resampled = resampler.process(input_buffer)
            ELSE:
                resampled = input_buffer
            END IF
            
            output_buffer = create_buffer(input_buffer.size)
            
            // Process in RNNoise frame sizes
            FOR frame_start = 0 TO resampled.size STEP frame_size:
                frame = extract_frame(resampled, frame_start, frame_size)
                
                // Apply RNNoise
                denoised_frame = rnnoise_process_frame(rnnoise_state, frame)
                
                // Calculate reduction amount for visualization
                reduction = calculate_reduction_amount(frame, denoised_frame)
                update_reduction_level(reduction)
                
                // Copy to output
                copy_frame_to_buffer(denoised_frame, output_buffer, frame_start)
            END FOR
            
            // Resample back if needed
            IF input_buffer.sample_rate != 48000:
                output_buffer = resampler.process_inverse(output_buffer)
            END IF
            
            RETURN output_buffer
        END FUNCTION
        
        FUNCTION set_enabled(enabled: Boolean):
            processing_enabled = enabled
            EventDispatcher.dispatch(AudioEvent.PROCESSING_TOGGLED, enabled)
        END FUNCTION
        
        FUNCTION calculate_reduction_amount(original: AudioFrame, processed: AudioFrame):
            original_energy = calculate_rms(original)
            processed_energy = calculate_rms(processed)
            reduction_db = 20 * log10(original_energy / processed_energy)
            RETURN reduction_db
        END FUNCTION
END CLASS
```

## 3. Platform-Specific Implementations

### 3.1 Windows Virtual Audio Implementation
```
CLASS WindowsVirtualAudioHandler:
    PRIVATE:
        wasapi_client: IAudioClient
        render_client: IAudioRenderClient
        
    PUBLIC:
        FUNCTION find_vb_cable_device():
            devices = enumerate_audio_devices(eRender)
            
            FOR device IN devices:
                name = device.get_friendly_name()
                IF "VB-Audio" IN name OR "CABLE Input" IN name:
                    RETURN create_virtual_device(device)
                END IF
            END FOR
            
            RETURN NULL
        END FUNCTION
        
        FUNCTION write_audio_data(buffer: AudioBuffer):
            // Get buffer from WASAPI
            frame_count = buffer.sample_count
            data_pointer = render_client.GetBuffer(frame_count)
            
            // Copy audio data
            copy_audio_data(buffer.data, data_pointer, frame_count * buffer.channel_count)
            
            // Release buffer
            render_client.ReleaseBuffer(frame_count, 0)
        END FUNCTION
END CLASS
```

### 3.2 macOS Virtual Audio Implementation
```
CLASS MacOSVirtualAudioHandler:
    PRIVATE:
        audio_unit: AudioUnit
        blackhole_device_id: AudioDeviceID
        
    PUBLIC:
        FUNCTION find_blackhole_device():
            device_count = AudioHardware.get_device_count()
            device_ids = AudioHardware.get_all_devices()
            
            FOR device_id IN device_ids:
                name = AudioDevice.get_name(device_id)
                IF "BlackHole" IN name:
                    blackhole_device_id = device_id
                    RETURN create_virtual_device(device_id)
                END IF
            END FOR
            
            RETURN NULL
        END FUNCTION
        
        FUNCTION setup_audio_unit():
            // Create output audio unit
            component = find_audio_component(kAudioUnitType_Output, kAudioUnitSubType_HALOutput)
            audio_unit = create_audio_unit(component)
            
            // Set BlackHole as output device
            AudioUnit.set_property(
                audio_unit,
                kAudioOutputUnitProperty_CurrentDevice,
                blackhole_device_id
            )
            
            // Set render callback
            AudioUnit.set_render_callback(audio_unit, render_callback)
            
            // Initialize and start
            AudioUnit.initialize(audio_unit)
            AudioUnit.start(audio_unit)
        END FUNCTION
END CLASS
```

## 4. Test Strategy Pseudocode

### 4.1 Unit Test Structure
```
TEST_SUITE NoiseReductionTests:
    TEST test_noise_reduction_initialization():
        processor = NoiseReductionProcessor()
        processor.initialize()
        
        ASSERT processor.rnnoise_state != NULL
        ASSERT processor.processing_enabled == true
        ASSERT processor.sample_rate == 48000
    END TEST
    
    TEST test_noise_reduction_processing():
        processor = NoiseReductionProcessor()
        processor.initialize()
        
        // Create test signal with noise
        test_signal = generate_sine_wave(1000, 48000, 1.0)  // 1kHz sine
        noise = generate_white_noise(0.1)  // 10% noise level
        noisy_signal = test_signal + noise
        
        // Process
        result = processor.process(noisy_signal)
        
        // Verify noise reduction
        snr_before = calculate_snr(test_signal, noisy_signal)
        snr_after = calculate_snr(test_signal, result)
        
        ASSERT snr_after > snr_before + 10  // At least 10dB improvement
    END TEST
    
    TEST test_latency_requirement():
        processor = NoiseReductionProcessor()
        processor.initialize()
        
        test_buffer = create_test_buffer(256, 48000)
        
        start_time = get_high_precision_time()
        processor.process(test_buffer)
        end_time = get_high_precision_time()
        
        processing_time = end_time - start_time
        ASSERT processing_time < 30  // Less than 30ms
    END TEST
END TEST_SUITE

TEST_SUITE AudioDeviceTests:
    TEST test_device_enumeration():
        manager = AudioDeviceManager()
        devices = manager.enumerate_system_audio_devices()
        
        ASSERT devices.length > 0
        ASSERT devices[0].id != NULL
        ASSERT devices[0].name != NULL
    END TEST
    
    TEST test_device_switching():
        manager = AudioDeviceManager()
        manager.initialize()
        
        devices = manager.enumerate_system_audio_devices()
        IF devices.length >= 2:
            original_device = manager.current_input_device
            manager.select_device(devices[1].id)
            
            ASSERT manager.current_input_device.id == devices[1].id
            ASSERT manager.current_input_device.id != original_device.id
        END IF
    END TEST
END TEST_SUITE
```

### 4.2 Integration Test Structure
```
TEST_SUITE IntegrationTests:
    TEST test_end_to_end_audio_flow():
        // Initialize all components
        app = Application()
        app.initialize()
        
        // Simulate audio input
        test_audio = generate_test_audio_stream()
        
        // Process through pipeline
        app.audio_engine.process_audio_stream(test_audio)
        
        // Verify output
        output = app.virtual_device_router.get_last_output()
        ASSERT output != NULL
        ASSERT output.sample_count == test_audio.sample_count
    END TEST
    
    TEST test_ui_audio_sync():
        app = Application()
        app.initialize()
        
        // Register test listener
        test_listener = TestEventListener()
        app.event_dispatcher.add_listener(test_listener)
        
        // Send audio through pipeline
        test_audio = generate_test_audio_stream()
        app.audio_engine.process_audio_stream(test_audio)
        
        // Verify UI events received
        ASSERT test_listener.received_event(AudioEvent.INPUT_BUFFER_READY)
        ASSERT test_listener.received_event(AudioEvent.OUTPUT_BUFFER_READY)
    END TEST
END TEST_SUITE
```

## 5. Performance Optimization Strategies

### 5.1 Lock-Free Audio Processing
```
FUNCTION optimize_audio_callback():
    // Use lock-free ring buffer for audio data
    ring_buffer = LockFreeRingBuffer(size = 8192)
    
    // Audio callback (real-time thread)
    FUNCTION audio_callback(input, output):
        // No locks, no allocations, no system calls
        success = ring_buffer.write(input)
        IF NOT success:
            // Buffer overflow - skip this block
            report_overflow()
        END IF
        
        // Process if data available
        IF ring_buffer.available() >= frame_size:
            data = ring_buffer.read(frame_size)
            processed = process_audio_fast(data)
            copy_to_output(processed, output)
        END IF
    END FUNCTION
    
    // Worker thread for heavy processing
    FUNCTION worker_thread():
        WHILE running:
            IF ring_buffer.available() >= frame_size:
                data = ring_buffer.read(frame_size)
                processed = apply_noise_reduction(data)
                output_ring_buffer.write(processed)
            ELSE:
                sleep(1)  // 1ms sleep
            END IF
        END WHILE
    END FUNCTION
END FUNCTION
```

### 5.2 SIMD Optimizations
```
FUNCTION optimize_buffer_operations():
    // Use SIMD for buffer operations
    FUNCTION copy_buffer_simd(source: Float*, dest: Float*, count: Integer):
        simd_count = count / 4  // Process 4 floats at a time
        
        FOR i = 0 TO simd_count:
            // Load 4 floats
            vec = simd_load(source + i * 4)
            // Store 4 floats
            simd_store(dest + i * 4, vec)
        END FOR
        
        // Handle remaining samples
        remaining = count % 4
        FOR i = simd_count * 4 TO count:
            dest[i] = source[i]
        END FOR
    END FUNCTION
    
    FUNCTION calculate_rms_simd(buffer: Float*, count: Integer):
        sum_vec = simd_zero()
        
        FOR i = 0 TO count STEP 4:
            vec = simd_load(buffer + i)
            squared = simd_multiply(vec, vec)
            sum_vec = simd_add(sum_vec, squared)
        END FOR
        
        // Sum all elements
        sum = simd_horizontal_sum(sum_vec)
        RETURN sqrt(sum / count)
    END FUNCTION
END FUNCTION
```

## 6. Error Handling and Recovery

### 6.1 Audio Device Error Handling
```
FUNCTION handle_audio_errors():
    TRY:
        audio_device.start()
    CATCH DeviceNotFoundError:
        // Device disconnected
        show_notification("Audio device disconnected")
        wait_for_device_reconnection()
    CATCH DeviceInUseError:
        // Device being used by another app
        show_error_dialog("Audio device is in use by another application")
        offer_device_alternatives()
    CATCH AudioDriverError:
        // Driver issue
        log_error("Audio driver error")
        attempt_driver_restart()
    END TRY
END FUNCTION

FUNCTION wait_for_device_reconnection():
    retry_count = 0
    max_retries = 30  // 30 seconds
    
    WHILE retry_count < max_retries:
        devices = enumerate_audio_devices()
        
        IF saved_device_id IN devices:
            // Device reconnected
            reconnect_to_device(saved_device_id)
            show_notification("Audio device reconnected")
            RETURN
        END IF
        
        sleep(1000)  // Wait 1 second
        retry_count++
    END WHILE
    
    // Device not reconnected - prompt user
    show_device_selection_dialog()
END FUNCTION
```

### 6.2 Virtual Device Error Recovery
```
FUNCTION handle_virtual_device_errors():
    IF NOT virtual_device.is_available():
        // Virtual device not installed
        result = show_dialog(
            "Virtual audio device not found. " +
            "Would you like to install it now?"
        )
        
        IF result == YES:
            IF platform == "Windows":
                download_and_install_vb_cable()
            ELSE IF platform == "macOS":
                download_and_install_blackhole()
            END IF
        END IF
    END IF
    
    IF virtual_device.connection_lost():
        // Try to reconnect
        attempts = 0
        WHILE attempts < 5:
            IF virtual_device.reconnect():
                RETURN
            END IF
            sleep(1000)
            attempts++
        END WHILE
        
        // Failed to reconnect
        show_error("Failed to connect to virtual audio device")
    END IF
END FUNCTION
```

## 7. Configuration Management
```
CLASS ConfigurationManager:
    PRIVATE:
        config_file_path: String
        settings: Dictionary
        
    PUBLIC:
        FUNCTION initialize():
            config_file_path = get_user_config_directory() + "/quiet/config.json"
            load_settings()
        END FUNCTION
        
        FUNCTION load_settings():
            IF file_exists(config_file_path):
                settings = parse_json_file(config_file_path)
            ELSE:
                settings = get_default_settings()
                save_settings()
            END IF
        END FUNCTION
        
        FUNCTION get_default_settings():
            RETURN {
                "audio": {
                    "input_device_id": "",
                    "buffer_size": 256,
                    "sample_rate": 48000
                },
                "processing": {
                    "noise_reduction_enabled": true,
                    "reduction_level": "medium"
                },
                "ui": {
                    "window_position": {"x": 100, "y": 100},
                    "window_size": {"width": 800, "height": 600},
                    "start_minimized": false,
                    "theme": "dark"
                },
                "system": {
                    "auto_start": false,
                    "check_updates": true
                }
            }
        END FUNCTION
        
        FUNCTION get_setting(key_path: String):
            keys = key_path.split(".")
            value = settings
            
            FOR key IN keys:
                IF key IN value:
                    value = value[key]
                ELSE:
                    RETURN NULL
                END IF
            END FOR
            
            RETURN value
        END FUNCTION
        
        FUNCTION set_setting(key_path: String, value: Any):
            keys = key_path.split(".")
            target = settings
            
            // Navigate to parent object
            FOR i = 0 TO keys.length - 2:
                key = keys[i]
                IF key NOT IN target:
                    target[key] = {}
                END IF
                target = target[key]
            END FOR
            
            // Set the value
            target[keys[-1]] = value
            save_settings()
        END FUNCTION
END CLASS
```

## Key Design Decisions

1. **Lock-free Ring Buffers**: Prevent audio glitches from thread contention
2. **RNNoise Integration**: Proven ML-based noise reduction with low latency
3. **Event-Driven Architecture**: Decoupled components with clean interfaces
4. **Platform Abstraction**: Clean separation of platform-specific code
5. **JUCE Framework**: Mature, cross-platform audio/UI framework
6. **Modular Architecture**: Easy to swap noise cancellation algorithms

## Performance Targets

1. **Latency**: <30ms end-to-end processing
2. **CPU Usage**: <15% on quad-core processor
3. **Memory**: <200MB total usage
4. **Audio Quality**: PESQ improvement >0.4, STOI improvement >5%
5. **Frame Rate**: 60 FPS for visualizations without impacting audio