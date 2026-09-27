#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

/// Pure helpers for the web routes (host-tested): which URI serves which file, MIME types, upload names.
namespace banana::web {

struct StaticFile {
    std::string_view uri;
    std::string_view file; ///< relative to the LittleFS mount point; `.gz` = sent with Content-Encoding gzip
};

/// The files the Arduino firmware served (only these: params.json has its own route without the password).
inline constexpr std::array kStaticFiles{
    StaticFile{.uri = "/", .file = "index.html"},
    StaticFile{.uri = "/index.html", .file = "index.html"},
    StaticFile{.uri = "/graphs.html", .file = "graphs.html"},
    StaticFile{.uri = "/settings.html", .file = "settings.html"},
    StaticFile{.uri = "/ota.html", .file = "ota.html"},
    StaticFile{.uri = "/log.html", .file = "log.html"},
    StaticFile{.uri = "/style.css", .file = "style.css"},
    StaticFile{.uri = "/favicon-16x16.png", .file = "favicon-16x16.png"},
    StaticFile{.uri = "/favicon-32x32.png", .file = "favicon-32x32.png"},
    StaticFile{.uri = "/apple-touch-icon.png", .file = "apple-touch-icon.png"},
    StaticFile{.uri = "/uPlot.min.js", .file = "uPlot.min.js.gz"}, // bundled: graphs work without internet
    StaticFile{.uri = "/uPlot.min.css", .file = "uPlot.min.css"},
    StaticFile{.uri = "/data.csv", .file = "data.csv"},
    StaticFile{.uri = "/recentlogfile.txt", .file = "logfile_recent.txt"},
    StaticFile{.uri = "/lastlogfile.txt", .file = "logfile_last.txt"},
};

[[nodiscard]] constexpr bool isGzipped(std::string_view file)
{
    return file.ends_with(".gz");
}

/// File name without a `.gz` suffix (determines the content type).
[[nodiscard]] constexpr std::string_view contentName(std::string_view file)
{
    return isGzipped(file) ? file.substr(0, file.size() - 3) : file;
}

/// Files written by the firmware itself: an upload would bypass the running configuration or collide
/// with the open recorder/logger.
inline constexpr std::array<std::string_view, 4> kRuntimeFiles{"params.json", "data.csv",
                                                               "logfile_recent.txt", "logfile_last.txt"};

/// URI without query string ("/data.csv?x=1" -> "/data.csv").
[[nodiscard]] constexpr std::string_view pathOf(std::string_view uri)
{
    return uri.substr(0, uri.find('?'));
}

[[nodiscard]] constexpr std::optional<std::string_view> staticFileFor(std::string_view uri)
{
    const std::string_view path = pathOf(uri);
    for (const StaticFile& entry : kStaticFiles) {
        if (entry.uri == path) {
            return entry.file;
        }
    }
    return std::nullopt;
}

[[nodiscard]] constexpr const char* contentType(std::string_view file)
{
    const auto endsWith = [file](std::string_view suffix) {
        return file.ends_with(suffix);
    };
    if (endsWith(".html")) {
        return "text/html";
    }
    if (endsWith(".js")) {
        return "text/javascript";
    }
    if (endsWith(".css")) {
        return "text/css";
    }
    if (endsWith(".png")) {
        return "image/png";
    }
    if (endsWith(".json")) {
        return "application/json";
    }
    if (endsWith(".csv") || endsWith(".txt")) {
        return "text/plain"; // as the Arduino firmware; graphs.html reads responseText
    }
    return "application/octet-stream";
}

/// Upload target in the LittleFS root: 1..32 characters of [A-Za-z0-9._-], no leading dot
/// (no paths, no "..", no hidden/temporary files), and not one of kRuntimeFiles.
[[nodiscard]] constexpr bool isValidUploadName(std::string_view name)
{
    if (name.empty() || name.size() > 32 || name.front() == '.') {
        return false;
    }
    for (const char c : name) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                             c == '.' || c == '_' || c == '-';
        if (!allowed) {
            return false;
        }
    }
    return std::ranges::find(kRuntimeFiles, name) == kRuntimeFiles.end();
}

using Md5 = std::array<std::uint8_t, 16>;

/// 32 hex digits (either case), surrounding whitespace ignored.
[[nodiscard]] constexpr std::optional<Md5> parseMd5(std::string_view hex)
{
    while (!hex.empty() && (hex.front() == ' ' || hex.front() == '\t')) {
        hex.remove_prefix(1);
    }
    while (!hex.empty() &&
           (hex.back() == ' ' || hex.back() == '\t' || hex.back() == '\r' || hex.back() == '\n')) {
        hex.remove_suffix(1);
    }
    if (hex.size() != 32) {
        return std::nullopt;
    }
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        return -1;
    };
    Md5 md5{};
    for (std::size_t i = 0; i < md5.size(); ++i) {
        const int high = nibble(hex[2 * i]);
        const int low = nibble(hex[(2 * i) + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        md5.at(i) = static_cast<std::uint8_t>((high << 4) | low);
    }
    return md5;
}

} // namespace banana::web
