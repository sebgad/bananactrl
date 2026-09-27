#include "banana/net/TimeSync.hpp"

#include <cstdlib>
#include <utility>

#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"

namespace banana::net {

Result<TimeSync> TimeSync::start(const char* server, const char* timeZone)
{
    setenv("TZ", timeZone, 1); // NOLINT(concurrency-mt-unsafe): once at boot
    tzset();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(server);
    if (auto res = toResult(esp_netif_sntp_init(&config)); !res) {
        return fail(res.error());
    }
    return TimeSync{};
}

TimeSync::TimeSync(TimeSync&& other) noexcept : running_(std::exchange(other.running_, false))
{
}

TimeSync& TimeSync::operator=(TimeSync&& other) noexcept
{
    if (this != &other) {
        release();
        running_ = std::exchange(other.running_, false);
    }
    return *this;
}

TimeSync::~TimeSync()
{
    release();
}

Result<void> TimeSync::waitForSync(std::chrono::seconds timeout) const
{
    if (!running_) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    const auto ms = std::chrono::milliseconds{timeout}.count();
    return toResult(esp_netif_sntp_sync_wait(pdMS_TO_TICKS(ms)));
}

std::optional<std::tm> TimeSync::now()
{
    const std::time_t seconds = std::time(nullptr);
    std::tm local{};
    localtime_r(&seconds, &local);
    // Before the first sync the clock starts at 1970.
    if (local.tm_year < (2024 - 1900)) {
        return std::nullopt;
    }
    return local;
}

void TimeSync::release()
{
    if (running_) {
        esp_netif_sntp_deinit();
        running_ = false;
    }
}

} // namespace banana::net
