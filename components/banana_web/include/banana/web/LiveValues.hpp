#pragma once

#include <span>
#include <string>
#include <string_view>

#include "banana/control/ControlLoop.hpp"
#include "banana/storage/MeasurementCsv.hpp"

namespace banana::web {

/// /lastvalues.json for index.html, with the key names of the Arduino firmware.
/// `wifiPercent`: station signal strength 0..100 % (0 without a station connection).
[[nodiscard]] std::string lastValuesJson(const control::ProcessSnapshot& snapshot, int wifiPercent);

/// data.csv rows as a JSON array of [time, temperature, heater %, target, brewing], same precision as the
/// file.
[[nodiscard]] std::string rowsJson(std::span<const storage::csv::Row> rows);

/// One server-sent event: `event: <name>` + `data: <data>` (single line) + blank line.
[[nodiscard]] std::string sseEvent(std::string_view name, std::string_view data);

} // namespace banana::web
