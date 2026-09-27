#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

/// data.csv exactly as the Arduino firmware wrote it (graphs.html skips 3 lines and parses the rest by
/// column index; Arduino's println() ends lines with CR LF, print(float) uses 2 decimals).
namespace banana::storage::csv {

struct Row {
    float seconds; ///< since the start of the recording
    float celsius;
    float heaterPercent; ///< SSR duty / HighLimitManipulation * 100
    float target;
    bool brewing;
};

struct Header {
    std::string timestamp; ///< epoch seconds, as strftime("%s") in the Arduino firmware
    std::uint16_t adsConfig = 0;
    std::uint16_t adsLowThreshold = 0;
    std::uint16_t adsHighThreshold = 0;
};

/// print(value, BIN): no leading zeros.
[[nodiscard]] inline std::string binary(std::uint16_t value)
{
    if (value == 0) {
        return "0";
    }
    std::string bits;
    for (int bit = 15; bit >= 0; --bit) {
        const bool set = ((value >> static_cast<unsigned>(bit)) & 1U) != 0;
        if (set || !bits.empty()) {
            bits += set ? '1' : '0';
        }
    }
    return bits;
}

[[nodiscard]] inline std::string header(const Header& h)
{
    return "Measurement File created on " + h.timestamp + "\r\n" + "ADS1115 Register Settings\r\n" +
           "Config Register: 0b" + binary(h.adsConfig) + "\r\n" + "Low Threshold Register: 0b" +
           binary(h.adsLowThreshold) + "\r\n" + "High Threshold Register: 0b" + binary(h.adsHighThreshold) +
           "\r\n" + "\r\n" + "Time,Temperature,TargetPWM,TargetTemperature,Brewing\r\n";
}

[[nodiscard]] inline std::string row(const Row& r)
{
    std::array<char, 96> line{};
    const int length = std::snprintf( // NOLINT(cppcoreguidelines-pro-type-vararg)
        line.data(), line.size(), "%.3f,%.2f,%.2f,%.2f,%d\r\n", static_cast<double>(r.seconds),
        static_cast<double>(r.celsius), static_cast<double>(r.heaterPercent), static_cast<double>(r.target),
        r.brewing ? 1 : 0);
    return {line.data(), static_cast<std::size_t>(length > 0 ? length : 0)};
}

} // namespace banana::storage::csv
