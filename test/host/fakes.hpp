// Fakes for the hardware interfaces of the control layer.
#pragma once

#include <vector>

#include "banana/io/Outputs.hpp"
#include "banana/io/TemperatureSensor.hpp"

namespace fakes {

using namespace banana;

class Sensor final : public io::ITemperatureSensor {
public:
    float celsius = 20.0F;
    bool ok = true; ///< readCelsius() succeeds
    bool isHealthy = true;
    bool filterActive = false;
    int reads = 0;

    Result<float> readCelsius() override
    {
        ++reads;
        return ok ? Result<float>{celsius} : fail(ESP_ERR_TIMEOUT);
    }
    bool healthy() override { return isHealthy; }
    void setFilterActive(bool active) override { filterActive = active; }
};

class Pwm final : public io::IPwmOutput {
public:
    std::vector<float> writes;
    void write(float counts) override { writes.push_back(counts); }
    [[nodiscard]] float last() const { return writes.empty() ? -1.0F : writes.back(); }
};

class Led final : public io::IStatusLed {
public:
    io::LedColor color = io::LedColor::White;
    bool gains = false;
    int shows = 0;
    void show(io::LedColor c, bool g) override
    {
        color = c;
        gains = g;
        ++shows;
    }
};

class Network final : public io::INetworkStatus {
public:
    bool disconnected = false;
    [[nodiscard]] bool stationDisconnected() const override { return disconnected; }
};

} // namespace fakes
