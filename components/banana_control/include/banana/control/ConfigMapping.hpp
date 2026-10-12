#pragma once

#include "banana/config/Config.hpp"
#include "banana/control/BrewFeedForward.hpp"
#include "banana/control/PidController.hpp"

/// Translation of the stored configuration into controller settings (today configPID()).
namespace banana::control {

[[nodiscard]] PidController::Settings toPidSettings(const config::PidSettings& pid);
/// Software steam switch: target `steam.pidTarget`, P, I and D factors from `steam`; active terms and limits
/// as for brewing; no thresholds (they are brewing temperatures; the high threshold would keep the
/// heater off).
[[nodiscard]] PidController::Settings toSteamPidSettings(const config::PidSettings& pid,
                                                         const config::SteamSettings& steam);
[[nodiscard]] BrewFeedForward::Settings toBrewFeedForwardSettings(const config::PidSettings& pid);

} // namespace banana::control
