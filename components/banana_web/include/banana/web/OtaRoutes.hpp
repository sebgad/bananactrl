#pragma once

#include <string>

#include "banana/core/Result.hpp"
#include "banana/web/WebServer.hpp"

namespace banana::web {

/// Firmware and data file uploads. esp_http_server has no multipart parser, so ota.html sends the file
/// as the raw request body with the headers `X-MD5` (firmware) or `X-Filename` (data file).
///
/// - GET  /failsafe      ota.html built into the firmware (works without LittleFS)
/// - POST /ota_firmware  image into the next OTA partition, then restart
/// - POST /ota_spiffs    file into the LittleFS root (name kept from the Arduino firmware)
class OtaRoutes {
public:
    /// `root`: LittleFS mount point, e.g. "/fs".
    explicit OtaRoutes(std::string root) : root_(std::move(root)) {}

    [[nodiscard]] Result<void> registerOn(WebServer& server);

private:
    esp_err_t failsafe(HttpRequest& request);
    esp_err_t firmware(HttpRequest& request);
    esp_err_t dataFile(HttpRequest& request);

    std::string root_;
};

} // namespace banana::web
