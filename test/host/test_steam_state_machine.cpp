// SteamStateMachine: steam mode from the temperature alone (no input for the steam switch).

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "banana/control/SteamStateMachine.hpp"

#include <gtest/gtest.h>

namespace {

using banana::control::SteamState;
using banana::control::SteamStateMachine;

constexpr float kTarget = 95.0F;

/// Feeds `from` .. `to` in 0.1 K steps; returns the states seen in order (without repeats).
std::vector<SteamState> ramp(SteamStateMachine& steam, float from, float to)
{
    std::vector<SteamState> seen{steam.state()};
    const float step = to > from ? 0.1F : -0.1F;
    const int steps = static_cast<int>(std::lround((to - from) / step));
    for (int i = 0; i <= steps; ++i) {
        if (steam.update(from + (static_cast<float>(i) * step), kTarget)) {
            seen.push_back(steam.state());
        }
    }
    return seen;
}

TEST(SteamStateMachine, HeatUpReadyAndSwitchOff)
{
    SteamStateMachine steam;
    EXPECT_EQ(ramp(steam, 90.0F, 104.9F), (std::vector{SteamState::Off}));
    EXPECT_FALSE(steam.steaming());
    EXPECT_EQ(ramp(steam, 105.0F, 118.9F), (std::vector{SteamState::Off, SteamState::HeatingUp}));
    EXPECT_TRUE(steam.steaming());
    EXPECT_EQ(ramp(steam, 119.0F, 121.0F), (std::vector{SteamState::HeatingUp, SteamState::Ready}));
    EXPECT_TRUE(steam.steaming());
    // Switched off: cools down, leaves steam mode below 115 °C, re-armed below 100 °C
    EXPECT_EQ(ramp(steam, 121.0F, 95.0F),
              (std::vector{SteamState::Ready, SteamState::Cooldown, SteamState::Off}));
    EXPECT_FALSE(steam.steaming());
}

TEST(SteamStateMachine, ReadyToleratesBimetalCycling)
{
    SteamStateMachine steam;
    ASSERT_TRUE(steam.update(120.0F, kTarget));
    ASSERT_EQ(steam.state(), SteamState::Ready);
    // Bimetal switch cycling between about 116 and 124 °C: stays ready
    for (int cycle = 0; cycle < 5; ++cycle) {
        EXPECT_EQ(ramp(steam, 124.0F, 115.1F), (std::vector{SteamState::Ready}));
        EXPECT_EQ(ramp(steam, 115.1F, 124.0F), (std::vector{SteamState::Ready}));
    }
}

TEST(SteamStateMachine, SteamingBelowReadyLeaveEndsSteamModeUntilReadyAgain)
{
    SteamStateMachine steam;
    ASSERT_TRUE(steam.update(120.0F, kTarget));
    // Frothing pulls the temperature down to 108 °C: brew mode (cooling down), no new heat-up blink
    EXPECT_EQ(ramp(steam, 120.0F, 108.0F), (std::vector{SteamState::Ready, SteamState::Cooldown}));
    EXPECT_FALSE(steam.steaming());
    EXPECT_EQ(ramp(steam, 108.0F, 118.9F), (std::vector{SteamState::Cooldown}));
    // The bimetal switch reheats it: ready again
    EXPECT_EQ(ramp(steam, 118.9F, 119.5F), (std::vector{SteamState::Cooldown, SteamState::Ready}));
}

TEST(SteamStateMachine, SwitchedOffBeforeReady)
{
    SteamStateMachine steam;
    EXPECT_EQ(ramp(steam, 95.0F, 110.0F), (std::vector{SteamState::Off, SteamState::HeatingUp}));
    EXPECT_EQ(ramp(steam, 110.0F, 99.9F), (std::vector{SteamState::HeatingUp, SteamState::Off}));
}

TEST(SteamStateMachine, BootedWhileSteaming)
{
    SteamStateMachine steam;
    EXPECT_TRUE(steam.update(121.0F, kTarget));
    EXPECT_EQ(steam.state(), SteamState::Ready);

    SteamStateMachine warm;
    EXPECT_TRUE(warm.update(110.0F, kTarget));
    EXPECT_EQ(warm.state(), SteamState::HeatingUp);
}

TEST(SteamStateMachine, OffForTargetsTooCloseToTheThreshold)
{
    SteamStateMachine steam;
    EXPECT_TRUE(steam.enabled(99.5F));
    EXPECT_FALSE(steam.enabled(100.0F)); // not below SteamExitTemp
    EXPECT_FALSE(steam.update(110.0F, 101.0F));
    EXPECT_EQ(steam.state(), SteamState::Off);
    // Target raised while steaming: steam mode ends
    ASSERT_TRUE(steam.update(120.0F, kTarget));
    EXPECT_TRUE(steam.update(120.0F, 101.0F));
    EXPECT_EQ(steam.state(), SteamState::Off);
}

TEST(SteamStateMachine, CustomThresholds)
{
    SteamStateMachine steam{{.enter = 110.0F, .exit = 105.0F, .ready = 124.0F, .readyLeave = 120.0F}};
    EXPECT_FALSE(steam.update(109.9F, kTarget));
    EXPECT_TRUE(steam.update(110.0F, kTarget));
    EXPECT_EQ(steam.state(), SteamState::HeatingUp);
    EXPECT_TRUE(steam.update(124.0F, kTarget));
    EXPECT_EQ(steam.state(), SteamState::Ready);
}

TEST(SteamStateMachine, SwitchedOffInTheSettings)
{
    SteamStateMachine steam;
    ASSERT_TRUE(steam.update(120.0F, kTarget));
    steam.configure({.active = false});
    EXPECT_FALSE(steam.enabled(kTarget));
    EXPECT_TRUE(steam.update(120.0F, kTarget));
    EXPECT_EQ(steam.state(), SteamState::Off);
    steam.configure({});
    EXPECT_TRUE(steam.update(120.0F, kTarget));
    EXPECT_EQ(steam.state(), SteamState::Ready);
}

/// The heat-up and thermostat cycling of the stock machine never looks like steam.
TEST(SteamStateMachine, StockMeasurementNeverSteams)
{
    std::ifstream file{std::string{BANANA_TEST_DATA_DIR} + "/rancilio_silvia_stock_measurement.csv"};
    ASSERT_TRUE(file.is_open());
    SteamStateMachine steam;
    std::string line;
    int rows = 0;
    while (std::getline(file, line)) {
        std::istringstream fields{line};
        std::string time;
        std::string celsius;
        if (!std::getline(fields, time, ',') || !std::getline(fields, celsius, ',') || celsius.empty() ||
            celsius == "Temperature") {
            continue;
        }
        steam.update(std::stof(celsius), 90.0F);
        EXPECT_EQ(steam.state(), SteamState::Off) << "at t=" << time;
        ++rows;
    }
    EXPECT_GT(rows, 6000);
}

} // namespace
