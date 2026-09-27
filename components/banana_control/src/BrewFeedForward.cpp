#include "banana/control/BrewFeedForward.hpp"

namespace banana::control {

float BrewFeedForward::compute(float target, float actual, std::chrono::milliseconds dt)
{
    const float dtSec = static_cast<float>(dt.count()) / 1000.0F;
    const float tau = settings_.tau.count();

    if (tau > 0.0F) {
        level_ += (settings_.end - level_) * dtSec / tau;
    } else {
        level_ = settings_.end;
    }
    return settings_.limits.clamp(level_ + (settings_.gain * (target - actual)));
}

} // namespace banana::control
