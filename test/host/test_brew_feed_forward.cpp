#include <chrono>

#include "banana/control/BrewFeedForward.hpp"

#include <gtest/gtest.h>

namespace {

using banana::control::BrewFeedForward;
using std::chrono::milliseconds;

BrewFeedForward::Settings factory()
{
    return {}; // start 255, end 10, tau 14 s, gain 35, limits 0..255
}

TEST(BrewFeedForward, StartsChargedAndClamped)
{
    BrewFeedForward ff{factory()};
    ff.begin();
    // First step right at the brew start: 255 + 35 * (85 - 80) = 430 -> clamped
    EXPECT_FLOAT_EQ(ff.compute(85.0F, 80.0F, milliseconds{0}), 255.0F);
    EXPECT_FLOAT_EQ(ff.level(), 255.0F);
}

TEST(BrewFeedForward, DecaysTowardsEndAndTrimsWithDeviation)
{
    BrewFeedForward ff{factory()};
    ff.begin();
    // Euler step: 255 + (10 - 255) * 1.4 / 14 = 230.5, trim 35 * (85 - 90) = -175
    EXPECT_FLOAT_EQ(ff.compute(85.0F, 90.0F, milliseconds{1400}), 55.5F);
    EXPECT_FLOAT_EQ(ff.level(), 230.5F);

    float previous = ff.level();
    for (int i = 0; i < 400; ++i) {
        (void)ff.compute(85.0F, 85.0F, milliseconds{450});
        EXPECT_LE(ff.level(), previous);
        previous = ff.level();
    }
    EXPECT_NEAR(ff.level(), 10.0F, 0.01F);
}

TEST(BrewFeedForward, ZeroTauJumpsToEnd)
{
    auto settings = factory();
    settings.tau = BrewFeedForward::Seconds{0.0F};
    BrewFeedForward ff{settings};
    ff.begin();
    EXPECT_FLOAT_EQ(ff.compute(85.0F, 85.0F, milliseconds{450}), 10.0F);
}

TEST(BrewFeedForward, ClampsAtLowerLimit)
{
    BrewFeedForward ff{factory()};
    ff.begin();
    EXPECT_FLOAT_EQ(ff.compute(85.0F, 100.0F, milliseconds{14000}), 0.0F);
}

} // namespace
