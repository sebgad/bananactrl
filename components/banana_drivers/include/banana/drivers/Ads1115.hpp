#pragma once

#include <cstdint>

#include "banana/core/Result.hpp"
#include "banana/drivers/Ads1115Config.hpp"
#include "banana/drivers/MovingAverage.hpp"
#include "banana/drivers/TemperatureConverter.hpp"
#include "banana/drivers/TemperatureSensor.hpp"
#include "banana/hal/I2cDevice.hpp"

namespace banana::drivers {

/// TI ADS1115 16-bit ADC reading the Pt1000 bridge.
///
/// The ALERT/RDY pin is not part of the driver: wire a hal::GpioInput to it and call
/// readCelsius() after each conversion-ready edge.
class Ads1115 final : public ITemperatureSensor {
public:
    static constexpr std::uint16_t kDefaultAddress = 0x48; ///< ADDR pin on GND
    static constexpr std::size_t kFilterLength = 12;       ///< ADS1115_CONV_BUF_SIZE of the Arduino driver

    struct Settings {
        ads1115::ConfigRegister config;
        /// Drive ALERT/RDY as conversion-ready pulse (thresholds 0xFFFF/0x0000, queue != disabled).
        bool conversionReadyPin = false;
        bool filterActive = true;
    };

    /// The settings of configADS1115() in the Arduino firmware.
    [[nodiscard]] static constexpr Settings coffeeMachineSettings()
    {
        return {.config = ads1115::kCoffeeMachineConfig, .conversionReadyPin = true, .filterActive = true};
    }

    /// Checks that the device answers, then applies `settings`.
    [[nodiscard]] static Result<Ads1115>
    create(hal::I2cDevice& device, const ITemperatureConverter& converter, const Settings& settings);

    /// Writes thresholds and config register and verifies the read-back.
    [[nodiscard]] Result<void> configure(const Settings& settings);

    [[nodiscard]] Result<float> readCelsius() override;

    /// Reads the conversion register, feeds the filter and returns the (filtered) bridge voltage.
    [[nodiscard]] Result<float> readVolts();

    [[nodiscard]] Result<ads1115::ConfigRegister> readConfig();
    [[nodiscard]] Result<std::uint16_t> readRegister(ads1115::Register reg);

    void setFilterActive(bool active) { settings_.filterActive = active; }
    [[nodiscard]] const Settings& settings() const { return settings_; }
    /// Raw value of the latest conversion (unfiltered).
    [[nodiscard]] std::int16_t latestRaw() const { return filter_.latest(); }

private:
    Ads1115(hal::I2cDevice& device, const ITemperatureConverter& converter)
        : device_(&device), converter_(&converter)
    {
    }

    [[nodiscard]] Result<void> writeRegister(ads1115::Register reg, std::uint16_t value);

    hal::I2cDevice* device_;
    const ITemperatureConverter* converter_;
    Settings settings_;
    MovingAverage<kFilterLength> filter_;
};

} // namespace banana::drivers
