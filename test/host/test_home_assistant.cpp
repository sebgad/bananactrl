#include <string>

#include "banana/mqtt/HomeAssistant.hpp"

#include <ArduinoJson.h>
#include <gtest/gtest.h>

namespace {

using namespace banana::mqtt;
using banana::config::Config;

const Device kDevice{.id = "bananactrl_3068b0", .version = "1.0", .url = "http://coffee.local/"};

TEST(HomeAssistant, Topics)
{
    const Topics topics{kDevice.id};
    EXPECT_EQ(topics.availability(), "bananactrl/bananactrl_3068b0/availability");
    EXPECT_EQ(topics.state(), "bananactrl/bananactrl_3068b0/state");
    EXPECT_EQ(topics.command(Command::Target), "bananactrl/bananactrl_3068b0/target/set");
    EXPECT_EQ(topics.commandFor("bananactrl/bananactrl_3068b0/restart/set"), Command::Restart);
    EXPECT_EQ(topics.commandFor("bananactrl/bananactrl_3068b0/standby_time/set"), Command::StandbyTime);
    EXPECT_EQ(topics.commandFor("bananactrl/bananactrl_3068b0/steam_mode/set"), Command::SteamMode);
    EXPECT_EQ(topics.commandFor("bananactrl/bananactrl_3068b0/steam_target/set"), Command::SteamTarget);
    EXPECT_FALSE(topics.commandFor("bananactrl/other/target/set").has_value());
    EXPECT_FALSE(topics.commandFor("bananactrl/bananactrl_3068b0/state").has_value());
}

TEST(HomeAssistant, DiscoveryMessages)
{
    const Topics topics{kDevice.id};
    const auto messages = discoveryMessages(kDevice, topics);
    EXPECT_EQ(messages.size(), 15U);

    bool foundTarget = false;
    bool foundSteam = false;
    bool foundState = false;
    for (const Message& message : messages) {
        EXPECT_TRUE(message.topic.starts_with("homeassistant/")) << message.topic;
        EXPECT_TRUE(message.topic.ends_with("/config")) << message.topic;
        JsonDocument doc;
        ASSERT_EQ(deserializeJson(doc, message.payload), DeserializationError::Ok) << message.topic;
        EXPECT_TRUE(doc["unique_id"].as<std::string>().starts_with(kDevice.id));
        EXPECT_EQ(doc["availability_topic"], topics.availability());
        EXPECT_EQ(doc["device"]["identifiers"][0], kDevice.id);
        EXPECT_LT(message.payload.size(), 1024U) << message.topic; // fits the MQTT buffer comfortably
        if (message.topic == "homeassistant/number/bananactrl_3068b0/target/config") {
            foundTarget = true;
            EXPECT_EQ(doc["command_topic"], topics.command(Command::Target));
            EXPECT_EQ(doc["min"].as<float>(), kTargetMin);
            EXPECT_EQ(doc["max"].as<float>(), kTargetMax);
            EXPECT_EQ(doc["value_template"], "{{ value_json.target }}");
        }
        if (message.topic == "homeassistant/switch/bananactrl_3068b0/steam_mode/config") {
            foundSteam = true;
            EXPECT_EQ(doc["command_topic"], topics.command(Command::SteamMode));
            EXPECT_EQ(doc["value_template"], "{{ value_json.steam_mode }}");
        }
        if (message.topic == "homeassistant/sensor/bananactrl_3068b0/state/config") {
            foundState = true;
            EXPECT_EQ(doc["device_class"], "enum");
            EXPECT_EQ(doc["options"].size(), banana::control::kMachineStateCount);
            EXPECT_EQ(doc["options"][5], "steam_ready");
        }
    }
    EXPECT_TRUE(foundTarget);
    EXPECT_TRUE(foundState);
    EXPECT_TRUE(foundSteam);
}

TEST(HomeAssistant, StatePayload)
{
    banana::control::ProcessSnapshot snapshot;
    snapshot.seconds = 600.0F; // 10 min after boot
    snapshot.celsius = 82.734F;
    snapshot.heaterPercent = 13.26F;
    snapshot.target = 120.0F; // steam target: Home Assistant gets the brew target setting
    snapshot.brewing = true;
    snapshot.steamMode = true;
    snapshot.steamRemaining = 61.0F;
    snapshot.state = banana::control::MachineState::SteamReady;
    snapshot.faults.set(banana::control::Fault::WifiDisconnect);
    Config config;
    config.system.timeToStandby = std::chrono::seconds{7200};
    config.pid.target = 83.0F;

    JsonDocument doc;
    ASSERT_EQ(deserializeJson(doc, statePayload(snapshot, config, 64)), DeserializationError::Ok);
    EXPECT_FLOAT_EQ(doc["temperature"].as<float>(), 82.73F);
    EXPECT_FLOAT_EQ(doc["heater"].as<float>(), 13.3F);
    EXPECT_FLOAT_EQ(doc["target"].as<float>(), 83.0F);
    EXPECT_EQ(doc["brewing"], "ON");
    EXPECT_EQ(doc["state"], "steam_ready");
    EXPECT_EQ(doc["standby"], "OFF");
    EXPECT_EQ(doc["fault"], "ON");
    EXPECT_EQ(doc["faults"], "Wi-Fi disconnected");
    EXPECT_EQ(doc["wifi"], 64);
    EXPECT_EQ(doc["standby_time"], 120);
    EXPECT_EQ(doc["standby_in"], 110);
    EXPECT_EQ(doc["steam_mode"], "ON");
    EXPECT_FLOAT_EQ(doc["steam_target"].as<float>(), 120.0F);
    EXPECT_EQ(doc["steam_off_in"], 2);

    snapshot.standby = true;
    snapshot.faults = {};
    ASSERT_EQ(deserializeJson(doc, statePayload(snapshot, config, 0)), DeserializationError::Ok);
    EXPECT_EQ(doc["standby_in"], 0);
    EXPECT_EQ(doc["faults"], "none");
}

TEST(HomeAssistant, CommandsAreClampedAndValidated)
{
    Config config;
    EXPECT_TRUE(applyCommand(Command::Target, "92.3", config));
    EXPECT_FLOAT_EQ(config.pid.target, 92.5F); // step 0.5
    EXPECT_TRUE(applyCommand(Command::Target, "150", config));
    EXPECT_FLOAT_EQ(config.pid.target, kTargetMax);
    EXPECT_TRUE(applyCommand(Command::Target, "-5", config));
    EXPECT_FLOAT_EQ(config.pid.target, kTargetMin);

    EXPECT_TRUE(applyCommand(Command::StandbyTime, "45", config));
    EXPECT_EQ(config.system.timeToStandby.count(), 45 * 60);
    EXPECT_TRUE(applyCommand(Command::StandbyTime, "1", config));
    EXPECT_EQ(config.system.timeToStandby.count(), kStandbyMinMinutes * 60);

    EXPECT_TRUE(applyCommand(Command::SteamTarget, "122.2", config));
    EXPECT_FLOAT_EQ(config.steam.pidTarget, 122.0F);
    EXPECT_TRUE(applyCommand(Command::SteamTarget, "200", config));
    EXPECT_FLOAT_EQ(config.steam.pidTarget, kSteamTargetMax);
    EXPECT_TRUE(applyCommand(Command::SteamTarget, "90", config));
    EXPECT_FLOAT_EQ(config.steam.pidTarget, kSteamTargetMin);
    EXPECT_TRUE(banana::config::invalidSetting(config).empty());

    const Config before = config;
    EXPECT_FALSE(applyCommand(Command::Target, "abc", config));
    EXPECT_FALSE(applyCommand(Command::Target, "", config));
    EXPECT_FALSE(applyCommand(Command::Target, "90x", config));
    EXPECT_FALSE(applyCommand(Command::Target, "nan", config));
    EXPECT_FALSE(applyCommand(Command::Restart, "PRESS", config));
    EXPECT_FALSE(applyCommand(Command::SteamMode, "ON", config));
    EXPECT_EQ(config, before);
}

TEST(HomeAssistant, SwitchPayload)
{
    EXPECT_EQ(parseSwitch("ON"), true);
    EXPECT_EQ(parseSwitch("OFF"), false);
    EXPECT_FALSE(parseSwitch("on").has_value());
    EXPECT_FALSE(parseSwitch("").has_value());
}

TEST(HomeAssistant, RetainedMomentaryCommandsAreIgnored)
{
    EXPECT_TRUE(isMomentary(Command::SteamMode));
    EXPECT_TRUE(isMomentary(Command::Restart));
    EXPECT_FALSE(isMomentary(Command::Target));
    EXPECT_FALSE(isMomentary(Command::StandbyTime));
    EXPECT_FALSE(isMomentary(Command::SteamTarget));
}

} // namespace
