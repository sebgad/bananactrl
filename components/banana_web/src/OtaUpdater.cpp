#include "banana/web/OtaUpdater.hpp"

#include <utility>

#include "esp_log.h"

namespace banana::web {
namespace {

constexpr const char* kTag = "ota";

} // namespace

Result<OtaUpdater> OtaUpdater::begin(const Md5& expected, std::size_t imageSize)
{
    const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
    if (partition == nullptr) {
        return fail(ESP_ERR_NOT_FOUND);
    }
    if (imageSize == 0 || imageSize > partition->size) {
        return fail(ESP_ERR_INVALID_SIZE);
    }
    esp_ota_handle_t handle = 0;
    // Sequential writes: each sector is erased when it is reached, instead of the whole partition up front
    if (auto res = toResult(esp_ota_begin(partition, OTA_WITH_SEQUENTIAL_WRITES, &handle)); !res) {
        return fail(res.error());
    }
    ESP_LOGI(kTag, "writing %zu bytes to %s at 0x%lx", imageSize, partition->label,
             static_cast<unsigned long>(partition->address));
    return OtaUpdater{handle, partition, expected};
}

OtaUpdater::OtaUpdater(esp_ota_handle_t handle, const esp_partition_t* partition, const Md5& expected)
    : handle_(handle), partition_(partition), expected_(expected), active_(true)
{
    esp_rom_md5_init(&md5_);
}

OtaUpdater::OtaUpdater(OtaUpdater&& other) noexcept
    : handle_(other.handle_), partition_(other.partition_), expected_(other.expected_), md5_(other.md5_),
      active_(std::exchange(other.active_, false))
{
}

OtaUpdater& OtaUpdater::operator=(OtaUpdater&& other) noexcept
{
    if (this != &other) {
        abort();
        handle_ = other.handle_;
        partition_ = other.partition_;
        expected_ = other.expected_;
        md5_ = other.md5_;
        active_ = std::exchange(other.active_, false);
    }
    return *this;
}

OtaUpdater::~OtaUpdater()
{
    abort();
}

Result<void> OtaUpdater::write(std::span<const char> data)
{
    if (!active_) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    esp_rom_md5_update(&md5_, data.data(), static_cast<std::uint32_t>(data.size()));
    return toResult(esp_ota_write(handle_, data.data(), data.size()));
}

Result<void> OtaUpdater::finish()
{
    if (!active_) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    Md5 actual{};
    esp_rom_md5_final(actual.data(), &md5_);
    if (actual != expected_) {
        ESP_LOGE(kTag, "MD5 mismatch");
        abort();
        return fail(ESP_ERR_INVALID_CRC);
    }
    active_ = false; // esp_ota_end() releases the handle, also on failure
    if (auto res = toResult(esp_ota_end(handle_)); !res) {
        ESP_LOGE(kTag, "image invalid: %s", esp_err_to_name(res.error()));
        return res;
    }
    return toResult(esp_ota_set_boot_partition(partition_));
}

void OtaUpdater::markRunningAppValid()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            ESP_LOGI(kTag, "new firmware in %s confirmed, rollback cancelled", running->label);
        }
    }
}

void OtaUpdater::abort()
{
    if (active_) {
        esp_ota_abort(handle_);
        active_ = false;
    }
}

} // namespace banana::web
