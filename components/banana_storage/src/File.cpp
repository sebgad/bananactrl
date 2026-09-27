#include "banana/storage/File.hpp"

#include <array>
#include <cerrno>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

namespace banana::storage {
namespace {

esp_err_t fromErrno()
{
    return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
}

} // namespace

Result<File> File::open(const std::string& path, const char* mode)
{
    std::FILE* file = std::fopen(path.c_str(), mode); // NOLINT(cppcoreguidelines-owning-memory): File owns it
    if (file == nullptr) {
        return fail(fromErrno());
    }
    return File{file};
}

File::File(File&& other) noexcept : file_(std::exchange(other.file_, nullptr))
{
}

File& File::operator=(File&& other) noexcept
{
    if (this != &other) {
        close();
        file_ = std::exchange(other.file_, nullptr);
    }
    return *this;
}

File::~File()
{
    close();
}

Result<void> File::write(std::string_view data)
{
    if (std::fwrite(data.data(), 1, data.size(), file_) != data.size()) {
        return fail(ESP_FAIL);
    }
    return {};
}

Result<void> File::flush()
{
    // fflush() only hands the data to the VFS; LittleFS commits it on fsync() (or close).
    if (std::fflush(file_) != 0 || ::fsync(::fileno(file_)) != 0) {
        return fail(ESP_FAIL);
    }
    return {};
}

Result<std::string> File::readAll()
{
    std::string content;
    std::array<char, 256> chunk{};
    std::size_t read = 0;
    while ((read = std::fread(chunk.data(), 1, chunk.size(), file_)) > 0) {
        content.append(chunk.data(), read);
    }
    if (std::ferror(file_) != 0) {
        return fail(ESP_FAIL);
    }
    return content;
}

Result<std::size_t> File::read(std::span<char> buffer)
{
    const std::size_t count = std::fread(buffer.data(), 1, buffer.size(), file_);
    if (count < buffer.size() && std::ferror(file_) != 0) {
        return fail(ESP_FAIL);
    }
    return count;
}

long File::size()
{
    const long position = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    const long end = std::ftell(file_);
    std::fseek(file_, position, SEEK_SET);
    return end;
}

void File::close()
{
    if (file_ != nullptr) {
        std::fclose(file_); // NOLINT(cppcoreguidelines-owning-memory): File owns it
        file_ = nullptr;
    }
}

bool exists(const std::string& path)
{
    struct stat info{};
    return ::stat(path.c_str(), &info) == 0;
}

Result<void> remove(const std::string& path)
{
    if (std::remove(path.c_str()) != 0 && errno != ENOENT) {
        return fail(ESP_FAIL);
    }
    return {};
}

Result<void> rename(const std::string& from, const std::string& to)
{
    // LittleFS replaces an existing `to` atomically (fails with EBUSY while either file is open).
    if (std::rename(from.c_str(), to.c_str()) != 0) {
        return fail(fromErrno());
    }
    return {};
}

} // namespace banana::storage
