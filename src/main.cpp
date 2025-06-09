#include <JuceHeader.h>
#include <memory>
#include <iostream>
#include "quiet/core/EventDispatcher.h"
#include "quiet/core/AudioDeviceManager.h"
#include "quiet/core/NoiseReductionProcessor.h"
#include "quiet/core/ConfigurationManager.h"
#include "quiet/core/VirtualDeviceRouter.h"
#include "quiet/ui/MainWindow.h"
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
        quiet::utils::Logger::getInstance().initialize("quiet.log", quiet::utils::Logger::Level::INFO);
        quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::INFO, "QUIET",
            "Starting QUIET application v" + getApplicationVersion().toStdString());

        // Parse command line arguments
        parseCommandLine(commandLine);

        // Initialize core subsystems
        if (!initializeSubsystems()) {
            quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::ERROR, "QUIET",
                "Failed to initialize core subsystems");
            setApplicationReturnValue(1);
            quit();
            return;
        }

        // Create and show main window
        createMainWindow();
        
        quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::INFO, "QUIET",
            "QUIET application initialized successfully");
    }

    void shutdown() override {
        quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::INFO, "QUIET",
            "Shutting down QUIET application");
        
        // Save configuration
        if (m_configManager) {
            m_configManager->saveConfiguration();
        }
        
        // Stop audio processing
        if (m_virtualRouter) {
            m_virtualRouter->stopRouting();
        }
        
        if (m_audioManager) {
            m_audioManager->stopAudioStream();
        }
        
        // Shutdown subsystems in reverse order
        m_mainWindow.reset();
        m_virtualRouter.reset();
        m_noiseProcessor.reset();
        m_audioManager.reset();
        m_configManager.reset();
        m_eventDispatcher.reset();
        
        quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::INFO, "QUIET",
            "QUIET application shutdown complete");
    }

    void systemRequestedQuit() override {
        // Allow graceful shutdown
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override {
        // Show main window if another instance is started
        if (m_mainWindow) {
            m_mainWindow->setVisible(true);
            m_mainWindow->toFront(true);
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
                quiet::utils::Logger::getInstance().setLevel(quiet::utils::Logger::Level::DEBUG);
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
            m_configManager->loadConfiguration();

            // Audio device manager
            m_audioManager = std::make_unique<quiet::core::AudioDeviceManager>(*m_eventDispatcher);
            if (!m_audioManager->initialize()) {
                quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::ERROR, "QUIET",
                    "Failed to initialize audio device manager");
                return false;
            }

            // Noise reduction processor
            m_noiseProcessor = std::make_unique<quiet::core::NoiseReductionProcessor>(*m_eventDispatcher);
            if (!m_noiseProcessor->initialize()) {
                quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::ERROR, "QUIET",
                    "Failed to initialize noise reduction processor");
                return false;
            }
            
            // Virtual device router
            m_virtualRouter = std::make_unique<quiet::core::VirtualDeviceRouter>(*m_eventDispatcher);
            if (!m_virtualRouter->initialize()) {
                quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::WARNING, "QUIET",
                    "Virtual device router initialization failed - routing disabled");
            }
            
            // Check virtual device installation
            checkVirtualDeviceSetup();

            // Connect audio pipeline
            m_audioManager->setAudioCallback(
                [this](const quiet::core::AudioBuffer& input, quiet::core::AudioBuffer& output) {
                    processAudioBlock(input, output);
                });
                
            // Start audio stream
            if (!m_audioManager->startAudioStream()) {
                quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::ERROR, "QUIET",
                    "Failed to start audio stream");
                return false;
            }
            
            // Start virtual routing if available
            if (m_virtualRouter && m_virtualRouter->hasVirtualDevice()) {
                m_virtualRouter->startRouting();
            }

            return true;
        } catch (const std::exception& e) {
            quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::ERROR, "QUIET",
                std::string("Exception during subsystem initialization: ") + e.what());
            return false;
        }
    }

    void createMainWindow() {
        m_mainWindow = quiet::ui::MainWindow::create(
            *m_audioManager, *m_configManager, *m_eventDispatcher);

        if (!m_mainWindow) {
            quiet::utils::Logger::getInstance().log(quiet::utils::Logger::Level::ERROR, "QUIET",
                "Failed to create main window");
            return;
        }
        
        if (!m_startMinimized) {
            m_mainWindow->setVisible(true);
        }
    }

    void processAudioBlock(const quiet::core::AudioBuffer& input, quiet::core::AudioBuffer& output) {
        // Process audio through noise reduction
        if (m_noiseProcessor && m_noiseProcessor->isInitialized()) {
            // Create a copy for processing
            quiet::core::AudioBuffer processedBuffer(input.getNumChannels(),
                                                   input.getNumSamples(),
                                                   input.getSampleRate());
            processedBuffer.copyFrom(input);
            
            // Apply noise reduction
            m_noiseProcessor->processBuffer(processedBuffer);
            
            // Copy to output
            output.copyFrom(processedBuffer);
            
            // Route to virtual device if enabled
            if (m_virtualRouter && m_virtualRouter->isRouting()) {
                m_virtualRouter->routeAudioBuffer(output);
            }
            
            // Update level meters
            publishAudioLevels(input, output);
        } else {
            // Passthrough if processing is disabled
            output.copyFrom(input);
        }
    }
    
    void publishAudioLevels(const quiet::core::AudioBuffer& input,
                           const quiet::core::AudioBuffer& output) {
        float inputLevel = input.getRMSLevel(0, 0, input.getNumSamples());
        float outputLevel = output.getRMSLevel(0, 0, output.getNumSamples());
        
        auto eventData = quiet::core::EventDataFactory::createAudioLevelData(inputLevel, true);
        m_eventDispatcher->publish(quiet::core::EventType::AudioLevelChanged, eventData);
        
        eventData = quiet::core::EventDataFactory::createAudioLevelData(outputLevel, false);
        m_eventDispatcher->publish(quiet::core::EventType::AudioLevelChanged, eventData);
    }
    
    void checkVirtualDeviceSetup() {
        if (!quiet::core::VirtualDeviceRouter::isVirtualDeviceInstalled()) {
            auto result = juce::AlertWindow::showYesNoCancelBox(
                juce::AlertWindow::InfoIcon,
                "Virtual Audio Device Required",
                "QUIET requires a virtual audio device to route processed audio to other applications.\n\n" +
                quiet::core::VirtualDeviceRouter::getVirtualDeviceInstallInstructions() + "\n\n" +
                "Would you like to open the download page?",
                "Open Download Page",
                "Continue Without",
                "Quit"
            );
            
            if (result == 1) {  // Yes - Open download page
            #ifdef _WIN32
                juce::URL("https://vb-audio.com/Cable/").launchInDefaultBrowser();
            #elif __APPLE__
                juce::URL("https://existential.audio/blackhole/").launchInDefaultBrowser();
            #endif
            } else if (result == 0) {  // Cancel - Quit
                quit();
            }
            // Continue without virtual device if result == 2
        }
    }

    // Subsystem instances
    std::unique_ptr<quiet::core::EventDispatcher> m_eventDispatcher;
    std::unique_ptr<quiet::core::ConfigurationManager> m_configManager;
    std::unique_ptr<quiet::core::AudioDeviceManager> m_audioManager;
    std::unique_ptr<quiet::core::NoiseReductionProcessor> m_noiseProcessor;
    std::unique_ptr<quiet::core::VirtualDeviceRouter> m_virtualRouter;
    
    std::unique_ptr<quiet::ui::MainWindow> m_mainWindow;

    // Command line options
    bool m_startMinimized{false};
};

// Application entry point
START_JUCE_APPLICATION(QuietApplication)