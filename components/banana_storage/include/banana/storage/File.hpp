#pragma once

#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>

#include "banana/core/Result.hpp"

namespace banana::storage {

/// RAII around a stdio FILE* (LittleFS is mounted into the VFS, so paths look like "/fs/params.json").
class File {
public:
    /// `mode` as for fopen(): "r", "w", "a", ...
    [[nodiscard]] static Result<File> open(const std::string& path, const char* mode);

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept;
    File& operator=(File&& other) noexcept;
    ~File();

    [[nodiscard]] Result<void> write(std::string_view data);
    /// Buffers to flash and commits (fsync): the data survives a reset.
    [[nodiscard]] Result<void> flush();
    [[nodiscard]] Result<std::string> readAll();
    /// Up to `buffer.size()` bytes; 0 at the end of the file.
    [[nodiscard]] Result<std::size_t> read(std::span<char> buffer);
    [[nodiscard]] long size();

private:
    explicit File(std::FILE* file) : file_(file) {}
    void close();

    std::FILE* file_ = nullptr;
};

[[nodiscard]] bool exists(const std::string& path);
/// Missing files are not an error.
[[nodiscard]] Result<void> remove(const std::string& path);
/// Atomically replaces `to` if it exists. Fails while either file is open.
[[nodiscard]] Result<void> rename(const std::string& from, const std::string& to);

} // namespace banana::storage
