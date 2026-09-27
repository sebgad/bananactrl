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
using banana::io::LedEffect;

TEST(StatusIndicator, Priorities)
{
    EXPECT_EQ(indicate(Faults{Fault::WifiDisconnect}, true, 85.0F, 85.0F),
              (LedCommand{LedColor::Purple, false}));
    EXPECT_EQ(indicate({}, true, 20.0F, 85.0F), (LedCommand{LedColor::Red, true}));
    EXPECT_EQ(indicate({}, false, 83.9F, 85.0F), (LedCommand{LedColor::Orange, true, LedEffect::Pulse}));
    EXPECT_EQ(indicate({}, false, 84.0F, 85.0F), (LedCommand{LedColor::Green, true}));
    EXPECT_EQ(indicate({}, false, 86.0F, 85.0F), (LedCommand{LedColor::Green, true}));
    EXPECT_EQ(indicate({}, false, 86.1F, 85.0F), (LedCommand{LedColor::Blue, true, LedEffect::Pulse}));
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

TEST(StatusIndicator, OnlyHeatingUpAndCoolingDownPulse)
{
    EXPECT_EQ(indicate({}, false, 70.0F, 85.0F).effect, LedEffect::Pulse);  // heating up
    EXPECT_EQ(indicate({}, false, 90.0F, 85.0F).effect, LedEffect::Pulse);  // cooling down
    EXPECT_EQ(indicate({}, false, 85.0F, 85.0F).effect, LedEffect::Steady); // ready
    EXPECT_EQ(indicate({}, true, 70.0F, 85.0F).effect, LedEffect::Steady);  // brewing
    EXPECT_EQ(indicate(Faults{Fault::TempOutOfRange}, false, 5.0F, 85.0F).effect, LedEffect::Steady);
}

TEST(LedPulse, BreathesOncePerPeriod)
{
    using banana::drivers::kPulsePeriod;
    using banana::drivers::pulseLevel;
    EXPECT_NEAR(pulseLevel(0ms), 1.0F, 1e-5F);
    EXPECT_NEAR(pulseLevel(kPulsePeriod), 1.0F, 1e-5F); // periodic
    const float dimmest = pulseLevel(kPulsePeriod / 2);
    EXPECT_LT(dimmest, 0.01F); // nearly off in the middle (gamma)
    EXPECT_GT(dimmest, 0.0F);
    float previous = pulseLevel(0ms);
    for (auto t = 20ms; t <= kPulsePeriod / 2; t += 20ms) { // monotonic fade out, then in
        const float level = pulseLevel(t);
        EXPECT_LE(level, previous + 1e-6F);
        previous = level;
    }
    EXPECT_NEAR(pulseLevel(kPulsePeriod / 4), pulseLevel(kPulsePeriod * 3 / 4), 1e-5F); // symmetric
}

TEST(LedPulse, DimmedCounts)
{
    using banana::drivers::dimmed;
    const std::array<std::uint32_t, 3> full{76, 3, 0};
    EXPECT_EQ(dimmed(full, 1.0F), full);
    EXPECT_EQ(dimmed(full, 0.5F), (std::array<std::uint32_t, 3>{38, 2, 0}));
    EXPECT_EQ(dimmed(full, 0.0F), (std::array<std::uint32_t, 3>{0, 0, 0}));
    EXPECT_EQ(dimmed(full, 2.0F), full); // clamped
}

} // namespace
