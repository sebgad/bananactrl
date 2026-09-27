#include "banana/storage/Nvs.hpp"

#include <utility>

#include "nvs_flash.h"

namespace banana::storage {

Result<Nvs> Nvs::init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (auto res = toResult(nvs_flash_erase()); !res) {
            return fail(res.error());
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return fail(err);
    }
    return Nvs{};
}

Nvs::Nvs(Nvs&& other) noexcept : owned_(std::exchange(other.owned_, false))
{
}

Nvs& Nvs::operator=(Nvs&& other) noexcept
{
    if (this != &other) {
        release();
        owned_ = std::exchange(other.owned_, false);
    }
    return *this;
}

Nvs::~Nvs()
{
    release();
}

void Nvs::release()
{
    if (owned_) {
        nvs_flash_deinit();
        owned_ = false;
    }
}

} // namespace banana::storage
