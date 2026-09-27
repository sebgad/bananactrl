#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "banana/core/Result.hpp"

namespace banana::rtos {

/// FreeRTOS task bound to an object: derive, override run(), call start().
///
/// The destructor deletes a still-running task. Derived classes that own resources used
/// by run() must make run() return (or not be running) before their own members are destroyed.
class Task {
public:
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&&) = delete;
    Task& operator=(Task&&) = delete;

    /// `name` is copied by FreeRTOS. `stackBytes` is in bytes (ESP-IDF convention).
    [[nodiscard]] Result<void> start(const char* name, std::uint32_t stackBytes, UBaseType_t priority,
                                     BaseType_t core = tskNO_AFFINITY);

    [[nodiscard]] bool running() const { return handle_ != nullptr; }
    [[nodiscard]] TaskHandle_t handle() const { return handle_; }

protected:
    Task() = default;
    ~Task(); ///< non-virtual: tasks are never deleted through Task*

    virtual void run() = 0;

private:
    static void trampoline(void* arg);

    TaskHandle_t handle_ = nullptr;
};

} // namespace banana::rtos
