#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "esp_event.h"
#include "esp_netif.h"

#include "banana/core/Result.hpp"
#include "banana/io/Outputs.hpp"
#include "banana/rtos/EventGroup.hpp"

namespace banana::net {

/// Station connection with fallback to an open SoftAP (connectWiFi() + WiFi.softAP() of the Arduino
/// firmware). Owns the network stack (netif, default event loop, Wi-Fi driver).
class WifiManager final : public io::INetworkStatus {
public:
    enum class Mode : std::uint8_t {
        Off,         ///< before start()
        Station,     ///< joined (or rejoining) the configured network
        AccessPoint, ///< fallback, SSID kAccessPointSsid, no password
    };

    struct Credentials {
        std::string ssid;
        std::string password;
    };

    static constexpr const char* kAccessPointSsid = "SilviaCoffeeCtrl";

    /// Nothing happens before start(). Not movable: the event handlers hold `this`.
    WifiManager() = default;
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;
    WifiManager(WifiManager&&) = delete;
    WifiManager& operator=(WifiManager&&) = delete;
    ~WifiManager() override;

    /// Blocks until connected or `timeout`; without connection it switches to the SoftAP.
    /// Call once. After a successful join, lost connections are retried in the background.
    [[nodiscard]] Result<Mode> start(const Credentials& credentials, std::chrono::seconds timeout);

    [[nodiscard]] Mode mode() const { return mode_.load(); }
    [[nodiscard]] bool isConnected() const { return connected_.load(); }
    /// Station: signal strength 0..100 % (calcWifiStrength()); nullopt if not connected.
    [[nodiscard]] std::optional<int> rssiPercent() const;
    [[nodiscard]] std::string ipAddress() const;

    /// Diagnostics: station mode but not connected (the SoftAP is not a fault).
    [[nodiscard]] bool stationDisconnected() const override;

    /// 2 * (dBm + 100), clamped to 0..100 %.
    [[nodiscard]] static constexpr int percentFromDbm(int dbm)
    {
        if (dbm >= -50) {
            return 100;
        }
        if (dbm <= -100) {
            return 0;
        }
        return 2 * (dbm + 100);
    }

private:
    [[nodiscard]] Result<void> startStation(const Credentials& credentials);
    [[nodiscard]] Result<void> startAccessPoint();
    static void onEvent(void* arg, esp_event_base_t base, int32_t id, void* data);
    void release();

    std::optional<rtos::EventGroup> events_; ///< created in start()
    esp_netif_t* stationNetif_ = nullptr;
    esp_netif_t* apNetif_ = nullptr;
    esp_event_handler_instance_t wifiHandler_ = nullptr;
    esp_event_handler_instance_t ipHandler_ = nullptr;
    bool driverStarted_ = false;
    std::atomic<Mode> mode_{Mode::Off};
    std::atomic<bool> connected_{false};
    std::atomic<bool> connecting_{false}; ///< first join attempt in start()
    std::atomic<bool> reconnect_{false};  ///< retry after disconnects (after a successful join)
    std::atomic<int> lastReason_{0};      ///< log each disconnect reason once per streak
};

} // namespace banana::net
