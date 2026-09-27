#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "banana/core/Result.hpp"

namespace banana::storage {

/// Mounted LittleFS partition (RAII: unmounted in the destructor).
class LittleFs {
public:
    struct Config {
        std::string_view mountPoint = "/fs";
        std::string_view partitionLabel = "storage";
        bool formatIfMountFailed = true;
    };

    struct Info {
        std::size_t totalBytes = 0;
        std::size_t usedBytes = 0;
    };

    [[nodiscard]] static Result<LittleFs> mount(const Config& config);

    LittleFs(const LittleFs&) = delete;
    LittleFs& operator=(const LittleFs&) = delete;
    LittleFs(LittleFs&& other) noexcept;
    LittleFs& operator=(LittleFs&& other) noexcept;
    ~LittleFs();

    [[nodiscard]] Result<Info> info() const;
    [[nodiscard]] std::string_view mountPoint() const { return mountPoint_; }

private:
    LittleFs(std::string mountPoint, std::string partitionLabel)
        : mountPoint_(std::move(mountPoint)), partitionLabel_(std::move(partitionLabel))
    {
    }
    void unmount();

    std::string mountPoint_;
    std::string partitionLabel_; ///< empty = nothing mounted (moved-from)
};

} // namespace banana::storage
