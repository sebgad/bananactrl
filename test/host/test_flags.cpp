#include "banana/core/Flags.hpp"

#include <gtest/gtest.h>

namespace {

enum class Fault { TempOutOfRange, MeasDeviceReset, WifiDisconnect };
using Faults = banana::Flags<Fault>;

TEST(Flags, EmptyByDefault)
{
    constexpr Faults f;
    static_assert(f.none() && !f.any() && f.count() == 0);
}

TEST(Flags, SetResetTest)
{
    Faults f;
    f.set(Fault::WifiDisconnect).set(Fault::TempOutOfRange);
    EXPECT_TRUE(f.test(Fault::WifiDisconnect));
    EXPECT_TRUE(f.test(Fault::TempOutOfRange));
    EXPECT_FALSE(f.test(Fault::MeasDeviceReset));
    EXPECT_EQ(f.count(), 2);
    f.reset(Fault::TempOutOfRange);
    EXPECT_EQ(f.raw(), 1U << 2);
}

// The Arduino check `iErrorId == WIFI_DISCONNECT` could never be true; only() is what it meant.
TEST(Flags, OnlyMeansExactlyThisFlag)
{
    Faults f{Fault::WifiDisconnect};
    EXPECT_TRUE(f.only(Fault::WifiDisconnect));
    f.set(Fault::MeasDeviceReset);
    EXPECT_FALSE(f.only(Fault::WifiDisconnect));
}

TEST(Flags, Operators)
{
    const Faults a{Fault::TempOutOfRange};
    const Faults b{Fault::WifiDisconnect};
    EXPECT_EQ((a | b), (Faults{Fault::TempOutOfRange, Fault::WifiDisconnect}));
    EXPECT_TRUE((a & b).none());
    Faults c = a;
    c.clear();
    EXPECT_TRUE(c.none());
}

} // namespace
