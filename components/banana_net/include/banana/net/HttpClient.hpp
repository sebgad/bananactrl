#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>

#include "esp_http_client.h"

#include "banana/core/Result.hpp"

namespace banana::net {

/// One streaming HTTPS GET (RAII around esp_http_client): get() follows redirects and returns once the
/// status line and headers of the final response are in, read() then streams the body. Servers are verified
/// with ESP-IDF's certificate bundle.
class HttpClient {
public:
    struct Request {
        const char* url = nullptr;
        const char* userAgent = "bananactrl";
        const char* accept = nullptr; ///< Accept header, if set
        int timeoutMs = 15'000;       ///< per network operation
    };

    /// ESP_ERR_NOT_FOUND for 404, ESP_ERR_INVALID_RESPONSE for any other status than 200 (after redirects).
    [[nodiscard]] static Result<HttpClient> get(const Request& request);

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    HttpClient(HttpClient&& other) noexcept;
    HttpClient& operator=(HttpClient&& other) noexcept;
    ~HttpClient();

    /// Content-Length of the response; nullopt for chunked responses.
    [[nodiscard]] std::optional<std::size_t> contentLength() const;
    /// Up to `buffer.size()` bytes of the body; 0 at its end. ESP_ERR_INVALID_SIZE if the connection ends
    /// before Content-Length bytes arrived.
    [[nodiscard]] Result<std::size_t> read(std::span<char> buffer);
    /// The whole body; ESP_ERR_INVALID_SIZE if it is longer than `maxBytes`.
    [[nodiscard]] Result<std::string> readAll(std::size_t maxBytes);

private:
    explicit HttpClient(esp_http_client_handle_t handle) : handle_(handle) {}
    void release();

    esp_http_client_handle_t handle_ = nullptr;
};

} // namespace banana::net
