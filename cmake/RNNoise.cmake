# RNNoise integration for Quiet
# This file handles downloading and building RNNoise

include(FetchContent)

# Option to use system RNNoise if available
option(USE_SYSTEM_RNNOISE "Use system-installed RNNoise instead of building from source" OFF)

if(USE_SYSTEM_RNNOISE)
    # Try to find system RNNoise
    find_path(RNNOISE_INCLUDE_DIR rnnoise.h)
    find_library(RNNOISE_LIBRARY rnnoise)
    
    if(RNNOISE_INCLUDE_DIR AND RNNOISE_LIBRARY)
        message(STATUS "Found system RNNoise: ${RNNOISE_LIBRARY}")
        add_library(rnnoise INTERFACE)
        target_include_directories(rnnoise INTERFACE ${RNNOISE_INCLUDE_DIR})
        target_link_libraries(rnnoise INTERFACE ${RNNOISE_LIBRARY})
    else()
        message(FATAL_ERROR "System RNNoise not found. Set USE_SYSTEM_RNNOISE=OFF to build from source.")
    endif()
else()
    # Build RNNoise from source
    # Since RNNoise uses autotools, we'll compile it manually
    
    set(RNNOISE_SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/rnnoise-src)
    set(RNNOISE_BUILD_DIR ${CMAKE_BINARY_DIR}/_deps/rnnoise-build)
    
    # Download RNNoise source
    FetchContent_Declare(
        rnnoise_download
        GIT_REPOSITORY https://github.com/xiph/rnnoise.git
        GIT_TAG        v0.2
        SOURCE_DIR     ${RNNOISE_SOURCE_DIR}
    )
    
    FetchContent_GetProperties(rnnoise_download)
    if(NOT rnnoise_download_POPULATED)
        FetchContent_Populate(rnnoise_download)
        
        # Create build directory
        file(MAKE_DIRECTORY ${RNNOISE_BUILD_DIR})
        
        # Compile RNNoise files directly
        set(RNNOISE_SOURCES
            ${RNNOISE_SOURCE_DIR}/src/denoise.c
            ${RNNOISE_SOURCE_DIR}/src/rnn.c
            ${RNNOISE_SOURCE_DIR}/src/rnn_data.c
            ${RNNOISE_SOURCE_DIR}/src/rnn_reader.c
            ${RNNOISE_SOURCE_DIR}/src/pitch.c
            ${RNNOISE_SOURCE_DIR}/src/kiss_fft.c
            ${RNNOISE_SOURCE_DIR}/src/celt_lpc.c
        )
        
        # Create static library
        add_library(rnnoise STATIC ${RNNOISE_SOURCES})
        
        # Set include directories
        target_include_directories(rnnoise PUBLIC 
            ${RNNOISE_SOURCE_DIR}/include
            ${RNNOISE_SOURCE_DIR}/src
        )
        
        # Set compile definitions
        target_compile_definitions(rnnoise PRIVATE
            HAVE_CONFIG_H=0
            OPUS_BUILD=0
        )
        
        # Platform-specific settings
        if(UNIX)
            target_compile_options(rnnoise PRIVATE -fPIC)
        endif()
        
        # Optimize for release builds
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            target_compile_options(rnnoise PRIVATE -O3 -ffast-math)
        endif()
        
        # Create rnnoise.h in the expected location if it doesn't exist
        if(NOT EXISTS ${RNNOISE_SOURCE_DIR}/include/rnnoise.h)
            file(MAKE_DIRECTORY ${RNNOISE_SOURCE_DIR}/include)
            file(COPY ${RNNOISE_SOURCE_DIR}/include/rnnoise.h 
                 DESTINATION ${RNNOISE_SOURCE_DIR}/include/
                 FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
        endif()
    endif()
endif()