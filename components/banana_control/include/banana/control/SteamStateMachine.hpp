#pragma once

#include <cstdint>

#include "banana/config/Config.hpp"

namespace banana::control {

enum class SteamState : std::uint8_t {
    Off,       ///< brew mode
    HeatingUp, ///< steam switch on: above `enter`, not yet at the bimetal temperature
    Ready,     ///< at the bimetal temperature
    Cooldown,  ///< fell below `readyLeave`: brew mode again, back to Ready only at `ready`, re-armed below
               ///< `exit`
};

/// Detects steaming from the temperature alone. The steam switch bypasses the SSR and heats up to the
/// bimetal switch (about 120 °C); the firmware's own control never gets the boiler that far above the target,
/// so a temperature above `enter` can only come from the steam switch. There is no input for the switch.
class SteamStateMachine {
public:
    constexpr SteamStateMachine() = default;
    constexpr explicit SteamStateMachine(const config::SteamSettings& settings) : settings_(settings) {}

    /// Takes effect at the next update().
    constexpr void configure(const config::SteamSettings& settings) { settings_ = settings; }

    /// Returns true if the state changed.
    constexpr bool update(float celsius, float target)
    {
        const SteamState next = nextState(celsius, target);
        const bool changed = next != state_;
        state_ = next;
        return changed;
    }

    [[nodiscard]] constexpr SteamState state() const { return state_; }
    /// Steam switch heating: the SSR stays off and the PID frozen.
    [[nodiscard]] constexpr bool steaming() const
    {
        return state_ == SteamState::HeatingUp || state_ == SteamState::Ready;
    }
    /// False if switched off, or if the target is not below `exit`: the PID's overshoot could then reach
    /// `enter`, and after steaming the temperature would never fall below `exit` to re-arm the detection.
    [[nodiscard]] constexpr bool enabled(float target) const
    {
        return settings_.active && target < settings_.exit;
    }

private:
    [[nodiscard]] constexpr SteamState nextState(float celsius, float target) const
    {
        const config::SteamSettings& t = settings_;
        if (!enabled(target)) {
            return SteamState::Off;
        }
        switch (state_) {
        case SteamState::Off:
            if (celsius >= t.ready) {
                return SteamState::Ready; // e.g. booted while steaming
            }
            return celsius >= t.enter ? SteamState::HeatingUp : SteamState::Off;
        case SteamState::HeatingUp:
            if (celsius < t.exit) {
                return SteamState::Off; // switched off before it was ready
            }
            return celsius >= t.ready ? SteamState::Ready : SteamState::HeatingUp;
        case SteamState::Ready:
            if (celsius < t.exit) {
                return SteamState::Off;
            }
            return celsius < t.readyLeave ? SteamState::Cooldown : SteamState::Ready;
        case SteamState::Cooldown:
            if (celsius < t.exit) {
                return SteamState::Off;
            }
            return celsius >= t.ready ? SteamState::Ready : SteamState::Cooldown;
        }
        return SteamState::Off;
    }

    config::SteamSettings settings_;
    SteamState state_ = SteamState::Off;
};

} // namespace banana::control
