#include "banana/web/TarReader.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

namespace banana::web {
namespace {

// ustar header fields: offset and length
constexpr std::size_t kNameOffset = 0;
constexpr std::size_t kNameLength = 100;
constexpr std::size_t kSizeOffset = 124;
constexpr std::size_t kSizeLength = 12;
constexpr std::size_t kChecksumOffset = 148;
constexpr std::size_t kChecksumLength = 8;
constexpr std::size_t kTypeOffset = 156;
constexpr std::size_t kMagicOffset = 257;
constexpr std::string_view kMagic = "ustar"; // POSIX "ustar\0" and GNU "ustar " both start with it
constexpr std::size_t kPrefixOffset = 345;
constexpr std::size_t kPrefixLength = 155;

using Block = std::array<char, TarReader::kBlockBytes>;

/// NUL-terminated (or full-length) text field.
std::string_view textField(const Block& block, std::size_t offset, std::size_t length)
{
    const std::string_view field{&block.at(offset), length};
    return field.substr(0, field.find('\0'));
}

/// Octal number, optionally padded with spaces and terminated by NUL or space.
std::optional<std::size_t> octalField(const Block& block, std::size_t offset, std::size_t length)
{
    std::string_view field = textField(block, offset, length);
    while (!field.empty() && field.front() == ' ') {
        field.remove_prefix(1);
    }
    while (!field.empty() && field.back() == ' ') {
        field.remove_suffix(1);
    }
    if (field.empty()) {
        return std::nullopt;
    }
    std::size_t value = 0;
    for (const char c : field) {
        if (c < '0' || c > '7') {
            return std::nullopt; // also the GNU base-256 encoding for files >= 8 GB
        }
        value = (value * 8) + static_cast<std::size_t>(c - '0');
    }
    return value;
}

/// Unsigned sum of all header bytes, the checksum field counted as spaces.
std::size_t checksumOf(const Block& block)
{
    std::size_t sum = 0;
    for (std::size_t i = 0; i < block.size(); ++i) {
        const bool inField = i >= kChecksumOffset && i < kChecksumOffset + kChecksumLength;
        sum += inField ? static_cast<unsigned char>(' ') : static_cast<unsigned char>(block.at(i));
    }
    return sum;
}

} // namespace

Result<void> TarReader::feed(std::span<const char> data)
{
    while (!data.empty()) {
        switch (state_) {
        case State::Header: {
            const std::size_t count = std::min(data.size(), header_.size() - headerBytes_);
            std::ranges::copy(data.first(count), std::span{header_}.subspan(headerBytes_).begin());
            headerBytes_ += count;
            data = data.subspan(count);
            if (headerBytes_ == header_.size()) {
                headerBytes_ = 0;
                if (auto res = parseHeader(); !res) {
                    return res;
                }
            }
            break;
        }
        case State::Data: {
            const std::size_t count = std::min(data.size(), remaining_);
            if (regular_) {
                if (auto res = sink_->fileData(data.first(count)); !res) {
                    return res;
                }
            }
            remaining_ -= count;
            data = data.subspan(count);
            if (remaining_ == 0) {
                if (regular_) {
                    if (auto res = sink_->endFile(); !res) {
                        return res;
                    }
                }
                remaining_ = padding_;
                state_ = remaining_ == 0 ? State::Header : State::Padding;
            }
            break;
        }
        case State::Padding: {
            const std::size_t count = std::min(data.size(), remaining_);
            remaining_ -= count;
            data = data.subspan(count);
            if (remaining_ == 0) {
                state_ = State::Header;
            }
            break;
        }
        case State::End:
            return {}; // the rest is the second zero block and the record padding
        }
    }
    return {};
}

Result<void> TarReader::finish() const
{
    return state_ == State::End ? Result<void>{} : fail(ESP_ERR_INVALID_SIZE);
}

Result<void> TarReader::parseHeader()
{
    if (std::ranges::all_of(header_, [](char c) { return c == '\0'; })) {
        state_ = State::End;
        return {};
    }
    const auto checksum = octalField(header_, kChecksumOffset, kChecksumLength);
    const auto size = octalField(header_, kSizeOffset, kSizeLength);
    if (!checksum || *checksum != checksumOf(header_) || !size ||
        !textField(header_, kMagicOffset, kMagic.size() + 1).starts_with(kMagic)) {
        return fail(ESP_ERR_INVALID_RESPONSE);
    }

    const char type = header_.at(kTypeOffset);
    regular_ = type == '0' || type == '\0';
    remaining_ = *size;
    padding_ = (kBlockBytes - (*size % kBlockBytes)) % kBlockBytes;
    if (regular_) {
        const std::string_view prefix = textField(header_, kPrefixOffset, kPrefixLength);
        std::string name{textField(header_, kNameOffset, kNameLength)};
        if (!prefix.empty()) {
            name = std::string{prefix} + "/" + name;
        }
        if (name.starts_with("./")) {
            name.erase(0, 2);
        }
        if (auto res = sink_->beginFile(name, *size); !res) {
            return res;
        }
        if (*size == 0) {
            if (auto res = sink_->endFile(); !res) {
                return res;
            }
        }
    }
    if (remaining_ > 0) {
        state_ = State::Data;
    } else {
        remaining_ = padding_; // 0: no data, no padding
        state_ = State::Header;
    }
    return {};
}

} // namespace banana::web
