// ControlLoop with fake hardware, driven like HeaterTask: sample every 125 ms (8 SPS),
// tick every 450 ms, pump level polled on every wake-up.

#include <algorithm>
#include <chrono>

#include "banana/control/ConfigMapping.hpp"
#include "banana/control/ControlLoop.hpp"

#include "fakes.hpp"
#include <gtest/gtest.h>

namespace {

using namespace std::chrono_literals;
using banana::control::ControlLoop;
using banana::control::Fault;
using banana::io::LedColor;
using Millis = ControlLoop::Millis;

struct Bench {
    explicit Bench(banana::config::Config c = {}) : config(c) {}

    fakes::Sensor sensor;
    fakes::Pwm heater;
    fakes::Led led;
    fakes::Network network;
    banana::config::Config config;
    ControlLoop loop{sensor, heater, led, network, config, Millis{0}};

    Millis now{0};
    Millis nextSample{125};
    Millis nextTick{450};
    bool pump = false;
    bool sampling = true; ///< false: the ADC stopped delivering conversions

    void run(Millis duration)
    {
        const Millis end = now + duration;
        for (;;) {
            const Millis next = std::min(nextSample, nextTick);
            if (next > end) {
                break;
            }
            now = next;
            loop.pollBrew(now, pump);
            if (now == nextSample) {
                if (sampling) {
                    loop.onSample(now);
                }
                nextSample += 125ms;
            }
            if (now == nextTick) {
                loop.onTick(now);
                nextTick += 450ms;
            }
        }
        now = end;
    }

    void setPump(bool active)
    {
        pump = active;
        loop.onPumpEdge(now);
    }
};

TEST(ControlLoop, PidOnEveryThirdSampleLikeStandalonePid)
{
    Bench bench;
    bench.sensor.celsius = 70.0F;
    banana::control::PidController reference{banana::control::toPidSettings(bench.config.pid)};

    bench.run(3s); // samples at 125, 250, ... 3000 -> 24 samples, PID on 0, 3, 6, ...
    ASSERT_EQ(bench.sensor.reads, 24);
    ASSERT_EQ(bench.heater.writes.size(), 8U);

    Millis last{0};
    for (std::size_t i = 0; i < bench.heater.writes.size(); ++i) {
        const Millis at{125 + static_cast<long>(i) * 375};
        EXPECT_FLOAT_EQ(bench.heater.writes[i], reference.compute(70.0F, at - last)) << "PID step " << i;
        last = at;
    }
}

TEST(ControlLoop, LedEveryThirdTickFollowsTemperature)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(450ms); // tick 0: LED
    EXPECT_EQ(bench.led.shows, 1);
    EXPECT_EQ(bench.led.color, LedColor::Orange);
    EXPECT_TRUE(bench.led.gains);

    bench.run(900ms); // ticks 1, 2: no LED
    EXPECT_EQ(bench.led.shows, 1);

    bench.sensor.celsius = 85.9F;
    bench.run(450ms); // tick 3
    EXPECT_EQ(bench.led.shows, 2);
    EXPECT_EQ(bench.led.color, LedColor::Green);

    bench.sensor.celsius = 86.5F;
    bench.run(1350ms);
    EXPECT_EQ(bench.led.color, LedColor::Blue);
}

TEST(ControlLoop, TemperatureOutOfRangeSwitchesHeaterOffUntilItRecovers)
{
    Bench bench;
    bench.sensor.celsius = 5.0F;
    bench.run(450ms); // tick 0: LED (no fault yet), then DIAG
    EXPECT_TRUE(bench.loop.faults().only(Fault::TempOutOfRange));

    bench.run(1s);
    EXPECT_EQ(bench.heater.last(), 0.0F);
    bench.run(1s);
    EXPECT_EQ(bench.led.color, LedColor::Purple);
    EXPECT_FALSE(bench.led.gains);

    bench.sensor.celsius = 80.0F;
    bench.run(2s); // next DIAG (every 0.9 s) clears the fault, the next PID step heats again
    EXPECT_TRUE(bench.loop.faults().none());
    EXPECT_GT(bench.heater.last(), 0.0F);
}

