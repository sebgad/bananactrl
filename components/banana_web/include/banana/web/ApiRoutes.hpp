#pragma once

#include "banana/config/ConfigStore.hpp"
#include "banana/control/HeaterControl.hpp"
#include "banana/core/Result.hpp"
#include "banana/net/WifiManager.hpp"
#include "banana/web/WebServer.hpp"

namespace banana::web {

/// Live values and settings: /lastvalues.json, /version.json, /params.json, /paramUpdate, /paramReset, /restartesp.
class ApiRoutes {
public:
    /// All referenced objects must outlive the server.
    ApiRoutes(config::ConfigStore& store, control::IHeaterControl& heater, const net::WifiManager& wifi)
        : store_(&store), heater_(&heater), wifi_(&wifi)
    {
    }

    [[nodiscard]] Result<void> registerOn(WebServer& server);

private:
    esp_err_t lastValues(HttpRequest& request);
    esp_err_t version(HttpRequest& request);
    esp_err_t params(HttpRequest& request);
    esp_err_t paramUpdate(HttpRequest& request);
    esp_err_t paramReset(HttpRequest& request);
    esp_err_t restart(HttpRequest& request);

    config::ConfigStore* store_;
    control::IHeaterControl* heater_;
    const net::WifiManager* wifi_;
};

} // namespace banana::web
