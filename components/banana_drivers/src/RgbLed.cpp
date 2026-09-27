#include "banana/drivers/RgbLed.hpp"

#include "esp_timer.h"

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
    auto state = std::make_unique<State>(
        std::move(*ledcTimer), std::array{std::move(*red), std::move(*green), std::move(*blue)}, settings);
    auto pulseTimer = hal::PeriodicTimer::create("led_pulse", &State::onPulse, state.get());
    if (!pulseTimer) {
        return fail(pulseTimer.error());
    }
    state->pulseTimer.emplace(std::move(*pulseTimer));
    return RgbLed{std::move(state)};
}

Result<void> RgbLed::configure(const config::LedSettings& settings)
{
    const std::scoped_lock lock{state_->mutex};
    if (auto res = state_->timer.reconfigure(settings.frequencyHz, settings.resolutionBits); !res) {
        return res;
    }
    state_->settings = settings;
    if (state_->shown && !state_->pulsing) {
        state_->write(1.0F); // new gains for a steady colour (a pulse picks them up at its next step)
    }
    return {};
}

void RgbLed::show(io::LedColor color, bool channelGains, io::LedEffect effect)
{
    State& s = *state_;
    const std::scoped_lock lock{s.mutex};
    if (s.shown && color == s.color && channelGains == s.channelGains && effect == s.effect) {
        return; // repeated every 1.35 s by the heater task: keep a running pulse in phase
    }
    s.color = color;
    s.channelGains = channelGains;
    s.effect = effect;
    s.shown = true;

    if (effect == io::LedEffect::Pulse) {
        s.pulseStartUs = esp_timer_get_time();
        s.write(1.0F);
        if (!s.pulsing && s.pulseTimer && s.pulseTimer->start(kPulseStep)) {
            s.pulsing = true;
        }
    } else {
        if (s.pulsing && s.pulseTimer) {
            s.pulseTimer->stop();
            s.pulsing = false;
        }
        s.write(1.0F);
    }
}

void RgbLed::State::write(float level)
{
    const auto counts = dimmed(rgbCounts(color, channelGains, settings), level);
    for (std::size_t i = 0; i < channels.size(); ++i) {
        channels.at(i).setCounts(counts.at(i));
    }
}

void RgbLed::State::onPulse(void* arg)
{
    auto* self = static_cast<State*>(arg);
    const std::scoped_lock lock{self->mutex};
    if (self->effect != io::LedEffect::Pulse) {
        return; // stopped meanwhile
    }
    const auto elapsed = std::chrono::milliseconds{(esp_timer_get_time() - self->pulseStartUs) / 1000};
    self->write(pulseLevel(elapsed));
}

} // namespace banana::drivers
