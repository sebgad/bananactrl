#include "App.hpp"

#include <array>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>

#include "driver/ledc.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "banana/core/board.hpp"
#include "banana/drivers/Pt1000.hpp"
#include "banana/rtos/Watchdog.hpp"
#include "banana/web/OtaUpdater.hpp"

namespace banana {
namespace {

constexpr const char* kTag = "app";
constexpr std::chrono::microseconds kTickPeriod{450'000}; // iInterruptLongIntervalMicros
constexpr std::chrono::seconds kWatchdogTimeout{75};      // WDT_Timeout
constexpr std::chrono::seconds kWifiTimeout{18};          // connectWiFi(3, 6000)
constexpr std::chrono::seconds kTimeSyncTimeout{10};
constexpr std::chrono::seconds kHealthPeriod{60};
constexpr std::chrono::milliseconds kEventPeriod{1000}; // /events: live values and new data.csv rows
constexpr std::chrono::seconds kMqttStatePeriod{5};
// Heater task and log writer share a core: they then take turns (the heater has the higher priority), so a
// log flash write rarely overlaps an ADS1115 read. Bench, stress build (a log line per tick), 170 s: I2C
// NACKs 16 unpinned, 23 with the logger on core 0, 0-1 on core 1. Wi-Fi runs on core 0.
constexpr BaseType_t kControlCore = 1;
constexpr const char* kHostname = "coffee"; // http://coffee.local

/// Boot-time objects without which the firmware cannot run: log and panic (-> reboot).
template <typename T>
T orAbort(Result<T> result, const char* what)
{
    if (!result) {
        ESP_LOGE(kTag, "%s failed: %s", what, esp_err_to_name(result.error()));
        ESP_ERROR_CHECK(result.error());
    }
    return std::move(*result);
}

#if CONFIG_BANANA_PT1000_LOOKUP_5V0
constexpr drivers::LookupTableConverter kConverter{drivers::pt1000::kLookup5V0};
constexpr const char* kConverterName = "lookup table 5.0 V";
#elif CONFIG_BANANA_PT1000_LOOKUP_3V3
constexpr drivers::LookupTableConverter kConverter{drivers::pt1000::kLookup3V3};
constexpr const char* kConverterName = "lookup table 3.3 V";
#elif CONFIG_BANANA_PT1000_LINEAR
constexpr const drivers::LinearConverter& kConverter = drivers::pt1000::kLinear3V3;
constexpr const char* kConverterName = "linear regression 3.3 V";
#elif CONFIG_BANANA_PT1000_QUADRATIC
constexpr const drivers::QuadraticConverter& kConverter = drivers::pt1000::kQuadratic3V3;
constexpr const char* kConverterName = "quadratic regression 3.3 V";
#endif

/// A missing ADC is not fatal: the web UI must stay reachable. Diagnostics reports it.
std::optional<drivers::Ads1115> createAds(hal::I2cDevice& device)
{
    auto ads = drivers::Ads1115::create(device, kConverter, drivers::Ads1115::coffeeMachineSettings());
    if (!ads) {
        ESP_LOGE(kTag, "ADS1115 at 0x%02X not usable: %s", device.address(), esp_err_to_name(ads.error()));
        return std::nullopt;
    }
    return std::move(*ads);
}

bool startLogger(storage::FileLogger& logger, const storage::LittleFs& fs)
{
    const std::string root{fs.mountPoint()};
    if (auto res = logger.start(root + "/logfile_recent.txt", root + "/logfile_last.txt", kControlCore);
        !res) {
        ESP_LOGE(kTag, "file logger failed: %s", esp_err_to_name(res.error()));
        return false;
    }
    return true;
}

config::Config loadConfig(config::ConfigStore& store)
{
    const auto loaded = store.load();
    switch (loaded.source) {
    case config::ConfigStore::Source::Stored:
        ESP_LOGI(kTag, "configuration loaded from NVS");
        break;
    case config::ConfigStore::Source::StoredCompleted:
        ESP_LOGW(kTag, "missing keys in the stored configuration: defaults used and written back");
        break;
    case config::ConfigStore::Source::Imported:
        ESP_LOGI(kTag, "configuration imported from %s into NVS", store.legacyFile().c_str());
        break;
    case config::ConfigStore::Source::Defaults:
        ESP_LOGW(kTag, "no configuration stored: factory settings used and written");
        break;
    }
    if (loaded.writeError != ESP_OK) {
        ESP_LOGE(kTag, "writing the configuration to NVS failed: %s", esp_err_to_name(loaded.writeError));
    }
    return loaded.config;
}

/// "bananactrl_" + the last three bytes of the station MAC: stable and unique per board.
std::string deviceId()
{
    std::array<std::uint8_t, 6> mac{};
    esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
    std::array<char, 24> id{};
    std::snprintf(id.data(), id.size(), "bananactrl_%02x%02x%02x", mac[3], mac[4],
                  mac[5]); // NOLINT(cppcoreguidelines-pro-type-vararg)
    return id.data();
}

/// Configured network, or the factory credentials from Kconfig if none is configured.
net::WifiManager::Credentials wifiCredentials(const config::WifiSettings& wifi)
{
    if (!wifi.ssid.empty()) {
        return {.ssid = wifi.ssid, .password = wifi.password};
    }
    return {.ssid = CONFIG_BANANA_WIFI_FACTORY_SSID, .password = CONFIG_BANANA_WIFI_FACTORY_PW};
}

void logI2cScan(const hal::I2cBus& bus)
{
    // ADDR pin: GND 0x48, VDD 0x49, SDA 0x4A, SCL 0x4B
    for (std::uint16_t address = 0x48; address <= 0x4B; ++address) {
        if (bus.probe(address)) {
            ESP_LOGI(kTag, "I2C device answers at 0x%02X", address);
        }
    }
}

} // namespace

App::App()
    : nvs_(orAbort(storage::Nvs::init(), "NVS")),
      fs_(orAbort(storage::LittleFs::mount({}), "LittleFS mount")), loggerStarted_(startLogger(logger_, fs_)),
      configStore_(std::string{fs_.mountPoint()} + "/params.json"), config_(loadConfig(configStore_)),
      statusLed_(orAbort(hal::GpioOutput::create(board::kStatusLed), "status LED")),
      sensorSupply_(orAbort(hal::GpioOutput::create(board::kSensorSupply, true), "sensor supply")),
      i2cBus_(orAbort(hal::I2cBus::create({.sda = board::kI2cSda, .scl = board::kI2cScl}), "I2C bus")),
      adsDevice_(orAbort(i2cBus_.addDevice(drivers::Ads1115::kDefaultAddress), "ADS1115 device")),
      ads_(createAds(adsDevice_)),
      ssr_(orAbort(drivers::Ssr::create(board::kSsrPwm, LEDC_TIMER_0, LEDC_CHANNEL_0, config_.ssr), "SSR")),
      rgbLed_(orAbort(drivers::RgbLed::create(
                          {.red = board::kLedRed, .green = board::kLedGreen, .blue = board::kLedBlue},
                          LEDC_TIMER_1, {LEDC_CHANNEL_1, LEDC_CHANNEL_2, LEDC_CHANNEL_3}, config_.led),
                      "RGB LED")),
      // ALERT/RDY is open drain; the internal pull-up helps on boards without the external 10k.
      adsReady_(
          orAbort(hal::GpioInput::create(board::kAdsAlertRdy, hal::GpioInput::Pull::Up), "ADS ready pin")),
      pumpRelay_(
          orAbort(hal::GpioInput::create(board::kPumpRelay, hal::GpioInput::Pull::Down), "pump relay pin")),
      events_(orAbort(rtos::EventGroup::create(), "event group")),
      heaterTask_(events_,
                  {.sensor = ads_ ? static_cast<io::ITemperatureSensor*>(&*ads_) : &missingSensor_,
                   .ssr = &ssr_,
                   .led = &rgbLed_,
                   .pumpRelay = &pumpRelay_,
                   .network = &wifi_},
                  config_),
      tick_(orAbort(hal::PeriodicTimer::create("tick", &HeaterTask::onTick, &heaterTask_), "tick timer")),
      health_(orAbort(hal::PeriodicTimer::create("health", &App::logHealth, this), "health timer")),
      releaseUpdater_({.repository = CONFIG_BANANA_UPDATE_REPOSITORY,
                       .fsRoot = std::string{fs_.mountPoint()},
                       .version = esp_app_get_description()->version},
                      wifi_),
      apiRoutes_(configStore_, heaterTask_, wifi_), otaRoutes_(std::string{fs_.mountPoint()}),
      updateRoutes_(releaseUpdater_), staticRoutes_(std::string{fs_.mountPoint()}),
      eventStream_(heaterTask_, wifi_)
{
}

void App::run()
{
    ESP_LOGI(kTag, "bananactrl starting, last reset reason: %d", static_cast<int>(esp_reset_reason()));

    // Early, so errors can be shown on the LED (as in the Arduino setup())
    rgbLed_.show(io::LedColor::White, true, io::LedEffect::Steady);
    statusLed_.set(true);

    if (auto info = fs_.info()) {
        ESP_LOGI(kTag, "LittleFS on %.*s: %zu of %zu bytes used", static_cast<int>(fs_.mountPoint().size()),
                 fs_.mountPoint().data(), info->usedBytes, info->totalBytes);
    } else {
        ESP_LOGW(kTag, "LittleFS info failed: %s", esp_err_to_name(info.error()));
    }

    ESP_LOGI(kTag, "Pt1000 conversion: %s (0 V -> %.2f C)", kConverterName, kConverter.toCelsius(0.0F));
    logI2cScan(i2cBus_);
    if (ads_) {
        if (auto config = ads_->readConfig()) {
            ESP_LOGI(kTag, "ADS1115 config: %s", config->toString().c_str());
        }
    }

    ESP_ERROR_CHECK(rtos::Watchdog::configure(kWatchdogTimeout, true).error_or(ESP_OK));
    ESP_ERROR_CHECK(heaterTask_.start("heater", 4096, 5, kControlCore).error_or(ESP_OK));
    ESP_ERROR_CHECK(configStore_.addListener(heaterTask_).error_or(ESP_OK)); // settings page, MQTT, reset

    // Interrupt sources only after the task exists: they set its event bits.
    if (ads_) {
        ESP_ERROR_CHECK(
            adsReady_.onEdge(hal::GpioInput::Edge::Rising, &HeaterTask::onSampleReadyIsr, &heaterTask_)
                .error_or(ESP_OK));
    }
    ESP_ERROR_CHECK(pumpRelay_.onEdge(hal::GpioInput::Edge::Any, &HeaterTask::onPumpEdgeIsr, &heaterTask_)
                        .error_or(ESP_OK));
    ESP_ERROR_CHECK(tick_.start(kTickPeriod).error_or(ESP_OK));
    ESP_ERROR_CHECK(health_.start(kHealthPeriod).error_or(ESP_OK));
    ESP_LOGI(kTag, "heater control running");

    // After the heater: the connection attempt blocks for up to kWifiTimeout.
    startNetwork();
    startWebServer();
    startMqtt();
    // data.csv after the time sync, like the Arduino firmware (its header carries the timestamp)
    startRecording();
    ESP_LOGI(kTag, "boot done, main task stack: %u bytes never used",
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}

void App::startRecording()
{
    storage::csv::Header header;
    const std::time_t now = std::time(nullptr);
    header.timestamp = net::TimeSync::now() ? std::to_string(static_cast<long long>(now)) : "unknown";
    if (ads_) {
        using drivers::ads1115::Register;
        header.adsConfig = ads_->readRegister(Register::Config).value_or(0);
        header.adsLowThreshold = ads_->readRegister(Register::LowThreshold).value_or(0);
        header.adsHighThreshold = ads_->readRegister(Register::HighThreshold).value_or(0);
    }
    const std::string path = std::string{fs_.mountPoint()} + "/data.csv";
    auto recorder = storage::MeasurementRecorder::create(path, header);
    if (!recorder) {
        ESP_LOGE(kTag, "%s: %s", path.c_str(), esp_err_to_name(recorder.error()));
        return;
    }
    recorder_.emplace(std::move(*recorder));
    heaterTask_.attachRecorder(&*recorder_);
    ESP_LOGI(kTag, "recording to %s", path.c_str());
}

void App::logHealth(void* arg)
{
    const auto* app = static_cast<const App*>(arg);
    ESP_LOGI(kTag, "health: heap free %lu, min %lu, largest block %zu; log dropped %zu, write errors %zu",
             static_cast<unsigned long>(esp_get_free_heap_size()),
             static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
             heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), app->logger_.droppedLines(),
             app->logger_.writeErrors());
}

void App::startWebServer()
{
    if (wifi_.mode() == net::WifiManager::Mode::Off) {
        return;
    }
    auto server = web::WebServer::start({});
    if (!server) {
        ESP_LOGE(kTag, "web server failed: %s", esp_err_to_name(server.error()));
        return;
    }
    // The static file wildcard last: handlers are matched in registration order
    if (auto res = eventStream_.start(*server, kEventPeriod); !res) {
        ESP_LOGE(kTag, "event stream failed: %s", esp_err_to_name(res.error())); // pages fall back to polling
    } else {
        heaterTask_.attachRowListener(&eventStream_);
    }
    for (auto res : {apiRoutes_.registerOn(*server), otaRoutes_.registerOn(*server),
                     updateRoutes_.registerOn(*server), staticRoutes_.registerOn(*server)}) {
        if (!res) {
            ESP_LOGE(kTag, "web route registration failed: %s", esp_err_to_name(res.error()));
            return;
        }
    }
    webServer_.emplace(std::move(*server));
    ESP_LOGI(kTag, "web server running on http://%s/", wifi_.ipAddress().c_str());
    // Network and web UI are up: a new OTA image is good (otherwise the bootloader rolls back on reset)
    web::OtaUpdater::markRunningAppValid();
}

void App::startMqtt()
{
    // Always started: MQTT can be enabled or reconfigured later on the settings page (it listens to the
    // store)
    mqtt::Device device{.id = deviceId(),
                        .version = esp_app_get_description()->version,
                        .url = std::string{"http://"} + kHostname + ".local/"};
    const mqtt::MqttService::Dependencies deps{
        .store = &configStore_, .heater = &heaterTask_, .wifi = &wifi_};
    if (auto res = mqtt_.start(config_.mqtt, std::move(device), deps, kMqttStatePeriod); !res) {
        ESP_LOGE(kTag, "MQTT start failed: %s", esp_err_to_name(res.error()));
        return;
    }
    if (auto res = configStore_.addListener(mqtt_); !res) {
        ESP_LOGE(kTag, "MQTT config listener: %s", esp_err_to_name(res.error()));
    }
}

void App::startNetwork()
{
    const auto mode = wifi_.start(wifiCredentials(config_.wifi), kWifiTimeout);
    if (!mode) {
        ESP_LOGE(kTag, "Wi-Fi start failed: %s", esp_err_to_name(mode.error()));
        return;
    }

    if (auto mdns = net::MdnsService::start(kHostname, "Silvia coffee control")) {
        mdns_.emplace(std::move(*mdns));
        ESP_LOGI(kTag, "mDNS: http://%s.local", kHostname);
    } else {
        ESP_LOGE(kTag, "mDNS failed: %s", esp_err_to_name(mdns.error()));
    }

    if (*mode != net::WifiManager::Mode::Station) {
        return; // no internet in SoftAP mode
    }
    auto sync = net::TimeSync::start();
    if (!sync) {
        ESP_LOGE(kTag, "SNTP failed: %s", esp_err_to_name(sync.error()));
        return;
    }
    timeSync_.emplace(std::move(*sync));
    if (timeSync_->waitForSync(kTimeSyncTimeout)) {
        if (const auto now = net::TimeSync::now()) {
            std::array<char, 32> text{};
            std::strftime(text.data(), text.size(), "%Y-%m-%d %H:%M:%S %Z", &*now);
            ESP_LOGI(kTag, "local time %s", text.data());
        }
    } else {
        ESP_LOGW(kTag, "time not synchronised yet (continues in the background)");
    }
}

} // namespace banana
