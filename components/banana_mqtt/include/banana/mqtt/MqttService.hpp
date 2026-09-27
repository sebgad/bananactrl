#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>

#include "banana/config/Config.hpp"
#include "banana/config/ConfigStore.hpp"
#include "banana/control/HeaterControl.hpp"
#include "banana/core/Result.hpp"
#include "banana/hal/PeriodicTimer.hpp"
#include "banana/mqtt/HomeAssistant.hpp"
#include "banana/net/WifiManager.hpp"
#include "banana/rtos/Task.hpp"

#include "mqtt_client.h"

namespace banana::mqtt {

/// Optional connection to an MQTT broker for Home Assistant (RAII around esp_mqtt_client).
///
/// - On connect: availability "online", discovery messages (retained), subscriptions, state.
/// - Every `statePeriod`: state. Published with esp_mqtt_client_enqueue(), so the timer task never waits on
/// the
///   network; the MQTT client task sends. Nothing is queued while disconnected.
/// - Commands change the configuration through ConfigStore::update() (like the settings page); the store
///   notifies the heater task. Restart restarts the chip.
/// - Settings changes apply at once: as a config listener it hands new MQTT settings to its own small task
///   ("mqtt_mgr"), which disconnects, rebuilds the client or switches MQTT off/on. That cannot happen in the
///   MQTT client task (it would wait for itself) or the timer task (blocking).
/// - Reconnects by itself (esp-mqtt). After a Home Assistant restart ("homeassistant/status" = online) the
///   discovery is published again.
class MqttService final : public config::IConfigListener, private rtos::Task {
public:
    struct Dependencies {
        config::ConfigStore* store;
        control::IHeaterControl* heater;
        const net::WifiManager* wifi;
    };

    /// Nothing happens before start(). Not movable: the MQTT event handler and the task hold `this`.
    MqttService() = default;
    MqttService(const MqttService&) = delete;
    MqttService& operator=(const MqttService&) = delete;
    MqttService(MqttService&&) = delete;
    MqttService& operator=(MqttService&&) = delete;
    ~MqttService() override;

    /// Call once, whether MQTT is enabled or not (it can be enabled later). Register it as config listener
    /// afterwards.
    [[nodiscard]] Result<void> start(const config::MqttSettings& settings, Device device,
                                     const Dependencies& dependencies, std::chrono::milliseconds statePeriod);

    void onConfigChanged(const config::Config& config) override;

    [[nodiscard]] bool connected() const { return connected_.load(); }

protected:
    void run() override; ///< applies requested settings

private:
    void apply(const config::MqttSettings& settings);
    void stopClient();
    [[nodiscard]] bool isCurrent(esp_mqtt_client_handle_t client);
    static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    static void onTimer(void* arg);
    void onConnected(esp_mqtt_client_handle_t client);
    void onData(esp_mqtt_client_handle_t client, std::string_view topic, std::string_view payload);
    void publishDiscovery(esp_mqtt_client_handle_t client);
    void publishState(esp_mqtt_client_handle_t client);

    Dependencies deps_{};
    Device device_;
    Topics topics_{""}; ///< set in start()
    std::string availabilityTopic_;
    std::optional<hal::PeriodicTimer> timer_;

    std::mutex clientMutex_; ///< client_ vs. the state timer; never held while waiting on MQTT
    esp_mqtt_client_handle_t client_ = nullptr; ///< current client; replaced only by the mqtt_mgr task
    std::atomic<bool> connected_{false};

    std::mutex pendingMutex_;
    std::optional<config::MqttSettings> pending_; ///< for the mqtt_mgr task
    config::MqttSettings requested_;              ///< last settings handed to the task
};

} // namespace banana::mqtt
