#include "banana/control/ConfigMapping.hpp"

namespace banana::control {
namespace {

Limits limitsOf(const config::PidSettings& pid)
{
    return {.lower = pid.lowLimit, .upper = pid.highLimit};
}

} // namespace

PidController::Settings toPidSettings(const config::PidSettings& pid)
{
    return {
        .target = pid.target,
        .gains = pid.timeFactor ? Gains::fromTimeConstants(pid.propFactor, pid.intFactor, pid.difFactor)
                                : Gains{.kp = pid.propFactor, .ki = pid.intFactor, .kd = pid.difFactor},
        .terms = {.proportional = pid.propActive, .integral = pid.intActive, .derivative = pid.difActive},
        .limits = limitsOf(pid),
        .thresholds = {.on = pid.lowThresholdActive ? std::optional{pid.lowThreshold} : std::nullopt,
                       .off = pid.highThresholdActive ? std::optional{pid.highThreshold} : std::nullopt},
        .derivativeFilterTime = pid.difFilterTime,
    };
}

BrewFeedForward::Settings toBrewFeedForwardSettings(const config::PidSettings& pid)
{
    return {
        .start = pid.brew.start,
        .end = pid.brew.end,
        .tau = pid.brew.tau,
        .gain = pid.brew.gain,
        .limits = limitsOf(pid),
    };
}

} // namespace banana::control
