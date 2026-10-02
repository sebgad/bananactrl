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
    auto effectTimer = hal::PeriodicTimer::create("led_effect", &State::onEffectStep, state.get());
    if (!effectTimer) {
        return fail(effectTimer.error());
    }
    state->effectTimer.emplace(std::move(*effectTimer));
    return RgbLed{std::move(state)};
}

Result<void> RgbLed::configure(const config::LedSettings& settings)
{
    const std::scoped_lock lock{state_->mutex};
    if (auto res = state_->timer.reconfigure(settings.frequencyHz, settings.resolutionBits); !res) {
        return res;
    }
    state_->settings = settings;
    if (state_->shown && !state_->animating) {
        state_->write(1.0F); // new gains for a steady colour (an effect picks them up at its next step)
    }
    return {};
}

void RgbLed::show(io::LedColor color, bool channelGains, io::LedEffect effect)
{
    State& s = *state_;
    const std::scoped_lock lock{s.mutex};
    if (s.shown && color == s.color && channelGains == s.channelGains && effect == s.effect) {
        return; // repeated every 1.35 s by the heater task: keep a running effect in phase
    }
    s.color = color;
    s.channelGains = channelGains;
    s.effect = effect;
    s.shown = true;

    if (effect != io::LedEffect::Steady) {
        s.effectStartUs = esp_timer_get_time();
        s.write(1.0F);
        if (!s.animating && s.effectTimer && s.effectTimer->start(kEffectStep)) {
            s.animating = true;
        }
    } else {
        if (s.animating && s.effectTimer) {
            s.effectTimer->stop();
            s.animating = false;
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

void RgbLed::State::onEffectStep(void* arg)
{
    auto* self = static_cast<State*>(arg);
    const std::scoped_lock lock{self->mutex};
    if (self->effect == io::LedEffect::Steady) {
        return; // stopped meanwhile
    }
    const auto elapsed = std::chrono::milliseconds{(esp_timer_get_time() - self->effectStartUs) / 1000};
    self->write(effectLevel(self->effect, elapsed));
}

} // namespace banana::drivers
