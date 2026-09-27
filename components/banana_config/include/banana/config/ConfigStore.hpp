#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"

namespace banana::config {

/// Gets every saved configuration (settings page, MQTT, reset), in save order. Called with the store locked,
/// from the saving task: keep it short and do not call back into the store.
class IConfigListener {
public:
    IConfigListener() = default;
    IConfigListener(const IConfigListener&) = default;
    IConfigListener& operator=(const IConfigListener&) = default;
    IConfigListener(IConfigListener&&) = default;
    IConfigListener& operator=(IConfigListener&&) = default;
    virtual ~IConfigListener() = default;

    virtual void onConfigChanged(const Config& config) = 0;
};

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

    static constexpr std::size_t kMaxListeners = 4;

    /// `legacyFile`: params.json on LittleFS, imported if NVS holds no configuration yet.
    explicit ConfigStore(std::string legacyFile) : legacyFile_(std::move(legacyFile)) {}

    /// Notified after every successful save. ESP_ERR_NO_MEM if kMaxListeners are registered.
    [[nodiscard]] Result<void> addListener(IConfigListener& listener);

    /// Never fails: without a usable configuration the factory settings are returned (and saved).
    [[nodiscard]] Loaded load();
    [[nodiscard]] Result<void> save(const Config& config);
    /// Factory settings, saved.
    [[nodiscard]] Result<Config> reset();
    /// The configuration last loaded or saved (what the web UI shows and edits).
    [[nodiscard]] Config current() const;

    /// Read-modify-write under one lock, so concurrent changes (web UI, MQTT) cannot overwrite each other.
    /// `change(Config&)` returns false to leave everything unchanged (ESP_ERR_INVALID_ARG); otherwise the
    /// result is saved and returned.
    template <typename Change>
    [[nodiscard]] Result<Config> update(Change&& change)
    {
        const std::scoped_lock lock{mutex_};
        Config updated = current_;
        if (!std::forward<Change>(change)(updated)) {
            return fail(ESP_ERR_INVALID_ARG);
        }
        if (auto res = saveLocked(updated); !res) {
            return fail(res.error());
        }
        return updated;
    }

    [[nodiscard]] const std::string& legacyFile() const { return legacyFile_; }

private:
    [[nodiscard]] Result<void> saveLocked(const Config& config);

    std::string legacyFile_;
    mutable std::mutex mutex_;
    Config current_;
    std::array<IConfigListener*, kMaxListeners> listeners_{};
};

} // namespace banana::config
