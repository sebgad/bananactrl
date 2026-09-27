#include "banana/hal/GpioInput.hpp"

#include <utility>

#include "driver/gpio.h"

namespace banana::hal {
namespace {

gpio_int_type_t toIntrType(GpioInput::Edge edge)
{
    switch (edge) {
    case GpioInput::Edge::Rising:
        return GPIO_INTR_POSEDGE;
    case GpioInput::Edge::Falling:
        return GPIO_INTR_NEGEDGE;
    case GpioInput::Edge::Any:
        return GPIO_INTR_ANYEDGE;
    }
    return GPIO_INTR_DISABLE;
}

} // namespace

Result<GpioInput> GpioInput::create(gpio_num_t pin, Pull pull)
{
    if (const bool valid = GPIO_IS_VALID_GPIO(pin); !valid) {
        return fail(ESP_ERR_INVALID_ARG);
    }
    const gpio_config_t config{
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = pull == Pull::Up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = pull == Pull::Down ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (auto res = toResult(gpio_config(&config)); !res) {
        return fail(res.error());
    }
    return GpioInput{pin};
}

GpioInput::GpioInput(GpioInput&& other) noexcept
    : pin_(std::exchange(other.pin_, GPIO_NUM_NC)), isrAdded_(std::exchange(other.isrAdded_, false))
{
}

GpioInput& GpioInput::operator=(GpioInput&& other) noexcept
{
    if (this != &other) {
        release();
        pin_ = std::exchange(other.pin_, GPIO_NUM_NC);
        isrAdded_ = std::exchange(other.isrAdded_, false);
    }
    return *this;
}

GpioInput::~GpioInput()
{
    release();
}

Result<void> GpioInput::onEdge(Edge edge, IsrCallback callback, void* arg)
{
    // One shared ISR service for all pins, installed on first use (thread-safe static init).
    static const esp_err_t kServiceInstalled = gpio_install_isr_service(0);
    if (kServiceInstalled != ESP_OK) {
        return fail(kServiceInstalled);
    }
    disableInterrupt();
    if (auto res = toResult(gpio_set_intr_type(pin_, toIntrType(edge))); !res) {
        return res;
    }
    if (auto res = toResult(gpio_isr_handler_add(pin_, callback, arg)); !res) {
        return res;
    }
    isrAdded_ = true;
    return toResult(gpio_intr_enable(pin_));
}

void GpioInput::disableInterrupt()
{
    if (isrAdded_) {
        gpio_intr_disable(pin_);
        gpio_isr_handler_remove(pin_);
        isrAdded_ = false;
    }
}

bool GpioInput::level() const
{
    return gpio_get_level(pin_) != 0;
}

void GpioInput::release()
{
    if (pin_ != GPIO_NUM_NC) {
        disableInterrupt();
        gpio_reset_pin(pin_);
        pin_ = GPIO_NUM_NC;
    }
}

} // namespace banana::hal
