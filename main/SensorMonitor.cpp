#include "SensorMonitor.hpp"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace banana {
namespace {

constexpr const char* kTag = "sensor";
constexpr TickType_t kReadyTimeout = pdMS_TO_TICKS(500); // 4 conversion periods at 8 SPS
constexpr int64_t kReportPeriodUs = 1'000'000;

} // namespace

void IRAM_ATTR SensorMonitor::onConversionReady(void* arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(static_cast<SensorMonitor*>(arg)->handle(), &woken);
    portYIELD_FROM_ISR(woken);
}

void SensorMonitor::run()
{
    int samples = 0;
    int timeouts = 0;
    int errors = 0;
    float celsius = 0.0F;
    int64_t windowStart = esp_timer_get_time();

    for (;;) {
        if (ulTaskNotifyTake(pdTRUE, kReadyTimeout) == 0) {
            ++timeouts; // no ready pulse: read anyway, so the values stay visible
        }
        if (auto value = ads_->readCelsius()) {
            celsius = *value;
            ++samples;
        } else {
            ++errors;
        }

        const int64_t now = esp_timer_get_time();
        if (now - windowStart >= kReportPeriodUs) {
            const float volts = static_cast<float>(ads_->latestRaw()) *
                                drivers::ads1115::lsbVolts(ads_->settings().config.pga);
            const double rate = samples * 1e6 / static_cast<double>(now - windowStart);
            ESP_LOGI(kTag,
                     "%.1f samples/s, raw=%d (%.6f V), filtered T=%.2f C, ready timeouts=%d, I2C errors=%d",
                     rate, ads_->latestRaw(), static_cast<double>(volts), static_cast<double>(celsius),
                     timeouts, errors);
            samples = timeouts = errors = 0;
            windowStart = now;
        }
    }
}

} // namespace banana
