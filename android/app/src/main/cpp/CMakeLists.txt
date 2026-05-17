cmake_minimum_required(VERSION 3.22)
project(qk4-android-bridge LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build libqk4core from the shared core directory
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core
                 ${CMAKE_CURRENT_BINARY_DIR}/core)

# JNI Bridge
add_library(qk4bridge SHARED
    jni_bridge.cpp
)

target_include_directories(qk4bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../core/src
)

target_link_libraries(qk4bridge PRIVATE
    qk4core
    android
    log
)
