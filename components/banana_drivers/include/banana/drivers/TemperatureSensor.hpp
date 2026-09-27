#pragma once

#include "banana/core/Result.hpp"

namespace banana::drivers {

/// Hardware boundary for the control layer (fakes in host tests).
class ITemperatureSensor {
public:
    ITemperatureSensor() = default;
    ITemperatureSensor(const ITemperatureSensor&) = default;
    ITemperatureSensor& operator=(const ITemperatureSensor&) = default;
    ITemperatureSensor(ITemperatureSensor&&) = default;
    ITemperatureSensor& operator=(ITemperatureSensor&&) = default;
    virtual ~ITemperatureSensor() = default;

    /// Reads the latest conversion and returns the (filtered) temperature in °C.
    [[nodiscard]] virtual Result<float> readCelsius() = 0;
};

} // namespace banana::drivers
