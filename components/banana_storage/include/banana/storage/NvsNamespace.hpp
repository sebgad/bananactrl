#pragma once

#include <string>

#include "banana/core/Result.hpp"

#include "nvs.h"

namespace banana::storage {

/// One NVS namespace opened read/write (RAII around nvs_handle_t). Requires an initialised NVS (`Nvs`).
class NvsNamespace {
public:
    [[nodiscard]] static Result<NvsNamespace> open(const char* name);

    NvsNamespace(const NvsNamespace&) = delete;
    NvsNamespace& operator=(const NvsNamespace&) = delete;
    NvsNamespace(NvsNamespace&& other) noexcept;
    NvsNamespace& operator=(NvsNamespace&& other) noexcept;
    ~NvsNamespace();

    /// ESP_ERR_NVS_NOT_FOUND if the key does not exist.
    [[nodiscard]] Result<std::string> readBlob(const char* key) const;
    /// Written and committed; NVS replaces the old value atomically (power-loss safe).
    [[nodiscard]] Result<void> writeBlob(const char* key, const std::string& value);

private:
    explicit NvsNamespace(nvs_handle_t handle) : handle_(handle), open_(true) {}
    void close();

    nvs_handle_t handle_ = 0;
    bool open_ = false;
};

} // namespace banana::storage
