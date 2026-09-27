#include "banana/web/LiveValues.hpp"

#include <array>
#include <cstdio>

#include <ArduinoJson.h>

namespace banana::web {

std::string lastValuesJson(const control::ProcessSnapshot& snapshot, int wifiPercent)
{
    JsonDocument doc;
    doc["Time"] = snapshot.seconds;
    doc["Temperature"] = snapshot.celsius;

    JsonObject pid = doc["PID"].to<JsonObject>();
    pid["TargetValue"] = snapshot.target;
    pid["TargetPWM"] = snapshot.heaterPercent;
    pid["ErrorIntegrator"] = snapshot.pidIntegrator;
    pid["ErrorDiff"] = snapshot.pidErrorDiff;

    doc["WiFi"]["SignalStrength in %"] = wifiPercent;

    std::string json;
    serializeJson(doc, json);
    return json;
}

std::string rowsJson(std::span<const storage::csv::Row> rows)
{
    std::string json = "[";
    std::array<char, 80> item{};
    for (const storage::csv::Row& row : rows) {
        const int length = std::snprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
            item.data(), item.size(), "%s[%.3f,%.2f,%.2f,%.2f,%d]", json.size() > 1 ? "," : "",
            static_cast<double>(row.seconds), static_cast<double>(row.celsius),
            static_cast<double>(row.heaterPercent), static_cast<double>(row.target), row.brewing ? 1 : 0);
        if (length > 0) {
            json.append(item.data(), static_cast<std::size_t>(length));
        }
    }
    json += ']';
    return json;
}

std::string sseEvent(std::string_view name, std::string_view data)
{
    std::string event;
    event.reserve(name.size() + data.size() + 16);
    event.append("event: ").append(name).append("\ndata: ").append(data).append("\n\n");
    return event;
}

} // namespace banana::web
