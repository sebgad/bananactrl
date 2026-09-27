#pragma once

#include <optional>

#include "banana/drivers/Ads1115.hpp"
#include "banana/hal/GpioInput.hpp"
#include "banana/hal/GpioOutput.hpp"
#include "banana/hal/I2cBus.hpp"
#include "banana/storage/LittleFs.hpp"

#include "SensorMonitor.hpp"

namespace banana {

/// Composition root: owns every object, wires the dependencies, starts the tasks.
/// Members are declared in dependency order, so destruction runs in reverse.
class App {
public:
    App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    ~App() = default;

    void run();

private:
    storage::LittleFs fs_;
    hal::GpioOutput statusLed_;
    hal::GpioOutput sensorSupply_;
    hal::I2cBus i2cBus_;
    hal::I2cDevice adsDevice_;
    std::optional<drivers::Ads1115> ads_; ///< empty if the ADS1115 does not answer
    hal::GpioInput adsReady_;
    std::optional<SensorMonitor> sensorMonitor_;
};

} // namespace banana
