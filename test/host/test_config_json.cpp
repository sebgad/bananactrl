// params.json codec: Arduino key names, missing keys, and the stored-false bug of the Arduino firmware.

#include <string>

#include "banana/config/ConfigJson.hpp"

#include <gtest/gtest.h>

namespace {

using banana::config::Config;
using banana::config::fromJson;
using banana::config::toJson;

/// params.json as saveConfiguration() wrote it (values changed from the defaults).
constexpr const char* kArduinoFile = R"({
  "Wifi": {"wifiSSID": "HomeNet", "wifiPassword": "secret"},
  "PID": {
    "CtrlTimeFactor": false, "CtrlPropActivate": true, "CtrlPropFactor": 12.5,
    "CtrlIntActivate": false, "CtrlIntFactor": 0, "CtrlDifActivate": true, "CtrlDifFactor": 3,
    "CtrlTarget": 93, "LowThresholdActivate": true, "LowThresholdValue": 60,
    "HighThresholdActivate": true, "HighTresholdValue": 98, "LowLimitManipulation": 0,
    "HighLimitManipulation": 200, "CtrlDifFilterTime": 2.5,
    "BrewFfStart": 240, "BrewFfEnd": 20, "BrewFfTau": 10, "BrewFfGain": 30
  },
  "SSR": {"SsrFreq": 10, "PwmSsrResolution": 10},
  "LED": {
    "RwmRgbFreq": 1000, "RwmRgbResolution": 8, "GainFactorRed": 0.5, "GainFactorGreen": 1, "GainFactorBlue": 2,
    "GainFactorColorRed": 1, "GainFactorColorGreen": 0.8, "GainFactorColorBlue": 1, "GainFactorColorOrange": 1,
    "GainFactorColorPurple": 0.3, "GainFactorColorWhite": 1
  },
  "Signal": {"SigFilterActive": false},
  "System": {"TimeToStandby": 7200}
})";

TEST(ConfigJson, ReadsArduinoFile)
{
    const auto parsed = fromJson(kArduinoFile);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->complete); // no "MQTT" section in Arduino files: defaults, written back
    const Config& c = parsed->config;
    EXPECT_FALSE(c.mqtt.enabled);
    EXPECT_EQ(c.mqtt.port, 1883U);
    EXPECT_EQ(c.wifi.ssid, "HomeNet");
    EXPECT_EQ(c.wifi.password, "secret");
    EXPECT_FALSE(c.pid.timeFactor);
    EXPECT_FLOAT_EQ(c.pid.propFactor, 12.5F);
    EXPECT_FALSE(c.pid.intActive);
    EXPECT_FLOAT_EQ(c.pid.intFactor, 0.0F);
    EXPECT_TRUE(c.pid.difActive);
    EXPECT_FLOAT_EQ(c.pid.target, 93.0F);
    EXPECT_FLOAT_EQ(c.pid.highThreshold, 98.0F); // from "HighTresholdValue"
    EXPECT_FLOAT_EQ(c.pid.highLimit, 200.0F);
    EXPECT_FLOAT_EQ(c.pid.difFilterTime.count(), 2.5F);
    EXPECT_FLOAT_EQ(c.pid.brew.tau.count(), 10.0F);
    EXPECT_EQ(c.ssr.frequencyHz, 10U);
    EXPECT_EQ(c.ssr.resolutionBits, 10U);
    EXPECT_EQ(c.led.frequencyHz, 1000U);
    EXPECT_FLOAT_EQ(c.led.channelGains.blue, 2.0F);
    EXPECT_FLOAT_EQ(c.led.colorGains.purple, 0.3F);
    EXPECT_FALSE(c.signal.filterActive);
    EXPECT_EQ(c.system.timeToStandby.count(), 7200);
}

// The Arduino loadConfiguration() tested `(json["x"]) ? ...`, which is false for stored false/0:
// such values were replaced by the defaults. They must survive now.
TEST(ConfigJson, StoredFalseAndZeroAreKept)
{
    Config c;
    c.pid.propActive = false;
    c.pid.timeFactor = false;
    c.signal.filterActive = false;
    c.pid.propFactor = 0.0F;
    const auto parsed = fromJson(toJson(c));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->complete);
    EXPECT_FALSE(parsed->config.pid.propActive);
    EXPECT_FALSE(parsed->config.pid.timeFactor);
    EXPECT_FALSE(parsed->config.signal.filterActive);
    EXPECT_EQ(parsed->config.pid.propFactor, 0.0F);
}

TEST(ConfigJson, RoundTrip)
{
    const auto parsed = fromJson(kArduinoFile);
    ASSERT_TRUE(parsed.has_value());
    const auto again = fromJson(toJson(parsed->config));
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(again->complete);
    EXPECT_EQ(again->config, parsed->config);
}

TEST(ConfigJson, MissingKeysUseBaseAndFlagIncomplete)
{
    // Older file without the brew feed-forward keys
    const auto parsed = fromJson(R"({"PID": {"CtrlTarget": 90}, "System": {"TimeToStandby": 60}})");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->complete);
    EXPECT_FLOAT_EQ(parsed->config.pid.target, 90.0F);
    EXPECT_EQ(parsed->config.system.timeToStandby.count(), 60);
    EXPECT_FLOAT_EQ(parsed->config.pid.brew.start, 255.0F); // default
    EXPECT_FLOAT_EQ(parsed->config.pid.propFactor, 10.0F);
}

