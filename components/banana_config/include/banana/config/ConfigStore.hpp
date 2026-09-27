#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"

namespace banana::config {

/// The configuration as params.json-formatted JSON (Arduino key names) in NVS, namespace "banana", key
/// "params". NVS instead of LittleFS: a full `idf.py flash` rewrites the LittleFS image but not NVS, and NVS
/// replaces a value atomically. A `params.json` from the Arduino firmware (or an earlier build) is imported
/// once and renamed to `params.json.imported`.
///
/// Thread-safe: the web server saves while the boot code may still read.
class ConfigStore {
public:
    enum class Source : std::uint8_t {
        Stored,          ///< all keys present in NVS
        StoredCompleted, ///< keys were missing: defaults used and written back
        Imported,        ///< taken over from the legacy file
        Defaults,        ///< nothing stored: factory settings written
    };

    struct Loaded {
        Config config;
        Source source = Source::Stored;
        esp_err_t writeError = ESP_OK; ///< writing to NVS failed (config is still usable)
    };

    /// `legacyFile`: params.json on LittleFS, imported if NVS holds no configuration yet.
    explicit ConfigStore(std::string legacyFile) : legacyFile_(std::move(legacyFile)) {}

    /// Never fails: without a usable configuration the factory settings are returned (and saved).
    [[nodiscard]] Loaded load();
    [[nodiscard]] Result<void> save(const Config& config);
    /// Factory settings, saved.
    [[nodiscard]] Result<Config> reset();
    /// The configuration last loaded or saved (what the web UI shows and edits).
    [[nodiscard]] Config current() const;

    [[nodiscard]] const std::string& legacyFile() const { return legacyFile_; }

private:
    [[nodiscard]] Result<void> saveLocked(const Config& config);

    std::string legacyFile_;
    mutable std::mutex mutex_;
    Config current_;
};

} // namespace banana::config
