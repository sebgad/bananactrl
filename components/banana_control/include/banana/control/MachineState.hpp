#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "banana/control/Diagnostics.hpp"
#include "banana/control/SteamStateMachine.hpp"

namespace banana::control {

/// What the machine is doing, for the LED, the log and Home Assistant.
enum class MachineState : std::uint8_t {
    HeatingUp,      ///< more than the ready band below the target
    Ready,          ///< within target ± ready band (PidSettings::readyBand)
    CoolingDown,    ///< more than the ready band above the target (also after steaming)
    Brewing,        ///< pump running
    SteamHeatingUp, ///< steam switch on, heating up to the bimetal temperature
    SteamReady,     ///< steam switch on, at the bimetal temperature
    Fault,          ///< any fault (the heater is off unless it is only a Wi-Fi fault)
};

inline constexpr std::size_t kMachineStateCount = 7;

/// Priority: fault, brewing, steam, then the temperature relative to target ± readyBand.
[[nodiscard]] constexpr MachineState machineState(Faults faults, bool brewing, SteamState steam,
                                                  float celsius, float target, float readyBand)
{
    if (faults.any()) {
        return MachineState::Fault;
    }
    if (brewing) {
        return MachineState::Brewing;
    }
    if (steam == SteamState::HeatingUp) {
        return MachineState::SteamHeatingUp;
    }
    if (steam == SteamState::Ready) {
        return MachineState::SteamReady;
    }
    if (celsius < target - readyBand) {
        return MachineState::HeatingUp;
    }
    if (celsius > target + readyBand) {
        return MachineState::CoolingDown;
    }
    return MachineState::Ready;
}

/// Name for the log, /lastvalues.json and the Home Assistant enum sensor.
[[nodiscard]] constexpr std::string_view toString(MachineState state)
{
    switch (state) {
    case MachineState::HeatingUp:
        return "heating_up";
    case MachineState::Ready:
        return "ready";
    case MachineState::CoolingDown:
        return "cooling_down";
    case MachineState::Brewing:
        return "brewing";
    case MachineState::SteamHeatingUp:
        return "steam_heating_up";
    case MachineState::SteamReady:
        return "steam_ready";
    case MachineState::Fault:
        return "fault";
    }
    return "unknown";
}

} // namespace banana::control
