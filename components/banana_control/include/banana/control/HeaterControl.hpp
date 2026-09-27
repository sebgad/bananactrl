#pragma once

#include "banana/config/Config.hpp"
#include "banana/control/ControlLoop.hpp"

namespace banana::control {

/// What other tasks (the web server) may do with the running heater control. Thread-safe.
class IHeaterControl {
public:
    IHeaterControl() = default;
    IHeaterControl(const IHeaterControl&) = default;
    IHeaterControl& operator=(const IHeaterControl&) = default;
    IHeaterControl(IHeaterControl&&) = default;
    IHeaterControl& operator=(IHeaterControl&&) = default;
    virtual ~IHeaterControl() = default;

    [[nodiscard]] virtual ProcessSnapshot snapshot() const = 0;
    /// Applied inside the heater task at its next wake-up.
    virtual void requestConfig(const config::Config& config) = 0;
};

} // namespace banana::control
