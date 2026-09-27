#pragma once

#include <expected>

#include "esp_err.h"

namespace banana {

/// Failures are values: every fallible operation returns a Result.
template <typename T>
using Result = std::expected<T, esp_err_t>;

/// Error value for `return fail(ESP_ERR_...);`
[[nodiscard]] constexpr std::unexpected<esp_err_t> fail(esp_err_t err)
{
    return std::unexpected(err);
}

/// Lifts an ESP-IDF status code into a Result<void>.
[[nodiscard]] constexpr Result<void> toResult(esp_err_t err)
{
    if (err != ESP_OK) {
        return fail(err);
    }
    return {};
}

} // namespace banana
