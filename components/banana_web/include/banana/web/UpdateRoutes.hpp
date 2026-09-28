#pragma once

#include "banana/core/Result.hpp"
#include "banana/web/ReleaseUpdater.hpp"
#include "banana/web/WebServer.hpp"

namespace banana::web {

/// Updates from GitHub releases (ota.html):
/// - GET  /update.json  state of ReleaseUpdater (updateStatusJson())
/// - POST /update       {"Action": "check", "Prereleases": false} or {"Action": "install"}; the job runs in
///                      the background, the page polls /update.json
class UpdateRoutes {
public:
    /// `updater` must outlive the server.
    explicit UpdateRoutes(ReleaseUpdater& updater) : updater_(&updater) {}

    [[nodiscard]] Result<void> registerOn(WebServer& server);

private:
    esp_err_t status(HttpRequest& request);
    esp_err_t start(HttpRequest& request);

    ReleaseUpdater* updater_;
};

} // namespace banana::web
