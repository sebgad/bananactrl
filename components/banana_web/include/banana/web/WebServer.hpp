#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "esp_http_server.h"

#include "banana/core/Result.hpp"

namespace banana::web {

enum class Status : std::uint8_t {
    Ok,
    BadRequest,
    Forbidden,
    NotFound,
    Conflict,
    ContentTooLarge,
    InternalError,
};

/// Non-owning view of one request: body reception and response helpers around httpd_req_t.
class HttpRequest {
public:
    explicit HttpRequest(httpd_req_t* request) : req_(request) {}

    [[nodiscard]] httpd_req_t* raw() const { return req_; }
    [[nodiscard]] std::string_view uri() const { return req_->uri; }
    [[nodiscard]] std::size_t contentLength() const { return req_->content_len; }
    [[nodiscard]] std::optional<std::string> header(const char* name) const;

    /// The whole body; ESP_ERR_INVALID_SIZE if it is longer than `maxBytes`.
    [[nodiscard]] Result<std::string> body(std::size_t maxBytes);
    /// Next part of the body into `buffer` (retries on socket timeouts); 0 when the body is complete.
    [[nodiscard]] Result<std::size_t> receive(std::span<char> buffer);

    esp_err_t send(Status status, const char* contentType, std::string_view body);
    esp_err_t sendText(Status status, std::string_view text) { return send(status, "text/plain", text); }
    /// Chunked response: set the type once, then chunks, then finishChunks().
    esp_err_t setContentType(const char* contentType);
    esp_err_t setHeader(const char* name, const char* value);
    esp_err_t sendChunk(std::string_view data);
    esp_err_t finishChunks();

private:
    httpd_req_t* req_;
    std::size_t received_ = 0;
};

/// RAII around httpd_handle_t (httpd_start/httpd_stop). Handlers are member functions of route objects,
/// which must outlive the server.
class WebServer {
public:
    struct Config {
        std::uint16_t port = 80;
        std::uint16_t maxUriHandlers = 16;
        std::size_t stackSize = 8192;
    };

    [[nodiscard]] static Result<WebServer> start(const Config& config);

    WebServer(const WebServer&) = delete;
    WebServer& operator=(const WebServer&) = delete;
    WebServer(WebServer&& other) noexcept;
    WebServer& operator=(WebServer&& other) noexcept;
    ~WebServer();

    /// For httpd_queue_work() and async requests. Stays valid when the WebServer object is moved.
    [[nodiscard]] httpd_handle_t handle() const { return handle_; }

    /// `uri` may end with a `*` wildcard. Handlers are matched in registration order.
    template <typename T, esp_err_t (T::*Method)(HttpRequest&)>
    [[nodiscard]] Result<void> on(const char* uri, httpd_method_t method, T& target)
    {
        const httpd_uri_t entry{
            .uri = uri, .method = method, .handler = &dispatch<T, Method>, .user_ctx = &target};
        return toResult(httpd_register_uri_handler(handle_, &entry));
    }

private:
    explicit WebServer(httpd_handle_t handle) : handle_(handle) {}
    void stop();

    template <typename T, esp_err_t (T::*Method)(HttpRequest&)>
    static esp_err_t dispatch(httpd_req_t* raw)
    {
        HttpRequest request{raw};
        return (static_cast<T*>(raw->user_ctx)->*Method)(request);
    }

    httpd_handle_t handle_ = nullptr;
};

} // namespace banana::web
