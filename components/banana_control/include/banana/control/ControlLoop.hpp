#pragma once

#include <chrono>
#include <cstdint>

#include "banana/config/Config.hpp"
#include "banana/control/BrewDetector.hpp"
#include "banana/control/BrewFeedForward.hpp"
#include "banana/control/Diagnostics.hpp"
#include "banana/control/MachineState.hpp"
#include "banana/control/PidController.hpp"
#include "banana/control/SteamStateMachine.hpp"
#include "banana/io/Outputs.hpp"
#include "banana/io/TemperatureSensor.hpp"

namespace banana::control {

/// Values for the web UI and the measurement file (fTime, fTemp, fTarPwm, ... of the Arduino firmware).
struct ProcessSnapshot {
    float seconds = 0.0F; ///< since start, at the last measurement
    float celsius = 0.0F;
    float heaterCounts = 0.0F;  ///< SSR duty in counts (0..255 by default)
    float heaterPercent = 0.0F; ///< heaterCounts / HighLimitManipulation * 100 (data.csv, web UI)
    float target = 0.0F;        ///< the PID's target: the steam target while the software steam switch is on
    float pidIntegrator = 0.0F; ///< /lastvalues.json
    float pidErrorDiff = 0.0F;
    bool brewing = false;
    bool standby = false;
    Faults faults;
    SteamState steam = SteamState::Off;
    bool steamMode = false;      ///< software steam switch
    float steamRemaining = 0.0F; ///< s until the software steam switch turns itself off
    MachineState state = MachineState::HeatingUp;
};

/// The Arduino loop() without RTOS: the caller (HeaterTask) turns interrupts and timers into these calls.
///
/// - onSample(): every ADC conversion (8 SPS); every 3rd one also runs the heater control.
/// - onTick(): every 450 ms; every 3rd one updates the LED, every 2nd one runs the diagnosis.
/// - onPumpEdge() / pollBrew(): brew detection with 200 ms debounce.
/// Steam mode has two sources:
/// - Hardware steam switch: detected from the temperature on every sample (SteamStateMachine); while
///   steaming the SSR stays off and the PID frozen.
/// - Software steam switch (setSteamMode()): the PID heats to SteamSettings::pidTarget until it is switched
///   off, standby or SteamSettings::pidTimeout. The temperature detection is off meanwhile and stays off
///   afterwards until the boiler is below SteamSettings::exit again (it would take the PID's steam
///   temperatures for the hardware switch).
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

    /// Software steam switch. Switching on again restarts the timeout. Returns false (and stays off) in
    /// standby.
    bool setSteamMode(bool on, Millis now);
    [[nodiscard]] bool steamMode() const { return steamMode_; }
    /// The PID's target: SteamSettings::pidTarget while the software steam switch is on.
    [[nodiscard]] float target() const { return steamMode_ ? steamSettings_.pidTarget : pidSettings_.target; }

    [[nodiscard]] ProcessSnapshot snapshot() const
    {
        ProcessSnapshot snapshot = snapshot_;
        snapshot.brewing = brew_.brewing();
        snapshot.standby = standby_;
        snapshot.faults = faults_;
        snapshot.target = target();
        snapshot.steam = steamState();
        snapshot.steamMode = steamMode_;
        snapshot.state = state();
        snapshot.heaterPercent =
            pidSettings_.highLimit != 0.0F ? snapshot.heaterCounts / pidSettings_.highLimit * 100.0F : 0.0F;
        snapshot.pidIntegrator = pid_.integrator();
        snapshot.pidErrorDiff = pid_.errorDiff();
        return snapshot;
    }
    [[nodiscard]] Faults faults() const { return faults_; }
    [[nodiscard]] bool standby() const { return standby_; }
    /// False if the target is too high to tell steam from brewing temperatures (see SteamStateMachine).
    [[nodiscard]] bool steamDetection() const { return steam_.enabled(pidSettings_.target); }
    [[nodiscard]] MachineState state() const
    {
        return machineState(faults_, brew_.brewing(), steamState(), celsius_, target(),
                            pidSettings_.readyBand);
    }

private:
    /// The software steam switch reports "heating up" below its target - readyBand, "ready" from there.
    [[nodiscard]] SteamState steamState() const
    {
        if (!steamMode_) {
            return steam_.state();
        }
        return celsius_ < steamSettings_.pidTarget - pidSettings_.readyBand ? SteamState::HeatingUp
                                                                            : SteamState::Ready;
    }
    /// Brew or steam PID settings from pidSettings_ and steamSettings_.
    [[nodiscard]] PidController::Settings activePidSettings() const;
    /// Clean start; with the software steam switch the integral part starts at SteamSettings::pidBaseOutput.
    void resetPid();
    void checkSteamTimeout(Millis now);
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
    SteamStateMachine steam_;
    config::PidSettings pidSettings_;
    config::SteamSettings steamSettings_;
    std::chrono::seconds timeToStandby_;

    Millis start_;
    Millis lastPid_; ///< time base of the PID (only advanced when it computes)
    Millis lastFeedForward_{0};
    Millis lastSample_;
    bool brewingPrev_ = false;
    bool pidStale_ = false; ///< steam mode froze the PID or changed its target: reset it before the next step
    bool steamMode_ = false;
    Millis steamModeStart_{0};
    bool steamDetectionHeld_ = false; ///< no temperature steam detection until below SteamSettings::exit
    std::uint32_t samples_ = 0;
    std::uint32_t ticks_ = 0;

    float celsius_ = 0.0F;
    float heaterCounts_ = 0.0F;
    bool standby_ = false;
    Faults faults_;
    ProcessSnapshot snapshot_;
};

} // namespace banana::control
