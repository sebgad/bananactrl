# Host unit tests for the pure logic components (no ESP-IDF).
#
#   cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host
cmake_minimum_required(VERSION 3.22)
project(bananactrl_host_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON) # gnu++26, same as the IDF build

find_package(GTest REQUIRED)
enable_testing()

set(COMPONENTS ${CMAKE_CURRENT_LIST_DIR}/../../components)

# Logic under test, compiled from the same sources as the firmware
add_library(banana_logic STATIC
    ${COMPONENTS}/banana_control/src/PidController.cpp
    ${COMPONENTS}/banana_control/src/BrewFeedForward.cpp
    ${COMPONENTS}/banana_control/src/ConfigMapping.cpp
    ${COMPONENTS}/banana_drivers/src/Ads1115Config.cpp
    ${COMPONENTS}/banana_drivers/src/TemperatureConverters.cpp)
target_include_directories(banana_logic PUBLIC
    ${COMPONENTS}/banana_config/include
    ${COMPONENTS}/banana_control/include
    ${COMPONENTS}/banana_drivers/include
    # Flags.hpp only; Result.hpp needs esp_err.h
    ${COMPONENTS}/banana_core/include)
target_compile_options(banana_logic PUBLIC -Wall -Wextra -Werror)

# The original Arduino PID controller, unchanged, as golden reference
add_library(arduino_reference STATIC reference/PidCtrl.cpp reference/Arduino.cpp)
target_include_directories(arduino_reference PUBLIC reference)

add_executable(host_tests
    test_ads1115.cpp
    test_brew_feed_forward.cpp
    test_config.cpp
    test_flags.cpp
    test_pid_controller.cpp
    test_temperature_converters.cpp)
target_link_libraries(host_tests PRIVATE banana_logic arduino_reference GTest::gtest_main)
target_compile_definitions(host_tests PRIVATE BANANA_TEST_DATA_DIR="${CMAKE_CURRENT_LIST_DIR}/data")

include(GoogleTest)
gtest_discover_tests(host_tests)