// Bench finding: with the ADC gone there are no samples and hence no PID steps; the heater
// must still be switched off (the Arduino firmware kept the last duty).
TEST(ControlLoop, HeaterOffImmediatelyWhenSamplesStop)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(5s);
    ASSERT_GT(bench.heater.last(), 0.0F);

    bench.sampling = false;
    bench.run(2s); // > kSampleTimeout, then the next DIAG
    EXPECT_TRUE(bench.loop.faults().only(Fault::MeasDeviceReset));
    EXPECT_EQ(bench.heater.last(), 0.0F);
    EXPECT_EQ(bench.loop.snapshot().heaterCounts, 0.0F);

    bench.sampling = true;
    bench.run(2s);
    EXPECT_TRUE(bench.loop.faults().none());
    EXPECT_GT(bench.heater.last(), 0.0F);
}

TEST(ControlLoop, UnhealthySensorSwitchesOffWithoutWaitingForPid)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(5s);
    bench.sampling = false;
    bench.sensor.isHealthy = false;
    bench.run(900ms); // DIAG runs on every 2nd tick
    EXPECT_EQ(bench.heater.last(), 0.0F);
}

TEST(ControlLoop, AdcResetIsAFault)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.sensor.isHealthy = false;
    bench.run(1s);
    EXPECT_TRUE(bench.loop.faults().only(Fault::MeasDeviceReset));
    EXPECT_EQ(bench.heater.last(), 0.0F);
}

// Changed from the Arduino firmware: losing Wi-Fi only turns the LED purple, heating continues.
TEST(ControlLoop, WifiDisconnectOnlyShowsOnLed)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.network.disconnected = true;
    bench.run(2s);
    EXPECT_TRUE(bench.loop.faults().only(Fault::WifiDisconnect));
    EXPECT_GT(bench.heater.last(), 0.0F);
    EXPECT_EQ(bench.led.color, LedColor::Purple);

    bench.network.disconnected = false;
    bench.run(2s);
    EXPECT_TRUE(bench.loop.faults().none());
    EXPECT_EQ(bench.led.color, LedColor::Orange);
}

TEST(ControlLoop, WifiAndSensorFaultStillSwitchOff)
{
    Bench bench;
    bench.sensor.celsius = 5.0F;
    bench.network.disconnected = true;
    bench.run(2s);
    EXPECT_EQ(bench.heater.last(), 0.0F);
}

TEST(ControlLoop, FailedReadKeepsLastTemperature)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(1s);
    bench.sensor.ok = false;
    bench.sensor.celsius = 20.0F;
    bench.run(1s);
    EXPECT_FLOAT_EQ(bench.loop.snapshot().celsius, 80.0F);
}

TEST(ControlLoop, BrewingUsesFeedForwardAndResetsPidAfterwards)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(5s);
    EXPECT_FALSE(bench.loop.snapshot().brewing);

    bench.setPump(true);
    bench.run(150ms); // inside the 200 ms debounce
    EXPECT_FALSE(bench.loop.snapshot().brewing);
    bench.run(450ms);
    EXPECT_TRUE(bench.loop.snapshot().brewing);
    // First brewing step: charged to 255 + 35 * (85 - 80), clamped
    EXPECT_FLOAT_EQ(bench.heater.last(), 255.0F);

    bench.sensor.celsius = 85.0F;
    bench.run(30s);
    EXPECT_GT(bench.heater.last(), 10.0F);
    EXPECT_LT(bench.heater.last(), 45.0F); // ~10 + 245 * e^(-30/14)

    bench.run(1350ms);
    EXPECT_EQ(bench.led.color, LedColor::Red);

    bench.sensor.celsius = 84.0F;
    bench.setPump(false);
    bench.run(600ms);
    EXPECT_FALSE(bench.loop.snapshot().brewing);
    // First PID step after brewing: reset and zero elapsed time -> only P: 10 * (85 - 84)
    const auto& writes = bench.heater.writes;
    const auto firstAfter =
        std::ranges::find_if(writes.rbegin(), writes.rend(), [](float w) { return w == 10.0F; });
    EXPECT_NE(firstAfter, writes.rend());
}

