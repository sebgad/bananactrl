#pragma once

#include "hal/gpio_types.h"

/// Pin map of the SparkFun ESP32 Thing Plus in the coffee machine.
namespace banana::board {

// Actuators
inline constexpr gpio_num_t kSsrPwm = GPIO_NUM_21;
inline constexpr gpio_num_t kLedRed = GPIO_NUM_13;
inline constexpr gpio_num_t kLedGreen = GPIO_NUM_27;
inline constexpr gpio_num_t kLedBlue = GPIO_NUM_12;
inline constexpr gpio_num_t kStatusLed = GPIO_NUM_33; ///< green on-board status LED

// Inputs
inline constexpr gpio_num_t kPumpRelay = GPIO_NUM_17;
inline constexpr gpio_num_t kAdsAlertRdy = GPIO_NUM_14;

// I2C (ADS1115)
inline constexpr gpio_num_t kI2cSda = GPIO_NUM_23;
inline constexpr gpio_num_t kI2cScl = GPIO_NUM_22;

/// Driven high as 3.3 V source for the sensor circuit (Arduino `A0` on esp32thing_plus).
inline constexpr gpio_num_t kSensorSupply = GPIO_NUM_26;

} // namespace banana::board
