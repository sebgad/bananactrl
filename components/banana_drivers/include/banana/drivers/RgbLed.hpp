#pragma once

#include <array>
#include <utility>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"
#include "banana/hal/Ledc.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::drivers {

/// Status RGB LED: three PWM channels on one LEDC timer, colour mixing and gains from LedSettings.
class RgbLed final : public io::IStatusLed {
public:
    struct Pins {
        gpio_num_t red;
        gpio_num_t green;
        gpio_num_t blue;
    };

    [[nodiscard]] static Result<RgbLed> create(const Pins& pins, ledc_timer_t timer,
                                               std::array<ledc_channel_t, 3> channels,
                                               const config::LedSettings& settings);

    /// Frequency, resolution and gains (configLED()); the colour shown is kept until the next show().
    [[nodiscard]] Result<void> configure(const config::LedSettings& settings);

    void show(io::LedColor color, bool channelGains) override;

private:
    RgbLed(hal::LedcTimer timer, std::array<hal::PwmChannel, 3> channels, const config::LedSettings& settings)
        : timer_(std::move(timer)), channels_(std::move(channels)), settings_(settings)
    {
    }

    hal::LedcTimer timer_;
    std::array<hal::PwmChannel, 3> channels_;
    config::LedSettings settings_;
};

} // namespace banana::drivers
