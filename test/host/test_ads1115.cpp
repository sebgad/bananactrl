// ADS1115 register packing and the conversion filter (the I2C part is tested on the bench).

#include <array>
#include <cstdint>
#include <random>

#include "banana/drivers/Ads1115Config.hpp"
#include "banana/drivers/MovingAverage.hpp"

#include <gtest/gtest.h>

namespace {

using namespace banana::drivers::ads1115;
using banana::drivers::MovingAverage;

TEST(Ads1115Config, PowerOnDefault)
{
    // Datasheet reset value; also the defaults of ConfigRegister
    EXPECT_EQ(ConfigRegister{}.pack(), 0x8583);
    EXPECT_EQ(ConfigRegister::unpack(0x8583), ConfigRegister{});
}

// configADS1115() changes the power-on value bit by bit: POL, MUX, DR, PGA, LAT, QUE, MODE.
TEST(Ads1115Config, CoffeeMachineMatchesArduinoRegister)
{
    EXPECT_EQ(kCoffeeMachineConfig.pack(), 0x880C);
}

TEST(Ads1115Config, RoundTripAllFields)
{
    for (unsigned mux = 0; mux < 8; ++mux) {
        for (unsigned pga = 0; pga < 6; ++pga) {
            for (unsigned rate = 0; rate < 8; ++rate) {
                for (unsigned low = 0; low < 32; ++low) {
                    const ConfigRegister reg{
                        .os = (low & 1U) != 0,
                        .mux = static_cast<Mux>(mux),
                        .pga = static_cast<Pga>(pga),
                        .mode = static_cast<Mode>((low >> 1U) & 1U),
                        .rate = static_cast<DataRate>(rate),
                        .comparatorMode = static_cast<ComparatorMode>((low >> 2U) & 1U),
                        .polarity = static_cast<Polarity>((low >> 3U) & 1U),
                        .latch = static_cast<Latch>((low >> 4U) & 1U),
                        .queue = static_cast<Queue>((mux + rate) % 4),
                    };
                    ASSERT_EQ(ConfigRegister::unpack(reg.pack()), reg) << std::hex << reg.pack();
                }
            }
        }
    }
}

TEST(Ads1115Config, ReservedPgaCodesReadAsSmallestRange)
{
    EXPECT_EQ(ConfigRegister::unpack(0b110U << 9U).pga, Pga::Fsr0V256);
    EXPECT_EQ(ConfigRegister::unpack(0b111U << 9U).pga, Pga::Fsr0V256);
}

// In continuous mode OS reads back 0 although 1 was written (seen on the bench: 0x880C -> 0x080C).
TEST(Ads1115Config, SameSettingsIgnoresOsBit)
{
    EXPECT_TRUE(ConfigRegister::unpack(0x080C).sameSettings(kCoffeeMachineConfig));
    EXPECT_FALSE(ConfigRegister::unpack(0x0583).sameSettings(kCoffeeMachineConfig));
}

TEST(Ads1115Config, LsbMatchesArduinoConstants)
{
    EXPECT_FLOAT_EQ(lsbVolts(Pga::Fsr6V144), 0.0001875F);
    EXPECT_FLOAT_EQ(lsbVolts(Pga::Fsr2V048), 0.0000625F);
    EXPECT_FLOAT_EQ(lsbVolts(Pga::Fsr0V512), 0.000015625F);
    EXPECT_EQ(conversionPeriodUs(DataRate::Sps8), 125'000U);
}

TEST(Ads1115Config, ToString)
{
    EXPECT_EQ(
        kCoffeeMachineConfig.toString(),
        "0x880C MUX=AIN0-AIN1 PGA=±0.512V MODE=continuous DR=8SPS COMP=traditional POL=high LAT=on QUE=1");
}

TEST(MovingAverage, AveragesFilledPartThenLastN)
{
    MovingAverage<4> filter;
    EXPECT_EQ(filter.value(), 0.0F);
    filter.push(10);
    EXPECT_FLOAT_EQ(filter.value(), 10.0F);
    filter.push(20);
    EXPECT_FLOAT_EQ(filter.value(), 15.0F);
    filter.push(30);
    filter.push(40);
    EXPECT_FLOAT_EQ(filter.value(), 25.0F);
    filter.push(-100); // replaces 10
    EXPECT_FLOAT_EQ(filter.value(), -2.5F);
    EXPECT_EQ(filter.latest(), -100);
    EXPECT_EQ(filter.size(), 4U);
    filter.clear();
    EXPECT_EQ(filter.size(), 0U);
    EXPECT_EQ(filter.value(), 0.0F);
}

/// ADS1115::getConvVal() of the Arduino driver with the filter active (buffer size 12 -> average).
class ArduinoFilter {
public:
    float push(std::int16_t raw)
    {
        count_ = (count_ + 1) % 12;
        maxFill_ = std::max(maxFill_, count_);
        buffer_.at(static_cast<std::size_t>(count_)) = raw;
        float sum = 0.0F;
        for (int i = 0; i <= maxFill_; ++i) {
            sum += buffer_.at(static_cast<std::size_t>(i));
        }
        return sum / static_cast<float>(maxFill_ + 1);
    }

private:
    std::array<std::int16_t, 12> buffer_{};
    int count_ = -1;
    int maxFill_ = 0;
};

TEST(MovingAverage, MatchesArduinoDriver)
{
    std::mt19937 rng{42};
    std::uniform_int_distribution<int> dist{-32768, 32767};
    ArduinoFilter reference;
    MovingAverage<12> filter;
    for (int i = 0; i < 1000; ++i) {
        const auto raw = static_cast<std::int16_t>(dist(rng));
        const float expected = reference.push(raw);
        filter.push(raw);
        ASSERT_FLOAT_EQ(filter.value(), expected) << "sample " << i;
    }
}

} // namespace
