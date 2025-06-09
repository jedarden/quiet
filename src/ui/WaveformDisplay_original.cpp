#include "quiet/ui/WaveformDisplay.h"
#include <algorithm>
#include <cmath>

namespace quiet {
namespace ui {

// Vertex shader for waveform rendering
static const char* vertexShader = R"(
    attribute vec2 position;
    uniform mat4 projectionMatrix;
    
    void main() {
        gl_Position = projectionMatrix * vec4(position, 0.0, 1.0);
    }
)";

// Fragment shader for waveform rendering
static const char* fragmentShader = R"(
    uniform vec4 colour;
    
    void main() {
        gl_FragColor = colour;
    }
)";

WaveformDisplay::WaveformDisplay() {
    // Set up OpenGL context
    openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    
    // Start refresh timer
    startTimerHz(settings.refreshRate);
    
    // Enable mouse events
    setWantsKeyboardFocus(false);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

WaveformDisplay::~WaveformDisplay() {
    stopTimer();
    openGLContext.detach();
}

void WaveformDisplay::pushInputSample(float sample) {
    inputBuffer.push(sample);
    needsRepaint = true;
}

void WaveformDisplay::pushOutputSample(float sample) {
    outputBuffer.push(sample);
    needsRepaint = true;
}

void WaveformDisplay::pushInputBuffer(const float* buffer, int numSamples) {
    inputBuffer.pushBatch(buffer, numSamples);
    needsRepaint = true;
}

void WaveformDisplay::pushOutputBuffer(const float* buffer, int numSamples) {
    outputBuffer.pushBatch(buffer, numSamples);
    needsRepaint = true;
}

void WaveformDisplay::setSettings(const WaveformSettings& newSettings) {
    std::lock_guard<std::mutex> lock(settingsMutex);
    settings = newSettings;
    
    if (settings.refreshRate != newSettings.refreshRate) {
        stopTimer();
        startTimerHz(settings.refreshRate);
    }
    
    repaint();
}

void WaveformDisplay::setZoomLevel(float zoom) {
    zoomLevel = std::max(0.1f, std::min(100.0f, zoom));
    needsRepaint = true;
}

void WaveformDisplay::setTimeOffset(float offsetInSeconds) {
    timeOffset = offsetInSeconds;
    needsRepaint = true;
}

void WaveformDisplay::setSampleRate(double sampleRate) {
    currentSampleRate = sampleRate;
    needsRepaint = true;
}

void WaveformDisplay::paint(juce::Graphics& g) {
    if (!openGLAvailable) {
        renderWithSoftware(g);
    }
    // OpenGL rendering is handled in renderOpenGL()
}

void WaveformDisplay::resized() {
    // Update downsampled data when size changes
    const int width = getWidth();
    inputDownsampled.update(inputBuffer, width, zoomLevel);
    outputDownsampled.update(outputBuffer, width, zoomLevel);
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& event) {
    lastMousePosition = event.position;
    isDragging = true;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event) {
    if (isDragging) {
        handleMouseDrag(event);
    }
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& event, 
                                    const juce::MouseWheelDetails& wheel) {
    handleMouseWheel(wheel);
}

void WaveformDisplay::newOpenGLContextCreated() {
    openGLAvailable = true;
    compileShaders();
    createVertexBuffers();
}

void WaveformDisplay::renderOpenGL() {
    if (!openGLAvailable || !shaderProgram) return;
    
    juce::OpenGLHelpers::clear(settings.backgroundColour);
    
    // Enable blending for antialiasing
    if (settings.antialiasing) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    }
    
    // Set line width
    glLineWidth(settings.lineThickness);
    
    // Update vertex data if needed
    if (needsRepaint.exchange(false)) {
        updateVertexData();
    }
    
    renderWithOpenGL();
    
    // Draw grid and markers using JUCE graphics
    if (settings.showGrid || settings.showTimeMarkers) {
        juce::Graphics g(openGLContext.getLowLevelGraphicsContext());
        if (settings.showGrid) drawGrid(g);
        if (settings.showTimeMarkers) drawTimeMarkers(g);
    }
}

void WaveformDisplay::openGLContextClosing() {
    openGLAvailable = false;
    shaderProgram.reset();
    
    if (inputVBO) glDeleteBuffers(1, &inputVBO);
    if (outputVBO) glDeleteBuffers(1, &outputVBO);
    if (inputVAO) glDeleteVertexArrays(1, &inputVAO);
    if (outputVAO) glDeleteVertexArrays(1, &outputVAO);
}

void WaveformDisplay::timerCallback() {
    if (needsRepaint) {
        repaint();
    }
}

