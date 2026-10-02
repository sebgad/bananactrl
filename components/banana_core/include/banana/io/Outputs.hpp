#pragma once

#include <cstdint>

/// Hardware boundaries of the control layer (fakes in host tests).
namespace banana::io {

/// PWM output driven in duty counts of its resolution (0 .. 2^bits - 1), like ledcWrite().
class IPwmOutput {
public:
    IPwmOutput() = default;
    IPwmOutput(const IPwmOutput&) = default;
    IPwmOutput& operator=(const IPwmOutput&) = default;
    IPwmOutput(IPwmOutput&&) = default;
    IPwmOutput& operator=(IPwmOutput&&) = default;
    virtual ~IPwmOutput() = default;

    /// Fractions are truncated, out-of-range values clamped.
    virtual void write(float counts) = 0;
};

enum class LedColor : std::uint8_t {
    Red,
    Green,
    Blue,
    Orange,
    Purple,
    White,
    Magenta, ///< Telekom magenta (#E20074): steam mode
};

enum class LedEffect : std::uint8_t {
    Steady,
    Pulse, ///< fades in and out ("breathing")
    Blink, ///< on and off
};

class IStatusLed {
public:
    IStatusLed() = default;
    IStatusLed(const IStatusLed&) = default;
    IStatusLed& operator=(const IStatusLed&) = default;
    IStatusLed(IStatusLed&&) = default;
    IStatusLed& operator=(IStatusLed&&) = default;
    virtual ~IStatusLed() = default;

    /// `channelGains`: apply the per-channel gain factors (off for fault colours). Repeating the current
    /// command keeps a running effect in phase.
    virtual void show(LedColor color, bool channelGains, LedEffect effect) = 0;
};

class IDigitalInput {
public:
    IDigitalInput() = default;
    IDigitalInput(const IDigitalInput&) = default;
    IDigitalInput& operator=(const IDigitalInput&) = default;
    IDigitalInput(IDigitalInput&&) = default;
    IDigitalInput& operator=(IDigitalInput&&) = default;
    virtual ~IDigitalInput() = default;

    [[nodiscard]] virtual bool level() const = 0;
};

class INetworkStatus {
public:
    INetworkStatus() = default;
    INetworkStatus(const INetworkStatus&) = default;
    INetworkStatus& operator=(const INetworkStatus&) = default;
    INetworkStatus(INetworkStatus&&) = default;
    INetworkStatus& operator=(INetworkStatus&&) = default;
    virtual ~INetworkStatus() = default;

    /// True if the device is in station mode but not connected (SoftAP mode is not a fault).
    [[nodiscard]] virtual bool stationDisconnected() const = 0;
};

} // namespace banana::io
