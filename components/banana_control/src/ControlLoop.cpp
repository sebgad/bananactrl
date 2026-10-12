#include "banana/control/ControlLoop.hpp"

#include "banana/control/ConfigMapping.hpp"
#include "banana/control/StatusIndicator.hpp"

namespace banana::control {

ControlLoop::ControlLoop(io::ITemperatureSensor& sensor, io::IPwmOutput& heater, io::IStatusLed& led,
                         const io::INetworkStatus& network, const config::Config& config, Millis start)
    : sensor_(&sensor), heater_(&heater), led_(&led), network_(&network), pid_(toPidSettings(config.pid)),
      feedForward_(toBrewFeedForwardSettings(config.pid)), steam_(config.steam), pidSettings_(config.pid),
      steamSettings_(config.steam), timeToStandby_(config.system.timeToStandby), start_(start),
      lastPid_(start), lastSample_(start)
{
    sensor_->setFilterActive(config.signal.filterActive);
}

PidController::Settings ControlLoop::activePidSettings() const
{
    return steamMode_ ? toSteamPidSettings(pidSettings_, steamSettings_) : toPidSettings(pidSettings_);
}

void ControlLoop::resetPid()
{
    if (steamMode_) {
        pid_.resetTo(steamSettings_.pidBaseOutput);
    } else {
        pid_.reset();
    }
}

void ControlLoop::applyConfig(const config::Config& config)
{
    const PidController::Settings previous = pid_.settings();
    pidSettings_ = config.pid;
    steamSettings_ = config.steam;
    const PidController::Settings next = activePidSettings();
    pid_.configure(next);
    feedForward_.configure(toBrewFeedForwardSettings(config.pid));
    steam_.configure(config.steam);
    sensor_->setFilterActive(config.signal.filterActive);
    timeToStandby_ = config.system.timeToStandby;
    // The integrator content belongs to the gains, active terms and output limits: start clean only if one of
    // them changed. The Arduino firmware reset on every settings update, so each target or standby change
    // (now easy from Home Assistant) restarted the slow integral approach.
    if (next.gains != previous.gains || next.terms != previous.terms || next.limits != previous.limits) {
        resetPid();
    }
}

bool ControlLoop::setSteamMode(bool on, Millis now)
{
    if (on && standby_) {
        return false;
    }
    steamModeStart_ = now;
    snapshot_.steamRemaining = on ? std::chrono::duration<float>(steamSettings_.pidTimeout).count() : 0.0F;
    if (on == steamMode_) {
        return true;
    }
    steamMode_ = on;
    // Either way the boiler can be above `enter` for a while (hot after PID steaming, or the hardware switch
    // was on): the temperature detection waits until it is below `exit`.
    steam_.reset();
    steamDetectionHeld_ = true;
    // The integrator belongs to the other target
    pid_.configure(activePidSettings());
    pidStale_ = true;
    return true;
}

void ControlLoop::checkSteamTimeout(Millis now)
{
    if (!steamMode_) {
        snapshot_.steamRemaining = 0.0F;
        return;
    }
    const auto elapsed = now - steamModeStart_;
    if (standby_ || elapsed >= steamSettings_.pidTimeout) {
        setSteamMode(false, now);
        return;
    }
    snapshot_.steamRemaining = std::chrono::duration<float>(steamSettings_.pidTimeout - elapsed).count();
}

void ControlLoop::onSample(Millis now)
{
    lastSample_ = now;

    // MEASURE
    if (auto celsius = sensor_->readCelsius()) {
        celsius_ = *celsius;
    }
    snapshot_.seconds = static_cast<float>((now - start_).count()) / 1000.0F;
    snapshot_.celsius = celsius_;
    if (steamDetectionHeld_ && !steamMode_ && celsius_ < steamSettings_.exit) {
        steamDetectionHeld_ = false;
    }
    if (!steamDetectionHeld_) {
        steam_.update(celsius_, pidSettings_.target);
    }

    // PID_CTRL on every 3rd conversion
    if (samples_++ % 3 == 0) {
        controlHeating(now);
    }
    checkStandby(now);
}

void ControlLoop::onTick(Millis now)
{
    const std::uint32_t tick = ticks_++;

    // LED_CTRL on every 3rd tick
    if (tick % 3 == 0) {
        const LedCommand led = indicate(state());
        led_->show(led.color, led.channelGains, led.effect);
    }

    checkStandby(now);
    checkSteamTimeout(now);

    // DIAG on every 2nd tick
    if (tick % 2 == 0) {
        // Samples stopping (dead ADC, broken ALERT wire) counts like an ADC reset.
        const bool sampling = now - lastSample_ <= kSampleTimeout;
        faults_ = diagnose({.celsius = celsius_,
                            .sensorHealthy = sampling && sensor_->healthy(),
                            .stationDisconnected = network_->stationDisconnected()});
    }
    switchOffIfNotAllowed();
}

void ControlLoop::switchOffIfNotAllowed()
{
    // Not deferred to the next PID step: without samples there is no next PID step
    // (the Arduino firmware kept the last duty on the SSR in that case).
    if ((standby_ || steam_.steaming() || blocksHeating(faults_)) && heaterCounts_ != 0.0F) {
        heaterCounts_ = 0.0F;
        heater_->write(0.0F);
        snapshot_.heaterCounts = 0.0F;
    }
}

void ControlLoop::controlHeating(Millis now)
{
    const bool brewing = brew_.brewing();
    const bool steaming = steam_.steaming();

    if (steaming) {
        // The steam switch heats past the SSR up to the bimetal switch. Keep the SSR off (it would heat while
        // the bimetal switch is open) and the PID frozen (above the target it only winds its integrator
        // down).
        heaterCounts_ = 0.0F;
        pidStale_ = true;
    } else if (!standby_ && !blocksHeating(faults_)) {
        if (brewing) {
            // Open loop while brewing: see BrewFeedForward
            if (!brewingPrev_) {
                feedForward_.begin();
                lastFeedForward_ = now;
            }
            heaterCounts_ = feedForward_.compute(target(), celsius_, now - lastFeedForward_);
            lastFeedForward_ = now;
        } else {
            if (brewingPrev_ || pidStale_) {
                // Brewing or steam mode ended, or the software steam switch changed the target: start clean
                resetPid();
                lastPid_ = now;
                pidStale_ = false;
            }
            heaterCounts_ = pid_.compute(celsius_, now - lastPid_);
            lastPid_ = now;
        }
    } else {
        heaterCounts_ = 0.0F;
    }
    brewingPrev_ = brewing && !steaming; // brewing that outlasts steam mode starts the feed-forward fresh
    heater_->write(heaterCounts_);

    snapshot_.heaterCounts = heaterCounts_;
}

void ControlLoop::checkStandby(Millis now)
{
    // Measured from start, not from the last use (as in the Arduino firmware); lasts until reboot.
    if (!standby_ && now - start_ >= timeToStandby_) {
        standby_ = true;
    }
}

} // namespace banana::control
