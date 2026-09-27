#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

#include "esp_http_server.h"

#include "banana/control/HeaterControl.hpp"
#include "banana/core/Result.hpp"
#include "banana/hal/PeriodicTimer.hpp"
#include "banana/net/WifiManager.hpp"
#include "banana/storage/MeasurementRecorder.hpp"
#include "banana/web/WebServer.hpp"

namespace banana::web {

/// GET /events: server-sent events instead of polling. Once per period every subscriber gets
///   - `values`: the /lastvalues.json object (index.html gauges),
///   - `rows`:   the data.csv rows recorded since the previous event (graphs.html), if any.
///
/// Subscriptions are async requests (httpd_req_async_handler_begin): the socket stays open and is excluded
/// from the LRU purge. Subscribing and sending both run in the httpd task (the timer only queues the work),
/// so the client list needs no lock. A client that cannot be written to is dropped; EventSource reconnects by
/// itself.
class EventStream final : public storage::IRowListener {
public:
    static constexpr std::size_t kMaxClients = 3;
    static constexpr std::size_t kMaxPendingRows = 32;

    EventStream(control::IHeaterControl& heater, const net::WifiManager& wifi)
        : heater_(&heater), wifi_(&wifi)
    {
    }

    EventStream(const EventStream&) = delete;
    EventStream& operator=(const EventStream&) = delete;
    EventStream(EventStream&&) = delete;
    EventStream& operator=(EventStream&&) = delete;
    ~EventStream() override = default; ///< the server (and its async requests) must be stopped first

    /// Registers /events and starts sending every `period`.
    [[nodiscard]] Result<void> start(WebServer& server, std::chrono::milliseconds period);

    /// From the heater task.
    void onRow(const storage::csv::Row& row) override;

private:
    esp_err_t subscribe(HttpRequest& request);
    static void onTimer(void* arg);
    static void broadcastWork(void* arg);
    void broadcast();
    void drop(std::size_t index);

    control::IHeaterControl* heater_;
    const net::WifiManager* wifi_;
    httpd_handle_t server_ = nullptr;
    std::optional<hal::PeriodicTimer> timer_;

    std::array<httpd_req_t*, kMaxClients> clients_{}; ///< httpd task only
    std::atomic<std::size_t> clientCount_{0};
    std::atomic<bool> workQueued_{false};

    std::mutex rowsMutex_;
    std::vector<storage::csv::Row> pendingRows_;
};

} // namespace banana::web
