#include "banana/web/LiveValues.hpp"

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

} // namespace banana::web
