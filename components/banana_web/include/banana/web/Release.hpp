#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "banana/core/Result.hpp"
#include "banana/web/WebPaths.hpp"

/// Pure helpers for updates from GitHub releases (host-tested): versions, the releases API response,
/// MD5SUMS.
namespace banana::web {

/// Semantic version as CMakeLists.txt derives it from `git describe`: "1.2.0", "1.3.0-rc.1", plus the
/// suffixes of untagged builds ("-3-gabc1234", "-gabc1234", "-dirty").
struct Version {
    unsigned major = 0;
    unsigned minor = 0;
    unsigned patch = 0;
    std::string prerelease; ///< "rc.1"; empty for a release
    bool derived = false;   ///< built from commits after the tag (or with local changes)
};

[[nodiscard]] std::optional<Version> parseVersion(std::string_view text);

/// SemVer precedence ("1.0.0-rc.1" < "1.0.0" < "1.0.1"); a derived build is newer than its tag.
[[nodiscard]] std::strong_ordering compare(const Version& a, const Version& b);

/// True if `candidate` is a newer version than `current`. An unparsable `current` counts as older.
[[nodiscard]] bool isNewer(std::string_view candidate, std::string_view current);

struct ReleaseAsset {
    std::string name; ///< as listed in MD5SUMS
    std::string
        url; ///< https://github.com/<repository>/releases/download/<tag>/<name> (redirects to the CDN)
    std::size_t size = 0;
};

/// The parts of a GitHub release the updater needs.
struct Release {
    std::string version; ///< tag without the leading "v"
    std::string page;    ///< release page with the notes
    bool prerelease = false;
    std::optional<ReleaseAsset> firmware; ///< bananactrl-<version>.bin
    std::optional<ReleaseAsset> webUi;    ///< webui-<version>.tar; older releases have none
    std::optional<ReleaseAsset> md5sums;  ///< MD5SUMS
};

/// Byte source for parseRelease(): the HTTP response is parsed while it streams in (a release with notes
/// and eight assets is ~10 kB of JSON; only tag, flags, asset names and sizes are kept). Same interface as
/// the Arduino Stream that ArduinoJson reads from.
class IByteReader {
public:
    /// Next byte, or -1 at the end.
    virtual int read() = 0;
    virtual std::size_t readBytes(char* buffer, std::size_t length) = 0;

protected:
    ~IByteReader() = default;
};

/// Picks the newest release from the response of GET /repos/<repository>/releases (newest first): the first
/// entry that is no draft and, unless `includePrereleases`, no pre-release. ESP_ERR_NOT_FOUND if there is
/// none, ESP_ERR_INVALID_RESPONSE if it is not a firmware release (tag not a version, no firmware image or
/// MD5SUMS) or the response is no release list.
[[nodiscard]] Result<Release> parseRelease(IByteReader& json, std::string_view repository,
                                           bool includePrereleases);

enum class UpdateState : std::uint8_t {
    Idle,       ///< nothing checked since boot
    Checking,   ///< asking GitHub for the newest release
    UpToDate,   ///< the running firmware is the newest release (or newer)
    Available,  ///< a newer release can be installed
    Installing, ///< downloading and writing
    Restarting, ///< installed, restart pending
    Failed,     ///< last check or installation failed (see `error`)
};

struct UpdateStatus {
    UpdateState state = UpdateState::Idle;
    std::string current;            ///< running firmware version
    std::optional<Release> release; ///< result of the last successful check
    std::size_t done = 0;           ///< bytes downloaded (Installing)
    std::size_t total = 0;          ///< bytes to download (Installing)
    std::string error;              ///< Failed: what went wrong
};

/// /update.json for ota.html: {"State", "Current", "Latest", "Prerelease", "Page", "WebUi", "Done", "Total",
/// "Error"}; the release keys are missing before the first successful check.
[[nodiscard]] std::string updateStatusJson(const UpdateStatus& status);

/// The checksum of `fileName` in an `md5sum` output ("<hex>  <name>" per line, "*" before binary names).
[[nodiscard]] std::optional<Md5> md5For(std::string_view md5sums, std::string_view fileName);

} // namespace banana::web
