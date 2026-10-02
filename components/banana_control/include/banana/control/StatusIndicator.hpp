#pragma once

#include "banana/control/MachineState.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::control {

struct LedCommand {
    io::LedColor color = io::LedColor::Green;
    bool channelGains = true;
    io::LedEffect effect = io::LedEffect::Steady;

    friend constexpr bool operator==(const LedCommand&, const LedCommand&) = default;
};

/// Which colour when (LED_CTRL block of the Arduino loop()):
/// fault → purple (no gains), brewing → red, heating up → orange, cooling down → blue, ready → green.
/// Heating up and cooling down pulse (not in the Arduino firmware), so "on its way" and "ready" differ at a
/// glance. Steam mode (not in the Arduino firmware): magenta, blinking while heating up, steady when ready.
[[nodiscard]] constexpr LedCommand indicate(MachineState state)
{
    switch (state) {
    case MachineState::Fault:
        return {.color = io::LedColor::Purple, .channelGains = false};
    case MachineState::Brewing:
        return {.color = io::LedColor::Red};
    case MachineState::SteamHeatingUp:
        return {.color = io::LedColor::Magenta, .effect = io::LedEffect::Blink};
    case MachineState::SteamReady:
        return {.color = io::LedColor::Magenta};
    case MachineState::HeatingUp:
        return {.color = io::LedColor::Orange, .effect = io::LedEffect::Pulse};
    case MachineState::CoolingDown:
        return {.color = io::LedColor::Blue, .effect = io::LedEffect::Pulse};
    case MachineState::Ready:
        return {.color = io::LedColor::Green};
    }
    return {.color = io::LedColor::Purple, .channelGains = false};
}

} // namespace banana::control
