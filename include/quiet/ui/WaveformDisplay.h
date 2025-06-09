#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <atomic>
#include <array>
#include <mutex>

namespace quiet {
namespace ui {

/**
 * @brief Real-time waveform display component with OpenGL acceleration
 * 
 * Displays audio waveforms with efficient rendering, supporting zoom, pan,
 * and multiple drawing modes. Thread-safe for real-time audio updates.
 */
class WaveformDisplay : public juce::Component,
                       public juce::OpenGLRenderer,
                       private juce::Timer {
public:
    enum class DrawingMode {
        Line,
        Filled,
        Dots
    };

    enum class ChannelMode {
        Input,
        Output,
        Both
    };

    struct WaveformSettings {
        DrawingMode drawingMode = DrawingMode::Line;
        ChannelMode channelMode = ChannelMode::Both;
        juce::Colour inputWaveformColour = juce::Colours::cyan;
        juce::Colour outputWaveformColour = juce::Colours::lightgreen;
        juce::Colour backgroundColour = juce::Colour(0xff1e1e1e);
        juce::Colour gridColour = juce::Colour(0xff333333);
        float lineThickness = 1.5f;
        int refreshRate = 60; // Hz
        bool showGrid = true;
        bool showTimeMarkers = true;
        bool antialiasing = true;
    };

    WaveformDisplay();
    ~WaveformDisplay() override;

    // Audio data push methods (thread-safe)
    void pushInputSample(float sample);
    void pushOutputSample(float sample);
    void pushInputBuffer(const float* buffer, int numSamples);
    void pushOutputBuffer(const float* buffer, int numSamples);

    // Configuration
    void setSettings(const WaveformSettings& settings);
    WaveformSettings getSettings() const { return settings; }
    
    // Zoom and pan
    void setZoomLevel(float zoom); // 1.0 = normal, >1.0 = zoomed in
    float getZoomLevel() const { return zoomLevel; }
    void setTimeOffset(float offsetInSeconds);
    float getTimeOffset() const { return timeOffset; }
    
    // Sample rate for time calculations
    void setSampleRate(double sampleRate);
    double getSampleRate() const { return currentSampleRate; }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // OpenGLRenderer overrides
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    // Ring buffer for waveform data
    static constexpr size_t BUFFER_SIZE = 48000 * 10; // 10 seconds at 48kHz
    
    struct RingBuffer {
        std::array<float, BUFFER_SIZE> data{};
        std::atomic<size_t> writeIndex{0};
        size_t readIndex{0};
        
        void push(float sample);
        void pushBatch(const float* samples, int numSamples);
        float getSample(size_t index) const;
        size_t getCurrentSize() const;
    };

    // Downsampling for efficient display
    struct DownsampledData {
        std::vector<float> minValues;
        std::vector<float> maxValues;
        std::vector<float> rmsValues;
        int downsampleFactor{1};
        
        void update(const RingBuffer& buffer, int displayWidth, float zoomLevel);
    };

    // Timer callback
    void timerCallback() override;

    // Rendering methods
    void renderWithOpenGL();
    void renderWithSoftware(juce::Graphics& g);
    void drawWaveform(juce::Graphics& g, const DownsampledData& data, 
                     juce::Colour colour, bool filled);
    void drawGrid(juce::Graphics& g);
    void drawTimeMarkers(juce::Graphics& g);
    
    // OpenGL rendering
    void compileShaders();
    void createVertexBuffers();
    void updateVertexData();
    
    // Mouse interaction
    void handleMouseDrag(const juce::MouseEvent& event);
    void handleMouseWheel(const juce::MouseWheelDetails& wheel);

    // Data members
    RingBuffer inputBuffer;
    RingBuffer outputBuffer;
    DownsampledData inputDownsampled;
    DownsampledData outputDownsampled;
    
    WaveformSettings settings;
    std::atomic<float> zoomLevel{1.0f};
    std::atomic<float> timeOffset{0.0f};
    std::atomic<double> currentSampleRate{48000.0};
    
    // OpenGL resources
    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> positionAttribute;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> projectionMatrix;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colour;
    
    GLuint inputVBO{0}, outputVBO{0};
    GLuint inputVAO{0}, outputVAO{0};
    
    // Mouse interaction state
    juce::Point<float> lastMousePosition;
    bool isDragging{false};
    
    // Thread safety
    mutable std::mutex settingsMutex;
    std::atomic<bool> needsRepaint{false};
    std::atomic<bool> openGLAvailable{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace ui
} // namespace quiet