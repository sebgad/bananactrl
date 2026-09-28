#include "banana/net/HttpClient.hpp"

#include <array>
#include <cstdint>
#include <utility>

#include "esp_crt_bundle.h"
#include "esp_log.h"

namespace banana::net {
namespace {

constexpr const char* kTag = "http";
constexpr int kMaxRedirects = 5;
// Receive buffer: also holds the response headers (the CDN sends long ones).
constexpr int kRxBufferBytes = 4096;
// Transmit buffer: one request header line must fit, e.g. the ~1.2 kB signed GitHub asset redirect URL.
constexpr int kTxBufferBytes = 2048;

bool isRedirect(int status)
{
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

} // namespace

Result<HttpClient> HttpClient::get(const Request& request)
{
    esp_http_client_config_t config{};
    config.url = request.url;
    config.user_agent = request.userAgent;
    config.timeout_ms = request.timeoutMs;
    config.buffer_size = kRxBufferBytes;
    config.buffer_size_tx = kTxBufferBytes;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t handle = esp_http_client_init(&config);
    if (handle == nullptr) {
        return fail(ESP_ERR_NO_MEM);
    }
    HttpClient client{handle}; // cleans up on every return below
    if (request.accept != nullptr) {
        if (auto res = toResult(esp_http_client_set_header(handle, "Accept", request.accept)); !res) {
            return fail(res.error());
        }
    }

    // esp_http_client_perform() would follow redirects itself, but it buffers the body; streaming needs
    // open/fetch_headers, which leave redirects to the caller.
    for (int redirects = 0;; ++redirects) {
        if (auto res = toResult(esp_http_client_open(handle, 0)); !res) {
            ESP_LOGW(kTag, "connecting failed: %s", esp_err_to_name(res.error()));
            return fail(res.error());
        }
        if (esp_http_client_fetch_headers(handle) < 0) {
            ESP_LOGW(kTag, "no response headers");
            return fail(ESP_ERR_HTTP_FETCH_HEADER);
        }
        const int status = esp_http_client_get_status_code(handle);
        if (status == 200) {
            return client;
        }
        if (!isRedirect(status)) {
            ESP_LOGW(kTag, "HTTP status %d for %s", status, request.url);
            return fail(status == 404 ? ESP_ERR_NOT_FOUND : ESP_ERR_INVALID_RESPONSE);
        }
        if (redirects == kMaxRedirects) {
            return fail(ESP_ERR_HTTP_MAX_REDIRECT);
        }
        // Discard the redirect body; set_redirection() takes the Location header and, for another host
        // (github.com -> release-assets.githubusercontent.com), closes the connection.
        if (auto res = toResult(esp_http_client_flush_response(handle, nullptr)); !res) {
            return fail(res.error());
        }
        if (auto res = toResult(esp_http_client_set_redirection(handle)); !res) {
            ESP_LOGW(kTag, "redirect failed: %s", esp_err_to_name(res.error()));
            return fail(res.error());
        }
    }
}

HttpClient::HttpClient(HttpClient&& other) noexcept : handle_(std::exchange(other.handle_, nullptr))
{
}

HttpClient& HttpClient::operator=(HttpClient&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

HttpClient::~HttpClient()
{
    release();
}

std::optional<std::size_t> HttpClient::contentLength() const
{
    if (esp_http_client_is_chunked_response(handle_)) {
        return std::nullopt;
    }
    const std::int64_t length = esp_http_client_get_content_length(handle_);
    if (length < 0) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(length);
}

Result<std::size_t> HttpClient::read(std::span<char> buffer)
{
    const int count = esp_http_client_read(handle_, buffer.data(), static_cast<int>(buffer.size()));
    if (count == -ESP_ERR_HTTP_EAGAIN) {
        return fail(ESP_ERR_TIMEOUT);
    }
    if (count < 0) {
        return fail(ESP_FAIL);
    }
    if (count == 0 && !esp_http_client_is_complete_data_received(handle_)) {
        return fail(ESP_ERR_INVALID_SIZE); // connection closed early
    }
    return static_cast<std::size_t>(count);
}

Result<std::string> HttpClient::readAll(std::size_t maxBytes)
{
    std::string body;
    std::array<char, 512> buffer{};
    for (;;) {
        auto count = read(buffer);
        if (!count) {
            return fail(count.error());
        }
        if (*count == 0) {
            return body;
        }
        if (body.size() + *count > maxBytes) {
            return fail(ESP_ERR_INVALID_SIZE);
        }
        body.append(buffer.data(), *count);
    }
}

void HttpClient::release()
{
    if (handle_ != nullptr) {
        esp_http_client_close(handle_);
        esp_http_client_cleanup(handle_);
        handle_ = nullptr;
    }
}

} // namespace banana::net
