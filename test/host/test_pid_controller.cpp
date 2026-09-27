// PidController vs. the original Arduino PidCtrl on a recorded temperature trace.

#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "banana/config/Config.hpp"
#include "banana/control/ConfigMapping.hpp"
#include "banana/control/PidController.hpp"

#include "Arduino.h"
#include "PidCtrl.h"
#include <gtest/gtest.h>

namespace {

using banana::control::Gains;
using banana::control::PidController;
using std::chrono::milliseconds;

struct Sample {
    unsigned long timeMs;
    float celsius;
};

/// Time,Temperature,... rows of a measurement file (two header lines + column names).
std::vector<Sample> loadTrace(const std::string& name)
{
    std::ifstream file(std::string{BANANA_TEST_DATA_DIR} + "/" + name);
    std::vector<Sample> samples;
    std::string line;
    for (int i = 0; i < 3 && std::getline(file, line); ++i) {
    }
    while (std::getline(file, line)) {
        std::istringstream row(line);
        std::string time;
        std::string temp;
        if (std::getline(row, time, ',') && std::getline(row, temp, ',')) {
            samples.push_back(
                {static_cast<unsigned long>(std::lround(std::stod(time) * 1000.0)), std::stof(temp)});
        }
    }
    return samples;
}

/// Configures the Arduino controller exactly like configPID() did.
void configureReference(PidCtrl& pid, const banana::config::PidSettings& cfg)
{
    pid.addOutputLimits(cfg.lowLimit, cfg.highLimit);
    pid.changeTargetValue(cfg.target);
    pid.changePidCoeffs(cfg.propFactor, cfg.intFactor, cfg.difFactor, cfg.timeFactor);
    pid.setDiffFilterTime(cfg.difFilterTime.count());
    cfg.lowThresholdActive ? pid.setOnThres(cfg.lowThreshold) : pid.deactivateOnThres();
    cfg.highThresholdActive ? pid.setOffThres(cfg.highThreshold) : pid.deactivateOffThres();
    pid.activate(cfg.propActive, cfg.intActive, cfg.difActive);
}

struct Scenario {
    const char* name;
    banana::config::PidSettings settings;
    /// Sample indices at which both controllers are reset (end of a brew).
    std::vector<std::size_t> resets;
};

class PidMatchesArduino : public testing::TestWithParam<Scenario> {};

TEST_P(PidMatchesArduino, SameOutputOnRecordedTrace)
{
    const Scenario& scenario = GetParam();
    const auto trace = loadTrace("rancilio_silvia_stock_measurement.csv");
    ASSERT_GT(trace.size(), 6000U);

    arduino_shim::nowMillis = trace.front().timeMs;
    PidCtrl reference;
    reference.begin();
    configureReference(reference, scenario.settings);

    PidController pid{banana::control::toPidSettings(scenario.settings)};
    unsigned long lastMs = trace.front().timeMs;

    for (std::size_t i = 0; i < trace.size(); ++i) {
        const Sample& sample = trace[i];
        arduino_shim::nowMillis = sample.timeMs;
        if (std::ranges::find(scenario.resets, i) != scenario.resets.end()) {
            // controlHeating() calls reset() and compute() in the same step
            reference.reset();
            pid.reset();
            lastMs = sample.timeMs; // PidCtrl::reset() restarts its time base
        }

        float expected = 0.0F;
        reference.compute(sample.celsius, expected);
        const float actual = pid.compute(sample.celsius, milliseconds{sample.timeMs - lastMs});
        lastMs = sample.timeMs;

        // Not bit-exact: PidCtrl divides the elapsed time as double.
        ASSERT_NEAR(actual, expected, 1e-3F) << "sample " << i << " at " << sample.celsius << " C";
        ASSERT_NEAR(pid.integrator(), reference.getErrorIntegrator(), 1e-3F) << "sample " << i;
        ASSERT_NEAR(pid.errorDiff(), reference.getErrorDiff(), 1e-5F) << "sample " << i;
    }
}

banana::config::PidSettings withDerivativeAndThresholds()
{
    banana::config::PidSettings s;
    s.propFactor = 12.0F;
    s.intFactor = 200.0F;
    s.difActive = true;
    s.difFactor = 25.0F;
    s.difFilterTime = banana::config::Seconds{5.0F};
    s.lowThresholdActive = true;
    s.lowThreshold = 60.0F;
    s.highThresholdActive = true;
    s.highThreshold = 88.0F;
    return s;
}

banana::config::PidSettings absoluteGainsUnfiltered()
{
    banana::config::PidSettings s;
    s.timeFactor = false;
    s.propFactor = 8.0F;
    s.intFactor = 0.05F;
    s.difActive = true;
    s.difFactor = 40.0F;
    s.difFilterTime = banana::config::Seconds{0.0F};
    s.lowLimit = 10.0F;
    s.highLimit = 200.0F;
    return s;
}

banana::config::PidSettings proportionalOnly()
{
    banana::config::PidSettings s;
    s.intActive = false;
    s.target = 70.0F;
    return s;
}

INSTANTIATE_TEST_SUITE_P(
    Trace, PidMatchesArduino,
    testing::Values(Scenario{"FactoryDefaults", {}, {}},
                    Scenario{"DerivativeAndThresholds", withDerivativeAndThresholds(), {1500, 4000}},
                    Scenario{"AbsoluteGainsUnfiltered", absoluteGainsUnfiltered(), {2500}},
                    Scenario{"ProportionalOnly", proportionalOnly(), {}}),
    [](const testing::TestParamInfo<Scenario>& info) { return std::string{info.param.name}; });

PidController::Settings simpleSettings()
{
    return {.target = 85.0F,
            .gains = {.kp = 10.0F, .ki = 0.1F, .kd = 5.0F},
            .terms = {.proportional = true, .integral = true, .derivative = true},
            .limits = {.lower = 0.0F, .upper = 255.0F},
            .thresholds = {},
            .derivativeFilterTime = PidController::Seconds{0.0F}};
}

TEST(PidController, GainsFromTimeConstants)
{
    EXPECT_EQ(Gains::fromTimeConstants(10.0F, 350.0F, 2.0F),
              (Gains{.kp = 10.0F, .ki = 10.0F / 350.0F, .kd = 20.0F}));
    EXPECT_EQ(Gains::fromTimeConstants(10.0F, 0.0F, 0.0F).ki, 0.0F);
}

TEST(PidController, NoDerivativeKickAfterReset)
{
    PidController pid{simpleSettings()};
    (void)pid.compute(20.0F, milliseconds{450});
    pid.reset();
    // Zero elapsed time right after reset(): neither NaN nor a derivative contribution.
    const float out = pid.compute(84.0F, milliseconds{0});
    EXPECT_FALSE(std::isnan(out));
    EXPECT_FLOAT_EQ(out, 10.0F * 1.0F);
    EXPECT_FLOAT_EQ(pid.errorDiff(), 0.0F);
}

TEST(PidController, IntegratorDoesNotWindUpAtUpperLimit)
{
    PidController pid{simpleSettings()};
    for (int i = 0; i < 100; ++i) {
        EXPECT_FLOAT_EQ(pid.compute(20.0F, milliseconds{450}), 255.0F);
    }
    EXPECT_FLOAT_EQ(pid.integrator(), 0.0F);
}

TEST(PidController, IntegratorNeverNegative)
{
    PidController pid{simpleSettings()};
    for (int i = 0; i < 100; ++i) {
        (void)pid.compute(95.0F, milliseconds{450});
    }
    EXPECT_FLOAT_EQ(pid.integrator(), 0.0F);
}

TEST(PidController, ThresholdsOverrideOutput)
{
    auto settings = simpleSettings();
    settings.thresholds = {.on = 60.0F, .off = 90.0F};
    PidController pid{settings};
    EXPECT_FLOAT_EQ(pid.compute(59.0F, milliseconds{450}), 255.0F);
    EXPECT_FLOAT_EQ(pid.compute(91.0F, milliseconds{450}), 0.0F);
}

TEST(PidController, ConfigureKeepsState)
{
    PidController pid{simpleSettings()};
    (void)pid.compute(80.0F, milliseconds{1000});
    const float integrator = pid.integrator();
    auto settings = simpleSettings();
    settings.target = 90.0F;
    pid.configure(settings);
    EXPECT_FLOAT_EQ(pid.integrator(), integrator);
    EXPECT_FLOAT_EQ(pid.target(), 90.0F);
}

} // namespace
