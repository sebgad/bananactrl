#include "HeaterTask.hpp"

#include <chrono>
#include <string_view>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "banana/rtos/Watchdog.hpp"

namespace banana {
namespace {

constexpr const char* kTag = "heater";
constexpr std::chrono::milliseconds kIdleWait{1000};
constexpr std::uint32_t kReportEveryTicks = 10; // 4.5 s

control::ControlLoop::Millis nowMillis()
{
    return control::ControlLoop::Millis{esp_timer_get_time() / 1000};
}

const char* faultState(control::Faults faults)
{
    return faults.none() ? "none" : "active";
}

const char* steamState(control::SteamState state)
{
    switch (state) {
    case control::SteamState::Off:
        return "off";
    case control::SteamState::HeatingUp:
        return "heating up";
    case control::SteamState::Ready:
        return "ready";
    case control::SteamState::Cooldown:
        return "ended, cooling down";
    }
    return "?";
}

void warnIfNoSteamDetection(const control::ControlLoop& loop)
{
    if (!loop.steamDetection()) {
        ESP_LOGW(kTag, "target too high to detect steam mode: steam detection off");
    }
}

void report(const control::ProcessSnapshot& before, const control::ProcessSnapshot& after)
{
    // Logged on change only (the Arduino firmware repeated fault messages every 0.9 s)
    if (after.brewing != before.brewing) {
        ESP_LOGI(kTag, "brewing %s", after.brewing ? "started" : "stopped");
    }
    if (after.steam != before.steam) {
        ESP_LOGI(kTag, "steam mode %s (T=%.2f C)", steamState(after.steam),
                 static_cast<double>(after.celsius));
    }
    if (after.standby && !before.standby) {
        ESP_LOGW(kTag, "timeout reached -> heater off (standby)");
    }
    if (after.faults != before.faults) {
        using control::Fault;
        ESP_LOGW(kTag, "faults %s:%s%s%s (T=%.2f C)", faultState(after.faults),
                 after.faults.test(Fault::TempOutOfRange) ? " temperature out of range" : "",
                 after.faults.test(Fault::MeasDeviceReset) ? " ADC reset/missing" : "",
                 after.faults.test(Fault::WifiDisconnect) ? " Wi-Fi disconnected" : "",
                 static_cast<double>(after.celsius));
    }
}

} // namespace

HeaterTask::HeaterTask(rtos::EventGroup& events, const Hardware& hardware, const config::Config& config)
    : events_(&events), hw_(hardware),
      loop_(*hardware.sensor, *hardware.ssr, *hardware.led, *hardware.network, config, nowMillis()),
      snapshot_(loop_.snapshot())
{
    warnIfNoSteamDetection(loop_);
}

void HeaterTask::requestConfig(const config::Config& config)
{
    {
        const std::scoped_lock lock{configMutex_};
        pendingConfig_ = config;
    }
    events_->set(kConfigChanged);
}

control::ProcessSnapshot HeaterTask::snapshot() const
{
    const std::scoped_lock lock{snapshotMutex_};
    return snapshot_;
}

void IRAM_ATTR HeaterTask::onSampleReadyIsr(void* arg)
{
    if (static_cast<HeaterTask*>(arg)->events_->setFromIsr(kSampleReady)) {
        portYIELD_FROM_ISR();
    }
}

void IRAM_ATTR HeaterTask::onPumpEdgeIsr(void* arg)
{
    if (static_cast<HeaterTask*>(arg)->events_->setFromIsr(kPumpEdge)) {
        portYIELD_FROM_ISR();
    }
}

void HeaterTask::onTick(void* arg)
{
    static_cast<HeaterTask*>(arg)->events_->set(kTick);
}

void HeaterTask::run()
{
    auto watchdog = rtos::Watchdog::subscribeCurrentTask();
    if (!watchdog) {
        ESP_LOGE(kTag, "watchdog subscription failed: %s", esp_err_to_name(watchdog.error()));
    }

    for (;;) {
        const auto& brew = loop_.brewDetector();
        const auto timeout = brew.pending() ? brew.remaining(nowMillis()) : kIdleWait;
        const auto bits = events_->waitAny(kSampleReady | kTick | kPumpEdge | kConfigChanged, timeout);
        const auto now = nowMillis();
        const control::ProcessSnapshot before = loop_.snapshot();

        // Same order as the Arduino loop(): brew detection, config, measurement + PID, LED, standby, DIAG
        if ((bits & kPumpEdge) != 0) {
            loop_.onPumpEdge(now);
        }
        loop_.pollBrew(now, hw_.pumpRelay->level());
        if ((bits & kConfigChanged) != 0) {
            applyPendingConfig();
        }
        if ((bits & kSampleReady) != 0) {
            loop_.onSample(now);
        }
        if ((bits & kTick) != 0) {
            loop_.onTick(now);
        }

        const control::ProcessSnapshot after = loop_.snapshot();
        {
            const std::scoped_lock lock{snapshotMutex_};
            snapshot_ = after;
        }
        report(before, after);
        if ((bits & kTick) != 0) {
            store(after);
        }
        if ((bits & kTick) != 0 && ++reportTicks_ % kReportEveryTicks == 0) {
            const std::string_view state = control::toString(after.state);
            ESP_LOGI(kTag, "T=%.2f C target=%.1f heater=%.1f counts state=%.*s standby=%d faults=0x%lX",
                     static_cast<double>(after.celsius), static_cast<double>(after.target),
                     static_cast<double>(after.heaterCounts), static_cast<int>(state.size()), state.data(),
                     after.standby, static_cast<unsigned long>(after.faults.raw()));
        }
        if (watchdog) {
            watchdog->feed();
        }
    }
}

void HeaterTask::store(const control::ProcessSnapshot& snapshot)
{
    storage::MeasurementRecorder* recorder = recorder_.load();
    if (recorder == nullptr) {
        return;
    }
    const storage::csv::Row row{.seconds = snapshot.seconds,
                                .celsius = snapshot.celsius,
                                .heaterPercent = snapshot.heaterPercent,
                                .target = snapshot.target,
                                .brewing = snapshot.brewing};
    auto written = recorder->append(row);
    if (!written) {
        if (!storeFailed_) {
            ESP_LOGE(kTag, "data.csv write failed: %s", esp_err_to_name(written.error()));
            storeFailed_ = true; // log once
        }
        return;
    }
    if (storage::IRowListener* listener = rowListener_.load(); listener != nullptr) {
        listener->onRow(*written);
    }
}

void HeaterTask::applyPendingConfig()
{
    std::optional<config::Config> config;
    {
        const std::scoped_lock lock{configMutex_};
        config.swap(pendingConfig_);
    }
    if (!config) {
        return;
    }
    if (auto res = hw_.ssr->configure(config->ssr); !res) {
        ESP_LOGE(kTag, "SSR config failed: %s", esp_err_to_name(res.error()));
    }
    if (auto res = hw_.led->configure(config->led); !res) {
        ESP_LOGE(kTag, "LED config failed: %s", esp_err_to_name(res.error()));
    }
    loop_.applyConfig(*config);
    ESP_LOGI(kTag, "new configuration applied");
    warnIfNoSteamDetection(loop_);
}

} // namespace banana
