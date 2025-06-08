cmake_minimum_required(VERSION 3.20)

project(QUIET VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# Options
option(QUIET_BUILD_TESTS "Build tests" ON)
option(QUIET_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(QUIET_STATIC_RUNTIME "Static runtime linking" OFF)

# Platform detection
if(WIN32)
    set(QUIET_PLATFORM_WINDOWS TRUE)
elseif(APPLE)
    set(QUIET_PLATFORM_MACOS TRUE)
elseif(UNIX)
    set(QUIET_PLATFORM_LINUX TRUE)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
    if(QUIET_STATIC_RUNTIME)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    add_compile_options(-Wall -Wextra -Werror -Wpedantic)
    if(QUIET_ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Find packages
find_package(PkgConfig REQUIRED)

# JUCE Framework
add_subdirectory(third_party/JUCE)

# RNNoise
find_path(RNNOISE_INCLUDE_DIR rnnoise.h)
find_library(RNNOISE_LIBRARY rnnoise)

if(NOT RNNOISE_INCLUDE_DIR OR NOT RNNOISE_LIBRARY)
    message(STATUS "RNNoise not found, building from source")
    add_subdirectory(third_party/rnnoise)
    set(RNNOISE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise/include)
    set(RNNOISE_LIBRARY rnnoise)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${RNNOISE_INCLUDE_DIR}
)

# Source files
set(QUIET_CORE_SOURCES
    src/core/AudioDeviceManager.cpp
    src/core/NoiseReductionProcessor.cpp
    src/core/VirtualDeviceRouter.cpp
    src/core/AudioBuffer.cpp
    src/core/EventDispatcher.cpp
    src/core/ConfigurationManager.cpp
)

set(QUIET_UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/SystemTrayController.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SpectrumAnalyzer.cpp
    src/ui/DeviceSelector.cpp
)

set(QUIET_UTILS_SOURCES
    src/utils/AudioUtils.cpp
    src/utils/Logger.cpp
    src/utils/PerformanceMonitor.cpp
)

# Platform-specific sources
if(QUIET_PLATFORM_WINDOWS)
    list(APPEND QUIET_CORE_SOURCES
        src/platform/windows/WindowsAudioDevice.cpp
        src/platform/windows/VBCableIntegration.cpp
    )
elseif(QUIET_PLATFORM_MACOS)
    list(APPEND QUIET_CORE_SOURCES
        src/platform/macos/CoreAudioDevice.cpp
        src/platform/macos/BlackHoleIntegration.cpp
    )
endif()

# Main executable
add_executable(QUIET
    src/main.cpp
    ${QUIET_CORE_SOURCES}
    ${QUIET_UI_SOURCES}
    ${QUIET_UTILS_SOURCES}
)

# Link libraries
target_link_libraries(QUIET PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    ${RNNOISE_LIBRARY}
)

# Platform-specific libraries
if(QUIET_PLATFORM_WINDOWS)
    target_link_libraries(QUIET PRIVATE
        winmm
        ole32
        oleaut32
        imm32
        comdlg32
        shlwapi
        rpcrt4
        wininet
        version
        ws2_32
        setupapi
    )
elseif(QUIET_PLATFORM_MACOS)
    target_link_libraries(QUIET PRIVATE
        "-framework Cocoa"
        "-framework CoreAudio"
        "-framework CoreMIDI"
        "-framework AudioUnit"
        "-framework AudioToolbox"
        "-framework Accelerate"
        "-framework IOKit"
        "-framework Carbon"
        "-framework WebKit"
    )
endif()

# Preprocessor definitions
target_compile_definitions(QUIET PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:QUIET,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:QUIET,JUCE_VERSION>"
    QUIET_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    QUIET_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    QUIET_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

# JUCE app properties
set_target_properties(QUIET PROPERTIES
    JUCE_COMPANY_NAME "QUIET Audio"
    JUCE_PRODUCT_NAME "QUIET"
    JUCE_VERSION "${PROJECT_VERSION}"
    JUCE_DESCRIPTION "AI-Powered Background Noise Removal"
    JUCE_BUNDLE_ID "com.quietaudio.quiet"
)

# Resource files
juce_add_binary_data(QuietBinaryData
    SOURCES
        resources/icons/app_icon.png
        resources/config/default_settings.json
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
)

target_link_libraries(QUIET PRIVATE QuietBinaryData)

# Tests
if(QUIET_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS QUIET
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(QUIET_PLATFORM_MACOS)
    # macOS app bundle
    set_target_properties(QUIET PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "QUIET")
set(CPACK_PACKAGE_VENDOR "QUIET Audio")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AI-Powered Background Noise Removal")

if(QUIET_PLATFORM_WINDOWS)
    set(CPACK_GENERATOR "WIX;ZIP")
    set(CPACK_WIX_UPGRADE_GUID "12345678-1234-1234-1234-123456789012")
elseif(QUIET_PLATFORM_MACOS)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_FORMAT "UDZO")
endif()

include(CPack)