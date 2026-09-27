#include "banana/web/ApiRoutes.hpp"

#include <chrono>

#include "esp_log.h"

#include "banana/config/ConfigJson.hpp"
#include "banana/web/LiveValues.hpp"

namespace banana::web {
namespace {

constexpr const char* kTag = "web";
constexpr std::size_t kMaxParamsBytes = 4096; // params.json is ~1.5 kB
constexpr std::chrono::milliseconds kRestartDelay{2000};

} // namespace

Result<void> ApiRoutes::registerOn(WebServer& server)
{
    if (auto res = server.on<ApiRoutes, &ApiRoutes::lastValues>("/lastvalues.json", HTTP_GET, *this); !res) {
        return res;
    }
    if (auto res = server.on<ApiRoutes, &ApiRoutes::params>("/params.json", HTTP_GET, *this); !res) {
        return res;
    }
    if (auto res = server.on<ApiRoutes, &ApiRoutes::paramUpdate>("/paramUpdate", HTTP_POST, *this); !res) {
        return res;
    }
    if (auto res = server.on<ApiRoutes, &ApiRoutes::paramReset>("/paramReset", HTTP_GET, *this); !res) {
        return res;
    }
    return server.on<ApiRoutes, &ApiRoutes::restart>("/restartesp", HTTP_GET, *this);
}

esp_err_t ApiRoutes::lastValues(HttpRequest& request)
{
    const std::string json = lastValuesJson(heater_->snapshot(), wifi_->rssiPercent().value_or(0));
    return request.send(Status::Ok, "application/json", json);
}

esp_err_t ApiRoutes::params(HttpRequest& request)
{
    // From memory, not the file: the Wi-Fi password is never sent
    return request.send(Status::Ok, "application/json", config::toPublicJson(store_->current()));
}

esp_err_t ApiRoutes::paramUpdate(HttpRequest& request)
{
    auto body = request.body(kMaxParamsBytes);
    if (!body) {
        return request.sendText(body.error() == ESP_ERR_INVALID_SIZE ? Status::ContentTooLarge
                                                                     : Status::BadRequest,
                                "Parameters are not updated: request not received");
    }
    const config::Config current = store_->current();
    auto updated = config::applyWebUpdate(*body, current);
    if (!updated) {
        return request.sendText(Status::BadRequest, "Parameters are not updated: invalid JSON");
    }
    if (auto res = store_->save(*updated); !res) {
        ESP_LOGE(kTag, "saving parameters failed: %s", esp_err_to_name(res.error()));
        return request.sendText(Status::InternalError,
                                "Parameters are not updated: params.json not writable");
    }
    heater_->requestConfig(*updated);
    ESP_LOGI(kTag, "parameters updated");
    if (updated->wifi != current.wifi) {
        return request.sendText(Status::Ok, "Parameters are updated and changes applied. "
                                            "Wi-Fi changes take effect after a restart.");
    }
    return request.sendText(Status::Ok, "Parameters are updated and changes applied.");
}

esp_err_t ApiRoutes::paramReset(HttpRequest& request)
{
    auto defaults = store_->reset();
    if (!defaults) {
        ESP_LOGE(kTag, "resetting parameters failed: %s", esp_err_to_name(defaults.error()));
        return request.sendText(Status::InternalError, "Parameters are not reset: params.json not writable");
    }
    heater_->requestConfig(*defaults);
    ESP_LOGI(kTag, "parameters reset to defaults");
    return request.sendText(Status::Ok, "Parameters are set back to default values");
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): member for WebServer::on()
esp_err_t ApiRoutes::restart(HttpRequest& request)
{
    request.sendText(Status::Ok, "ESP is going to restart");
    restartAfter(kRestartDelay);
}

} // namespace banana::web
