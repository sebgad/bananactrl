#include "banana/storage/LittleFs.hpp"

#include <utility>

#include "esp_littlefs.h"

namespace banana::storage {

Result<LittleFs> LittleFs::mount(const Config& config)
{
    LittleFs fs{std::string{config.mountPoint}, std::string{config.partitionLabel}};

    esp_vfs_littlefs_conf_t conf{}; // littlefs copies both strings
    conf.base_path = fs.mountPoint_.c_str();
    conf.partition_label = fs.partitionLabel_.c_str();
    conf.format_if_mount_failed = config.formatIfMountFailed ? 1 : 0;

    if (auto res = toResult(esp_vfs_littlefs_register(&conf)); !res) {
        fs.partitionLabel_.clear(); // not mounted, nothing to release
        return fail(res.error());
    }
    return fs;
}

LittleFs::LittleFs(LittleFs&& other) noexcept
    : mountPoint_(std::exchange(other.mountPoint_, {})),
      partitionLabel_(std::exchange(other.partitionLabel_, {}))
{
}

LittleFs& LittleFs::operator=(LittleFs&& other) noexcept
{
    if (this != &other) {
        unmount();
        mountPoint_ = std::exchange(other.mountPoint_, {});
        partitionLabel_ = std::exchange(other.partitionLabel_, {});
    }
    return *this;
}

LittleFs::~LittleFs()
{
    unmount();
}

Result<LittleFs::Info> LittleFs::info() const
{
    Info info;
    if (auto res = toResult(esp_littlefs_info(partitionLabel_.c_str(), &info.totalBytes, &info.usedBytes));
        !res) {
        return fail(res.error());
    }
    return info;
}

void LittleFs::unmount()
{
    if (!partitionLabel_.empty()) {
        esp_vfs_littlefs_unregister(partitionLabel_.c_str());
        partitionLabel_.clear();
    }
}

} // namespace banana::storage
