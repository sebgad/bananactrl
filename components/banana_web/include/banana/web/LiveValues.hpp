#pragma once

#include <string>

#include "banana/control/ControlLoop.hpp"

namespace banana::web {

/// /lastvalues.json for index.html, with the key names of the Arduino firmware.
/// `wifiPercent`: station signal strength 0..100 % (0 without a station connection).
[[nodiscard]] std::string lastValuesJson(const control::ProcessSnapshot& snapshot, int wifiPercent);

} // namespace banana::web
