#pragma once

#include "banana/core/Result.hpp"

namespace banana::storage {

/// Default NVS partition (RAII). Needed by the Wi-Fi driver for its calibration data.
class Nvs {
public:
    /// Erases and re-initialises the partition if it is full or from a newer NVS version.
    [[nodiscard]] static Result<Nvs> init();

    Nvs(const Nvs&) = delete;
    Nvs& operator=(const Nvs&) = delete;
    Nvs(Nvs&& other) noexcept;
    Nvs& operator=(Nvs&& other) noexcept;
    ~Nvs();

private:
    Nvs() : owned_(true) {}
    void release();

    bool owned_ = false;
};

} // namespace banana::storage
