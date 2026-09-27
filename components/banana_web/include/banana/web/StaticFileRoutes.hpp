#pragma once

#include <string>

#include "banana/core/Result.hpp"
#include "banana/web/WebServer.hpp"

namespace banana::web {

/// GET of the pages, data.csv and the log files from LittleFS (kStaticFiles), streamed in chunks.
class StaticFileRoutes {
public:
    /// `root`: LittleFS mount point, e.g. "/fs".
    explicit StaticFileRoutes(std::string root) : root_(std::move(root)) {}

    /// Registers a `/*` wildcard: call after all other routes.
    [[nodiscard]] Result<void> registerOn(WebServer& server);

private:
    esp_err_t get(HttpRequest& request);

    std::string root_;
};

} // namespace banana::web
