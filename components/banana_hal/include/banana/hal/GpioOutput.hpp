#pragma once

#include "hal/gpio_types.h"

#include "banana/core/Result.hpp"

namespace banana::hal {

/// Push-pull digital output. Resets the pin in its destructor.
class GpioOutput {
public:
    [[nodiscard]] static Result<GpioOutput> create(gpio_num_t pin, bool initialLevel = false);

    GpioOutput(const GpioOutput&) = delete;
    GpioOutput& operator=(const GpioOutput&) = delete;
    GpioOutput(GpioOutput&& other) noexcept;
    GpioOutput& operator=(GpioOutput&& other) noexcept;
    ~GpioOutput();

    void set(bool high);
    void toggle() { set(!level_); }

    [[nodiscard]] bool level() const { return level_; }
    [[nodiscard]] gpio_num_t pin() const { return pin_; }

private:
    GpioOutput(gpio_num_t pin, bool level) : pin_(pin), level_(level) {}
    void release();

    gpio_num_t pin_ = GPIO_NUM_NC;
    bool level_ = false;
};

} // namespace banana::hal
