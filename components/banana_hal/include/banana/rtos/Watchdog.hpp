#pragma once

#include <chrono>

#include "banana/core/Result.hpp"

namespace banana::rtos {

/// Task watchdog subscription of the calling task (RAII: unsubscribed in the destructor).
/// Create it inside the task that feeds it.
class Watchdog {
public:
    /// Changes timeout and panic behaviour of the task watchdog (Kconfig caps the boot value at 60 s).
    [[nodiscard]] static Result<void> configure(std::chrono::seconds timeout, bool panic);

    [[nodiscard]] static Result<Watchdog> subscribeCurrentTask();

    Watchdog(const Watchdog&) = delete;
    Watchdog& operator=(const Watchdog&) = delete;
    Watchdog(Watchdog&& other) noexcept;
    Watchdog& operator=(Watchdog&& other) noexcept;
    ~Watchdog();

    void feed() const;

private:
    Watchdog() : subscribed_(true) {}
    void release();

    bool subscribed_ = false;
};

} // namespace banana::rtos
