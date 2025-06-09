cmake_minimum_required(VERSION 3.20)

# Enable testing
enable_testing()

# Create a library with common test utilities
add_library(quiet_test_utils STATIC
    TestUtils.cpp
    TestUtils.h
)

target_include_directories(quiet_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(quiet_test_utils PUBLIC
    GTest::gtest
    GTest::gmock
)

# Core library for tests
add_library(quiet_core STATIC
    ${CMAKE_SOURCE_DIR}/src/core/AudioBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AudioDeviceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/NoiseReductionProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/VirtualDeviceRouter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/EventDispatcher.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ConfigurationManager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
)

target_include_directories(quiet_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${RNNOISE_INSTALL_DIR}/include
)

target_link_libraries(quiet_core PUBLIC
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_events
    juce::juce_dsp
    rnnoise
    Threads::Threads
)

# Unit tests
add_executable(quiet_unit_tests
    unit/AudioBufferTest.cpp
    unit/NoiseReductionProcessorTest.cpp
    unit/AudioDeviceManagerTest.cpp
    unit/VirtualDeviceRouterTest.cpp
    unit/LoggerTest.cpp
)

target_link_libraries(quiet_unit_tests PRIVATE
    quiet_core
    quiet_test_utils
    GTest::gtest_main
    GTest::gmock
)

# Integration tests
add_executable(quiet_integration_tests
    integration/AudioPipelineTest.cpp
)

target_link_libraries(quiet_integration_tests PRIVATE
    quiet_core
    quiet_test_utils
    GTest::gtest_main
    GTest::gmock
)

# Performance tests
add_executable(quiet_performance_tests
    performance/PerformanceValidation.cpp
)

# Fetch Google Benchmark if not found
find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(benchmark)
endif()

target_link_libraries(quiet_performance_tests PRIVATE
    quiet_core
    quiet_test_utils
    GTest::gtest_main
    benchmark::benchmark
)

# Platform-specific settings
if(WIN32)
    set(PLATFORM_LIBS winmm ole32 oleaut32 uuid ws2_32)
elseif(APPLE)
    set(PLATFORM_LIBS "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreMIDI")
else()
    set(PLATFORM_LIBS)
endif()

target_link_libraries(quiet_core PUBLIC ${PLATFORM_LIBS})

# Add tests to CTest
add_test(NAME UnitTests COMMAND quiet_unit_tests)
add_test(NAME IntegrationTests COMMAND quiet_integration_tests)
add_test(NAME PerformanceTests COMMAND quiet_performance_tests)

# Set test properties
set_tests_properties(UnitTests PROPERTIES
    TIMEOUT 60
    LABELS "unit"
)

set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 120
    LABELS "integration"
)

set_tests_properties(PerformanceTests PROPERTIES
    TIMEOUT 300
    LABELS "performance"
)

# Enable code coverage if requested
option(ENABLE_COVERAGE "Enable code coverage" OFF)
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(quiet_core PRIVATE -fprofile-arcs -ftest-coverage)
    target_link_options(quiet_core PRIVATE -fprofile-arcs -ftest-coverage)
endif()

# Create test output directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_results)