#include "banana/web/StaticFileRoutes.hpp"

#include <vector>

#include "esp_log.h"

#include "banana/storage/File.hpp"
#include "banana/web/WebPaths.hpp"

namespace banana::web {
namespace {

constexpr const char* kTag = "web";
constexpr std::size_t kChunkBytes = 4096;

} // namespace

Result<void> StaticFileRoutes::registerOn(WebServer& server)
{
    return server.on<StaticFileRoutes, &StaticFileRoutes::get>("/*", HTTP_GET, *this);
}

esp_err_t StaticFileRoutes::get(HttpRequest& request)
{
    const auto name = staticFileFor(request.uri());
    if (!name) {
        return request.sendText(Status::NotFound, "Not found");
    }
    const std::string path = root_ + "/" + std::string{*name};
    auto file = storage::File::open(path, "r");
    if (!file) {
        return request.sendText(file.error() == ESP_ERR_NOT_FOUND ? Status::NotFound : Status::InternalError,
                                "File not available");
    }

    request.setContentType(contentType(contentName(*name)));
    if (isGzipped(*name)) {
        request.setHeader("Content-Encoding", "gzip");
    }
    std::vector<char> buffer(kChunkBytes);
    for (;;) {
        auto count = file->read(buffer);
        if (!count) {
            ESP_LOGE(kTag, "reading %s failed", path.c_str());
            return ESP_FAIL; // closes the connection: the client sees a truncated response
        }
        if (*count == 0) {
            break;
        }
        if (request.sendChunk({buffer.data(), *count}) != ESP_OK) {
            return ESP_FAIL; // client gone
        }
    }
    return request.finishChunks();
}

} // namespace banana::web
