#pragma once

#include <atomic>
#include <mutex>
#include <optional>

#include "banana/config/Config.hpp"
#include "banana/config/ConfigStore.hpp"
#include "banana/control/ControlLoop.hpp"
#include "banana/control/HeaterControl.hpp"
#include "banana/drivers/RgbLed.hpp"
#include "banana/drivers/Ssr.hpp"
#include "banana/io/Outputs.hpp"
#include "banana/rtos/EventGroup.hpp"
#include "banana/rtos/Task.hpp"
#include "banana/storage/MeasurementRecorder.hpp"

namespace banana {

/// Runs the ControlLoop: turns interrupts, the 450 ms timer and config changes into loop calls,
/// feeds the task watchdog and publishes the process snapshot. Replaces the Arduino loop().
class HeaterTask final : public rtos::Task, public control::IHeaterControl, public config::IConfigListener {
public:
    static constexpr rtos::EventGroup::Bits kSampleReady = 1U << 0U;   ///< ADS1115 ALERT/RDY (8 SPS)
    static constexpr rtos::EventGroup::Bits kTick = 1U << 1U;          ///< 450 ms timer
    static constexpr rtos::EventGroup::Bits kPumpEdge = 1U << 2U;      ///< pump relay changed
    static constexpr rtos::EventGroup::Bits kConfigChanged = 1U << 3U; ///< requestConfig()

    /// All non-null, owned by App.
    struct Hardware {
        io::ITemperatureSensor* sensor;
        drivers::Ssr* ssr;
        drivers::RgbLed* led;
        const io::IDigitalInput* pumpRelay;
        const io::INetworkStatus* network;
    };

    HeaterTask(rtos::EventGroup& events, const Hardware& hardware, const config::Config& config);

    HeaterTask(const HeaterTask&) = delete;
    HeaterTask& operator=(const HeaterTask&) = delete;
    HeaterTask(HeaterTask&&) = delete;
    HeaterTask& operator=(HeaterTask&&) = delete;
    ~HeaterTask() override = default;

    /// From any task: applied inside the heater task at its next wake-up.
    void requestConfig(const config::Config& config) override;
    /// Every saved configuration (settings page, MQTT, reset) is applied.
    void onConfigChanged(const config::Config& config) override { requestConfig(config); }
    [[nodiscard]] control::ProcessSnapshot snapshot() const override;
    /// Appends a row to data.csv on every 450 ms tick (STORE). The recorder must outlive the task.
    void attachRecorder(storage::MeasurementRecorder* recorder) { recorder_.store(recorder); }
    /// Gets every recorded row (web UI live view). Must outlive the task.
    void attachRowListener(storage::IRowListener* listener) { rowListener_.store(listener); }

    /// ISR / timer trampolines, `arg` is the HeaterTask.
    static void onSampleReadyIsr(void* arg);
    static void onPumpEdgeIsr(void* arg);
    static void onTick(void* arg);

protected:
    void run() override;

private:
    void applyPendingConfig();
    void store(const control::ProcessSnapshot& snapshot);

    rtos::EventGroup* events_;
    Hardware hw_;
    control::ControlLoop loop_;
    std::uint32_t reportTicks_ = 0;
    std::atomic<storage::MeasurementRecorder*> recorder_{nullptr};
    std::atomic<storage::IRowListener*> rowListener_{nullptr};
    bool storeFailed_ = false;

    std::mutex configMutex_;
    std::optional<config::Config> pendingConfig_;

    mutable std::mutex snapshotMutex_;
    control::ProcessSnapshot snapshot_;
};

} // namespace banana
