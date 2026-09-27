#pragma once

#include <cstdint>
#include <mutex>
#include <string>

#include "banana/config/Config.hpp"
#include "banana/core/Result.hpp"

namespace banana::config {

/// params.json on LittleFS (loadConfiguration()/saveConfiguration()/resetConfiguration()).
/// Thread-safe: the web server saves while the boot code may still read. A std::mutex replaces
/// the bParamFileLocked flag, so a concurrent save waits instead of failing.
class ConfigStore {
public:
    enum class Source : std::uint8_t {
        File,          ///< all keys present
        FileCompleted, ///< keys were missing: defaults used and the file written back
        Defaults,      ///< no file or not readable: factory settings written
    };

    struct Loaded {
        Config config;
        Source source = Source::File;
        esp_err_t writeError = ESP_OK; ///< writing back failed (config is still usable)
    };

    explicit ConfigStore(std::string path) : path_(std::move(path)) {}

    /// Never fails: without a usable file the factory settings are returned (and saved).
    [[nodiscard]] Loaded load();
    /// Writes a temporary file and renames it, so a power loss never leaves a half-written params.json.
    [[nodiscard]] Result<void> save(const Config& config);
    /// Factory settings, saved.
    [[nodiscard]] Result<Config> reset();

    [[nodiscard]] const std::string& path() const { return path_; }

private:
    [[nodiscard]] Result<void> saveLocked(const Config& config);

    std::string path_;
    std::mutex mutex_;
};

} // namespace banana::config
