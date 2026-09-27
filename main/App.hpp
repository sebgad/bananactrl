#pragma once

#include <optional>

#include "banana/config/Config.hpp"
#include "banana/drivers/Ads1115.hpp"
#include "banana/drivers/RgbLed.hpp"
#include "banana/drivers/Ssr.hpp"
#include "banana/hal/GpioInput.hpp"
#include "banana/hal/GpioOutput.hpp"
#include "banana/hal/I2cBus.hpp"
#include "banana/hal/PeriodicTimer.hpp"
#include "banana/io/Outputs.hpp"
#include "banana/io/TemperatureSensor.hpp"
#include "banana/rtos/EventGroup.hpp"
#include "banana/storage/LittleFs.hpp"

#include "HeaterTask.hpp"

namespace banana {

/// Stands in for a missing ADS1115: every read fails, so Diagnostics keeps the heater off.
class MissingSensor final : public io::ITemperatureSensor {
public:
    [[nodiscard]] Result<float> readCelsius() override { return fail(ESP_ERR_NOT_FOUND); }
    [[nodiscard]] bool healthy() override { return false; }
    void setFilterActive(bool /*active*/) override {}
};

/// Until Phase 5 (WifiManager): never reports a disconnected station.
class NoNetwork final : public io::INetworkStatus {
public:
    [[nodiscard]] bool stationDisconnected() const override { return false; }
};

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
    config::Config config_; ///< factory defaults until ConfigStore (Phase 6)
    storage::LittleFs fs_;
    hal::GpioOutput statusLed_;
    hal::GpioOutput sensorSupply_;
    hal::I2cBus i2cBus_;
    hal::I2cDevice adsDevice_;
    std::optional<drivers::Ads1115> ads_; ///< empty if the ADS1115 does not answer
    MissingSensor missingSensor_;
    drivers::Ssr ssr_;
    drivers::RgbLed rgbLed_;
    hal::GpioInput adsReady_;
    hal::GpioInput pumpRelay_;
    NoNetwork network_;
    rtos::EventGroup events_;
    HeaterTask heaterTask_;
    hal::PeriodicTimer tick_;
};

} // namespace banana
