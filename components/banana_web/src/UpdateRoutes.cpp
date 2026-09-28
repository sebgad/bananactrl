#include "banana/web/UpdateRoutes.hpp"

#include <string_view>

#include <ArduinoJson.h>

namespace banana::web {
namespace {

constexpr std::size_t kMaxRequestBytes = 128;

} // namespace

Result<void> UpdateRoutes::registerOn(WebServer& server)
{
    if (auto res = server.on<UpdateRoutes, &UpdateRoutes::status>("/update.json", HTTP_GET, *this); !res) {
        return res;
    }
    return server.on<UpdateRoutes, &UpdateRoutes::start>("/update", HTTP_POST, *this);
}

esp_err_t UpdateRoutes::status(HttpRequest& request)
{
    return request.send(Status::Ok, "application/json", updateStatusJson(updater_->status()));
}

esp_err_t UpdateRoutes::start(HttpRequest& request)
{
    auto body = request.body(kMaxRequestBytes);
    if (!body) {
        return request.sendText(Status::BadRequest, "Request not received");
    }
    JsonDocument doc;
    if (deserializeJson(doc, *body)) {
        return request.sendText(Status::BadRequest, "Invalid JSON");
    }
    const auto action = doc["Action"].as<std::string_view>();
    Result<void> started;
    if (action == "check") {
        started = updater_->check(doc["Prereleases"].as<bool>());
    } else if (action == "install") {
        started = updater_->install();
    } else {
        return request.sendText(Status::BadRequest, "Unknown action");
    }
    if (started) {
        return request.sendText(Status::Ok, action == "check" ? "Checking for updates" : "Update started");
    }
    switch (started.error()) {
    case ESP_ERR_INVALID_STATE:
        return request.sendText(Status::Conflict, "An update job is already running");
    case ESP_ERR_NOT_SUPPORTED:
        return request.sendText(Status::Conflict,
                                "No internet: the controller is not connected to a Wi-Fi network");
    case ESP_ERR_NOT_FOUND:
        return request.sendText(Status::Conflict, "Check for updates first");
    default:
        return request.sendText(Status::InternalError, "Update task could not start");
    }
}

} // namespace banana::web
