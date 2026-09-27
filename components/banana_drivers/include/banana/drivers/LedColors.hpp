#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

#include "banana/config/Config.hpp"
#include "banana/io/Outputs.hpp"

namespace banana::drivers {

/// Duty counts for red, green, blue (setColor() of the Arduino firmware). The base values are on a
/// 0..255 scale regardless of the resolution; they are only clamped to 2^bits - 1.
[[nodiscard]] inline std::array<std::uint32_t, 3> rgbCounts(io::LedColor color, bool channelGains,
                                                            const config::LedSettings& settings)
{
    const config::ColorGains& f = settings.colorGains;
    std::array<float, 3> rgb{};
    switch (color) {
    case io::LedColor::Red:
        rgb = {255.0F * f.red, 0.0F, 0.0F};
        break;
    case io::LedColor::Green:
        rgb = {0.0F, 255.0F * f.green, 0.0F};
        break;
    case io::LedColor::Blue:
        rgb = {0.0F, 0.0F, 255.0F * f.blue};
        break;
    case io::LedColor::Orange:
        rgb = {255.0F * f.orange, 10.0F * f.orange, 0.0F};
        break;
    case io::LedColor::Purple:
        rgb = {170.0F * f.purple, 0.0F, 255.0F * f.purple};
        break;
    case io::LedColor::White:
        rgb = {100.0F * f.white, 100.0F * f.white, 100.0F * f.white};
        break;
    }
    if (channelGains) {
        rgb[0] *= settings.channelGains.red;
        rgb[1] *= settings.channelGains.green;
        rgb[2] *= settings.channelGains.blue;
    }
    const auto maxCounts = static_cast<float>((1U << settings.resolutionBits) - 1U);
    std::array<std::uint32_t, 3> counts{};
    std::ranges::transform(rgb, counts.begin(), [maxCounts](float value) {
        return static_cast<std::uint32_t>(std::clamp(value, 0.0F, maxCounts)); // truncates like (int)
    });
    return counts;
}

/// Pulse ("breathing"): one fade in and out per period.
inline constexpr std::chrono::milliseconds kPulsePeriod{2000};
inline constexpr float kPulseMinLevel = 0.05F; ///< perceived brightness at the dimmest point
inline constexpr float kPulseGamma = 2.2F;     ///< perceived brightness -> duty (eyes are logarithmic)

/// Duty factor 0..1 at `elapsed` since the pulse started: starts and ends each period at full brightness,
/// dimmest in the middle, cosine-shaped in perceived brightness.
[[nodiscard]] inline float pulseLevel(std::chrono::milliseconds elapsed)
{
    const auto period = kPulsePeriod.count();
    const float phase = static_cast<float>(elapsed.count() % period) / static_cast<float>(period);
    constexpr float kTwoPi = 6.2831853F;
    const float perceived =
        kPulseMinLevel + ((1.0F - kPulseMinLevel) * 0.5F * (1.0F + std::cos(kTwoPi * phase)));
    return std::pow(perceived, kPulseGamma);
}

/// `counts` dimmed by `level` (0..1).
[[nodiscard]] inline std::array<std::uint32_t, 3> dimmed(const std::array<std::uint32_t, 3>& counts,
                                                         float level)
{
    std::array<std::uint32_t, 3> result{};
    std::ranges::transform(counts, result.begin(), [level](std::uint32_t value) {
        return static_cast<std::uint32_t>(
            std::lround(static_cast<float>(value) * std::clamp(level, 0.0F, 1.0F)));
    });
    return result;
}

} // namespace banana::drivers
