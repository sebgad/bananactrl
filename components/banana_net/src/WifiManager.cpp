#include "banana/net/WifiManager.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "esp_log.h"
#include "esp_wifi.h"

namespace banana::net {
namespace {

constexpr const char* kTag = "wifi";
constexpr rtos::EventGroup::Bits kConnected = 1U << 0U;

template <std::size_t N>
void copyString(std::array<std::uint8_t, N>& target, const std::string& value)
{
    target.fill(0);
    std::memcpy(target.data(), value.data(), std::min(value.size(), N - 1)); // NUL-terminated
}

// calcWifiStrength() of the Arduino firmware
static_assert(WifiManager::percentFromDbm(-40) == 100);
static_assert(WifiManager::percentFromDbm(-50) == 100);
static_assert(WifiManager::percentFromDbm(-75) == 50);
static_assert(WifiManager::percentFromDbm(-100) == 0);
static_assert(WifiManager::percentFromDbm(-110) == 0);

} // namespace

WifiManager::~WifiManager()
{
    release();
}

Result<WifiManager::Mode> WifiManager::start(const Credentials& credentials, std::chrono::seconds timeout)
{
    if (mode_.load() != Mode::Off || events_) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    auto events = rtos::EventGroup::create();
    if (!events) {
        return fail(events.error());
    }
    rtos::EventGroup& connectedEvent = events_.emplace(std::move(*events));

    if (auto res = toResult(esp_netif_init()); !res) {
        return fail(res.error());
    }
    if (const esp_err_t err = esp_event_loop_create_default();
        err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return fail(err);
    }
    const wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
    if (auto res = toResult(esp_wifi_init(&initConfig)); !res) {
        return fail(res.error());
    }
    if (auto res = toResult(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                                &WifiManager::onEvent, this, &wifiHandler_));
        !res) {
        return fail(res.error());
    }
    if (auto res = toResult(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                                &WifiManager::onEvent, this, &ipHandler_));
        !res) {
        return fail(res.error());
    }

    if (!credentials.ssid.empty()) {
        ESP_LOGI(kTag, "connecting to '%s' (timeout %lld s)", credentials.ssid.c_str(),
                 static_cast<long long>(timeout.count()));
        connecting_ = true;
        if (auto res = startStation(credentials); !res) {
            connecting_ = false;
            return fail(res.error());
        }
        const auto bits = connectedEvent.waitAny(kConnected, timeout);
        connecting_ = false;
        if ((bits & kConnected) != 0) {
            mode_ = Mode::Station;
            reconnect_ = true; // from now on, retry after every disconnect
            const auto rssi = rssiPercent();
            ESP_LOGI(kTag, "connected, IP %s, signal %d %%", ipAddress().c_str(), rssi.value_or(0));
            return Mode::Station;
        }
        ESP_LOGW(kTag, "connection to '%s' not possible", credentials.ssid.c_str());
        esp_wifi_stop();
        driverStarted_ = false;
    } else {
        ESP_LOGW(kTag, "no SSID configured");
    }

    if (auto res = startAccessPoint(); !res) {
        return fail(res.error());
    }
    mode_ = Mode::AccessPoint;
    ESP_LOGI(kTag, "SoftAP '%s' started, IP %s", kAccessPointSsid, ipAddress().c_str());
    return Mode::AccessPoint;
}

Result<void> WifiManager::startStation(const Credentials& credentials)
{
    if (stationNetif_ == nullptr) {
        stationNetif_ = esp_netif_create_default_wifi_sta();
    }
    wifi_config_t config{};
    std::array<std::uint8_t, sizeof(config.sta.ssid)> ssid{};
    std::array<std::uint8_t, sizeof(config.sta.password)> password{};
    copyString(ssid, credentials.ssid);
    copyString(password, credentials.password);
    std::memcpy(config.sta.ssid, ssid.data(), ssid.size());
    std::memcpy(config.sta.password, password.data(), password.size());
    config.sta.threshold.authmode = credentials.password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    if (auto res = toResult(esp_wifi_set_mode(WIFI_MODE_STA)); !res) {
        return res;
    }
    if (auto res = toResult(esp_wifi_set_config(WIFI_IF_STA, &config)); !res) {
        return res;
    }
    if (auto res = toResult(esp_wifi_start()); !res) {
        return res;
    }
    driverStarted_ = true;
    return {}; // WIFI_EVENT_STA_START triggers esp_wifi_connect()
}

