cmake_minimum_required(VERSION 3.22)
project(qk4core VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Platform detection
if(NOT DEFINED QK4_PLATFORM)
    if(ANDROID)
        set(QK4_PLATFORM "ANDROID")
    else()
        set(QK4_PLATFORM "DESKTOP")
    endif()
endif()

message(STATUS "QK4 Platform: ${QK4_PLATFORM}")

# Options
option(QK4_BUILD_SHARED "Build as shared library" ON)
option(QK4_BUILD_TESTS "Build unit tests" OFF)

# Sources
set(QK4_CORE_SOURCES
    # Public API
    src/qk4_api.cpp

    # Network
    src/network/tcp_client.cpp
    src/network/protocol.cpp
    src/network/cat_server.cpp
    src/network/dx_cluster_client.cpp
    src/network/kpa1500_client.cpp
    src/network/k4_discovery.cpp
    src/network/reconnect_policy.cpp

    # Models
    src/models/radio_state.cpp
    src/models/handler_registry.cpp
    src/models/vfo_state.cpp
    src/models/tx_state.cpp
    src/models/spectrum_state.cpp
    src/models/audio_state.cpp
    src/models/nr_state.cpp
    src/models/cw_state.cpp
    src/models/kpa_state.cpp
    src/models/connection_state.cpp
    src/models/cat_server_state.cpp
    src/models/dx_cluster_state.cpp

    # Audio
    src/audio/opus_decoder.cpp
    src/audio/opus_encoder.cpp
    src/audio/sidetone_generator.cpp
    src/audio/ring_buffer.cpp

    # DSP
    src/dsp/spectrum_processor.cpp
    src/dsp/nr_lms.cpp
    src/dsp/nr_ssnr.cpp

    # Utils
    src/utils/radio_utils.cpp
    src/utils/frequency_utils.cpp
    src/utils/band_utils.cpp

    # Platform
    src/platform/event_loop.cpp
    src/platform/timer.cpp
)

# Platform-specific sources
if(QK4_PLATFORM STREQUAL "ANDROID")
    list(APPEND QK4_CORE_SOURCES
        src/platform/android/socket_impl.cpp
        src/platform/android/tls_impl.cpp
        src/platform/android/mdns_impl.cpp
    )
else()
    list(APPEND QK4_CORE_SOURCES
        src/platform/desktop/socket_impl.cpp
        src/platform/desktop/tls_impl.cpp
        src/platform/desktop/mdns_impl.cpp
    )
endif()

# Library target
if(QK4_BUILD_SHARED)
    add_library(qk4core SHARED ${QK4_CORE_SOURCES})
else()
    add_library(qk4core STATIC ${QK4_CORE_SOURCES})
endif()

target_include_directories(qk4core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler definitions
target_compile_definitions(qk4core PRIVATE
    QK4_PLATFORM_${QK4_PLATFORM}
)

if(QK4_BUILD_SHARED)
    target_compile_definitions(qk4core PRIVATE QK4_BUILDING_SHARED)
    target_compile_definitions(qk4core PUBLIC QK4_SHARED)
endif()

# Dependencies: OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(qk4core PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# Dependencies: Opus
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(OPUS IMPORTED_TARGET opus)
    if(OPUS_FOUND)
        target_link_libraries(qk4core PRIVATE PkgConfig::OPUS)
    endif()
endif()

if(NOT OPUS_FOUND)
    find_library(OPUS_LIBRARY NAMES opus)
    find_path(OPUS_INCLUDE_DIR NAMES opus/opus.h)
    if(OPUS_LIBRARY AND OPUS_INCLUDE_DIR)
        target_link_libraries(qk4core PRIVATE ${OPUS_LIBRARY})
        target_include_directories(qk4core PRIVATE ${OPUS_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "Opus library not found")
    endif()
endif()

# Platform-specific linking
if(QK4_PLATFORM STREQUAL "ANDROID")
    target_link_libraries(qk4core PRIVATE
        android
        log
    )
elseif(UNIX)
    target_link_libraries(qk4core PRIVATE pthread)
endif()

# Symbol visibility
set_target_properties(qk4core PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Tests
if(QK4_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
