#include "banana/control/ControlLoop.hpp"

#include "banana/control/ConfigMapping.hpp"
#include "banana/control/StatusIndicator.hpp"

namespace banana::control {

ControlLoop::ControlLoop(io::ITemperatureSensor& sensor, io::IPwmOutput& heater, io::IStatusLed& led,
                         const io::INetworkStatus& network, const config::Config& config, Millis start)
    : sensor_(&sensor), heater_(&heater), led_(&led), network_(&network), pid_(toPidSettings(config.pid)),
      feedForward_(toBrewFeedForwardSettings(config.pid)), pidSettings_(config.pid),
      timeToStandby_(config.system.timeToStandby), start_(start), lastPid_(start), lastSample_(start)
{
    sensor_->setFilterActive(config.signal.filterActive);
    snapshot_.target = pidSettings_.target;
}

void ControlLoop::applyConfig(const config::Config& config)
{
    pidSettings_ = config.pid;
    pid_.configure(toPidSettings(config.pid));
    feedForward_.configure(toBrewFeedForwardSettings(config.pid));
    sensor_->setFilterActive(config.signal.filterActive);
    timeToStandby_ = config.system.timeToStandby;
    pid_.reset();
    snapshot_.target = pidSettings_.target;
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
        const LedCommand led = indicate(faults_, brew_.brewing(), celsius_, pidSettings_.target);
        led_->show(led.color, led.channelGains);
    }

    checkStandby(now);

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
    if ((standby_ || blocksHeating(faults_)) && heaterCounts_ != 0.0F) {
        heaterCounts_ = 0.0F;
        heater_->write(0.0F);
        snapshot_.heaterCounts = 0.0F;
    }
}

void ControlLoop::controlHeating(Millis now)
{
    const bool brewing = brew_.brewing();

    if (!standby_ && !blocksHeating(faults_)) {
        if (brewing) {
            // Open loop while brewing: see BrewFeedForward
            if (!brewingPrev_) {
                feedForward_.begin();
                lastFeedForward_ = now;
            }
            heaterCounts_ = feedForward_.compute(pidSettings_.target, celsius_, now - lastFeedForward_);
            lastFeedForward_ = now;
        } else {
            if (brewingPrev_) {
                // Brewing ended: start the PID clean
                pid_.reset();
                lastPid_ = now;
            }
            heaterCounts_ = pid_.compute(celsius_, now - lastPid_);
            lastPid_ = now;
        }
    } else {
        heaterCounts_ = 0.0F;
    }
    brewingPrev_ = brewing;
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
