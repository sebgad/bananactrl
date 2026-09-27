#pragma once

#include <cstdint>

#include "banana/core/Flags.hpp"

namespace banana::control {

enum class Fault : std::uint8_t {
    TempOutOfRange,  ///< below 10 °C: sensor or conversion broken (the lookup table returns 0 outside)
    MeasDeviceReset, ///< ADC lost its configuration or does not answer
    WifiDisconnect,  ///< station mode but not connected
};
using Faults = Flags<Fault>;

struct DiagnosticInputs {
    float celsius = 0.0F;
    bool sensorHealthy = true;
    bool stationDisconnected = false;
};

/// Faults that switch the heater off. A Wi-Fi fault only shows on the LED (purple): heating does not
/// depend on the network. (The Arduino firmware switched off on any fault.)
[[nodiscard]] constexpr bool blocksHeating(Faults faults)
{
    return faults.test(Fault::TempOutOfRange) || faults.test(Fault::MeasDeviceReset);
}

/// The DIAG block of the Arduino loop().
[[nodiscard]] constexpr Faults diagnose(const DiagnosticInputs& in)
{
    Faults faults;
    if (in.celsius < 10.0F) {
        faults.set(Fault::TempOutOfRange);
    }
    if (!in.sensorHealthy) {
        faults.set(Fault::MeasDeviceReset);
    }
    if (in.stationDisconnected) {
        faults.set(Fault::WifiDisconnect);
    }
    return faults;
}

} // namespace banana::control
