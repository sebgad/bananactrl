#pragma once

#include "banana/core/Result.hpp"

namespace banana::io {

/// Temperature measurement as seen by the control layer.
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
    /// False if the device lost its configuration (e.g. reset to single-shot mode) or does not answer.
    [[nodiscard]] virtual bool healthy() = 0;
    virtual void setFilterActive(bool active) = 0;
};

} // namespace banana::io
