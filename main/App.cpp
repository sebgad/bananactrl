#include "App.hpp"

#include <utility>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "banana/core/board.hpp"
#include "banana/drivers/Pt1000.hpp"

namespace banana {
namespace {

constexpr const char* kTag = "app";

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
    : fs_(orAbort(storage::LittleFs::mount({}), "LittleFS mount")),
      statusLed_(orAbort(hal::GpioOutput::create(board::kStatusLed), "status LED")),
      sensorSupply_(orAbort(hal::GpioOutput::create(board::kSensorSupply, true), "sensor supply")),
      i2cBus_(orAbort(hal::I2cBus::create({.sda = board::kI2cSda, .scl = board::kI2cScl}), "I2C bus")),
      adsDevice_(orAbort(i2cBus_.addDevice(drivers::Ads1115::kDefaultAddress), "ADS1115 device")),
      ads_(createAds(adsDevice_)),
      // ALERT/RDY is open drain; the internal pull-up helps on boards without the external 10k.
      adsReady_(
          orAbort(hal::GpioInput::create(board::kAdsAlertRdy, hal::GpioInput::Pull::Up), "ADS ready pin"))
{
}

void App::run()
{
    ESP_LOGI(kTag, "bananactrl starting, last reset reason: %d", static_cast<int>(esp_reset_reason()));

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
        sensorMonitor_.emplace(*ads_);
        ESP_ERROR_CHECK(sensorMonitor_->start("sensor", 4096, 5).error_or(ESP_OK));
        // Only after the task exists: the ISR notifies it.
        if (auto res = adsReady_.onEdge(hal::GpioInput::Edge::Rising, &SensorMonitor::onConversionReady,
                                        &*sensorMonitor_);
            !res) {
            ESP_LOGE(kTag, "ADS ready interrupt failed: %s", esp_err_to_name(res.error()));
        }
    }

    statusLed_.set(true);
}

} // namespace banana