void WaveformDisplay::renderWithOpenGL() {
    if (!shaderProgram) return;
    
    shaderProgram->use();
    
    // Set projection matrix
    auto projMatrix = juce::Matrix3D<float>::fromFrustum(
        -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f);
    projectionMatrix->setMatrix4(projMatrix.mat, 1, false);
    
    // Draw input waveform
    if (settings.channelMode == ChannelMode::Input || 
        settings.channelMode == ChannelMode::Both) {
        colour->set(settings.inputWaveformColour.getFloatRed(),
                    settings.inputWaveformColour.getFloatGreen(),
                    settings.inputWaveformColour.getFloatBlue(),
                    settings.inputWaveformColour.getFloatAlpha());
        
        glBindVertexArray(inputVAO);
        
        switch (settings.drawingMode) {
            case DrawingMode::Line:
                glDrawArrays(GL_LINE_STRIP, 0, inputDownsampled.minValues.size());
                break;
            case DrawingMode::Filled:
                glDrawArrays(GL_TRIANGLE_STRIP, 0, inputDownsampled.minValues.size() * 2);
                break;
            case DrawingMode::Dots:
                glDrawArrays(GL_POINTS, 0, inputDownsampled.minValues.size());
                break;
        }
    }
    
    // Draw output waveform
    if (settings.channelMode == ChannelMode::Output || 
        settings.channelMode == ChannelMode::Both) {
        colour->set(settings.outputWaveformColour.getFloatRed(),
                    settings.outputWaveformColour.getFloatGreen(),
                    settings.outputWaveformColour.getFloatBlue(),
                    settings.outputWaveformColour.getFloatAlpha());
        
        glBindVertexArray(outputVAO);
        
        switch (settings.drawingMode) {
            case DrawingMode::Line:
                glDrawArrays(GL_LINE_STRIP, 0, outputDownsampled.minValues.size());
                break;
            case DrawingMode::Filled:
                glDrawArrays(GL_TRIANGLE_STRIP, 0, outputDownsampled.minValues.size() * 2);
                break;
            case DrawingMode::Dots:
                glDrawArrays(GL_POINTS, 0, outputDownsampled.minValues.size());
                break;
        }
    }
    
    glBindVertexArray(0);
}

void WaveformDisplay::renderWithSoftware(juce::Graphics& g) {
    g.fillAll(settings.backgroundColour);
    
    // Update downsampled data
    const int width = getWidth();
    inputDownsampled.update(inputBuffer, width, zoomLevel);
    outputDownsampled.update(outputBuffer, width, zoomLevel);
    
    // Draw grid first
    if (settings.showGrid) {
        drawGrid(g);
    }
    
    // Draw waveforms
    if (settings.channelMode == ChannelMode::Input || 
        settings.channelMode == ChannelMode::Both) {
        drawWaveform(g, inputDownsampled, settings.inputWaveformColour,
                    settings.drawingMode == DrawingMode::Filled);
    }
    
    if (settings.channelMode == ChannelMode::Output || 
        settings.channelMode == ChannelMode::Both) {
        drawWaveform(g, outputDownsampled, settings.outputWaveformColour,
                    settings.drawingMode == DrawingMode::Filled);
    }
    
    // Draw time markers
    if (settings.showTimeMarkers) {
        drawTimeMarkers(g);
    }
}

