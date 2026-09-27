#include "banana/rtos/Restart.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace banana::rtos {

void restartAfter(std::chrono::milliseconds delay)
{
    ESP_LOGW("restart", "restart in %lld ms", static_cast<long long>(delay.count()));
    vTaskDelay(pdMS_TO_TICKS(delay.count()));
    esp_restart();
}

} // namespace banana::rtos
