#include "banana/config/ConfigJson.hpp"

#include <ArduinoJson.h>

namespace banana::config {
namespace {

/// Assigns obj[key] to `target` if present, otherwise flags the document as incomplete.
template <typename T>
void read(JsonVariantConst object, const char* key, T& target, bool& complete)
{
    const JsonVariantConst value = object[key];
    if (value.isNull()) {
        complete = false;
        return;
    }
    target = value.as<T>();
}

void read(JsonVariantConst object, const char* key, Seconds& target, bool& complete)
{
    float seconds = target.count();
    read(object, key, seconds, complete);
    target = Seconds{seconds};
}

void read(JsonVariantConst object, const char* key, std::chrono::seconds& target, bool& complete)
{
    auto seconds = static_cast<std::uint32_t>(target.count());
    read(object, key, seconds, complete);
    target = std::chrono::seconds{seconds};
}

} // namespace

std::string toJson(const Config& c)
{
    JsonDocument doc;

    doc["Wifi"]["wifiSSID"] = c.wifi.ssid;
    doc["Wifi"]["wifiPassword"] = c.wifi.password;

    JsonObject pid = doc["PID"].to<JsonObject>();
    pid["CtrlTimeFactor"] = c.pid.timeFactor;
    pid["CtrlPropActivate"] = c.pid.propActive;
    pid["CtrlPropFactor"] = c.pid.propFactor;
    pid["CtrlIntActivate"] = c.pid.intActive;
    pid["CtrlIntFactor"] = c.pid.intFactor;
    pid["CtrlDifActivate"] = c.pid.difActive;
    pid["CtrlDifFactor"] = c.pid.difFactor;
    pid["CtrlTarget"] = c.pid.target;
    pid["LowThresholdActivate"] = c.pid.lowThresholdActive;
    pid["LowThresholdValue"] = c.pid.lowThreshold;
    pid["HighThresholdActivate"] = c.pid.highThresholdActive;
    pid["HighTresholdValue"] = c.pid.highThreshold; // sic
    pid["LowLimitManipulation"] = c.pid.lowLimit;
    pid["HighLimitManipulation"] = c.pid.highLimit;
    pid["CtrlDifFilterTime"] = c.pid.difFilterTime.count();
    pid["BrewFfStart"] = c.pid.brew.start;
    pid["BrewFfEnd"] = c.pid.brew.end;
    pid["BrewFfTau"] = c.pid.brew.tau.count();
    pid["BrewFfGain"] = c.pid.brew.gain;

    doc["SSR"]["SsrFreq"] = c.ssr.frequencyHz;
    doc["SSR"]["PwmSsrResolution"] = c.ssr.resolutionBits;

    JsonObject led = doc["LED"].to<JsonObject>();
    led["RwmRgbFreq"] = c.led.frequencyHz;
    led["RwmRgbResolution"] = c.led.resolutionBits;
    led["GainFactorRed"] = c.led.channelGains.red;
    led["GainFactorGreen"] = c.led.channelGains.green;
    led["GainFactorBlue"] = c.led.channelGains.blue;
    led["GainFactorColorRed"] = c.led.colorGains.red;
    led["GainFactorColorGreen"] = c.led.colorGains.green;
    led["GainFactorColorBlue"] = c.led.colorGains.blue;
    led["GainFactorColorOrange"] = c.led.colorGains.orange;
    led["GainFactorColorPurple"] = c.led.colorGains.purple;
    led["GainFactorColorWhite"] = c.led.colorGains.white;

    doc["Signal"]["SigFilterActive"] = c.signal.filterActive;
    doc["System"]["TimeToStandby"] = static_cast<std::uint32_t>(c.system.timeToStandby.count());

    std::string json;
    serializeJsonPretty(doc, json);
    return json;
}

Result<ParsedConfig> fromJson(std::string_view json, const Config& base)
{
    JsonDocument doc;
    if (deserializeJson(doc, json.data(), json.size()) != DeserializationError::Ok || !doc.is<JsonObject>()) {
        return fail(ESP_ERR_INVALID_ARG);
    }

    ParsedConfig parsed{.config = base, .complete = true};
    Config& c = parsed.config;
    bool& ok = parsed.complete;

    const JsonVariantConst wifi = doc["Wifi"];
    read(wifi, "wifiSSID", c.wifi.ssid, ok);
    read(wifi, "wifiPassword", c.wifi.password, ok);

    const JsonVariantConst pid = doc["PID"];
    read(pid, "CtrlTimeFactor", c.pid.timeFactor, ok);
    read(pid, "CtrlPropActivate", c.pid.propActive, ok);
    read(pid, "CtrlPropFactor", c.pid.propFactor, ok);
    read(pid, "CtrlIntActivate", c.pid.intActive, ok);
    read(pid, "CtrlIntFactor", c.pid.intFactor, ok);
    read(pid, "CtrlDifActivate", c.pid.difActive, ok);
    read(pid, "CtrlDifFactor", c.pid.difFactor, ok);
    read(pid, "CtrlTarget", c.pid.target, ok);
    read(pid, "LowThresholdActivate", c.pid.lowThresholdActive, ok);
    read(pid, "LowThresholdValue", c.pid.lowThreshold, ok);
    read(pid, "HighThresholdActivate", c.pid.highThresholdActive, ok);
    read(pid, "HighTresholdValue", c.pid.highThreshold, ok); // sic
    read(pid, "LowLimitManipulation", c.pid.lowLimit, ok);
    read(pid, "HighLimitManipulation", c.pid.highLimit, ok);
    read(pid, "CtrlDifFilterTime", c.pid.difFilterTime, ok);
    read(pid, "BrewFfStart", c.pid.brew.start, ok);
    read(pid, "BrewFfEnd", c.pid.brew.end, ok);
    read(pid, "BrewFfTau", c.pid.brew.tau, ok);
    read(pid, "BrewFfGain", c.pid.brew.gain, ok);

    const JsonVariantConst ssr = doc["SSR"];
    read(ssr, "SsrFreq", c.ssr.frequencyHz, ok);
    read(ssr, "PwmSsrResolution", c.ssr.resolutionBits, ok);

    const JsonVariantConst led = doc["LED"];
    read(led, "RwmRgbFreq", c.led.frequencyHz, ok);
    read(led, "RwmRgbResolution", c.led.resolutionBits, ok);
    read(led, "GainFactorRed", c.led.channelGains.red, ok);
    read(led, "GainFactorGreen", c.led.channelGains.green, ok);
    read(led, "GainFactorBlue", c.led.channelGains.blue, ok);
    read(led, "GainFactorColorRed", c.led.colorGains.red, ok);
    read(led, "GainFactorColorGreen", c.led.colorGains.green, ok);
    read(led, "GainFactorColorBlue", c.led.colorGains.blue, ok);
    read(led, "GainFactorColorOrange", c.led.colorGains.orange, ok);
    read(led, "GainFactorColorPurple", c.led.colorGains.purple, ok);
    read(led, "GainFactorColorWhite", c.led.colorGains.white, ok);

    read(doc["Signal"], "SigFilterActive", c.signal.filterActive, ok);
    read(doc["System"], "TimeToStandby", c.system.timeToStandby, ok);

    return parsed;
}

std::string toPublicJson(const Config& config)
{
    Config redacted = config;
    redacted.wifi.password.clear();
    return toJson(redacted);
}

Result<Config> applyWebUpdate(std::string_view json, const Config& current)
{
    auto parsed = fromJson(json, current);
    if (!parsed) {
        return fail(parsed.error());
    }
    Config& updated = parsed->config;
    if (updated.wifi.password.empty()) {
        updated.wifi.password = current.wifi.password;
    }
    return updated;
}

} // namespace banana::config
