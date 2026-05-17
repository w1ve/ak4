cmake_minimum_required(VERSION 3.22)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Test: Protocol
add_executable(test_protocol
    test_protocol.cpp
    ${CMAKE_SOURCE_DIR}/src/network/protocol.cpp
)
target_include_directories(test_protocol PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_protocol PRIVATE GTest::gtest_main)
add_test(NAME test_protocol COMMAND test_protocol)

# Test: RadioUtils
add_executable(test_radio_utils
    test_radio_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/radio_utils.cpp
)
target_include_directories(test_radio_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_radio_utils PRIVATE GTest::gtest_main)
add_test(NAME test_radio_utils COMMAND test_radio_utils)

# Test: RingBuffer
add_executable(test_ring_buffer
    test_ring_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/ring_buffer.cpp
)
target_include_directories(test_ring_buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_ring_buffer PRIVATE GTest::gtest_main)
add_test(NAME test_ring_buffer COMMAND test_ring_buffer)

# Test: ReconnectPolicy
add_executable(test_reconnect_policy
    test_reconnect_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/network/reconnect_policy.cpp
)
target_include_directories(test_reconnect_policy PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_reconnect_policy PRIVATE GTest::gtest_main)
add_test(NAME test_reconnect_policy COMMAND test_reconnect_policy)

# Test: RadioState
add_executable(test_radio_state
    test_radio_state.cpp
    ${CMAKE_SOURCE_DIR}/src/models/radio_state.cpp
)
target_include_directories(test_radio_state PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_radio_state PRIVATE GTest::gtest_main)
add_test(NAME test_radio_state COMMAND test_radio_state)
