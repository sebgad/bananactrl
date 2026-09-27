#pragma once

#include <chrono>
#include <cstdint>
#include <string>

/// Runtime configuration (`/fs/params.json`). Default member initialisers are the factory settings.
namespace banana::config {

using Seconds = std::chrono::duration<float>;

struct WifiSettings {
    /// Empty = use the factory credentials from Kconfig.
    std::string ssid;
    std::string password;

    friend bool operator==(const WifiSettings&, const WifiSettings&) = default;
};

/// Brewing feed-forward: decays from `start` towards `end` with `tau`,
/// plus `gain` counts per Kelvin of remaining control deviation.
struct BrewFeedForwardSettings {
    float start = 255.0F;
    float end = 10.0F;
    Seconds tau{14.0F};
    float gain = 35.0F;

    friend constexpr bool operator==(const BrewFeedForwardSettings&,
                                     const BrewFeedForwardSettings&) = default;
};

struct PidSettings {
    float target = 85.0F; ///< °C

    /// true: `intFactor` is the reset time Tn [s] and `difFactor` the lead time Tv [s];
    /// false: both are absolute gains Ki, Kd.
    bool timeFactor = true;
    bool propActive = true;
    float propFactor = 10.0F;
    bool intActive = true;
    float intFactor = 350.0F;
    bool difActive = false;
    float difFactor = 0.0F;
    Seconds difFilterTime{5.0F};

    bool lowThresholdActive = false;
    float lowThreshold = 0.0F; ///< °C; below -> full power
    bool highThresholdActive = false;
    float highThreshold = 0.0F; ///< °C; above -> heater off

    float lowLimit = 0.0F;    ///< SSR duty counts
    float highLimit = 255.0F; ///< SSR duty counts

    BrewFeedForwardSettings brew;

    friend constexpr bool operator==(const PidSettings&, const PidSettings&) = default;
};

struct SsrSettings {
    std::uint32_t frequencyHz = 15;
    std::uint32_t resolutionBits = 8;

    friend constexpr bool operator==(const SsrSettings&, const SsrSettings&) = default;
};

struct RgbGains {
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;

    friend constexpr bool operator==(const RgbGains&, const RgbGains&) = default;
};

/// Brightness factor per status colour.
struct ColorGains {
    float red = 1.0F;
    float green = 1.0F;
    float blue = 1.0F;
    float orange = 1.0F;
    float purple = 1.0F;
    float white = 1.0F;

    friend constexpr bool operator==(const ColorGains&, const ColorGains&) = default;
};

struct LedSettings {
    std::uint32_t frequencyHz = 500;
    std::uint32_t resolutionBits = 8;
    RgbGains channelGains;
    ColorGains colorGains;

    friend constexpr bool operator==(const LedSettings&, const LedSettings&) = default;
};

struct SignalSettings {
    bool filterActive = true;

    friend constexpr bool operator==(const SignalSettings&, const SignalSettings&) = default;
};

struct SystemSettings {
    std::chrono::seconds timeToStandby{3600};

    friend constexpr bool operator==(const SystemSettings&, const SystemSettings&) = default;
};

/// Optional MQTT connection to Home Assistant (banana_mqtt). Applied at boot.
struct MqttSettings {
    bool enabled = false;
    std::string host; ///< broker hostname or IP, e.g. the Home Assistant host running Mosquitto
    std::uint32_t port = 1883;
    std::string user;
    std::string password;

    friend bool operator==(const MqttSettings&, const MqttSettings&) = default;
};

struct Config {
    WifiSettings wifi;
    PidSettings pid;
    SsrSettings ssr;
    LedSettings led;
    SignalSettings signal;
    SystemSettings system;
    MqttSettings mqtt;

    friend bool operator==(const Config&, const Config&) = default;
};

} // namespace banana::config
