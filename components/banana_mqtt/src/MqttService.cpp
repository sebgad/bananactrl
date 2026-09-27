#include "banana/mqtt/MqttService.hpp"

#include <utility>

#include "esp_log.h"

#include "banana/rtos/Restart.hpp"

namespace banana::mqtt {
namespace {

constexpr const char* kTag = "mqtt";
constexpr const char* kOnline = "online";
constexpr const char* kOffline = "offline";
constexpr int kBufferBytes = 2048;            ///< largest discovery message is ~700 bytes
constexpr std::uint64_t kOutboxLimit = 16384; ///< bounded queue while the network is slow
constexpr std::chrono::milliseconds kRestartDelay{1000};
constexpr std::uint32_t kManagerStackBytes = 4096;
constexpr UBaseType_t kManagerPriority = 3;

} // namespace

MqttService::~MqttService()
{
    timer_.reset(); // no more publishes
    stopClient();
}

Result<void> MqttService::start(const config::MqttSettings& settings, Device device,
                                const Dependencies& dependencies, std::chrono::milliseconds statePeriod)
{
    if (running()) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    deps_ = dependencies;
    device_ = std::move(device);
    topics_ = Topics{device_.id};
    availabilityTopic_ = topics_.availability();

    auto timer = hal::PeriodicTimer::create("mqtt", &MqttService::onTimer, this);
    if (!timer) {
        return fail(timer.error());
    }
    timer_.emplace(std::move(*timer));
    if (auto res = timer_->start(std::chrono::duration_cast<std::chrono::microseconds>(statePeriod)); !res) {
        return res;
    }
    {
        const std::scoped_lock lock{pendingMutex_};
        pending_ = settings;
        requested_ = settings;
    }
    return Task::start("mqtt_mgr", kManagerStackBytes, kManagerPriority); // applies `settings` first
}

void MqttService::onConfigChanged(const config::Config& config)
{
    {
        const std::scoped_lock lock{pendingMutex_};
        if (config.mqtt == requested_) {
            return; // another setting changed
        }
        requested_ = config.mqtt;
        pending_ = config.mqtt;
    }
    if (handle() != nullptr) {
        xTaskNotifyGive(handle());
    }
}

void MqttService::run()
{
    for (;;) {
        std::optional<config::MqttSettings> settings;
        {
            const std::scoped_lock lock{pendingMutex_};
            settings.swap(pending_);
        }
        if (settings) {
            apply(*settings);
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void MqttService::apply(const config::MqttSettings& settings)
{
    stopClient();
    if (!settings.enabled) {
        ESP_LOGI(kTag, "disabled");
        return;
    }
    if (settings.host.empty()) {
        ESP_LOGW(kTag, "enabled, but no broker host set");
        return;
    }
    if (deps_.wifi->mode() != net::WifiManager::Mode::Station) {
        ESP_LOGW(kTag, "enabled, but no Wi-Fi station connection (SoftAP mode): not started");
        return;
    }

    esp_mqtt_client_config_t config{}; // esp-mqtt copies the strings
    config.broker.address.hostname = settings.host.c_str();
    config.broker.address.port = settings.port;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    config.credentials.client_id = device_.id.c_str();
    if (!settings.user.empty()) {
        config.credentials.username = settings.user.c_str();
        config.credentials.authentication.password = settings.password.c_str();
    }
    config.session.last_will.topic = availabilityTopic_.c_str();
    config.session.last_will.msg = kOffline;
    config.session.last_will.qos = 1;
    config.session.last_will.retain = 1;
    config.buffer.size = kBufferBytes;
    config.outbox.limit = kOutboxLimit;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(kTag, "client init failed");
        return;
    }
    if (esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, &MqttService::onEvent, this) != ESP_OK) {
        ESP_LOGE(kTag, "client setup failed");
        esp_mqtt_client_destroy(client);
        return;
    }
    {
        const std::scoped_lock lock{clientMutex_};
        client_ = client; // before start(): its first events must count as current
    }
    if (esp_mqtt_client_start(client) != ESP_OK) {
        ESP_LOGE(kTag, "client start failed");
        stopClient();
        return;
    }
    ESP_LOGI(kTag, "connecting to %s:%lu as %s", settings.host.c_str(),
             static_cast<unsigned long>(settings.port), device_.id.c_str());
}

void MqttService::stopClient()
{
    esp_mqtt_client_handle_t client = nullptr;
    {
        const std::scoped_lock lock{clientMutex_};
        client = std::exchange(client_, nullptr); // the timer stops using it
    }
    if (client == nullptr) {
        return;
    }
    if (connected_.exchange(false)) {
        // A clean disconnect does not trigger the last will: say goodbye explicitly
        esp_mqtt_client_publish(client, availabilityTopic_.c_str(), kOffline, 0, 1, 1);
    }
    esp_mqtt_client_destroy(client); // waits for the client task: its handlers do not use clientMutex_
    ESP_LOGI(kTag, "client stopped");
}

void MqttService::onEvent(void* arg, esp_event_base_t /*base*/, int32_t id, void* data)
{
    auto* self = static_cast<MqttService*>(arg);
    const auto* event = static_cast<const esp_mqtt_event_t*>(data);
    if (!self->isCurrent(event->client)) {
        return; // a client being replaced (stopClient() is waiting for it)
    }
    switch (static_cast<esp_mqtt_event_id_t>(id)) {
    case MQTT_EVENT_CONNECTED:
        self->connected_ = true;
        ESP_LOGI(kTag, "connected to the broker");
        self->onConnected(event->client);
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (self->connected_.exchange(false)) {
            ESP_LOGW(kTag, "disconnected from the broker (reconnects automatically)");
        }
        break;
    case MQTT_EVENT_DATA:
        // Commands are short; a payload split over several events is ignored
        if (event->data_len == event->total_data_len && event->current_data_offset == 0) {
            self->onData(event->client, {event->topic, static_cast<std::size_t>(event->topic_len)},
                         {event->data, static_cast<std::size_t>(event->data_len)});
        }
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle != nullptr &&
            event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(kTag, "broker refused the connection (code %d): check user/password",
                     static_cast<int>(event->error_handle->connect_return_code));
        }
        break;
    default:
        break;
    }
}

bool MqttService::isCurrent(esp_mqtt_client_handle_t client)
{
    const std::scoped_lock lock{clientMutex_};
    return client == client_;
}

void MqttService::onConnected(esp_mqtt_client_handle_t client)
{
    esp_mqtt_client_enqueue(client, availabilityTopic_.c_str(), kOnline, 0, 1, 1, true);
    publishDiscovery(client);
    for (const Command command : {Command::Target, Command::StandbyTime, Command::Restart}) {
        esp_mqtt_client_subscribe_single(client, topics_.command(command).c_str(), 1);
    }
    esp_mqtt_client_subscribe_single(client, std::string{kHomeAssistantStatusTopic}.c_str(), 1);
    publishState(client);
}

void MqttService::publishDiscovery(esp_mqtt_client_handle_t client)
{
    for (const Message& message : discoveryMessages(device_, topics_)) {
        esp_mqtt_client_enqueue(client, message.topic.c_str(), message.payload.data(),
                                static_cast<int>(message.payload.size()), 1, 1, true);
    }
}

void MqttService::onData(esp_mqtt_client_handle_t client, std::string_view topic, std::string_view payload)
{
    if (topic == kHomeAssistantStatusTopic) {
        if (payload == kOnline) {
            publishDiscovery(client); // Home Assistant restarted
            publishState(client);
        }
        return;
    }
    const auto command = topics_.commandFor(topic);
    if (!command) {
        return;
    }
    if (*command == Command::Restart) {
        ESP_LOGW(kTag, "restart requested via MQTT");
        // Sent directly (this is the MQTT task, which the restart delay blocks): an enqueued message would
        // not go out
        esp_mqtt_client_publish(client, availabilityTopic_.c_str(), kOffline, 0, 1, 1);
        rtos::restartAfter(kRestartDelay);
    }
    // The store notifies the heater task (and this service) after saving
    auto updated =
        deps_.store->update([&](config::Config& config) { return applyCommand(*command, payload, config); });
    if (!updated) {
        ESP_LOGW(kTag, "command %.*s = '%.*s' not applied: %s", static_cast<int>(topic.size()), topic.data(),
                 static_cast<int>(payload.size()), payload.data(), esp_err_to_name(updated.error()));
    } else {
        ESP_LOGI(kTag, "via MQTT: target %.1f C, standby after %lld min",
                 static_cast<double>(updated->pid.target),
                 static_cast<long long>(updated->system.timeToStandby.count() / 60));
    }
    publishState(client); // new value, or the unchanged one again
}

void MqttService::onTimer(void* arg)
{
    auto* self = static_cast<MqttService*>(arg);
    const std::scoped_lock lock{self->clientMutex_}; // held only briefly by the mqtt_mgr task
    if (self->client_ != nullptr) {
        self->publishState(self->client_);
    }
}

void MqttService::publishState(esp_mqtt_client_handle_t client)
{
    if (!connected_.load()) {
        return; // nothing piles up in the outbox while the broker is unreachable
    }
    const std::string payload =
        statePayload(deps_.heater->snapshot(), deps_.store->current(), deps_.wifi->rssiPercent().value_or(0));
    // enqueue: returns at once, the MQTT task sends (store=true is required for QoS 0)
    esp_mqtt_client_enqueue(client, topics_.state().c_str(), payload.data(), static_cast<int>(payload.size()),
                            0, 0, true);
}

} // namespace banana::mqtt
