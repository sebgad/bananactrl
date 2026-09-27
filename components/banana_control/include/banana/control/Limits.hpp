#pragma once

#include <algorithm>

namespace banana::control {

/// Range of the manipulated variable (SSR duty in counts, 0..255 by default).
struct Limits {
    float lower = 0.0F;
    float upper = 255.0F;

    [[nodiscard]] constexpr float clamp(float value) const
    {
        // Same order as the Arduino firmware: lower bound first, upper bound wins on overlap.
        value = std::max(value, lower);
        return std::min(value, upper);
    }

    friend constexpr bool operator==(const Limits&, const Limits&) = default;
};

} // namespace banana::control
