#pragma once

#include "hal/gpio_types.h"

#include "banana/core/Result.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::hal {

/// Digital input with optional edge interrupt. Resets the pin (and removes the ISR) in its destructor.
class GpioInput final : public io::IDigitalInput {
public:
    enum class Pull { None, Up, Down };
    enum class Edge { Rising, Falling, Any };

    /// Runs in interrupt context: only notify a task or set an event bit.
    using IsrCallback = void (*)(void* arg);

    [[nodiscard]] static Result<GpioInput> create(gpio_num_t pin, Pull pull);

    GpioInput(const GpioInput&) = delete;
    GpioInput& operator=(const GpioInput&) = delete;
    GpioInput(GpioInput&& other) noexcept;
    GpioInput& operator=(GpioInput&& other) noexcept;
    ~GpioInput() override;

    /// Enables the interrupt. `arg` is passed to `callback` unchanged (typically `this` of the owner).
    [[nodiscard]] Result<void> onEdge(Edge edge, IsrCallback callback, void* arg);
    void disableInterrupt();

    [[nodiscard]] bool level() const override;
    [[nodiscard]] gpio_num_t pin() const { return pin_; }

private:
    explicit GpioInput(gpio_num_t pin) : pin_(pin) {}
    void release();

    gpio_num_t pin_ = GPIO_NUM_NC;
    bool isrAdded_ = false;
};

} // namespace banana::hal
