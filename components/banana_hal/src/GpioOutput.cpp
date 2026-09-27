#include "banana/hal/GpioOutput.hpp"

#include <utility>

#include "driver/gpio.h"

namespace banana::hal {

Result<GpioOutput> GpioOutput::create(gpio_num_t pin, bool initialLevel)
{
    if (const bool valid = GPIO_IS_VALID_OUTPUT_GPIO(pin); !valid) {
        return fail(ESP_ERR_INVALID_ARG);
    }
    // Set the level before enabling the driver so the pin never glitches.
    if (auto res = toResult(gpio_set_level(pin, initialLevel ? 1 : 0)); !res) {
        return fail(res.error());
    }
    const gpio_config_t config{
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (auto res = toResult(gpio_config(&config)); !res) {
        return fail(res.error());
    }
    return GpioOutput{pin, initialLevel};
}

GpioOutput::GpioOutput(GpioOutput&& other) noexcept
    : pin_(std::exchange(other.pin_, GPIO_NUM_NC)), level_(other.level_)
{
}

GpioOutput& GpioOutput::operator=(GpioOutput&& other) noexcept
{
    if (this != &other) {
        release();
        pin_ = std::exchange(other.pin_, GPIO_NUM_NC);
        level_ = other.level_;
    }
    return *this;
}

GpioOutput::~GpioOutput()
{
    release();
}

void GpioOutput::set(bool high)
{
    // Cannot fail: the pin was validated in create().
    gpio_set_level(pin_, high ? 1 : 0);
    level_ = high;
}

void GpioOutput::release()
{
    if (pin_ != GPIO_NUM_NC) {
        gpio_reset_pin(pin_);
        pin_ = GPIO_NUM_NC;
    }
}

} // namespace banana::hal
