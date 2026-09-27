#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

/// ADS1115 config register (datasheet SBAS444, table 8-3) as a typed value.
namespace banana::drivers::ads1115 {

enum class Mux : std::uint8_t { Ain0Ain1, Ain0Ain3, Ain1Ain3, Ain2Ain3, Ain0Gnd, Ain1Gnd, Ain2Gnd, Ain3Gnd };
enum class Pga : std::uint8_t { Fsr6V144, Fsr4V096, Fsr2V048, Fsr1V024, Fsr0V512, Fsr0V256 };
enum class Mode : std::uint8_t { Continuous, SingleShot };
enum class DataRate : std::uint8_t { Sps8, Sps16, Sps32, Sps64, Sps128, Sps250, Sps475, Sps860 };
enum class ComparatorMode : std::uint8_t { Traditional, Window };
enum class Polarity : std::uint8_t { ActiveLow, ActiveHigh };
enum class Latch : std::uint8_t { NonLatching, Latching };
enum class Queue : std::uint8_t { AssertAfter1, AssertAfter2, AssertAfter4, Disabled };

enum class Register : std::uint8_t {
    Conversion = 0x00,
    Config = 0x01,
    LowThreshold = 0x02,
    HighThreshold = 0x03
};

/// Volts per LSB for a full-scale range.
[[nodiscard]] constexpr float lsbVolts(Pga pga)
{
    switch (pga) {
    case Pga::Fsr6V144:
        return 6.144F / 32768.0F;
    case Pga::Fsr4V096:
        return 4.096F / 32768.0F;
    case Pga::Fsr2V048:
        return 2.048F / 32768.0F;
    case Pga::Fsr1V024:
        return 1.024F / 32768.0F;
    case Pga::Fsr0V512:
        return 0.512F / 32768.0F;
    case Pga::Fsr0V256:
        return 0.256F / 32768.0F;
    }
    return 0.0F;
}

/// Conversion period for a data rate in microseconds.
[[nodiscard]] constexpr std::uint32_t conversionPeriodUs(DataRate rate)
{
    constexpr std::array<std::uint32_t, 8> kSps{8, 16, 32, 64, 128, 250, 475, 860};
    return 1'000'000U / kSps.at(static_cast<std::size_t>(rate));
}

struct ConfigRegister {
    /// Write: 1 starts a single-shot conversion. Read: 1 = no conversion in progress.
    bool os = true;
    Mux mux = Mux::Ain0Ain1;
    Pga pga = Pga::Fsr2V048;
    Mode mode = Mode::SingleShot;
    DataRate rate = DataRate::Sps128;
    ComparatorMode comparatorMode = ComparatorMode::Traditional;
    Polarity polarity = Polarity::ActiveLow;
    Latch latch = Latch::NonLatching;
    Queue queue = Queue::Disabled;

    [[nodiscard]] constexpr std::uint16_t pack() const
    {
        return static_cast<std::uint16_t>((unsigned{os} << 15U) | (bits(mux) << 12U) | (bits(pga) << 9U) |
                                          (bits(mode) << 8U) | (bits(rate) << 5U) |
                                          (bits(comparatorMode) << 4U) | (bits(polarity) << 3U) |
                                          (bits(latch) << 2U) | bits(queue));
    }

    /// Reserved PGA codes 110/111 read back as ±0.256 V (datasheet).
    [[nodiscard]] static constexpr ConfigRegister unpack(std::uint16_t value)
    {
        const auto field = [value](unsigned shift, unsigned mask) {
            return (value >> shift) & mask;
        };
        const unsigned pgaCode = field(9, 0b111);
        return {
            .os = field(15, 1) != 0,
            .mux = static_cast<Mux>(field(12, 0b111)),
            .pga = static_cast<Pga>(pgaCode > 5 ? 5 : pgaCode),
            .mode = static_cast<Mode>(field(8, 1)),
            .rate = static_cast<DataRate>(field(5, 0b111)),
            .comparatorMode = static_cast<ComparatorMode>(field(4, 1)),
            .polarity = static_cast<Polarity>(field(3, 1)),
            .latch = static_cast<Latch>(field(2, 1)),
            .queue = static_cast<Queue>(field(0, 0b11)),
        };
    }

    /// Equal apart from the OS bit, which reads back differently from what was written.
    [[nodiscard]] constexpr bool sameSettings(const ConfigRegister& other) const
    {
        return (pack() | 0x8000U) == (other.pack() | 0x8000U);
    }

    /// e.g. "0x880C MUX=AIN0-AIN1 PGA=±0.512V MODE=continuous DR=8SPS COMP=traditional POL=high LAT=on QUE=1"
    [[nodiscard]] std::string toString() const;

    friend constexpr bool operator==(const ConfigRegister&, const ConfigRegister&) = default;

private:
    template <typename E>
    static constexpr unsigned bits(E value)
    {
        return static_cast<unsigned>(value);
    }
};

/// Result of configADS1115() in the Arduino firmware: AIN0-AIN1, ±0.512 V, 8 SPS, continuous,
/// ALERT/RDY as conversion-ready pulse, active high, latching.
inline constexpr ConfigRegister kCoffeeMachineConfig{.os = true,
                                                     .mux = Mux::Ain0Ain1,
                                                     .pga = Pga::Fsr0V512,
                                                     .mode = Mode::Continuous,
                                                     .rate = DataRate::Sps8,
                                                     .comparatorMode = ComparatorMode::Traditional,
                                                     .polarity = Polarity::ActiveHigh,
                                                     .latch = Latch::Latching,
                                                     .queue = Queue::AssertAfter1};

} // namespace banana::drivers::ads1115
