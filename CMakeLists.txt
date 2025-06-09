cmake_minimum_required(VERSION 3.15)
project(Quiet VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE
add_subdirectory(JUCE)

# Find packages
find_package(Threads REQUIRED)

# Include ExternalProject for downloading dependencies
include(ExternalProject)

# Define the main executable
juce_add_gui_app(Quiet
    PRODUCT_NAME "Quiet"
    COMPANY_NAME "Quiet Development Team"
    BUNDLE_ID "com.quietapp.quiet"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon_512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon_128.png"
)

# Add source files
target_sources(Quiet PRIVATE
    # Main
    src/main.cpp
    
    # Core
    src/core/AudioProcessor.cpp
    src/core/AudioDeviceManager.cpp
    src/core/NoiseReductionProcessor.cpp
    src/core/VirtualDevice.cpp
    
    # UI Components
    src/ui/MainWindow.cpp
    src/ui/SystemTray.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SpectrumAnalyzer.cpp
    
    # Platform specific
    $<$<PLATFORM_ID:Windows>:src/platform/windows/WASAPIDevice.cpp>
    $<$<PLATFORM_ID:Windows>:src/platform/windows/VBCableIntegration.cpp>
    $<$<PLATFORM_ID:Darwin>:src/platform/macos/CoreAudioDevice.cpp>
    $<$<PLATFORM_ID:Darwin>:src/platform/macos/BlackHoleIntegration.cpp>
)

# Add include directories
target_include_directories(Quiet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/quiet
)

# JUCE configuration
target_compile_definitions(Quiet PRIVATE
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

# Link libraries
target_link_libraries(Quiet PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
    Threads::Threads
)

# Add math library for Linux
if(UNIX AND NOT APPLE)
    target_link_libraries(Quiet PRIVATE m)
endif()

# Platform-specific settings
if(WIN32)
    target_link_libraries(Quiet PRIVATE
        winmm
        wsock32
        ws2_32
        version
        Shlwapi
        Dbghelp
    )
elseif(APPLE)
    find_library(COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(COREMIDI_LIBRARY CoreMIDI REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(FOUNDATION_LIBRARY Foundation REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)
    find_library(ACCELERATE_LIBRARY Accelerate REQUIRED)
    
    target_link_libraries(Quiet PRIVATE
        ${COREAUDIO_LIBRARY}
        ${COREMIDI_LIBRARY}
        ${IOKIT_LIBRARY}
        ${FOUNDATION_LIBRARY}
        ${APPKIT_LIBRARY}
        ${ACCELERATE_LIBRARY}
    )
endif()

# Enable OpenGL for spectrum analyzer performance
target_compile_definitions(Quiet PRIVATE
    JUCE_OPENGL=1
)

# Add RNNoise library as external project
set(RNNOISE_PREFIX ${CMAKE_BINARY_DIR}/rnnoise)
set(RNNOISE_INSTALL_DIR ${RNNOISE_PREFIX}/install)

# Option to use system RNNoise if available
option(USE_SYSTEM_RNNOISE "Use system-installed RNNoise instead of building from source" OFF)

if(USE_SYSTEM_RNNOISE)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(RNNOISE rnnoise)
    endif()
    
    if(RNNOISE_FOUND)
        target_include_directories(Quiet PRIVATE ${RNNOISE_INCLUDE_DIRS})
        target_link_libraries(Quiet PRIVATE ${RNNOISE_LIBRARIES})
    else()
        message(FATAL_ERROR "System RNNoise not found. Set USE_SYSTEM_RNNOISE=OFF to build from source.")
    endif()
else()
    # Build RNNoise from source
    ExternalProject_Add(
        rnnoise_external
        GIT_REPOSITORY https://github.com/xiph/rnnoise.git
        GIT_TAG v0.2  # Use a stable release tag
        PREFIX ${RNNOISE_PREFIX}
        CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${RNNOISE_INSTALL_DIR}
            -DCMAKE_BUILD_TYPE=Release
            -DBUILD_SHARED_LIBS=OFF
            -DRNNOISE_COMPILE_OPUS=OFF
            -DRNNOISE_COMPILE_EXAMPLES=OFF
        BUILD_IN_SOURCE 0
    )
    
    # Alternative: If RNNoise doesn't have CMakeLists.txt, use custom build
    if(NOT EXISTS ${RNNOISE_PREFIX}/src/rnnoise_external/CMakeLists.txt)
        ExternalProject_Add(
            rnnoise_external_autotools
            GIT_REPOSITORY https://github.com/xiph/rnnoise.git
            GIT_TAG v0.2
            PREFIX ${RNNOISE_PREFIX}
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${RNNOISE_INSTALL_DIR}/include
                && ${CMAKE_COMMAND} -E make_directory ${RNNOISE_INSTALL_DIR}/lib
                && cd ${RNNOISE_PREFIX}/src/rnnoise_external_autotools
                && test -f autogen.sh && ./autogen.sh || autoreconf -fi
                && ./configure --prefix=${RNNOISE_INSTALL_DIR} --disable-examples --disable-doc
            BUILD_COMMAND cd ${RNNOISE_PREFIX}/src/rnnoise_external_autotools && make
            INSTALL_COMMAND cd ${RNNOISE_PREFIX}/src/rnnoise_external_autotools && make install
            BUILD_IN_SOURCE 1
        )
        set(rnnoise_target rnnoise_external_autotools)
    else()
        set(rnnoise_target rnnoise_external)
    endif()
    
    # Create imported target for RNNoise
    add_library(rnnoise STATIC IMPORTED)
    set_target_properties(rnnoise PROPERTIES
        IMPORTED_LOCATION ${RNNOISE_INSTALL_DIR}/lib/librnnoise.a
        INTERFACE_INCLUDE_DIRECTORIES ${RNNOISE_INSTALL_DIR}/include
    )
    add_dependencies(rnnoise ${rnnoise_target})
    
    # Add RNNoise include directory
    target_include_directories(Quiet PRIVATE
        ${RNNOISE_INSTALL_DIR}/include
    )
    
    # Link RNNoise library
    target_link_libraries(Quiet PRIVATE rnnoise)
endif()

# Installation
install(TARGETS Quiet
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION Applications
)

# Testing
enable_testing()
add_subdirectory(tests)