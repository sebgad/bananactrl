#include "banana/mqtt/HomeAssistant.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>

#include <ArduinoJson.h>

namespace banana::mqtt {
namespace {

std::string_view commandName(Command command)
{
    switch (command) {
    case Command::Target:
        return "target";
    case Command::StandbyTime:
        return "standby_time";
    case Command::Restart:
        return "restart";
    }
    return "unknown";
}

constexpr std::array kCommands{Command::Target, Command::StandbyTime, Command::Restart};

/// Fields every entity shares: name, ids, availability, device.
JsonDocument entity(const Device& device, const Topics& topics, std::string_view key, std::string_view name)
{
    JsonDocument doc;
    doc["name"] = name;
    doc["unique_id"] = device.id + "_" + std::string{key};
    doc["object_id"] = "banana_coffee_" + std::string{key};
    doc["availability_topic"] = topics.availability();
    JsonObject dev = doc["device"].to<JsonObject>();
    dev["identifiers"].add(device.id);
    dev["name"] = device.name;
    dev["manufacturer"] = "bananactrl";
    dev["model"] = "Rancilio Silvia PID (ESP32)";
    if (!device.version.empty()) {
        dev["sw_version"] = device.version;
    }
    if (!device.url.empty()) {
        dev["configuration_url"] = device.url;
    }
    return doc;
}

/// Entity reading one field of the state JSON.
JsonDocument stateEntity(const Device& device, const Topics& topics, std::string_view key,
                         std::string_view name)
{
    JsonDocument doc = entity(device, topics, key, name);
    doc["state_topic"] = topics.state();
    doc["value_template"] = "{{ value_json." + std::string{key} + " }}";
    return doc;
}

Message message(std::string_view component, const Device& device, std::string_view key,
                const JsonDocument& doc)
{
    Message msg;
    msg.topic = std::string{kDiscoveryPrefix} + "/" + std::string{component} + "/" + device.id + "/" +
                std::string{key} + "/config";
    serializeJson(doc, msg.payload);
    return msg;
}

std::string faultText(control::Faults faults)
{
    using control::Fault;
    if (faults.none()) {
        return "none";
    }
    std::string text;
    const auto add = [&text](const char* name) {
        text += text.empty() ? name : std::string{", "} + name;
    };
    if (faults.test(Fault::TempOutOfRange)) {
        add("temperature out of range");
    }
    if (faults.test(Fault::MeasDeviceReset)) {
        add("ADC reset/missing");
    }
    if (faults.test(Fault::WifiDisconnect)) {
        add("Wi-Fi disconnected");
    }
    return text;
}

std::optional<float> parseNumber(std::string_view payload)
{
    while (!payload.empty() && (payload.back() == ' ' || payload.back() == '\n' || payload.back() == '\r')) {
        payload.remove_suffix(1);
    }
    const std::string text{payload}; // NUL-terminated for strtof
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    // the whole payload must be the number
    const auto consumed = static_cast<std::size_t>(
        end - text.c_str()); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (text.empty() || consumed != text.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::string Topics::command(Command command) const
{
    return base_ + "/" + std::string{commandName(command)} + "/set";
}

std::optional<Command> Topics::commandFor(std::string_view topic) const
{
    for (const Command command : kCommands) {
        if (topic == this->command(command)) {
            return command;
        }
    }
    return std::nullopt;
}

std::vector<Message> discoveryMessages(const Device& device, const Topics& topics)
{
    std::vector<Message> messages;

    {
        JsonDocument doc = stateEntity(device, topics, "temperature", "Temperature");
        doc["device_class"] = "temperature";
        doc["unit_of_measurement"] = "°C";
        doc["state_class"] = "measurement";
        doc["suggested_display_precision"] = 1;
        messages.push_back(message("sensor", device, "temperature", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "heater", "Heater power");
        doc["unit_of_measurement"] = "%";
        doc["state_class"] = "measurement";
        doc["icon"] = "mdi:fire";
        doc["suggested_display_precision"] = 0;
        messages.push_back(message("sensor", device, "heater", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "standby_in", "Time to standby");
        doc["device_class"] = "duration";
        doc["unit_of_measurement"] = "min";
        doc["icon"] = "mdi:timer-sand";
        messages.push_back(message("sensor", device, "standby_in", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "brewing", "Brewing");
        doc["device_class"] = "running";
        doc["icon"] = "mdi:coffee";
        messages.push_back(message("binary_sensor", device, "brewing", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "standby", "Standby");
        doc["icon"] = "mdi:sleep";
        messages.push_back(message("binary_sensor", device, "standby", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "fault", "Fault");
        doc["device_class"] = "problem";
        doc["entity_category"] = "diagnostic";
        messages.push_back(message("binary_sensor", device, "fault", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "faults", "Fault details");
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:alert-circle-outline";
        messages.push_back(message("sensor", device, "faults", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "wifi", "Wi-Fi signal");
        doc["unit_of_measurement"] = "%";
        doc["state_class"] = "measurement";
        doc["entity_category"] = "diagnostic";
        doc["icon"] = "mdi:wifi";
        messages.push_back(message("sensor", device, "wifi", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "target", "Target temperature");
        doc["command_topic"] = topics.command(Command::Target);
        doc["device_class"] = "temperature";
        doc["unit_of_measurement"] = "°C";
        doc["min"] = kTargetMin;
        doc["max"] = kTargetMax;
        doc["step"] = kTargetStep;
        doc["mode"] = "box";
        messages.push_back(message("number", device, "target", doc));
    }
    {
        JsonDocument doc = stateEntity(device, topics, "standby_time", "Standby after");
        doc["command_topic"] = topics.command(Command::StandbyTime);
        doc["device_class"] = "duration";
        doc["unit_of_measurement"] = "min";
        doc["min"] = kStandbyMinMinutes;
        doc["max"] = kStandbyMaxMinutes;
        doc["step"] = 5;
        doc["mode"] = "box";
        doc["entity_category"] = "config";
        messages.push_back(message("number", device, "standby_time", doc));
    }
    {
        JsonDocument doc = entity(device, topics, "restart", "Restart");
        doc["command_topic"] = topics.command(Command::Restart);
        doc["payload_press"] = "PRESS";
        doc["device_class"] = "restart";
        doc["entity_category"] = "config";
        messages.push_back(message("button", device, "restart", doc));
    }
    return messages;
}

std::string statePayload(const control::ProcessSnapshot& snapshot, const config::Config& config,
                         int wifiPercent)
{
    const auto standbySeconds = static_cast<float>(config.system.timeToStandby.count());
    const float remaining = snapshot.standby ? 0.0F : std::max(0.0F, standbySeconds - snapshot.seconds);

    JsonDocument doc;
    doc["temperature"] = std::round(snapshot.celsius * 100.0F) / 100.0F;
    doc["heater"] = std::round(snapshot.heaterPercent * 10.0F) / 10.0F;
    doc["target"] = snapshot.target;
    doc["brewing"] = snapshot.brewing ? "ON" : "OFF";
    doc["standby"] = snapshot.standby ? "ON" : "OFF";
    doc["fault"] = snapshot.faults.none() ? "OFF" : "ON";
    doc["faults"] = faultText(snapshot.faults);
    doc["wifi"] = wifiPercent;
    doc["standby_time"] = static_cast<int>(config.system.timeToStandby.count() / 60);
    doc["standby_in"] = static_cast<int>(std::ceil(remaining / 60.0F));

    std::string json;
    serializeJson(doc, json);
    return json;
}

bool applyCommand(Command command, std::string_view payload, config::Config& config)
{
    const auto value = parseNumber(payload);
    if (!value) {
        return false;
    }
    switch (command) {
    case Command::Target: {
        const float clamped = std::clamp(*value, kTargetMin, kTargetMax);
        config.pid.target = std::round(clamped / kTargetStep) * kTargetStep;
        return true;
    }
    case Command::StandbyTime: {
        const auto minutes =
            std::clamp(static_cast<int>(std::lround(*value)), kStandbyMinMinutes, kStandbyMaxMinutes);
        config.system.timeToStandby = std::chrono::minutes{minutes};
        return true;
    }
    case Command::Restart:
        return false;
    }
    return false;
}

} // namespace banana::mqtt
