#pragma once

#include <chrono>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "banana/core/Result.hpp"

namespace banana::rtos {

/// FreeRTOS event group (RAII). Bits are set atomically from tasks, timers and ISRs.
class EventGroup {
public:
    using Bits = EventBits_t;

    [[nodiscard]] static Result<EventGroup> create();

    EventGroup(const EventGroup&) = delete;
    EventGroup& operator=(const EventGroup&) = delete;
    EventGroup(EventGroup&& other) noexcept;
    EventGroup& operator=(EventGroup&& other) noexcept;
    ~EventGroup();

    void set(Bits bits);
    /// From an ISR. Returns true if a higher-priority task was woken (pass to portYIELD_FROM_ISR).
    [[nodiscard]] bool setFromIsr(Bits bits);

    /// Waits until any of `bits` is set or `timeout` expires; returns and clears the bits that were set.
    [[nodiscard]] Bits waitAny(Bits bits, std::chrono::milliseconds timeout);

private:
    explicit EventGroup(EventGroupHandle_t handle) : handle_(handle) {}
    void release();

    EventGroupHandle_t handle_ = nullptr;
};

} // namespace banana::rtos
