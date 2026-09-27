#include "banana/storage/NvsNamespace.hpp"

#include <utility>

namespace banana::storage {

Result<NvsNamespace> NvsNamespace::open(const char* name)
{
    nvs_handle_t handle = 0;
    if (auto res = toResult(nvs_open(name, NVS_READWRITE, &handle)); !res) {
        return fail(res.error());
    }
    return NvsNamespace{handle};
}

NvsNamespace::NvsNamespace(NvsNamespace&& other) noexcept
    : handle_(other.handle_), open_(std::exchange(other.open_, false))
{
}

NvsNamespace& NvsNamespace::operator=(NvsNamespace&& other) noexcept
{
    if (this != &other) {
        close();
        handle_ = other.handle_;
        open_ = std::exchange(other.open_, false);
    }
    return *this;
}

NvsNamespace::~NvsNamespace()
{
    close();
}

Result<std::string> NvsNamespace::readBlob(const char* key) const
{
    std::size_t size = 0;
    if (auto res = toResult(nvs_get_blob(handle_, key, nullptr, &size)); !res) {
        return fail(res.error());
    }
    std::string value(size, '\0');
    if (auto res = toResult(nvs_get_blob(handle_, key, value.data(), &size)); !res) {
        return fail(res.error());
    }
    value.resize(size);
    return value;
}

// NOLINTNEXTLINE(readability-make-member-function-const): changes the stored data
Result<void> NvsNamespace::writeBlob(const char* key, const std::string& value)
{
    if (auto res = toResult(nvs_set_blob(handle_, key, value.data(), value.size())); !res) {
        return res;
    }
    return toResult(nvs_commit(handle_));
}

void NvsNamespace::close()
{
    if (open_) {
        nvs_close(handle_);
        open_ = false;
    }
}

} // namespace banana::storage
