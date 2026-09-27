// StatusIndicator, BrewDetector, Diagnostics and the RGB mixing of setColor().

#include <chrono>

#include "banana/control/BrewDetector.hpp"
#include "banana/control/Diagnostics.hpp"
#include "banana/control/StatusIndicator.hpp"
#include "banana/drivers/LedColors.hpp"

#include <gtest/gtest.h>

namespace {

using namespace std::chrono_literals;
using banana::control::Fault;
using banana::control::Faults;
using banana::control::indicate;
using banana::control::LedCommand;
using banana::drivers::rgbCounts;
using banana::io::LedColor;

TEST(StatusIndicator, Priorities)
{
    EXPECT_EQ(indicate(Faults{Fault::WifiDisconnect}, true, 85.0F, 85.0F),
              (LedCommand{LedColor::Purple, false}));
    EXPECT_EQ(indicate({}, true, 20.0F, 85.0F), (LedCommand{LedColor::Red, true}));
    EXPECT_EQ(indicate({}, false, 83.9F, 85.0F), (LedCommand{LedColor::Orange, true}));
    EXPECT_EQ(indicate({}, false, 84.0F, 85.0F), (LedCommand{LedColor::Green, true}));
    EXPECT_EQ(indicate({}, false, 86.0F, 85.0F), (LedCommand{LedColor::Green, true}));
    EXPECT_EQ(indicate({}, false, 86.1F, 85.0F), (LedCommand{LedColor::Blue, true}));
}

TEST(Diagnostics, Inputs)
{
    using banana::control::diagnose;
    EXPECT_TRUE(diagnose({.celsius = 10.0F, .sensorHealthy = true, .stationDisconnected = false}).none());
    EXPECT_TRUE(diagnose({.celsius = 9.9F}).only(Fault::TempOutOfRange));
    EXPECT_EQ(diagnose({.celsius = 0.0F, .sensorHealthy = false, .stationDisconnected = true}).count(), 3);
}

TEST(Diagnostics, OnlySensorFaultsBlockHeating)
{
    using banana::control::blocksHeating;
    EXPECT_FALSE(blocksHeating({}));
    EXPECT_FALSE(blocksHeating(Faults{Fault::WifiDisconnect}));
    EXPECT_TRUE(blocksHeating(Faults{Fault::TempOutOfRange}));
    EXPECT_TRUE(blocksHeating(Faults{Fault::MeasDeviceReset, Fault::WifiDisconnect}));
}

TEST(BrewDetector, DebounceThenLevel)
{
    banana::control::BrewDetector brew;
    using Millis = banana::control::BrewDetector::Millis;
    EXPECT_FALSE(brew.poll(Millis{0}, true)); // no edge: level ignored
    brew.edge(Millis{1000});
    brew.edge(Millis{1100}); // inside the window: ignored
    EXPECT_EQ(brew.remaining(Millis{1100}), Millis{101});
    EXPECT_FALSE(brew.poll(Millis{1200}, true));
    EXPECT_TRUE(brew.poll(Millis{1201}, true));
    EXPECT_TRUE(brew.brewing());
    EXPECT_FALSE(brew.pending());
}

/// Values of setColor() for the factory settings (8 bit, all factors 1).
TEST(RgbCounts, FactoryColors)
{
    const banana::config::LedSettings led;
    using Rgb = std::array<std::uint32_t, 3>;
    EXPECT_EQ(rgbCounts(LedColor::Red, true, led), (Rgb{255, 0, 0}));
    EXPECT_EQ(rgbCounts(LedColor::Green, true, led), (Rgb{0, 255, 0}));
    EXPECT_EQ(rgbCounts(LedColor::Blue, true, led), (Rgb{0, 0, 255}));
    EXPECT_EQ(rgbCounts(LedColor::Orange, true, led), (Rgb{255, 10, 0}));
    EXPECT_EQ(rgbCounts(LedColor::Purple, false, led), (Rgb{170, 0, 255}));
    EXPECT_EQ(rgbCounts(LedColor::White, true, led), (Rgb{100, 100, 100}));
}

TEST(RgbCounts, GainsSaturationAndResolution)
{
    banana::config::LedSettings led;
    led.channelGains = {.red = 2.0F, .green = 0.5F, .blue = -1.0F};
    led.colorGains.white = 1.5F;
    using Rgb = std::array<std::uint32_t, 3>;
    EXPECT_EQ(rgbCounts(LedColor::White, true, led), (Rgb{255, 75, 0})); // 300 -> 255, 150*0.5, negative -> 0
    EXPECT_EQ(rgbCounts(LedColor::White, false, led), (Rgb{150, 150, 150})); // gains off (fault colour)
    led.resolutionBits = 10;
    EXPECT_EQ(rgbCounts(LedColor::White, true, led), (Rgb{300, 75, 0})); // 0..255 scale is not rescaled
}

} // namespace
