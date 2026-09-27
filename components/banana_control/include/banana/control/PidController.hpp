#pragma once

#include <chrono>
#include <optional>

#include "banana/control/Limits.hpp"

namespace banana::control {

/// Absolute gains: u = kp * e + ki * integral(e dt) + kd * de/dt
struct Gains {
    float kp = 0.0F;
    float ki = 0.0F;
    float kd = 0.0F;

    /// From the time form u = Kp * (e + 1/Tn * integral(e dt) + Tv * de/dt).
    /// Tn = 0 disables the integral part.
    [[nodiscard]] static constexpr Gains fromTimeConstants(float kp, float tn, float tv)
    {
        return {.kp = kp, .ki = (tn != 0.0F) ? kp / tn : 0.0F, .kd = kp * tv};
    }

    friend constexpr bool operator==(const Gains&, const Gains&) = default;
};

/// Which terms take part. I and D only act if P is enabled (as in the Arduino firmware).
struct Terms {
    bool proportional = true;
    bool integral = true;
    bool derivative = false;

    friend constexpr bool operator==(const Terms&, const Terms&) = default;
};

/// Hard override of the output: below `on` -> upper limit, above `off` -> lower limit.
struct Thresholds {
    std::optional<float> on;
    std::optional<float> off;

    friend constexpr bool operator==(const Thresholds&, const Thresholds&) = default;
};

/// PID controller with anti-windup, filtered derivative and on/off thresholds.
/// Pure logic: the caller passes the elapsed time, no clock or hardware access.
class PidController {
public:
    using Seconds = std::chrono::duration<float>;

    struct Settings {
        float target = 0.0F;
        Gains gains;
        Terms terms;
        Limits limits;
        Thresholds thresholds;
        /// Low pass on the derivative (removes the quantisation noise); 0 disables it.
        Seconds derivativeFilterTime{0.0F};

        friend constexpr bool operator==(const Settings&, const Settings&) = default;
    };

    explicit PidController(const Settings& settings) : settings_(settings) {}

    /// Takes effect with the next compute(); the controller state is kept (call reset() to drop it).
    void configure(const Settings& settings) { settings_ = settings; }

    /// One control step. `dt` is the time since the previous step (or since reset()).
    [[nodiscard]] float compute(float actual, std::chrono::milliseconds dt);

    /// Clears integrator and derivative history.
    void reset();

    [[nodiscard]] const Settings& settings() const { return settings_; }
    [[nodiscard]] float target() const { return settings_.target; }
    [[nodiscard]] float integrator() const { return integrator_; }
    /// Difference between the last two control deviations.
    [[nodiscard]] float errorDiff() const { return errorDiff_; }

private:
    Settings settings_;
    float integrator_ = 0.0F;
    float errorDiff_ = 0.0F;
    float errorRateFiltered_ = 0.0F;
    float lastError_ = 0.0F;
    bool hasLastError_ = false;
};

} // namespace banana::control
