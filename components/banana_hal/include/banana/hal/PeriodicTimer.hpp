#pragma once

#include <chrono>

#include "esp_timer.h"

#include "banana/core/Result.hpp"

namespace banana::hal {

/// Periodic esp_timer (RAII). The callback runs in the esp_timer task: keep it short,
/// e.g. set an event bit.
class PeriodicTimer {
public:
    using Callback = void (*)(void* arg);

    [[nodiscard]] static Result<PeriodicTimer> create(const char* name, Callback callback, void* arg);

    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;
    PeriodicTimer(PeriodicTimer&& other) noexcept;
    PeriodicTimer& operator=(PeriodicTimer&& other) noexcept;
    ~PeriodicTimer();

    [[nodiscard]] Result<void> start(std::chrono::microseconds period);
    void stop();

private:
    explicit PeriodicTimer(esp_timer_handle_t handle) : handle_(handle) {}
    void release();

    esp_timer_handle_t handle_ = nullptr;
};

} // namespace banana::hal
