#include "banana/drivers/RgbLed.hpp"

#include "banana/drivers/LedColors.hpp"

namespace banana::drivers {

Result<RgbLed> RgbLed::create(const Pins& pins, ledc_timer_t timer, std::array<ledc_channel_t, 3> channels,
                              const config::LedSettings& settings)
{
    auto ledcTimer = hal::LedcTimer::create(
        {.timer = timer, .frequencyHz = settings.frequencyHz, .resolutionBits = settings.resolutionBits});
    if (!ledcTimer) {
        return fail(ledcTimer.error());
    }
    auto red = hal::PwmChannel::create(pins.red, channels[0], *ledcTimer);
    if (!red) {
        return fail(red.error());
    }
    auto green = hal::PwmChannel::create(pins.green, channels[1], *ledcTimer);
    if (!green) {
        return fail(green.error());
    }
    auto blue = hal::PwmChannel::create(pins.blue, channels[2], *ledcTimer);
    if (!blue) {
        return fail(blue.error());
    }
    return RgbLed{std::move(*ledcTimer), {std::move(*red), std::move(*green), std::move(*blue)}, settings};
}

Result<void> RgbLed::configure(const config::LedSettings& settings)
{
    if (auto res = timer_.reconfigure(settings.frequencyHz, settings.resolutionBits); !res) {
        return res;
    }
    settings_ = settings;
    return {};
}

void RgbLed::show(io::LedColor color, bool channelGains)
{
    const auto counts = rgbCounts(color, channelGains, settings_);
    for (std::size_t i = 0; i < channels_.size(); ++i) {
        channels_.at(i).setCounts(counts.at(i));
    }
}

} // namespace banana::drivers