TEST(ControlLoop, SteamModeKeepsHeaterOffAndFreezesPid)
{
    using banana::control::MachineState;
    using banana::control::SteamState;
    using banana::io::LedEffect;
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(5s);
    EXPECT_GT(bench.heater.last(), 0.0F);
    const float integrator = bench.loop.snapshot().pidIntegrator;

    // The steam switch heats past the SSR: above 105 °C with the PID at 0
    bench.sensor.celsius = 110.0F;
    bench.run(1350ms);
    EXPECT_EQ(bench.loop.snapshot().steam, SteamState::HeatingUp);
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::SteamHeatingUp);
    EXPECT_FLOAT_EQ(bench.heater.last(), 0.0F);
    EXPECT_EQ(bench.led.color, LedColor::Magenta);
    EXPECT_EQ(bench.led.effect, LedEffect::Blink);

    bench.sensor.celsius = 120.0F;
    bench.run(60s);
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::SteamReady);
    EXPECT_EQ(bench.led.color, LedColor::Magenta);
    EXPECT_EQ(bench.led.effect, LedEffect::Steady);
    EXPECT_TRUE(std::ranges::all_of(bench.heater.writes.end() - 100, bench.heater.writes.end(),
                                    [](float w) { return w == 0.0F; }));
    EXPECT_FLOAT_EQ(bench.loop.snapshot().pidIntegrator, integrator); // frozen, not wound down

    // Below 115 °C: steam mode over, cooling down to the target
    bench.sensor.celsius = 114.0F;
    bench.run(1350ms);
    EXPECT_EQ(bench.loop.snapshot().steam, SteamState::Cooldown);
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::CoolingDown);
    EXPECT_EQ(bench.led.color, LedColor::Blue);
    EXPECT_EQ(bench.led.effect, LedEffect::Pulse);
}

TEST(ControlLoop, PidStartsCleanAfterSteamMode)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(30s); // integrator charged
    ASSERT_NE(bench.loop.snapshot().pidIntegrator, 0.0F);
    bench.sensor.celsius = 120.0F;
    bench.run(5s);
    ASSERT_TRUE(bench.loop.snapshot().steam == banana::control::SteamState::Ready);

    bench.sensor.celsius = 84.0F; // e.g. after a long cool-down
    bench.run(450ms);
    EXPECT_EQ(bench.loop.snapshot().steam, banana::control::SteamState::Off);
    // First PID step after steam mode: reset and zero elapsed time -> only P: 10 * (85 - 84)
    const auto& writes = bench.heater.writes;
    EXPECT_NE(std::ranges::find(writes.end() - 2, writes.end(), 10.0F), writes.end());
}

TEST(ControlLoop, NoSteamDetectionForHighTargets)
{
    banana::config::Config config;
    config.pid.target = 101.0F;
    Bench bench{config};
    EXPECT_FALSE(bench.loop.steamDetection());
    bench.sensor.celsius = 110.0F;
    bench.run(2s);
    EXPECT_EQ(bench.loop.snapshot().steam, banana::control::SteamState::Off);
    EXPECT_EQ(bench.loop.snapshot().state, banana::control::MachineState::CoolingDown);
}

