#pragma once

#include <optional>

#include "banana/config/Config.hpp"
#include "banana/config/ConfigStore.hpp"
#include "banana/drivers/Ads1115.hpp"
#include "banana/drivers/RgbLed.hpp"
#include "banana/drivers/Ssr.hpp"
#include "banana/hal/GpioInput.hpp"
#include "banana/hal/GpioOutput.hpp"
#include "banana/hal/I2cBus.hpp"
#include "banana/hal/PeriodicTimer.hpp"
#include "banana/io/Outputs.hpp"
#include "banana/io/TemperatureSensor.hpp"
#include "banana/mqtt/MqttService.hpp"
#include "banana/net/MdnsService.hpp"
#include "banana/net/TimeSync.hpp"
#include "banana/net/WifiManager.hpp"
#include "banana/rtos/EventGroup.hpp"
#include "banana/storage/FileLogger.hpp"
#include "banana/storage/LittleFs.hpp"
#include "banana/storage/MeasurementRecorder.hpp"
#include "banana/storage/Nvs.hpp"
#include "banana/web/ApiRoutes.hpp"
#include "banana/web/EventStream.hpp"
#include "banana/web/OtaRoutes.hpp"
#include "banana/web/ReleaseUpdater.hpp"
#include "banana/web/StaticFileRoutes.hpp"
#include "banana/web/UpdateRoutes.hpp"
#include "banana/web/WebServer.hpp"

#include "HeaterTask.hpp"

namespace banana {

/// Stands in for a missing ADS1115: every read fails, so Diagnostics keeps the heater off.
class MissingSensor final : public io::ITemperatureSensor {
public:
    [[nodiscard]] Result<float> readCelsius() override { return fail(ESP_ERR_NOT_FOUND); }
    [[nodiscard]] bool healthy() override { return false; }
    void setFilterActive(bool /*active*/) override {}
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
    void startNetwork();
    void startRecording();
    void startWebServer();
    void startMqtt();
    /// Once a minute: heap and logger statistics (for soak tests).
    static void logHealth(void* arg);

    storage::Nvs nvs_;
    storage::LittleFs fs_;
    storage::FileLogger logger_;
    bool loggerStarted_; ///< logger_ started right after mounting, before anything else logs
    config::ConfigStore configStore_;
    config::Config config_; ///< params.json at boot
    hal::GpioOutput statusLed_;
    hal::GpioOutput sensorSupply_;
    hal::I2cBus i2cBus_;
    hal::I2cDevice adsDevice_;
    std::optional<drivers::Ads1115> ads_;                  ///< empty if the ADS1115 does not answer
    std::optional<storage::MeasurementRecorder> recorder_; ///< data.csv, created after the time sync
    MissingSensor missingSensor_;
    drivers::Ssr ssr_;
    drivers::RgbLed rgbLed_;
    hal::GpioInput adsReady_;
    hal::GpioInput pumpRelay_;
    net::WifiManager wifi_; ///< started in run(), after the heater
    rtos::EventGroup events_;
    HeaterTask heaterTask_;
    hal::PeriodicTimer tick_;
    hal::PeriodicTimer health_;
    std::optional<net::MdnsService> mdns_;
    std::optional<net::TimeSync> timeSync_;
    mqtt::MqttService mqtt_;             ///< connects only if enabled in the settings (and in station mode)
    web::ReleaseUpdater releaseUpdater_; ///< updates from GitHub releases, task started on first use
    web::ApiRoutes apiRoutes_;
    web::OtaRoutes otaRoutes_;
    web::UpdateRoutes updateRoutes_;
    web::StaticFileRoutes staticRoutes_;
    web::EventStream eventStream_;
    std::optional<web::WebServer> webServer_; ///< after the routes: stopped before they are destroyed
};

} // namespace banana
