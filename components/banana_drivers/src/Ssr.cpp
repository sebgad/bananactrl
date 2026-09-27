#include "banana/drivers/Ssr.hpp"

#include <algorithm>

namespace banana::drivers {

Result<Ssr> Ssr::create(gpio_num_t pin, ledc_timer_t timer, ledc_channel_t channel,
                        const config::SsrSettings& settings)
{
    auto ledcTimer = hal::LedcTimer::create(
        {.timer = timer, .frequencyHz = settings.frequencyHz, .resolutionBits = settings.resolutionBits});
    if (!ledcTimer) {
        return fail(ledcTimer.error());
    }
    auto pwm = hal::PwmChannel::create(pin, channel, *ledcTimer); // starts at duty 0
    if (!pwm) {
        return fail(pwm.error());
    }
    return Ssr{std::move(*ledcTimer), std::move(*pwm)};
}

Result<void> Ssr::configure(const config::SsrSettings& settings)
{
    return timer_.reconfigure(settings.frequencyHz, settings.resolutionBits);
}

void Ssr::write(float counts)
{
    // ledcWrite(channel, (int)fTarPwm): truncate; 2^bits means always on
    const auto limit = static_cast<float>(timer_.maxCounts() + 1U);
    channel_.setCounts(static_cast<std::uint32_t>(std::clamp(counts, 0.0F, limit)));
}

} // namespace banana::drivers
