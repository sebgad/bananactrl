#include "banana/web/WebServer.hpp"

#include <algorithm>
#include <utility>

namespace banana::web {
namespace {

const char* statusLine(Status status)
{
    switch (status) {
    case Status::Ok:
        return HTTPD_200;
    case Status::BadRequest:
        return HTTPD_400;
    case Status::Forbidden:
        return "403 Forbidden";
    case Status::NotFound:
        return HTTPD_404;
    case Status::ContentTooLarge:
        return "413 Content Too Large";
    case Status::InternalError:
        return HTTPD_500;
    }
    return HTTPD_500;
}

constexpr int kMaxTimeouts = 3; // x recv_wait_timeout (5 s)

} // namespace

std::optional<std::string> HttpRequest::header(const char* name) const
{
    const std::size_t length = httpd_req_get_hdr_value_len(req_, name);
    if (length == 0) {
        return std::nullopt;
    }
    std::string value(length + 1, '\0');
    if (httpd_req_get_hdr_value_str(req_, name, value.data(), value.size()) != ESP_OK) {
        return std::nullopt;
    }
    value.resize(length);
    return value;
}

Result<std::size_t> HttpRequest::receive(std::span<char> buffer)
{
    const std::size_t remaining = req_->content_len - received_;
    if (remaining == 0 || buffer.empty()) {
        return 0U;
    }
    const std::size_t wanted = std::min(buffer.size(), remaining);
    for (int timeouts = 0;;) {
        const int count = httpd_req_recv(req_, buffer.data(), wanted);
        if (count > 0) {
            received_ += static_cast<std::size_t>(count);
            return static_cast<std::size_t>(count);
        }
        if (count != HTTPD_SOCK_ERR_TIMEOUT || ++timeouts >= kMaxTimeouts) {
            return fail(count == HTTPD_SOCK_ERR_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_FAIL);
        }
    }
}

Result<std::string> HttpRequest::body(std::size_t maxBytes)
{
    if (req_->content_len > maxBytes) {
        return fail(ESP_ERR_INVALID_SIZE);
    }
    std::string content(req_->content_len, '\0');
    std::size_t offset = 0;
    while (offset < content.size()) {
        auto count = receive(std::span{content}.subspan(offset));
        if (!count) {
            return fail(count.error());
        }
        if (*count == 0) {
            return fail(ESP_FAIL);
        }
        offset += *count;
    }
    return content;
}

esp_err_t HttpRequest::send(Status status, const char* contentType, std::string_view body)
{
    httpd_resp_set_status(req_, statusLine(status));
    httpd_resp_set_type(req_, contentType);
    return httpd_resp_send(req_, body.data(), static_cast<ssize_t>(body.size()));
}

esp_err_t HttpRequest::setContentType(const char* contentType)
{
    return httpd_resp_set_type(req_, contentType);
}

esp_err_t HttpRequest::setHeader(const char* name, const char* value)
{
    return httpd_resp_set_hdr(req_, name, value);
}

esp_err_t HttpRequest::sendChunk(std::string_view data)
{
    if (data.empty()) {
        return ESP_OK; // an empty chunk would end the response
    }
    return httpd_resp_send_chunk(req_, data.data(), static_cast<ssize_t>(data.size()));
}

esp_err_t HttpRequest::finishChunks()
{
    return httpd_resp_send_chunk(req_, nullptr, 0);
}

Result<WebServer> WebServer::start(const Config& config)
{
    httpd_config_t httpd = HTTPD_DEFAULT_CONFIG();
    httpd.server_port = config.port;
    httpd.max_uri_handlers = config.maxUriHandlers;
    httpd.stack_size = config.stackSize;
    httpd.uri_match_fn = httpd_uri_match_wildcard;
    httpd.lru_purge_enable = true; // browsers keep idle connections open; drop the oldest instead of refusing

    httpd_handle_t handle = nullptr;
    if (auto res = toResult(httpd_start(&handle, &httpd)); !res) {
        return fail(res.error());
    }
    return WebServer{handle};
}

WebServer::WebServer(WebServer&& other) noexcept : handle_(std::exchange(other.handle_, nullptr))
{
}

WebServer& WebServer::operator=(WebServer&& other) noexcept
{
    if (this != &other) {
        stop();
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

WebServer::~WebServer()
{
    stop();
}

void WebServer::stop()
{
    if (handle_ != nullptr) {
        httpd_stop(handle_);
        handle_ = nullptr;
    }
}

} // namespace banana::web
