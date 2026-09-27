#pragma once

#include <chrono>

namespace banana::control {

/// Debounces the pump relay: the first edge starts a window, edges inside it are ignored,
/// after 200 ms the pin level decides (pump relay active → brewing).
class BrewDetector {
public:
    using Millis = std::chrono::milliseconds;
    static constexpr Millis kDebounce{200};

    void edge(Millis now)
    {
        if (!pending_) {
            pending_ = true;
            edgeTime_ = now;
        }
    }

    /// Returns true if the brewing state changed.
    bool poll(Millis now, bool pumpActive)
    {
        if (!pending_ || now - edgeTime_ <= kDebounce) {
            return false;
        }
        pending_ = false;
        const bool changed = brewing_ != pumpActive;
        brewing_ = pumpActive;
        return changed;
    }

    [[nodiscard]] bool brewing() const { return brewing_; }
    [[nodiscard]] bool pending() const { return pending_; }
    /// Time until poll() can decide (for the wait timeout of the caller).
    [[nodiscard]] Millis remaining(Millis now) const
    {
        const Millis left = edgeTime_ + kDebounce - now;
        return left > Millis{0} ? left + Millis{1} : Millis{1};
    }

private:
    Millis edgeTime_{0};
    bool pending_ = false;
    bool brewing_ = false;
};

} // namespace banana::control
