cmake_minimum_required(VERSION 3.20)
project(Quiet VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(ENABLE_TRACY "Enable Tracy Profiler" OFF)

# Include custom modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Dependencies
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# JUCE Framework
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.0
    GIT_SHALLOW TRUE
)

# Google Test for testing
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(JUCE)
if(BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
endif()

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
    src/core/AudioBuffer.cpp
    src/core/AudioDeviceManager.cpp
    src/core/ConfigurationManager.cpp
    src/core/EventDispatcher.cpp
    src/core/NoiseReductionProcessor.cpp
    src/core/VirtualDeviceRouter.cpp
    
    # UI Components
    src/ui/MainWindow.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SpectrumAnalyzer.cpp
    
    # Utils
    src/utils/Logger.cpp
    
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
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# CPack Configuration for Installers
set(CPACK_PACKAGE_NAME "QUIET")
set(CPACK_PACKAGE_VENDOR "QUIET Development Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AI-Powered Background Noise Removal")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://quietaudio.com")
set(CPACK_PACKAGE_CONTACT "support@quietaudio.com")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "QUIET")

# Set package file name
set(CPACK_PACKAGE_FILE_NAME "QUIET-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Common files to include
set(CPACK_PACKAGE_EXECUTABLES "Quiet;QUIET")

# Platform-specific installer configurations
if(WIN32)
    # Windows NSIS Installer
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "QUIET")
    set(CPACK_NSIS_PACKAGE_NAME "QUIET - AI-Powered Background Noise Removal")
    set(CPACK_NSIS_HELP_LINK "https://quietaudio.com/support")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://quietaudio.com")
    set(CPACK_NSIS_CONTACT "support@quietaudio.com")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    
    # Create shortcuts
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\QUIET.lnk' '$INSTDIR\\\\bin\\\\Quiet.exe'"
        "CreateShortCut '$DESKTOP\\\\QUIET.lnk' '$INSTDIR\\\\bin\\\\Quiet.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$START_MENU\\\\QUIET.lnk'"
        "Delete '$DESKTOP\\\\QUIET.lnk'"
    )
    
    # Install VB-Cable prompt
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "MessageBox MB_YESNO 'QUIET requires VB-Audio Virtual Cable for virtual device functionality. Would you like to download and install it now?' /SD IDYES IDNO skip_vbcable
         ExecShell 'open' 'https://vb-audio.com/Cable/index.htm'
         skip_vbcable:"
    )
    
    # MUI settings
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.ico")
    
    # Install directory permissions
    set(CPACK_NSIS_DEFINES "RequestExecutionLevel admin")
    
elseif(APPLE)
    # macOS DMG Installer
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "QUIET")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/installer/macos/dmg_setup.scpt")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/installer/macos/dmg_background.png")
    
    # Bundle settings
    set(CPACK_BUNDLE_NAME "QUIET")
    set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/icon.icns")
    set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/installer/macos/Info.plist")
    
    # Code signing (configure these with your certificate)
    if(DEFINED ENV{MACOS_CERTIFICATE_NAME})
        set(CPACK_BUNDLE_APPLE_CERT_APP "$ENV{MACOS_CERTIFICATE_NAME}")
        set(CPACK_BUNDLE_APPLE_CODESIGN_PARAMETER "--deep --force --verify --verbose --options runtime")
    endif()
    
elseif(UNIX)
    # Linux installers
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    
    # Debian package
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "QUIET Development Team")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasound2, libfreetype6, libx11-6, libxext6, libxinerama1, libxrandr2, libxcursor1, libgl1")
    
    # RPM package
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_REQUIRES "alsa-lib, freetype, libX11, libXext, libXinerama, libXrandr, libXcursor, mesa-libGL")
endif()

# Include CPack
include(CPack)