void WaveformDisplay::drawWaveform(juce::Graphics& g, const DownsampledData& data,
                                  juce::Colour colour, bool filled) {
    if (data.minValues.empty()) return;
    
    const float height = static_cast<float>(getHeight());
    const float midY = height * 0.5f;
    
    juce::Path waveformPath;
    
    if (filled) {
        // Create filled waveform
        for (size_t i = 0; i < data.minValues.size(); ++i) {
            const float x = static_cast<float>(i);
            const float minY = midY - data.minValues[i] * midY;
            const float maxY = midY - data.maxValues[i] * midY;
            
            if (i == 0) {
                waveformPath.startNewSubPath(x, minY);
            }
            waveformPath.lineTo(x, minY);
        }
        
        // Draw the top half, then bottom
        for (int i = static_cast<int>(data.maxValues.size()) - 1; i >= 0; --i) {
            const float x = static_cast<float>(i);
            const float maxY = midY - data.maxValues[i] * midY;
            waveformPath.lineTo(x, maxY);
        }
        
        waveformPath.closeSubPath();
        g.setColour(colour.withAlpha(0.7f));
        g.fillPath(waveformPath);
    } else {
        // Draw line waveform using RMS values
        for (size_t i = 0; i < data.rmsValues.size(); ++i) {
            const float x = static_cast<float>(i);
            const float y = midY - data.rmsValues[i] * midY;
            
            if (i == 0) {
                waveformPath.startNewSubPath(x, y);
            } else {
                waveformPath.lineTo(x, y);
            }
        }
        
        g.setColour(colour);
        g.strokePath(waveformPath, juce::PathStrokeType(settings.lineThickness));
    }
    
    // Draw dots mode
    if (settings.drawingMode == DrawingMode::Dots) {
        g.setColour(colour);
        const float dotSize = settings.lineThickness * 2.0f;
        
        for (size_t i = 0; i < data.rmsValues.size(); i += 2) {
            const float x = static_cast<float>(i);
            const float y = midY - data.rmsValues[i] * midY;
            g.fillEllipse(x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
        }
    }
}

void WaveformDisplay::drawGrid(juce::Graphics& g) {
    g.setColour(settings.gridColour);
    
    const float width = static_cast<float>(getWidth());
    const float height = static_cast<float>(getHeight());
    
    // Horizontal lines
    const int numHorizontalLines = 8;
    for (int i = 0; i <= numHorizontalLines; ++i) {
        const float y = (height / numHorizontalLines) * i;
        g.drawLine(0, y, width, y, 0.5f);
    }
    
    // Vertical lines (time-based)
    const double samplesPerPixel = (currentSampleRate / zoomLevel) / width;
    const double pixelsPerSecond = currentSampleRate / samplesPerPixel;
    
    // Draw vertical lines every 0.1 seconds when zoomed in, every second when zoomed out
    const double interval = (zoomLevel > 10.0f) ? 0.1 : 1.0;
    const double startTime = timeOffset;
    const double endTime = startTime + (width / pixelsPerSecond);
    
    for (double t = std::floor(startTime / interval) * interval; t <= endTime; t += interval) {
        const float x = static_cast<float>((t - startTime) * pixelsPerSecond);
        if (x >= 0 && x <= width) {
            g.drawLine(x, 0, x, height, (std::fmod(t, 1.0) < 0.01) ? 1.0f : 0.5f);
        }
    }
}

void WaveformDisplay::drawTimeMarkers(juce::Graphics& g) {
    g.setColour(settings.gridColour.brighter());
    g.setFont(juce::Font(10.0f));
    
    const float width = static_cast<float>(getWidth());
    const double samplesPerPixel = (currentSampleRate / zoomLevel) / width;
    const double pixelsPerSecond = currentSampleRate / samplesPerPixel;
    
    // Draw time markers
    const double interval = (zoomLevel > 10.0f) ? 0.1 : 1.0;
    const double startTime = timeOffset;
    const double endTime = startTime + (width / pixelsPerSecond);
    
    for (double t = std::floor(startTime / interval) * interval; t <= endTime; t += interval) {
        const float x = static_cast<float>((t - startTime) * pixelsPerSecond);
        if (x >= 0 && x <= width) {
            juce::String timeText = juce::String(t, 1) + "s";
            g.drawText(timeText, static_cast<int>(x - 20), 2, 40, 15,
                      juce::Justification::centred);
        }
    }
}

void WaveformDisplay::compileShaders() {
    shaderProgram = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
    
    if (shaderProgram->addVertexShader(vertexShader) &&
        shaderProgram->addFragmentShader(fragmentShader) &&
        shaderProgram->link()) {
        
        positionAttribute = std::make_unique<juce::OpenGLShaderProgram::Attribute>(
            *shaderProgram, "position");
        projectionMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(
            *shaderProgram, "projectionMatrix");
        colour = std::make_unique<juce::OpenGLShaderProgram::Uniform>(
            *shaderProgram, "colour");
    } else {
        shaderProgram.reset();
        openGLAvailable = false;
    }
}

void WaveformDisplay::createVertexBuffers() {
    glGenVertexArrays(1, &inputVAO);
    glGenVertexArrays(1, &outputVAO);
    glGenBuffers(1, &inputVBO);
    glGenBuffers(1, &outputVBO);
}

void WaveformDisplay::updateVertexData() {
    const int width = getWidth();
    inputDownsampled.update(inputBuffer, width, zoomLevel);
    outputDownsampled.update(outputBuffer, width, zoomLevel);
    
    // Update input vertex buffer
    if (inputVAO && inputVBO && !inputDownsampled.minValues.empty()) {
        std::vector<float> vertices;
        const float height = static_cast<float>(getHeight());
        
        if (settings.drawingMode == DrawingMode::Filled) {
            // Create vertices for triangle strip
            for (size_t i = 0; i < inputDownsampled.minValues.size(); ++i) {
                const float x = (static_cast<float>(i) / width) * 2.0f - 1.0f;
                const float minY = -inputDownsampled.minValues[i];
                const float maxY = -inputDownsampled.maxValues[i];
                
                vertices.push_back(x);
                vertices.push_back(minY);
                vertices.push_back(x);
                vertices.push_back(maxY);
            }
        } else {
            // Create vertices for line strip or points
            for (size_t i = 0; i < inputDownsampled.rmsValues.size(); ++i) {
                const float x = (static_cast<float>(i) / width) * 2.0f - 1.0f;
                const float y = -inputDownsampled.rmsValues[i];
                
                vertices.push_back(x);
                vertices.push_back(y);
            }
        }
        
        glBindVertexArray(inputVAO);
        glBindBuffer(GL_ARRAY_BUFFER, inputVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                    vertices.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
    }
    
    // Update output vertex buffer (similar to input)
    if (outputVAO && outputVBO && !outputDownsampled.minValues.empty()) {
        std::vector<float> vertices;
        const float height = static_cast<float>(getHeight());
        
        if (settings.drawingMode == DrawingMode::Filled) {
            for (size_t i = 0; i < outputDownsampled.minValues.size(); ++i) {
                const float x = (static_cast<float>(i) / width) * 2.0f - 1.0f;
                const float minY = -outputDownsampled.minValues[i];
                const float maxY = -outputDownsampled.maxValues[i];
                
                vertices.push_back(x);
                vertices.push_back(minY);
                vertices.push_back(x);
                vertices.push_back(maxY);
            }
        } else {
            for (size_t i = 0; i < outputDownsampled.rmsValues.size(); ++i) {
                const float x = (static_cast<float>(i) / width) * 2.0f - 1.0f;
                const float y = -outputDownsampled.rmsValues[i];
                
                vertices.push_back(x);
                vertices.push_back(y);
            }
        }
        
        glBindVertexArray(outputVAO);
        glBindBuffer(GL_ARRAY_BUFFER, outputVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
                    vertices.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
    }
    
    glBindVertexArray(0);
}

void WaveformDisplay::handleMouseDrag(const juce::MouseEvent& event) {
    const float deltaX = event.position.x - lastMousePosition.x;
    const float width = static_cast<float>(getWidth());
    
    // Calculate time shift based on current zoom level
    const double samplesPerPixel = (currentSampleRate / zoomLevel) / width;
    const double timeShift = deltaX * samplesPerPixel / currentSampleRate;
    
    setTimeOffset(timeOffset - static_cast<float>(timeShift));
    lastMousePosition = event.position;
}

void WaveformDisplay::handleMouseWheel(const juce::MouseWheelDetails& wheel) {
    // Zoom with mouse wheel
    const float zoomFactor = 1.1f;
    const float newZoom = wheel.deltaY > 0 ? zoomLevel * zoomFactor : zoomLevel / zoomFactor;
    setZoomLevel(newZoom);
}

// RingBuffer implementation
void WaveformDisplay::RingBuffer::push(float sample) {
    size_t index = writeIndex.fetch_add(1) % BUFFER_SIZE;
    data[index] = sample;
}

void WaveformDisplay::RingBuffer::pushBatch(const float* samples, int numSamples) {
    size_t currentWrite = writeIndex.load();
    for (int i = 0; i < numSamples; ++i) {
        data[(currentWrite + i) % BUFFER_SIZE] = samples[i];
    }
    writeIndex.fetch_add(numSamples);
}

float WaveformDisplay::RingBuffer::getSample(size_t index) const {
    return data[index % BUFFER_SIZE];
}

size_t WaveformDisplay::RingBuffer::getCurrentSize() const {
    size_t write = writeIndex.load();
    return (write > BUFFER_SIZE) ? BUFFER_SIZE : write;
}

// DownsampledData implementation
void WaveformDisplay::DownsampledData::update(const RingBuffer& buffer, 
                                             int displayWidth, float zoomLevel) {
    const size_t bufferSize = buffer.getCurrentSize();
    if (bufferSize == 0 || displayWidth <= 0) return;
    
    // Calculate downsample factor based on zoom level
    const size_t samplesVisible = static_cast<size_t>(bufferSize / zoomLevel);
    downsampleFactor = std::max(1, static_cast<int>(samplesVisible / displayWidth));
    
    // Resize arrays
    const size_t numPoints = std::min(static_cast<size_t>(displayWidth), 
                                     samplesVisible / downsampleFactor);
    minValues.resize(numPoints);
    maxValues.resize(numPoints);
    rmsValues.resize(numPoints);
    
    // Calculate min/max/RMS for each display point
    const size_t startIndex = bufferSize > samplesVisible ? bufferSize - samplesVisible : 0;
    
    for (size_t i = 0; i < numPoints; ++i) {
        float minVal = 1.0f, maxVal = -1.0f, sum = 0.0f;
        
        for (int j = 0; j < downsampleFactor; ++j) {
            const size_t sampleIndex = startIndex + (i * downsampleFactor + j);
            const float sample = buffer.getSample(sampleIndex);
            
            minVal = std::min(minVal, sample);
            maxVal = std::max(maxVal, sample);
            sum += sample * sample;
        }
        
        minValues[i] = minVal;
        maxValues[i] = maxVal;
        rmsValues[i] = std::sqrt(sum / downsampleFactor);
    }
}

} // namespace ui
} // namespace quiet