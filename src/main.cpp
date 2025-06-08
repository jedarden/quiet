#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "quiet/core/EventDispatcher.h"
#include "quiet/core/AudioDeviceManager.h"
#include "quiet/core/NoiseReductionProcessor.h"
#include "quiet/core/ConfigurationManager.h"
#include "quiet/ui/MainWindow.h"
#include "quiet/ui/SystemTrayController.h"
#include "quiet/utils/Logger.h"

/**
 * @brief Main QUIET application class
 * 
 * Coordinates all subsystems and manages the application lifecycle.
 */
class QuietApplication : public juce::JUCEApplication {
public:
    QuietApplication() = default;

    const juce::String getApplicationName() override {
        return ProjectInfo::projectName;
    }

    const juce::String getApplicationVersion() override {
        return ProjectInfo::versionString;
    }

    bool moreThanOneInstanceAllowed() override {
        return false;
    }

    void initialise(const juce::String& commandLine) override {
        // Initialize logging
        quiet::utils::Logger::getInstance().initialize("QUIET", quiet::utils::Logger::Level::Info);
        LOG_INFO("Starting QUIET application v{}", getApplicationVersion().toStdString());

        // Parse command line arguments
        parseCommandLine(commandLine);

        // Initialize core subsystems
        if (!initializeSubsystems()) {
            LOG_ERROR("Failed to initialize core subsystems");
            setApplicationReturnValue(1);
            quit();
            return;
        }

        // Create and show main window
        createMainWindow();
        
        LOG_INFO("QUIET application initialized successfully");
    }

    void shutdown() override {
        LOG_INFO("Shutting down QUIET application");
        
        // Save configuration
        if (m_configManager) {
            m_configManager->saveConfiguration();
        }
        
        // Shutdown subsystems in reverse order
        m_mainWindow.reset();
        m_systemTray.reset();
        
        if (m_audioManager) {
            m_audioManager->stopAudio();
            m_audioManager->shutdown();
        }
        
        m_noiseProcessor.reset();
        m_audioManager.reset();
        m_configManager.reset();
        
        if (m_eventDispatcher) {
            m_eventDispatcher->stop();
        }
        m_eventDispatcher.reset();
        
        LOG_INFO("QUIET application shutdown complete");
    }

    void systemRequestedQuit() override {
        // Allow graceful shutdown
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override {
        // Show main window if another instance is started
        if (m_mainWindow) {
            m_mainWindow->showWindow();
        }
    }

private:
    void parseCommandLine(const juce::String& commandLine) {
        juce::StringArray args;
        args.addTokens(commandLine, true);
        
        for (const auto& arg : args) {
            if (arg == "--minimized" || arg == "-m") {
                m_startMinimized = true;
            } else if (arg == "--debug" || arg == "-d") {
                quiet::utils::Logger::getInstance().setLevel(quiet::utils::Logger::Level::Debug);
            } else if (arg == "--help" || arg == "-h") {
                showUsage();
                quit();
                return;
            }
        }
    }

    void showUsage() {
        std::cout << "QUIET - AI-Powered Background Noise Removal\n"
                  << "Usage: QUIET [options]\n"
                  << "\nOptions:\n"
                  << "  -m, --minimized    Start minimized to system tray\n"
                  << "  -d, --debug        Enable debug logging\n"
                  << "  -h, --help         Show this help message\n"
                  << std::endl;
    }

    bool initializeSubsystems() {
        try {
            // Event dispatcher
            m_eventDispatcher = std::make_unique<quiet::core::EventDispatcher>();
            m_eventDispatcher->start();

            // Configuration manager
            m_configManager = std::make_unique<quiet::core::ConfigurationManager>(*m_eventDispatcher);
            if (!m_configManager->initialize()) {
                LOG_ERROR("Failed to initialize configuration manager");
                return false;
            }

            // Audio device manager
            m_audioManager = std::make_unique<quiet::core::AudioDeviceManager>(*m_eventDispatcher);
            if (!m_audioManager->initialize()) {
                LOG_ERROR("Failed to initialize audio device manager");
                return false;
            }

            // Noise reduction processor
            m_noiseProcessor = std::make_unique<quiet::core::NoiseReductionProcessor>(*m_eventDispatcher);
            if (!m_noiseProcessor->initialize()) {
                LOG_ERROR("Failed to initialize noise reduction processor");
                return false;
            }

            // Connect audio pipeline
            m_audioManager->setAudioCallback([this](const quiet::core::AudioBuffer& buffer) {
                handleAudioCallback(buffer);
            });

            // System tray controller
            m_systemTray = std::make_unique<quiet::ui::SystemTrayController>(
                *m_eventDispatcher, *m_audioManager, *m_noiseProcessor);

            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception during subsystem initialization: {}", e.what());
            return false;
        }
    }

    void createMainWindow() {
        m_mainWindow = std::make_unique<quiet::ui::MainWindow>(
            "QUIET", *m_eventDispatcher, *m_audioManager, *m_noiseProcessor);

        if (m_startMinimized) {
            m_mainWindow->minimizeToTray();
        } else {
            m_mainWindow->showWindow();
        }
    }

    void handleAudioCallback(const quiet::core::AudioBuffer& inputBuffer) {
        // Process audio through noise reduction
        if (m_noiseProcessor && m_noiseProcessor->isEnabled()) {
            auto processedBuffer = inputBuffer;  // Copy for processing
            m_noiseProcessor->process(processedBuffer);
            
            // Send processed audio to virtual device
            // This would be handled by VirtualDeviceRouter in full implementation
        }
    }

    // Subsystem instances
    std::unique_ptr<quiet::core::EventDispatcher> m_eventDispatcher;
    std::unique_ptr<quiet::core::ConfigurationManager> m_configManager;
    std::unique_ptr<quiet::core::AudioDeviceManager> m_audioManager;
    std::unique_ptr<quiet::core::NoiseReductionProcessor> m_noiseProcessor;
    
    std::unique_ptr<quiet::ui::MainWindow> m_mainWindow;
    std::unique_ptr<quiet::ui::SystemTrayController> m_systemTray;

    // Command line options
    bool m_startMinimized{false};
};

// Application entry point
START_JUCE_APPLICATION(QuietApplication)