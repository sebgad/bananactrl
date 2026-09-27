#pragma once

#include "banana/control/Diagnostics.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::control {

struct LedCommand {
    io::LedColor color = io::LedColor::Green;
    bool channelGains = true;
    io::LedEffect effect = io::LedEffect::Steady;

    friend constexpr bool operator==(const LedCommand&, const LedCommand&) = default;
};

/// Which colour when (LED_CTRL block of the Arduino loop()):
/// fault → purple (no gains), brewing → red, more than 1 K below target → orange (heating up),
/// more than 1 K above → blue (cooling down), otherwise green. Heating up and cooling down pulse (not in the
/// Arduino firmware), so "on its way" and "ready" differ at a glance.
[[nodiscard]] constexpr LedCommand indicate(Faults faults, bool brewing, float celsius, float target)
{
    if (faults.any()) {
        return {.color = io::LedColor::Purple, .channelGains = false};
    }
    if (brewing) {
        return {.color = io::LedColor::Red, .channelGains = true};
    }
    if (celsius < target - 1.0F) {
        return {.color = io::LedColor::Orange, .channelGains = true, .effect = io::LedEffect::Pulse};
    }
    if (celsius > target + 1.0F) {
        return {.color = io::LedColor::Blue, .channelGains = true, .effect = io::LedEffect::Pulse};
    }
    return {.color = io::LedColor::Green, .channelGains = true};
}

} // namespace banana::control