TEST(ControlLoop, SteamAndReadyBandFromTheSettings)
{
    using banana::control::MachineState;
    Bench bench;
    bench.sensor.celsius = 110.0F;
    bench.run(450ms);
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::SteamHeatingUp);

    banana::config::Config config;
    config.steam.active = false;
    config.pid.readyBand = 30.0F;
    bench.loop.applyConfig(config);
    bench.run(450ms);
    EXPECT_FALSE(bench.loop.steamDetection());
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::Ready); // 110 °C is within 85 ± 30 K
    EXPECT_GT(bench.heater.writes.size(), 0U);

    config.steam = {.enter = 115.0F, .exit = 105.0F, .ready = 125.0F, .readyLeave = 120.0F};
    config.pid.readyBand = 1.0F;
    bench.loop.applyConfig(config);
    bench.run(450ms);
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::CoolingDown); // below the new enter threshold
    bench.sensor.celsius = 116.0F;
    bench.run(450ms);
    EXPECT_EQ(bench.loop.snapshot().state, MachineState::SteamHeatingUp);
}

TEST(ControlLoop, PumpGlitchInsideDebounceIsIgnored)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(1s);
    bench.setPump(true);
    bench.run(100ms);
    bench.setPump(false); // second edge inside the window: ignored, level decides at the end
    bench.run(500ms);
    EXPECT_FALSE(bench.loop.snapshot().brewing);
}

TEST(ControlLoop, StandbyAfterTimeToStandbySinceStart)
{
    banana::config::Config config;
    config.system.timeToStandby = 10s;
    Bench bench{config};
    bench.sensor.celsius = 80.0F;

    bench.run(9s);
    EXPECT_FALSE(bench.loop.standby());
    EXPECT_GT(bench.heater.last(), 0.0F);

    bench.run(2s);
    EXPECT_TRUE(bench.loop.standby());
    const int shows = bench.led.shows;
    bench.run(5s);
    EXPECT_EQ(bench.heater.last(), 0.0F);
    EXPECT_GT(bench.led.shows, shows); // LED keeps working
    bench.sensor.celsius = 20.0F;
    bench.run(5s);
    EXPECT_EQ(bench.heater.last(), 0.0F); // stays off until reboot
}

TEST(ControlLoop, ApplyConfigTakesEffect)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    EXPECT_TRUE(bench.sensor.filterActive);
    bench.run(2s);

    banana::config::Config config;
    config.pid.target = 90.0F;
    config.pid.intActive = false;
    config.signal.filterActive = false;
    bench.loop.applyConfig(config);
    EXPECT_FALSE(bench.sensor.filterActive);
    EXPECT_FLOAT_EQ(bench.loop.snapshot().target, 90.0F);

    bench.run(1s);
    EXPECT_FLOAT_EQ(bench.heater.last(), 10.0F * (90.0F - 80.0F)); // P only now
}

TEST(ControlLoop, SnapshotTimeSinceStart)
{
    Bench bench;
    bench.run(1s);
    EXPECT_FLOAT_EQ(bench.loop.snapshot().seconds, 1.0F);
}

TEST(ControlLoop, TargetOrStandbyChangeKeepsIntegrator)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(10s);
    const float integrator = bench.loop.snapshot().pidIntegrator;
    ASSERT_GT(integrator, 0.0F);

    banana::config::Config config = bench.config;
    config.pid.target = 86.0F;
    config.system.timeToStandby = std::chrono::seconds{7200};
    config.led.colorGains.red = 0.5F;
    config.mqtt.enabled = true;
    bench.loop.applyConfig(config);
    bench.run(1s);
    EXPECT_GT(bench.loop.snapshot().pidIntegrator, integrator); // kept and still integrating
}

TEST(ControlLoop, GainChangeResetsIntegrator)
{
    Bench bench;
    bench.sensor.celsius = 80.0F;
    bench.run(10s);
    const float integrator = bench.loop.snapshot().pidIntegrator;
    ASSERT_GT(integrator, 0.0F);

    banana::config::Config config = bench.config;
    config.pid.intFactor = 700.0F; // different Ki: the old integral means something else now
    bench.loop.applyConfig(config);
    bench.run(1s);
    EXPECT_LT(bench.loop.snapshot().pidIntegrator, integrator);
    EXPECT_LE(bench.loop.snapshot().pidIntegrator,
              5.0F * 2); // restarted from 0: at most 1 s x 5 K (x2 margin)
}

} // namespace
