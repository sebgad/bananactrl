#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "banana/core/Result.hpp"

namespace banana::web {

/// Receives the regular files of a tar archive, in archive order.
class ITarSink {
public:
    [[nodiscard]] virtual Result<void> beginFile(std::string_view name, std::size_t size) = 0;
    [[nodiscard]] virtual Result<void> fileData(std::span<const char> data) = 0;
    [[nodiscard]] virtual Result<void> endFile() = 0;

protected:
    ~ITarSink() = default;
};

/// Streaming reader for ustar archives (`tar --format=ustar`, the web UI of a release): feed() takes the
/// download in pieces of any size, only one 512-byte header is buffered. Directories and other non-regular
/// entries are skipped; names are passed on unchecked (a leading "./" removed), so the sink validates them.
/// Pure logic (host-tested).
class TarReader {
public:
    static constexpr std::size_t kBlockBytes = 512;

    explicit TarReader(ITarSink& sink) : sink_(&sink) {}

    /// ESP_ERR_INVALID_RESPONSE for a corrupt header (checksum, size field), or the sink's error.
    [[nodiscard]] Result<void> feed(std::span<const char> data);
    /// ESP_ERR_INVALID_SIZE if the archive ended before its end-of-archive block.
    [[nodiscard]] Result<void> finish() const;

private:
    enum class State : std::uint8_t { Header, Data, Padding, End };

    [[nodiscard]] Result<void> parseHeader();

    ITarSink* sink_;
    State state_ = State::Header;
    std::array<char, kBlockBytes> header_{};
    std::size_t headerBytes_ = 0;
    std::size_t remaining_ = 0; ///< file bytes (Data) or padding bytes (Padding) still to come
    std::size_t padding_ = 0;   ///< padding after the current file
    bool regular_ = false;      ///< current entry goes to the sink
};

} // namespace banana::web
