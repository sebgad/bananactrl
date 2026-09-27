#include "banana/rtos/Watchdog.hpp"

#include <utility>

#include "esp_task_wdt.h"
#include "sdkconfig.h"

namespace banana::rtos {

Result<void> Watchdog::configure(std::chrono::seconds timeout, bool panic)
{
    std::uint32_t idleMask = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
    idleMask |= 1U << 0U;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
    idleMask |= 1U << 1U;
#endif
    const esp_task_wdt_config_t config{
        .timeout_ms = static_cast<std::uint32_t>(std::chrono::milliseconds{timeout}.count()),
        .idle_core_mask = idleMask,
        .trigger_panic = panic,
    };
    return toResult(esp_task_wdt_reconfigure(&config));
}

Result<Watchdog> Watchdog::subscribeCurrentTask()
{
    if (auto res = toResult(esp_task_wdt_add(nullptr)); !res) {
        return fail(res.error());
    }
    return Watchdog{};
}

Watchdog::Watchdog(Watchdog&& other) noexcept : subscribed_(std::exchange(other.subscribed_, false))
{
}

Watchdog& Watchdog::operator=(Watchdog&& other) noexcept
{
    if (this != &other) {
        release();
        subscribed_ = std::exchange(other.subscribed_, false);
    }
    return *this;
}

Watchdog::~Watchdog()
{
    release();
}

void Watchdog::feed() const
{
    if (subscribed_) {
        esp_task_wdt_reset();
    }
}

void Watchdog::release()
{
    if (subscribed_) {
        esp_task_wdt_delete(nullptr);
        subscribed_ = false;
    }
}

} // namespace banana::rtos
