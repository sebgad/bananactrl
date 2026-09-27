#include "banana/drivers/Ads1115Config.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace banana::drivers::ads1115 {
namespace {

template <typename E, std::size_t N>
const char* nameOf(E value, const std::array<const char*, N>& names)
{
    const auto index = static_cast<std::size_t>(value);
    return index < N ? names.at(index) : "?";
}

constexpr std::array kMux{"AIN0-AIN1", "AIN0-AIN3", "AIN1-AIN3", "AIN2-AIN3",
                          "AIN0-GND",  "AIN1-GND",  "AIN2-GND",  "AIN3-GND"};
constexpr std::array kPga{"6.144V", "4.096V", "2.048V", "1.024V", "0.512V", "0.256V"};
constexpr std::array kRate{"8", "16", "32", "64", "128", "250", "475", "860"};
constexpr std::array kQueue{"1", "2", "4", "off"};

} // namespace

std::string ConfigRegister::toString() const
{
    // snprintf instead of std::format: <format> costs ~300 KB of flash on the ESP32.
    std::array<char, 128> buffer{};
    const int length = std::snprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
        buffer.data(), buffer.size(), "0x%04X MUX=%s PGA=±%s MODE=%s DR=%sSPS COMP=%s POL=%s LAT=%s QUE=%s",
        static_cast<unsigned>(pack()), nameOf(mux, kMux), nameOf(pga, kPga),
        mode == Mode::Continuous ? "continuous" : "single", nameOf(rate, kRate),
        comparatorMode == ComparatorMode::Window ? "window" : "traditional",
        polarity == Polarity::ActiveHigh ? "high" : "low", latch == Latch::Latching ? "on" : "off",
        nameOf(queue, kQueue));
    return {buffer.data(), static_cast<std::size_t>(std::max(length, 0))};
}

} // namespace banana::drivers::ads1115
