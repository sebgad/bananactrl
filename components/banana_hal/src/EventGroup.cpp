#include "banana/rtos/EventGroup.hpp"

#include <utility>

namespace banana::rtos {

Result<EventGroup> EventGroup::create()
{
    EventGroupHandle_t handle = xEventGroupCreate();
    if (handle == nullptr) {
        return fail(ESP_ERR_NO_MEM);
    }
    return EventGroup{handle};
}

EventGroup::EventGroup(EventGroup&& other) noexcept : handle_(std::exchange(other.handle_, nullptr))
{
}

EventGroup& EventGroup::operator=(EventGroup&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

EventGroup::~EventGroup()
{
    release();
}

void EventGroup::set(Bits bits)
{
    xEventGroupSetBits(handle_, bits);
}

bool EventGroup::setFromIsr(Bits bits)
{
    BaseType_t woken = pdFALSE;
    xEventGroupSetBitsFromISR(handle_, bits, &woken);
    return woken == pdTRUE;
}

EventGroup::Bits EventGroup::waitAny(Bits bits, std::chrono::milliseconds timeout)
{
    const Bits set = xEventGroupWaitBits(handle_, bits, pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout.count()));
    return set & bits;
}

void EventGroup::release()
{
    if (handle_ != nullptr) {
        vEventGroupDelete(handle_);
        handle_ = nullptr;
    }
}

} // namespace banana::rtos
