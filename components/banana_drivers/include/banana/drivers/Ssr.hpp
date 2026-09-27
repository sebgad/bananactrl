#pragma once

#include <utility>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"
#include "banana/hal/Ledc.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::drivers {

/// Solid state relay of the heater: slow PWM (15 Hz by default) on its own LEDC timer.
class Ssr final : public io::IPwmOutput {
public:
    [[nodiscard]] static Result<Ssr> create(gpio_num_t pin, ledc_timer_t timer, ledc_channel_t channel,
                                            const config::SsrSettings& settings);

    /// Frequency and resolution (the ledcSetup() in configPID()); the duty is kept.
    [[nodiscard]] Result<void> configure(const config::SsrSettings& settings);

    void write(float counts) override;
    [[nodiscard]] std::uint32_t counts() const { return channel_.counts(); }

private:
    Ssr(hal::LedcTimer timer, hal::PwmChannel channel)
        : timer_(std::move(timer)), channel_(std::move(channel))
    {
    }

    hal::LedcTimer timer_;
    hal::PwmChannel channel_;
};

} // namespace banana::drivers
