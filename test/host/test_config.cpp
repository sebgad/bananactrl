#include "banana/config/Config.hpp"
#include "banana/control/ConfigMapping.hpp"

#include <gtest/gtest.h>

namespace {

using banana::config::Config;

/// Values of resetConfiguration() in the Arduino firmware.
TEST(Config, DefaultsMatchArduinoFactorySettings)
{
    const Config c;
    EXPECT_TRUE(c.pid.timeFactor);
    EXPECT_TRUE(c.pid.propActive);
    EXPECT_EQ(c.pid.propFactor, 10.0F);
    EXPECT_TRUE(c.pid.intActive);
    EXPECT_EQ(c.pid.intFactor, 350.0F);
    EXPECT_FALSE(c.pid.difActive);
    EXPECT_EQ(c.pid.difFactor, 0.0F);
    EXPECT_EQ(c.pid.difFilterTime.count(), 5.0F);
    EXPECT_EQ(c.pid.target, 85.0F);
    EXPECT_FALSE(c.pid.lowThresholdActive);
    EXPECT_EQ(c.pid.lowThreshold, 0.0F);
    EXPECT_FALSE(c.pid.highThresholdActive);
    EXPECT_EQ(c.pid.highThreshold, 0.0F);
    EXPECT_EQ(c.pid.lowLimit, 0.0F);
    EXPECT_EQ(c.pid.highLimit, 255.0F);
    EXPECT_EQ(c.pid.brew.start, 255.0F);
    EXPECT_EQ(c.pid.brew.end, 10.0F);
    EXPECT_EQ(c.pid.brew.tau.count(), 14.0F);
    EXPECT_EQ(c.pid.brew.gain, 35.0F);
    EXPECT_EQ(c.ssr.frequencyHz, 15U);
    EXPECT_EQ(c.ssr.resolutionBits, 8U);
    EXPECT_EQ(c.led.frequencyHz, 500U);
    EXPECT_EQ(c.led.resolutionBits, 8U);
    EXPECT_EQ(c.led.channelGains, (banana::config::RgbGains{1.0F, 1.0F, 1.0F}));
    EXPECT_EQ(c.led.colorGains, (banana::config::ColorGains{1.0F, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F}));
    EXPECT_TRUE(c.signal.filterActive);
    EXPECT_EQ(c.system.timeToStandby.count(), 3600);
}

TEST(Config, SteamAndReadyBandDefaults)
{
    const Config c;
    EXPECT_EQ(c.pid.readyBand, 1.0F);
    EXPECT_TRUE(c.steam.active);
    EXPECT_EQ(c.steam.enter, 105.0F);
    EXPECT_EQ(c.steam.exit, 100.0F);
    EXPECT_EQ(c.steam.ready, 119.0F);
    EXPECT_EQ(c.steam.readyLeave, 115.0F);
    EXPECT_EQ(c.steam.pidTarget, 120.0F);
    EXPECT_EQ(c.steam.pidTimeout, std::chrono::minutes{15});
    EXPECT_EQ(c.steam.pidPropFactor, 40.0F);
    EXPECT_EQ(c.steam.pidIntFactor, 300.0F);
    EXPECT_EQ(c.steam.pidDifFactor, 15.0F);
    EXPECT_EQ(c.steam.pidBaseOutput, 30.0F);
    EXPECT_TRUE(banana::config::invalidSetting(c).empty());
}

TEST(Config, InvalidSteamOrderOrReadyBandIsRejected)
{
    using banana::config::invalidSetting;
    const auto withSteam = [](auto change) {
        Config c;
        change(c);
        return invalidSetting(c);
    };
    EXPECT_FALSE(withSteam([](Config& c) { c.pid.readyBand = 0.0F; }).empty());
    EXPECT_FALSE(withSteam([](Config& c) { c.steam.exit = 105.0F; }).empty());  // exit == enter
    EXPECT_FALSE(withSteam([](Config& c) { c.steam.enter = 119.0F; }).empty()); // enter == ready
    EXPECT_FALSE(withSteam([](Config& c) { c.steam.readyLeave = 119.5F; }).empty());
    EXPECT_FALSE(withSteam([](Config& c) { c.steam.readyLeave = 99.0F; }).empty());
    EXPECT_TRUE(withSteam([](Config& c) { c.steam.readyLeave = 104.0F; }).empty()); // below enter is fine
}

TEST(Config, InvalidSteamPidTargetOrTimeoutIsRejected)
{
    using banana::config::invalidSetting;
    const auto with = [](auto change) {
        Config c;
        change(c);
        return invalidSetting(c);
    };
    EXPECT_FALSE(with([](Config& c) { c.steam.pidTarget = 85.0F; }).empty()); // == CtrlTarget
    EXPECT_FALSE(with([](Config& c) { c.steam.pidTarget = 140.5F; }).empty());
    EXPECT_FALSE(with([](Config& c) { c.steam.pidTimeout = std::chrono::seconds{0}; }).empty());
    EXPECT_FALSE(with([](Config& c) { c.steam.pidPropFactor = -1.0F; }).empty());
    EXPECT_FALSE(with([](Config& c) { c.steam.pidIntFactor = -1.0F; }).empty());
    EXPECT_FALSE(with([](Config& c) { c.steam.pidDifFactor = -1.0F; }).empty());
    EXPECT_FALSE(with([](Config& c) { c.steam.pidBaseOutput = -1.0F; }).empty());
    EXPECT_FALSE(with([](Config& c) { c.pid.highLimit = 20.0F; }).empty());    // base output above the limit
    EXPECT_TRUE(with([](Config& c) { c.steam.pidIntFactor = 0.0F; }).empty()); // Tn 0: no I part
    EXPECT_TRUE(with([](Config& c) { c.steam.pidTarget = 140.0F; }).empty());
    EXPECT_TRUE(with([](Config& c) { c.steam.pidTarget = 95.0F; }).empty()); // below the bimetal is fine
}

TEST(Config, EqualityComparesAllGroups)
{
    Config a;
    Config b;
    EXPECT_EQ(a, b);
    b.led.colorGains.purple = 0.5F;
    EXPECT_NE(a, b);
    b = a;
    b.steam.ready = 120.0F;
    EXPECT_NE(a, b);
}

TEST(ConfigMapping, TimeFactorConvertsResetAndLeadTime)
{
    banana::config::PidSettings pid;
    pid.difFactor = 3.0F;
    const auto s = banana::control::toPidSettings(pid);
    EXPECT_EQ(s.gains, (banana::control::Gains{.kp = 10.0F, .ki = 10.0F / 350.0F, .kd = 30.0F}));
    EXPECT_EQ(s.target, 85.0F);
    EXPECT_FALSE(s.thresholds.on.has_value());
    EXPECT_FALSE(s.thresholds.off.has_value());
    EXPECT_EQ(s.limits, (banana::control::Limits{.lower = 0.0F, .upper = 255.0F}));
}

TEST(ConfigMapping, AbsoluteGainsAndThresholds)
{
    banana::config::PidSettings pid;
    pid.timeFactor = false;
    pid.intFactor = 0.2F;
    pid.difFactor = 4.0F;
    pid.lowThresholdActive = true;
    pid.lowThreshold = 50.0F;
    pid.highThresholdActive = true;
    pid.highThreshold = 95.0F;
    const auto s = banana::control::toPidSettings(pid);
    EXPECT_EQ(s.gains, (banana::control::Gains{.kp = 10.0F, .ki = 0.2F, .kd = 4.0F}));
    EXPECT_EQ(s.thresholds.on, 50.0F);
    EXPECT_EQ(s.thresholds.off, 95.0F);
}

TEST(ConfigMapping, SteamPidHasOwnFactorsWithoutThresholds)
{
    banana::config::PidSettings pid;
    pid.propFactor = 25.0F;
    pid.difActive = true;
    pid.highThresholdActive = true;
    pid.highThreshold = 95.0F;
    pid.highLimit = 200.0F;
    banana::config::SteamSettings steam;
    steam.pidTarget = 122.0F;
    const auto brew = banana::control::toPidSettings(pid);
    const auto s = banana::control::toSteamPidSettings(pid, steam);
    EXPECT_EQ(s.target, 122.0F);
    EXPECT_EQ(s.gains, banana::control::Gains::fromTimeConstants(40.0F, 300.0F, 15.0F)); // not the brew Kp
    EXPECT_EQ(s.terms, brew.terms);
    EXPECT_EQ(s.limits.upper, 200.0F);
    EXPECT_FALSE(s.thresholds.on.has_value());
    EXPECT_FALSE(s.thresholds.off.has_value());
}

TEST(ConfigMapping, BrewFeedForwardSharesPidLimits)
{
    banana::config::PidSettings pid;
    pid.highLimit = 200.0F;
    const auto s = banana::control::toBrewFeedForwardSettings(pid);
    EXPECT_EQ(s.start, 255.0F);
    EXPECT_EQ(s.limits.upper, 200.0F);
}

} // namespace
