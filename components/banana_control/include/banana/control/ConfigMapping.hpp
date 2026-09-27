#pragma once

#include "banana/config/Config.hpp"
#include "banana/control/BrewFeedForward.hpp"
#include "banana/control/PidController.hpp"

/// Translation of the stored configuration into controller settings (today configPID()).
namespace banana::control {

[[nodiscard]] PidController::Settings toPidSettings(const config::PidSettings& pid);
[[nodiscard]] BrewFeedForward::Settings toBrewFeedForwardSettings(const config::PidSettings& pid);

} // namespace banana::control
