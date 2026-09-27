#include "banana/hal/Ledc.hpp"

#include <utility>

namespace banana::hal {
namespace {

constexpr ledc_mode_t kMode = LEDC_LOW_SPEED_MODE;

Result<void> applyTimer(const LedcTimer::Config& config)
{
    ledc_timer_config_t timerConfig{};
    timerConfig.speed_mode = kMode;
    timerConfig.duty_resolution = static_cast<ledc_timer_bit_t>(config.resolutionBits);
    timerConfig.timer_num = config.timer;
    timerConfig.freq_hz = config.frequencyHz;
    timerConfig.clk_cfg = LEDC_AUTO_CLK;
    return toResult(ledc_timer_config(&timerConfig));
}

} // namespace

Result<LedcTimer> LedcTimer::create(const Config& config)
{
    if (auto res = applyTimer(config); !res) {
        return fail(res.error());
    }
    return LedcTimer{config};
}

LedcTimer::LedcTimer(LedcTimer&& other) noexcept
    : config_(other.config_), owned_(std::exchange(other.owned_, false))
{
}

LedcTimer& LedcTimer::operator=(LedcTimer&& other) noexcept
{
    if (this != &other) {
        release();
        config_ = other.config_;
        owned_ = std::exchange(other.owned_, false);
    }
    return *this;
}

LedcTimer::~LedcTimer()
{
    release();
}

Result<void> LedcTimer::reconfigure(std::uint32_t frequencyHz, std::uint32_t resolutionBits)
{
    Config next = config_;
    next.frequencyHz = frequencyHz;
    next.resolutionBits = resolutionBits;
    if (auto res = applyTimer(next); !res) {
        return res;
    }
    config_ = next;
    return {};
}

void LedcTimer::release()
{
    if (owned_) {
        ledc_timer_pause(kMode, config_.timer);
        ledc_timer_config_t timerConfig{};
        timerConfig.speed_mode = kMode;
        timerConfig.timer_num = config_.timer;
        timerConfig.deconfigure = true;
        ledc_timer_config(&timerConfig);
        owned_ = false;
    }
}

Result<PwmChannel> PwmChannel::create(gpio_num_t pin, ledc_channel_t channel, const LedcTimer& timer)
{
    ledc_channel_config_t channelConfig{};
    channelConfig.gpio_num = pin;
    channelConfig.speed_mode = kMode;
    channelConfig.channel = channel;
    channelConfig.timer_sel = timer.id();
    channelConfig.duty = 0;
    channelConfig.hpoint = 0;
    if (auto res = toResult(ledc_channel_config(&channelConfig)); !res) {
        return fail(res.error());
    }
    return PwmChannel{channel};
}

PwmChannel::PwmChannel(PwmChannel&& other) noexcept
    : channel_(std::exchange(other.channel_, LEDC_CHANNEL_MAX)), counts_(other.counts_)
{
}

PwmChannel& PwmChannel::operator=(PwmChannel&& other) noexcept
{
    if (this != &other) {
        release();
        channel_ = std::exchange(other.channel_, LEDC_CHANNEL_MAX);
        counts_ = other.counts_;
    }
    return *this;
}

PwmChannel::~PwmChannel()
{
    release();
}

void PwmChannel::setCounts(std::uint32_t counts)
{
    counts_ = counts;
    ledc_set_duty(kMode, channel_, counts_);
    ledc_update_duty(kMode, channel_);
}

void PwmChannel::release()
{
    if (channel_ != LEDC_CHANNEL_MAX) {
        ledc_stop(kMode, channel_, 0);
        channel_ = LEDC_CHANNEL_MAX;
    }
}

} // namespace banana::hal