Result<void> WifiManager::startAccessPoint()
{
    if (apNetif_ == nullptr) {
        apNetif_ = esp_netif_create_default_wifi_ap();
    }
    wifi_config_t config{};
    std::array<std::uint8_t, sizeof(config.ap.ssid)> ssid{};
    copyString(ssid, kAccessPointSsid);
    std::memcpy(config.ap.ssid, ssid.data(), ssid.size());
    config.ap.ssid_len = static_cast<std::uint8_t>(std::strlen(kAccessPointSsid));
    config.ap.channel = 1;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN; // as WiFi.softAP("SilviaCoffeeCtrl")

    if (auto res = toResult(esp_wifi_set_mode(WIFI_MODE_AP)); !res) {
        return res;
    }
    if (auto res = toResult(esp_wifi_set_config(WIFI_IF_AP, &config)); !res) {
        return res;
    }
    if (auto res = toResult(esp_wifi_start()); !res) {
        return res;
    }
    driverStarted_ = true;
    return {};
}

void WifiManager::onEvent(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    auto* self = static_cast<WifiManager*>(arg);
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const auto* event = static_cast<const wifi_event_sta_disconnected_t*>(data);
        const bool wasConnected = self->connected_.exchange(false);
        // Reason codes: wifi_err_reason_t (e.g. 15 = 4-way handshake timeout ~ wrong password,
        // 201 = no AP found, 202 = auth fail)
        const int reason = event->reason;
        if (wasConnected || reason != self->lastReason_.exchange(reason)) {
            ESP_LOGW(kTag, "disconnected, reason %d, RSSI %d dBm", reason, event->rssi);
        }
        // During start(): keep trying until the timeout. Afterwards: only after a successful join.
        if (self->connecting_.load() || self->reconnect_.load()) {
            esp_wifi_connect();
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        self->lastReason_ = 0;
        const bool wasConnected = self->connected_.exchange(true);
        if (!wasConnected && self->mode_.load() == Mode::Station) {
            ESP_LOGI(kTag, "reconnected");
        }
        if (self->events_) {
            self->events_->set(kConnected);
        }
    }
}

std::optional<int> WifiManager::rssiPercent() const
{
    wifi_ap_record_t info{};
    if (!connected_.load() || esp_wifi_sta_get_ap_info(&info) != ESP_OK) {
        return std::nullopt;
    }
    return percentFromDbm(info.rssi);
}

std::string WifiManager::ipAddress() const
{
    esp_netif_t* netif = mode_.load() == Mode::AccessPoint ? apNetif_ : stationNetif_;
    esp_netif_ip_info_t info{};
    if (netif == nullptr || esp_netif_get_ip_info(netif, &info) != ESP_OK) {
        return {};
    }
    std::array<char, 16> text{};
    esp_ip4addr_ntoa(&info.ip, text.data(), static_cast<int>(text.size()));
    return text.data();
}

bool WifiManager::stationDisconnected() const
{
    return mode_.load() == Mode::Station && !connected_.load();
}

void WifiManager::release()
{
    reconnect_ = false;
    if (wifiHandler_ != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiHandler_);
        wifiHandler_ = nullptr;
    }
    if (ipHandler_ != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ipHandler_);
        ipHandler_ = nullptr;
    }
    if (driverStarted_) {
        esp_wifi_stop();
        driverStarted_ = false;
    }
    if (events_) {
        esp_wifi_deinit();
    }
    if (stationNetif_ != nullptr) {
        esp_netif_destroy_default_wifi(stationNetif_);
        stationNetif_ = nullptr;
    }
    if (apNetif_ != nullptr) {
        esp_netif_destroy_default_wifi(apNetif_);
        apNetif_ = nullptr;
    }
    mode_ = Mode::Off;
    connected_ = false;
}

} // namespace banana::net