TEST(ConfigJson, SteamSettingsFromTheSettingsPage)
{
    // settings.html sends numbers as strings and booleans as booleans
    const auto parsed = fromJson(R"({"PID": {"ReadyBand": "0.5"},
        "Steam": {"SteamDetectionActivate": false, "SteamEnterTemp": "108", "SteamExitTemp": "102",
                  "SteamReadyTemp": "121.5", "SteamReadyLeaveTemp": "117"}})",
                                 Config{});
    ASSERT_TRUE(parsed.has_value());
    const Config& c = parsed->config;
    EXPECT_FLOAT_EQ(c.pid.readyBand, 0.5F);
    EXPECT_FALSE(c.steam.active);
    EXPECT_FLOAT_EQ(c.steam.enter, 108.0F);
    EXPECT_FLOAT_EQ(c.steam.exit, 102.0F);
    EXPECT_FLOAT_EQ(c.steam.ready, 121.5F);
    EXPECT_FLOAT_EQ(c.steam.readyLeave, 117.0F);
}

TEST(ConfigJson, FilesWithoutSteamSettingsGetTheDefaults)
{
    const auto parsed = fromJson(kArduinoFile); // has no "Steam" section and no "ReadyBand"
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FALSE(parsed->complete); // written back with the defaults
    EXPECT_EQ(parsed->config.steam, banana::config::SteamSettings{});
    EXPECT_FLOAT_EQ(parsed->config.pid.readyBand, 1.0F);
}

TEST(ConfigJson, PartialUpdateKeepsCurrentValues)
{
    Config current;
    current.pid.target = 92.0F;
    current.led.frequencyHz = 800;
    const auto parsed = fromJson(R"({"PID": {"CtrlPropFactor": 15}})", current);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_FLOAT_EQ(parsed->config.pid.propFactor, 15.0F);
    EXPECT_FLOAT_EQ(parsed->config.pid.target, 92.0F);
    EXPECT_EQ(parsed->config.led.frequencyHz, 800U);
}

TEST(ConfigJson, InvalidJsonIsAnError)
{
    EXPECT_FALSE(fromJson("{not json").has_value());
    EXPECT_FALSE(fromJson("[1, 2]").has_value());
    EXPECT_FALSE(fromJson("").has_value());
}

TEST(ConfigJson, WritesArduinoKeyNames)
{
    const std::string json = toJson(Config{});
    for (const char* key :
         {"\"wifiSSID\"", "\"HighTresholdValue\"", "\"PwmSsrResolution\"", "\"GainFactorColorWhite\"",
          "\"SigFilterActive\"", "\"TimeToStandby\"", "\"BrewFfGain\"", "\"ReadyBand\"",
          "\"SteamDetectionActivate\"", "\"SteamEnterTemp\"", "\"SteamExitTemp\"", "\"SteamReadyTemp\"",
          "\"SteamReadyLeaveTemp\""}) {
        EXPECT_NE(json.find(key), std::string::npos) << key;
    }
}

TEST(ConfigJson, PublicJsonHidesThePassword)
{
    Config config;
    config.wifi.ssid = "home";
    config.wifi.password = "secret";
    const std::string json = toPublicJson(config);
    EXPECT_EQ(json.find("secret"), std::string::npos);
    const auto parsed = fromJson(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->config.wifi.ssid, "home");
    EXPECT_EQ(parsed->config.wifi.password, "");
}

TEST(ConfigJson, WebUpdateWithEmptyPasswordKeepsTheStoredOne)
{
    Config current;
    current.wifi.ssid = "home";
    current.wifi.password = "secret";
    const auto updated = applyWebUpdate(R"({"Wifi": {"wifiSSID": "home", "wifiPassword": ""}})", current);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->wifi.password, "secret");

    const auto changed = applyWebUpdate(R"({"Wifi": {"wifiSSID": "other", "wifiPassword": "new"}})", current);
    ASSERT_TRUE(changed.has_value());
    EXPECT_EQ(changed->wifi.ssid, "other");
    EXPECT_EQ(changed->wifi.password, "new");
}

TEST(ConfigJson, WebUpdateAcceptsNumbersAsStrings)
{
    // settings.html sends every text input as a JSON string
    const auto updated = applyWebUpdate(
        R"({"PID": {"CtrlTarget": "93.5", "CtrlPropActivate": false}, "SSR": {"SsrFreq": "20"}})", Config{});
    ASSERT_TRUE(updated.has_value());
    EXPECT_FLOAT_EQ(updated->pid.target, 93.5F);
    EXPECT_FALSE(updated->pid.propActive);
    EXPECT_EQ(updated->ssr.frequencyHz, 20U);
    EXPECT_FALSE(applyWebUpdate("{broken", Config{}).has_value());
}

TEST(ConfigJson, MqttSettingsRoundTripAndHiddenPassword)
{
    Config config;
    config.mqtt = {
        .enabled = true, .host = "homeassistant.local", .port = 1884, .user = "coffee", .password = "pw"};
    const auto parsed = fromJson(toJson(config));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_TRUE(parsed->complete);
    EXPECT_EQ(parsed->config.mqtt, config.mqtt);

    EXPECT_EQ(toPublicJson(config).find("\"pw\""), std::string::npos);
    const auto updated = applyWebUpdate(
        R"({"MQTT": {"MqttEnabled": true, "MqttHost": "10.0.0.2", "MqttPort": "1883", "MqttPassword": ""}})",
        config);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->mqtt.host, "10.0.0.2");
    EXPECT_EQ(updated->mqtt.port, 1883U);
    EXPECT_EQ(updated->mqtt.user, "coffee");
    EXPECT_EQ(updated->mqtt.password, "pw"); // empty keeps the stored one
}

} // namespace
