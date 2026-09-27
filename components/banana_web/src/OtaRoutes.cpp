#include "banana/web/OtaRoutes.hpp"

#include <chrono>
#include <cinttypes>
#include <vector>

#include "esp_log.h"

#include "banana/rtos/Restart.hpp"
#include "banana/storage/File.hpp"
#include "banana/web/OtaUpdater.hpp"
#include "banana/web/WebPaths.hpp"

// ota.html, gzip-compressed at build time (see CMakeLists.txt)
// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): linker symbols
extern const char kFailsafeStart[] asm("_binary_ota_html_gz_start");
extern const char kFailsafeEnd[] asm("_binary_ota_html_gz_end");
// NOLINTEND(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

namespace banana::web {
namespace {

constexpr const char* kTag = "ota";
constexpr std::size_t kChunkBytes = 4096;
constexpr std::size_t kMaxDataFileBytes = 512U * 1024U;
constexpr std::chrono::milliseconds kRestartDelay{2000};

void removeQuietly(const std::string& path)
{
    if (auto res = storage::remove(path); !res) {
        ESP_LOGW(kTag, "%s not removed: %s", path.c_str(), esp_err_to_name(res.error()));
    }
}

/// Request body into a new file, committed to flash. The file is closed on return.
Result<void> receiveToFile(HttpRequest& request, const std::string& path)
{
    auto file = storage::File::open(path, "w");
    if (!file) {
        return fail(file.error());
    }
    std::vector<char> buffer(kChunkBytes);
    for (;;) {
        auto count = request.receive(buffer);
        if (!count) {
            return fail(count.error());
        }
        if (*count == 0) {
            return file->flush();
        }
        if (auto res = file->write({buffer.data(), *count}); !res) {
            return res;
        }
    }
}

} // namespace

Result<void> OtaRoutes::registerOn(WebServer& server)
{
    if (auto res = server.on<OtaRoutes, &OtaRoutes::failsafe>("/failsafe", HTTP_GET, *this); !res) {
        return res;
    }
    if (auto res = server.on<OtaRoutes, &OtaRoutes::firmware>("/ota_firmware", HTTP_POST, *this); !res) {
        return res;
    }
    return server.on<OtaRoutes, &OtaRoutes::dataFile>("/ota_spiffs", HTTP_POST, *this);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): member for WebServer::on()
esp_err_t OtaRoutes::failsafe(HttpRequest& request)
{
    request.setHeader("Content-Encoding", "gzip");
    return request.send(Status::Ok, "text/html",
                        {kFailsafeStart, static_cast<std::size_t>(kFailsafeEnd - kFailsafeStart)});
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): member for WebServer::on()
esp_err_t OtaRoutes::firmware(HttpRequest& request)
{
    const auto md5Text = request.header("X-MD5");
    if (!md5Text) {
        return request.sendText(Status::BadRequest, "MD5 parameter missing");
    }
    const auto md5 = parseMd5(*md5Text);
    if (!md5) {
        return request.sendText(Status::BadRequest, "MD5 parameter invalid");
    }
    auto update = OtaUpdater::begin(*md5, request.contentLength());
    if (!update) {
        ESP_LOGE(kTag, "OTA begin failed: %s", esp_err_to_name(update.error()));
        return request.sendText(update.error() == ESP_ERR_INVALID_SIZE ? Status::ContentTooLarge
                                                                       : Status::InternalError,
                                "OTA could not begin");
    }

    std::vector<char> buffer(kChunkBytes);
    for (;;) {
        auto count = request.receive(buffer);
        if (!count) {
            ESP_LOGE(kTag, "upload interrupted: %s", esp_err_to_name(count.error()));
            return ESP_FAIL; // connection is broken; ~OtaUpdater aborts the update
        }
        if (*count == 0) {
            break;
        }
        if (auto res = update->write({buffer.data(), *count}); !res) {
            ESP_LOGE(kTag, "OTA write failed: %s", esp_err_to_name(res.error()));
            return request.sendText(Status::InternalError, "OTA aborted.");
        }
    }
    if (auto res = update->finish(); !res) {
        return request.sendText(
            res.error() == ESP_ERR_INVALID_CRC ? Status::BadRequest : Status::InternalError,
            res.error() == ESP_ERR_INVALID_CRC ? "Firmware flash FAIL: MD5 mismatch" : "Firmware flash FAIL");
    }

    ESP_LOGI(kTag, "firmware flash successful (%zu bytes, %s). Restart ESP.", request.contentLength(),
             update->partition()->label);
    request.setHeader("Connection", "close");
    request.sendText(Status::Ok, "Firmware flash OK");
    rtos::restartAfter(kRestartDelay);
}

esp_err_t OtaRoutes::dataFile(HttpRequest& request)
{
    const auto name = request.header("X-Filename");
    if (!name || !isValidUploadName(*name)) {
        return request.sendText(Status::BadRequest, "File name not allowed");
    }
    if (request.contentLength() > kMaxDataFileBytes) {
        return request.sendText(Status::ContentTooLarge, "File too large");
    }

    const std::string target = root_ + "/" + *name;
    const std::string temporary = root_ + "/." + *name + ".upload"; // hidden: cannot be an upload name
    ESP_LOGI(kTag, "start uploading file: %s (%zu bytes)", name->c_str(), request.contentLength());
    if (auto res = receiveToFile(request, temporary); !res) {
        ESP_LOGE(kTag, "upload of %s failed: %s", name->c_str(), esp_err_to_name(res.error()));
        removeQuietly(temporary);
        return request.sendText(Status::InternalError, "File upload failed");
    }
    if (auto res = storage::rename(temporary, target); !res) {
        removeQuietly(temporary);
        return request.sendText(Status::InternalError, "File cannot be replaced");
    }
    ESP_LOGI(kTag, "file upload finished: %s", name->c_str());
    return request.sendText(Status::Ok, "File " + *name + " uploaded");
}

} // namespace banana::web
