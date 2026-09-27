#pragma once

#include "banana/drivers/Ads1115.hpp"
#include "banana/rtos/Task.hpp"

namespace banana {

/// Bench check for Phase 3 (replaced by HeaterController in Phase 4): reads the ADS1115 on every
/// conversion-ready interrupt and logs a summary once per second.
class SensorMonitor final : public rtos::Task {
public:
    explicit SensorMonitor(drivers::Ads1115& ads) : ads_(&ads) {}

    SensorMonitor(const SensorMonitor&) = delete;
    SensorMonitor& operator=(const SensorMonitor&) = delete;
    SensorMonitor(SensorMonitor&&) = delete;
    SensorMonitor& operator=(SensorMonitor&&) = delete;
    ~SensorMonitor() = default;

    /// GPIO ISR for ALERT/RDY, `arg` is the SensorMonitor.
    static void onConversionReady(void* arg);

protected:
    void run() override;

private:
    drivers::Ads1115* ads_;
};

} // namespace banana
