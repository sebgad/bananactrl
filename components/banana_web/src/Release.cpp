#include "banana/web/Release.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <memory>
#include <string_view>
#include <tuple>

#include <ArduinoJson.h>

namespace banana::web {
namespace {

constexpr std::string_view kMd5Sums = "MD5SUMS";

[[nodiscard]] bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

[[nodiscard]] bool isHexDigit(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'f');
}

[[nodiscard]] bool allOf(std::string_view text, bool (*predicate)(char))
{
    return !text.empty() && std::ranges::all_of(text, predicate);
}

/// Removes a `git describe` suffix ("-dirty", "-3-gabc1234", "-gabc1234"); true if there was one.
bool stripDescribeSuffix(std::string_view& text)
{
    bool derived = false;
    if (text.ends_with("-dirty")) {
        text.remove_suffix(6);
        derived = true;
    }
    const auto dash = text.rfind('-');
    if (dash == std::string_view::npos || dash + 2 >= text.size() || text[dash + 1] != 'g' ||
        !allOf(text.substr(dash + 2), isHexDigit)) {
        return derived;
    }
    text = text.substr(0, dash); // "-g<hash>"
    const auto countDash = text.rfind('-');
    if (countDash != std::string_view::npos && allOf(text.substr(countDash + 1), isDigit)) {
        text = text.substr(0, countDash); // "-<commits>" before it
    }
    return true;
}

/// Leading decimal number, consumed from `text`.
std::optional<unsigned> takeNumber(std::string_view& text)
{
    unsigned value = 0;
    const auto [end, error] =
        std::from_chars(std::to_address(text.begin()), std::to_address(text.end()), value);
    if (error != std::errc{} || end == text.data()) {
        return std::nullopt;
    }
    text.remove_prefix(static_cast<std::size_t>(end - text.data()));
    return value;
}

/// SemVer 11.4: identifiers from left to right, numeric ones numerically and below alphanumeric ones; a
/// shorter list that is a prefix of the longer one is lower.
std::strong_ordering comparePrerelease(std::string_view a, std::string_view b)
{
    while (!a.empty() && !b.empty()) {
        const std::string_view idA = a.substr(0, a.find('.'));
        const std::string_view idB = b.substr(0, b.find('.'));
        const bool numericA = allOf(idA, isDigit);
        const bool numericB = allOf(idB, isDigit);
        std::strong_ordering order = std::strong_ordering::equal;
        if (numericA && numericB) {
            order = idA.size() != idB.size() ? idA.size() <=> idB.size() : idA <=> idB;
        } else if (numericA != numericB) {
            order = numericA ? std::strong_ordering::less : std::strong_ordering::greater;
        } else {
            order = idA <=> idB;
        }
        if (order != std::strong_ordering::equal) {
            return order;
        }
        a.remove_prefix(std::min(a.size(), idA.size() + 1));
        b.remove_prefix(std::min(b.size(), idB.size() + 1));
    }
    return a.size() <=> b.size();
}

std::optional<ReleaseAsset> assetNamed(JsonArrayConst assets, std::string_view name,
                                       const std::string& downloadBase)
{
    for (JsonObjectConst asset : assets) {
        if (asset["name"].as<std::string_view>() == name) {
            return ReleaseAsset{.name = std::string{name},
                                .url = downloadBase + std::string{name},
                                .size = asset["size"].as<std::size_t>()};
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<Version> parseVersion(std::string_view text)
{
    if (text.starts_with('v')) {
        text.remove_prefix(1);
    }
    text = text.substr(0, text.find('+')); // build metadata has no precedence
    Version version;
    version.derived = stripDescribeSuffix(text);

    const auto major = takeNumber(text);
    if (!major || !text.starts_with('.')) {
        return std::nullopt;
    }
    text.remove_prefix(1);
    const auto minor = takeNumber(text);
    if (!minor || !text.starts_with('.')) {
        return std::nullopt;
    }
    text.remove_prefix(1);
    const auto patch = takeNumber(text);
    if (!patch) {
        return std::nullopt;
    }
    if (text.starts_with('-') && text.size() > 1) {
        version.prerelease = text.substr(1);
    } else if (!text.empty()) {
        return std::nullopt;
    }
    version.major = *major;
    version.minor = *minor;
    version.patch = *patch;
    return version;
}

std::strong_ordering compare(const Version& a, const Version& b)
{
    if (auto order = std::tie(a.major, a.minor, a.patch) <=> std::tie(b.major, b.minor, b.patch);
        order != std::strong_ordering::equal) {
        return order;
    }
    if (a.prerelease.empty() != b.prerelease.empty()) {
        return a.prerelease.empty() ? std::strong_ordering::greater : std::strong_ordering::less;
    }
    if (auto order = comparePrerelease(a.prerelease, b.prerelease); order != std::strong_ordering::equal) {
        return order;
    }
    return static_cast<int>(a.derived) <=> static_cast<int>(b.derived);
}

bool isNewer(std::string_view candidate, std::string_view current)
{
    const auto next = parseVersion(candidate);
    if (!next) {
        return false;
    }
    const auto running = parseVersion(current);
    return !running || compare(*next, *running) == std::strong_ordering::greater;
}

Result<Release> parseRelease(IByteReader& json, std::string_view repository, bool includePrereleases)
{
    JsonDocument filter;
    JsonObject fields = filter[0].to<JsonObject>(); // for every element of the list
    fields["tag_name"] = true;
    fields["prerelease"] = true;
    fields["draft"] = true;
    JsonObject assetFields = fields["assets"][0].to<JsonObject>();
    assetFields["name"] = true;
    assetFields["size"] = true;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(doc, json, DeserializationOption::Filter(filter));
    if (error) {
        return fail(error == DeserializationError::NoMemory ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_RESPONSE);
    }
    if (!doc.is<JsonArrayConst>()) {
        return fail(ESP_ERR_INVALID_RESPONSE); // e.g. {"message": "API rate limit exceeded ..."}
    }

    JsonObjectConst entry;
    for (JsonObjectConst candidate : doc.as<JsonArrayConst>()) {
        if (!candidate["draft"].as<bool>() && (includePrereleases || !candidate["prerelease"].as<bool>())) {
            entry = candidate;
            break;
        }
    }
    if (entry.isNull()) {
        return fail(ESP_ERR_NOT_FOUND);
    }

    const auto tag = entry["tag_name"].as<std::string>();
    if (!tag.starts_with('v') || !parseVersion(tag)) {
        return fail(ESP_ERR_INVALID_RESPONSE);
    }
    const std::string base = "https://github.com/" + std::string{repository} + "/releases/";
    const std::string version = tag.substr(1);
    const std::string downloadBase = base + "download/" + tag + "/";
    const auto assets = entry["assets"].as<JsonArrayConst>();
    Release release{.version = version,
                    .page = base + "tag/" + tag,
                    .prerelease = entry["prerelease"].as<bool>(),
                    .firmware = assetNamed(assets, "bananactrl-" + version + ".bin", downloadBase),
                    .webUi = assetNamed(assets, "webui-" + version + ".tar", downloadBase),
                    .md5sums = assetNamed(assets, kMd5Sums, downloadBase)};
    if (!release.firmware || !release.md5sums) {
        return fail(ESP_ERR_INVALID_RESPONSE);
    }
    return release;
}

std::string updateStatusJson(const UpdateStatus& status)
{
    static constexpr std::array kStateNames{"idle",       "checking",   "uptodate", "available",
                                            "installing", "restarting", "failed"};
    static_assert(kStateNames.size() == static_cast<std::size_t>(UpdateState::Failed) + 1);

    JsonDocument doc;
    doc["State"] = kStateNames.at(static_cast<std::size_t>(status.state));
    doc["Current"] = status.current;
    if (status.release) {
        doc["Latest"] = status.release->version;
        doc["Prerelease"] = status.release->prerelease;
        doc["Page"] = status.release->page;
        doc["WebUi"] = status.release->webUi.has_value();
    }
    doc["Done"] = status.done;
    doc["Total"] = status.total;
    doc["Error"] = status.error;

    std::string json;
    serializeJson(doc, json);
    return json;
}

std::optional<Md5> md5For(std::string_view md5sums, std::string_view fileName)
{
    while (!md5sums.empty()) {
        const auto end = md5sums.find('\n');
        std::string_view line = md5sums.substr(0, end);
        md5sums.remove_prefix(end == std::string_view::npos ? md5sums.size() : end + 1);
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        // "<32 hex><space><space or *><name>"
        if (line.size() < 35 || line[32] != ' ' || (line[33] != ' ' && line[33] != '*')) {
            continue;
        }
        if (line.substr(34) == fileName) {
            return parseMd5(line.substr(0, 32));
        }
    }
    return std::nullopt;
}

} // namespace banana::web
