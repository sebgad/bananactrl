#pragma once

#include <cstdint>

#include "driver/ledc.h"
#include "hal/gpio_types.h"

#include "banana/core/Result.hpp"

namespace banana::hal {

/// LEDC timer (low-speed mode): frequency and duty resolution shared by its channels.
class LedcTimer {
public:
    struct Config {
        ledc_timer_t timer = LEDC_TIMER_0;
        std::uint32_t frequencyHz = 1000;
        std::uint32_t resolutionBits = 8;
    };

    [[nodiscard]] static Result<LedcTimer> create(const Config& config);

    LedcTimer(const LedcTimer&) = delete;
    LedcTimer& operator=(const LedcTimer&) = delete;
    LedcTimer(LedcTimer&& other) noexcept;
    LedcTimer& operator=(LedcTimer&& other) noexcept;
    ~LedcTimer();

    /// New frequency/resolution at runtime; attached channels keep running (like ledcSetup()).
    [[nodiscard]] Result<void> reconfigure(std::uint32_t frequencyHz, std::uint32_t resolutionBits);

    [[nodiscard]] ledc_timer_t id() const { return config_.timer; }
    [[nodiscard]] std::uint32_t resolutionBits() const { return config_.resolutionBits; }
    [[nodiscard]] std::uint32_t maxCounts() const { return (1U << config_.resolutionBits) - 1U; }

private:
    explicit LedcTimer(const Config& config) : config_(config), owned_(true) {}
    void release();

    Config config_;
    bool owned_ = false;
};

/// LEDC channel on a GPIO, driven by a LedcTimer. Stops the output (low) in its destructor.
class PwmChannel {
public:
    [[nodiscard]] static Result<PwmChannel> create(gpio_num_t pin, ledc_channel_t channel,
                                                   const LedcTimer& timer);

    PwmChannel(const PwmChannel&) = delete;
    PwmChannel& operator=(const PwmChannel&) = delete;
    PwmChannel(PwmChannel&& other) noexcept;
    PwmChannel& operator=(PwmChannel&& other) noexcept;
    ~PwmChannel();

    /// Duty in counts of the timer resolution (like ledcWrite()); the owner clamps to 0 .. 2^bits.
    void setCounts(std::uint32_t counts);
    [[nodiscard]] std::uint32_t counts() const { return counts_; }

private:
    explicit PwmChannel(ledc_channel_t channel) : channel_(channel) {}
    void release();

    ledc_channel_t channel_ = LEDC_CHANNEL_MAX;
    std::uint32_t counts_ = 0;
};

} // namespace banana::hal
