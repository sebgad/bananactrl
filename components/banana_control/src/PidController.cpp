#include "banana/control/PidController.hpp"

#include <algorithm>

namespace banana::control {

float PidController::compute(float actual, std::chrono::milliseconds dt)
{
    const float dtSec = static_cast<float>(dt.count()) / 1000.0F;
    const float error = settings_.target - actual;
    const Gains& gains = settings_.gains;
    const Terms& terms = settings_.terms;

    float output = 0.0F;
    if (terms.proportional) {
        output += gains.kp * error;

        if (terms.derivative) {
            // After start or reset() there is no previous deviation: suppress the derivative kick.
            errorDiff_ = hasLastError_ ? error - lastError_ : 0.0F;

            // compute() directly after reset() sees no elapsed time; dividing would yield NaN.
            if (dtSec > 0.0F) {
                const float rate = errorDiff_ / dtSec;
                const float filterTime = settings_.derivativeFilterTime.count();
                if (filterTime > 0.0F) {
                    const float alpha = std::min(dtSec / filterTime, 1.0F);
                    errorRateFiltered_ += (rate - errorRateFiltered_) * alpha;
                } else {
                    errorRateFiltered_ = rate;
                }
                output += gains.kd * errorRateFiltered_;
            }
        }

        if (terms.integral) {
            const float step = dtSec * error;
            output += gains.ki * (integrator_ + step);

            // Anti-windup: integrate only below the upper limit, and never below zero (no cooling).
            if (output < settings_.limits.upper && integrator_ + step >= 0.0F) {
                integrator_ += step;
            }
        }
    }

    output = settings_.limits.clamp(output);

    if (settings_.thresholds.on && actual < *settings_.thresholds.on) {
        output = settings_.limits.upper;
    }
    if (settings_.thresholds.off && actual > *settings_.thresholds.off) {
        output = settings_.limits.lower;
    }

    lastError_ = error;
    hasLastError_ = true;
    return output;
}

void PidController::reset()
{
    integrator_ = 0.0F;
    errorDiff_ = 0.0F;
    errorRateFiltered_ = 0.0F;
    lastError_ = 0.0F;
    hasLastError_ = false;
}

} // namespace banana::control
