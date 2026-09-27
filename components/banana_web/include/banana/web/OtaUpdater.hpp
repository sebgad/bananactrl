#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "esp_ota_ops.h"
#include "esp_rom_md5.h"

#include "banana/core/Result.hpp"
#include "banana/web/WebPaths.hpp"

namespace banana::web {

/// Writes a firmware image into the next OTA partition (Update.begin/write/end of the Arduino firmware).
/// If finish() is not reached, the destructor aborts the update: the running image stays the boot image.
class OtaUpdater {
public:
    /// Fails if `imageSize` does not fit into the partition.
    [[nodiscard]] static Result<OtaUpdater> begin(const Md5& expected, std::size_t imageSize);

    OtaUpdater(const OtaUpdater&) = delete;
    OtaUpdater& operator=(const OtaUpdater&) = delete;
    OtaUpdater(OtaUpdater&& other) noexcept;
    OtaUpdater& operator=(OtaUpdater&& other) noexcept;
    ~OtaUpdater();

    [[nodiscard]] Result<void> write(std::span<const char> data);
    /// Checks the MD5 (ESP_ERR_INVALID_CRC on mismatch) and the image, then selects it for the next boot.
    [[nodiscard]] Result<void> finish();

    [[nodiscard]] const esp_partition_t* partition() const { return partition_; }

    /// Confirms the running image after an OTA update (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE): without it,
    /// the bootloader returns to the previous image at the next reset.
    static void markRunningAppValid();

private:
    OtaUpdater(esp_ota_handle_t handle, const esp_partition_t* partition, const Md5& expected);
    void abort();

    esp_ota_handle_t handle_ = 0;
    const esp_partition_t* partition_ = nullptr;
    Md5 expected_{};
    md5_context_t md5_{};
    bool active_ = false;
};

} // namespace banana::web
