#pragma once

#include "banana/control/Diagnostics.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::control {

struct LedCommand {
    io::LedColor color;
    bool channelGains;

    friend constexpr bool operator==(const LedCommand&, const LedCommand&) = default;
};

/// Which colour when (LED_CTRL block of the Arduino loop()):
/// fault → purple (no gains), brewing → red, more than 1 K below target → orange (heating up),
/// more than 1 K above → blue (cooling down), otherwise green.
[[nodiscard]] constexpr LedCommand indicate(Faults faults, bool brewing, float celsius, float target)
{
    if (faults.any()) {
        return {.color = io::LedColor::Purple, .channelGains = false};
    }
    if (brewing) {
        return {.color = io::LedColor::Red, .channelGains = true};
    }
    if (celsius < target - 1.0F) {
        return {.color = io::LedColor::Orange, .channelGains = true};
    }
    if (celsius > target + 1.0F) {
        return {.color = io::LedColor::Blue, .channelGains = true};
    }
    return {.color = io::LedColor::Green, .channelGains = true};
}

} // namespace banana::control
