#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"
#include "banana/hal/Ledc.hpp"
#include "banana/hal/PeriodicTimer.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::drivers {

/// Status RGB LED: three PWM channels on one LEDC timer, colour mixing and gains from LedSettings.
/// LedEffect::Pulse fades the colour in and out with its own timer (every kPulseStep, only while pulsing).
/// Thread-safe: the heater task sets the colour, the timer task animates it.
class RgbLed final : public io::IStatusLed {
public:
    struct Pins {
        gpio_num_t red;
        gpio_num_t green;
        gpio_num_t blue;
    };

    static constexpr std::chrono::milliseconds kPulseStep{20}; ///< 50 updates per second: smooth to the eye

    [[nodiscard]] static Result<RgbLed> create(const Pins& pins, ledc_timer_t timer,
                                               std::array<ledc_channel_t, 3> channels,
                                               const config::LedSettings& settings);

    /// Frequency, resolution and gains (configLED()); the colour and effect shown are kept.
    [[nodiscard]] Result<void> configure(const config::LedSettings& settings);

    void show(io::LedColor color, bool channelGains, io::LedEffect effect) override;

private:
    /// On the heap: the timer callback holds a pointer to it, and RgbLed itself stays movable.
    struct State {
        State(hal::LedcTimer timer, std::array<hal::PwmChannel, 3> channels,
              const config::LedSettings& settings)
            : timer(std::move(timer)), channels(std::move(channels)), settings(settings)
        {
        }

        void write(float level); ///< mutex held
        static void onPulse(void* arg);

        std::mutex mutex;
        hal::LedcTimer timer;
        std::array<hal::PwmChannel, 3> channels;
        config::LedSettings settings;
        io::LedColor color = io::LedColor::White;
        bool channelGains = true;
        io::LedEffect effect = io::LedEffect::Steady;
        bool shown = false;            ///< a colour was set
        std::int64_t pulseStartUs = 0; ///< esp_timer time the current pulse began
        std::optional<hal::PeriodicTimer> pulseTimer;
        bool pulsing = false;
    };

    explicit RgbLed(std::unique_ptr<State> state) : state_(std::move(state)) {}

    std::unique_ptr<State> state_;
};

} // namespace banana::drivers
