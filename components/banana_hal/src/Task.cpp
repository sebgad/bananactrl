#include "banana/rtos/Task.hpp"

namespace banana::rtos {

Result<void> Task::start(const char* name, std::uint32_t stackBytes, UBaseType_t priority, BaseType_t core)
{
    if (handle_ != nullptr) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    if (xTaskCreatePinnedToCore(&Task::trampoline, name, stackBytes, this, priority, &handle_, core) !=
        pdPASS) {
        handle_ = nullptr;
        return fail(ESP_ERR_NO_MEM);
    }
    return {};
}

Task::~Task()
{
    if (handle_ != nullptr && handle_ != xTaskGetCurrentTaskHandle()) {
        vTaskDelete(handle_);
    }
}

void Task::trampoline(void* arg)
{
    auto* self = static_cast<Task*>(arg);
    self->run();
    self->handle_ = nullptr;
    vTaskDelete(nullptr);
}

} // namespace banana::rtos
