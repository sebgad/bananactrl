#pragma once

#include <chrono>
#include <cstdint>

#include "banana/config/Config.hpp"
#include "banana/control/BrewDetector.hpp"
#include "banana/control/BrewFeedForward.hpp"
#include "banana/control/Diagnostics.hpp"
#include "banana/control/PidController.hpp"
#include "banana/io/Outputs.hpp"
#include "banana/io/TemperatureSensor.hpp"

namespace banana::control {

/// Values for the web UI and the measurement file (fTime, fTemp, fTarPwm, ... of the Arduino firmware).
struct ProcessSnapshot {
    float seconds = 0.0F; ///< since start, at the last measurement
    float celsius = 0.0F;
    float heaterCounts = 0.0F;  ///< SSR duty in counts (0..255 by default)
    float heaterPercent = 0.0F; ///< heaterCounts / HighLimitManipulation * 100 (data.csv, web UI)
    float target = 0.0F;
    float pidIntegrator = 0.0F; ///< /lastvalues.json
    float pidErrorDiff = 0.0F;
    bool brewing = false;
    bool standby = false;
    Faults faults;
};

/// The Arduino loop() without RTOS: the caller (HeaterTask) turns interrupts and timers into these calls.
///
/// - onSample(): every ADC conversion (8 SPS); every 3rd one also runs the heater control.
/// - onTick(): every 450 ms; every 3rd one updates the LED, every 2nd one runs the diagnosis.
/// - onPumpEdge() / pollBrew(): brew detection with 200 ms debounce.
class ControlLoop {
public:
    using Millis = std::chrono::milliseconds;
    /// No conversion for this long (8 are expected) counts as a measurement fault.
    static constexpr Millis kSampleTimeout{1000};

    ControlLoop(io::ITemperatureSensor& sensor, io::IPwmOutput& heater, io::IStatusLed& led,
                const io::INetworkStatus& network, const config::Config& config, Millis start);

    /// configPID() + configLED() part that is not hardware; resets the PID (integrator belongs to the old
    /// coefficients). Takes effect for the next step.
    void applyConfig(const config::Config& config);

    void onPumpEdge(Millis now) { brew_.edge(now); }
    /// Returns true if the brewing state changed.
    bool pollBrew(Millis now, bool pumpActive) { return brew_.poll(now, pumpActive); }
    [[nodiscard]] const BrewDetector& brewDetector() const { return brew_; }

    void onSample(Millis now);
    void onTick(Millis now);

    [[nodiscard]] ProcessSnapshot snapshot() const
    {
        ProcessSnapshot snapshot = snapshot_;
        snapshot.brewing = brew_.brewing();
        snapshot.standby = standby_;
        snapshot.faults = faults_;
        snapshot.heaterPercent =
            pidSettings_.highLimit != 0.0F ? snapshot.heaterCounts / pidSettings_.highLimit * 100.0F : 0.0F;
        snapshot.pidIntegrator = pid_.integrator();
        snapshot.pidErrorDiff = pid_.errorDiff();
        return snapshot;
    }
    [[nodiscard]] Faults faults() const { return faults_; }
    [[nodiscard]] bool standby() const { return standby_; }

private:
    void controlHeating(Millis now);
    void checkStandby(Millis now);
    void switchOffIfNotAllowed();

    io::ITemperatureSensor* sensor_;
    io::IPwmOutput* heater_;
    io::IStatusLed* led_;
    const io::INetworkStatus* network_;

    PidController pid_;
    BrewFeedForward feedForward_;
    BrewDetector brew_;
    config::PidSettings pidSettings_;
    std::chrono::seconds timeToStandby_;

    Millis start_;
    Millis lastPid_; ///< time base of the PID (only advanced when it computes)
    Millis lastFeedForward_{0};
    Millis lastSample_;
    bool brewingPrev_ = false;
    std::uint32_t samples_ = 0;
    std::uint32_t ticks_ = 0;

    float celsius_ = 0.0F;
    float heaterCounts_ = 0.0F;
    bool standby_ = false;
    Faults faults_;
    ProcessSnapshot snapshot_;
};

} // namespace banana::control
