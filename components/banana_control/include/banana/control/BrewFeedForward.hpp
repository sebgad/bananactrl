#pragma once

#include <chrono>

#include "banana/control/Limits.hpp"

namespace banana::control {

/// Open-loop heater output while brewing.
///
/// Cold water enters the boiler and the element lags by about a minute, so the PID cannot keep up.
/// The output starts at `start`, decays exponentially towards `end` with `tau`, and is trimmed by
/// `gain` counts per Kelvin of remaining control deviation. Front loading limits the temperature
/// drop; the decay avoids overshoot after the shot.
class BrewFeedForward {
public:
    using Seconds = std::chrono::duration<float>;

    struct Settings {
        float start = 255.0F;
        float end = 10.0F;
        Seconds tau{14.0F}; ///< 0 = jump to `end` immediately
        float gain = 35.0F;
        Limits limits;

        friend constexpr bool operator==(const Settings&, const Settings&) = default;
    };

    explicit BrewFeedForward(const Settings& settings) : settings_(settings) {}

    void configure(const Settings& settings) { settings_ = settings; }

    /// Brewing has started: charge the feed-forward to `start`.
    void begin() { level_ = settings_.start; }

    /// One step while brewing. `dt` is the time since begin() or the previous step.
    [[nodiscard]] float compute(float target, float actual, std::chrono::milliseconds dt);

    [[nodiscard]] const Settings& settings() const { return settings_; }
    /// Current feed-forward level without the deviation trim.
    [[nodiscard]] float level() const { return level_; }

private:
    Settings settings_;
    float level_ = 0.0F;
};

} // namespace banana::control
