#include "banana/web/EventStream.hpp"

#include <string>

#include "esp_log.h"

#include "banana/web/LiveValues.hpp"

namespace banana::web {
namespace {

constexpr const char* kTag = "events";

} // namespace

Result<void> EventStream::start(WebServer& server, std::chrono::milliseconds period)
{
    if (auto res = server.on<EventStream, &EventStream::subscribe>("/events", HTTP_GET, *this); !res) {
        return res;
    }
    server_ = server.handle();
    auto timer = hal::PeriodicTimer::create("events", &EventStream::onTimer, this);
    if (!timer) {
        return fail(timer.error());
    }
    timer_.emplace(std::move(*timer));
    return timer_->start(std::chrono::duration_cast<std::chrono::microseconds>(period));
}

void EventStream::onRow(const storage::csv::Row& row)
{
    if (clientCount_.load() == 0) {
        return; // nobody watching: nothing to buffer
    }
    const std::scoped_lock lock{rowsMutex_};
    if (pendingRows_.size() < kMaxPendingRows) {
        pendingRows_.push_back(row);
    }
}

esp_err_t EventStream::subscribe(HttpRequest& request)
{
    std::size_t slot = kMaxClients;
    for (std::size_t i = 0; i < kMaxClients; ++i) {
        if (clients_.at(i) == nullptr) {
            slot = i;
            break;
        }
    }
    if (slot == kMaxClients) {
        drop(0); // the oldest subscriber makes room (often a closed tab that was not noticed yet)
        slot = 0;
    }

    httpd_req_t* async = nullptr;
    if (httpd_req_async_handler_begin(request.raw(), &async) != ESP_OK) {
        return request.sendText(Status::InternalError, "event stream not available");
    }
    HttpRequest stream{async};
    stream.setContentType("text/event-stream");
    stream.setHeader("Cache-Control", "no-cache");
    if (stream.sendChunk("retry: 3000\n\n") != ESP_OK) {
        httpd_req_async_handler_complete(async);
        return ESP_OK;
    }
    clients_.at(slot) = async;
    clientCount_.fetch_add(1);
    ESP_LOGI(kTag, "subscriber %u connected (%u active)", static_cast<unsigned>(httpd_req_to_sockfd(async)),
             static_cast<unsigned>(clientCount_.load()));
    return ESP_OK;
}

void EventStream::onTimer(void* arg)
{
    auto* self = static_cast<EventStream*>(arg);
    if (self->clientCount_.load() == 0 || self->workQueued_.exchange(true)) {
        return; // no subscribers, or the previous broadcast has not run yet
    }
    if (httpd_queue_work(self->server_, &EventStream::broadcastWork, self) != ESP_OK) {
        self->workQueued_ = false;
    }
}

void EventStream::broadcastWork(void* arg)
{
    auto* self = static_cast<EventStream*>(arg);
    self->workQueued_ = false;
    self->broadcast();
}

void EventStream::broadcast()
{
    std::vector<storage::csv::Row> rows;
    {
        const std::scoped_lock lock{rowsMutex_};
        rows.swap(pendingRows_);
    }
    std::string events =
        sseEvent("values", lastValuesJson(heater_->snapshot(), wifi_->rssiPercent().value_or(0)));
    if (!rows.empty()) {
        events += sseEvent("rows", rowsJson(rows));
    }
    for (std::size_t i = 0; i < kMaxClients; ++i) {
        httpd_req_t* client = clients_.at(i);
        if (client != nullptr && HttpRequest{client}.sendChunk(events) != ESP_OK) {
            drop(i);
        }
    }
}

void EventStream::drop(std::size_t index)
{
    httpd_req_t* client = clients_.at(index);
    if (client == nullptr) {
        return;
    }
    const int socket = httpd_req_to_sockfd(client);
    clients_.at(index) = nullptr;
    clientCount_.fetch_sub(1);
    httpd_req_async_handler_complete(client);
    httpd_sess_trigger_close(server_, socket); // the chunked response never ended: the socket is not reusable
    ESP_LOGI(kTag, "subscriber %d dropped (%u active)", socket, static_cast<unsigned>(clientCount_.load()));
}

} // namespace banana::web
