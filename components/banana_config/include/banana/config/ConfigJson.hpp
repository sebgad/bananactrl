#pragma once

#include <string>
#include <string_view>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"

/// Config ⇄ params.json with the key names of the Arduino firmware (including the `HighTresholdValue`
/// typo), so existing files and settings.html keep working.
namespace banana::config {

struct ParsedConfig {
    Config config;
    /// False if keys were missing (their base values were used): write the file back.
    bool complete = true;
};

/// Pretty-printed JSON with all keys.
[[nodiscard]] std::string toJson(const Config& config);

/// Reads every known key; missing keys keep the value from `base` (defaults when loading the file,
/// the current configuration for a partial update). Values are checked with isNull(), so stored
/// `false`/`0` are kept (the Arduino firmware treated them as missing and restored the defaults).
[[nodiscard]] Result<ParsedConfig> fromJson(std::string_view json, const Config& base = {});

/// params.json as served to the web UI: like toJson(), but the Wi-Fi password is left empty.
[[nodiscard]] std::string toPublicJson(const Config& config);

/// A settings update from the web UI (/paramUpdate), merged into `current`. The page gets no password
/// (toPublicJson()), so an empty password keeps the stored one.
[[nodiscard]] Result<Config> applyWebUpdate(std::string_view json, const Config& current);

} // namespace banana::config
