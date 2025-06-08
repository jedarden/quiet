cmake_minimum_required(VERSION 3.20)

# Google Test
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9fd7188ec8e896187e04b12.zip
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Test sources
set(TEST_SOURCES
    unit/AudioBufferTest.cpp
    unit/NoiseReductionProcessorTest.cpp
    unit/AudioDeviceManagerTest.cpp
    unit/EventDispatcherTest.cpp
    integration/AudioPipelineTest.cpp
    integration/UIIntegrationTest.cpp
    TestUtils.cpp
)

# Test executable
add_executable(QuietTests ${TEST_SOURCES})

target_link_libraries(QuietTests PRIVATE
    gtest
    gtest_main
    gmock
    gmock_main
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_core
    ${RNNOISE_LIBRARY}
)

target_include_directories(QuietTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(QuietTests PRIVATE
    JUCE_APPLICATION_NAME_STRING="QuietTests"
    JUCE_APPLICATION_VERSION_STRING="1.0.0"
)

# Discover tests
include(GoogleTest)
gtest_discover_tests(QuietTests)