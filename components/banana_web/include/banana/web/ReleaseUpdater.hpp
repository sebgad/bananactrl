#pragma once

#include <mutex>
#include <string>

#include "banana/core/Result.hpp"
#include "banana/net/WifiManager.hpp"
#include "banana/rtos/Task.hpp"
#include "banana/web/Release.hpp"

namespace banana::web {

/// Updates from the project's GitHub releases. check() looks up the newest release, install() downloads it:
/// the web UI archive (if the release has one) into hidden files in LittleFS, the firmware into the next
/// OTA slot, both verified against the release's MD5SUMS. Only when everything arrived intact are the web
/// files renamed into place and the device restarted into the new firmware; any earlier failure leaves the
/// running firmware and pages untouched.
///
/// Both jobs run in a task of their own (a TLS connection plus a 1.2 MB download take up to a minute, too
/// long for an httpd handler); the page polls status(). The task is created on the first request and then
/// waits for further ones, so its stack is only used on devices that ever look for updates.
class ReleaseUpdater final : private rtos::Task {
public:
    struct Settings {
        std::string repository; ///< "owner/name" on GitHub
        std::string fsRoot;     ///< LittleFS mount point, e.g. "/fs"
        std::string version;    ///< running firmware version
    };

    /// `wifi` must outlive the updater.
    ReleaseUpdater(Settings settings, const net::WifiManager& wifi);

    ReleaseUpdater(const ReleaseUpdater&) = delete;
    ReleaseUpdater& operator=(const ReleaseUpdater&) = delete;
    ReleaseUpdater(ReleaseUpdater&&) = delete;
    ReleaseUpdater& operator=(ReleaseUpdater&&) = delete;
    ~ReleaseUpdater() = default;

    /// Starts a check in the background. ESP_ERR_INVALID_STATE while a job runs, ESP_ERR_NOT_SUPPORTED
    /// without a station connection (SoftAP: no internet).
    [[nodiscard]] Result<void> check(bool includePrereleases);
    /// Installs the release found by the last check (also the running version again). As check(), plus
    /// ESP_ERR_NOT_FOUND if no check has succeeded yet.
    [[nodiscard]] Result<void> install();

    [[nodiscard]] UpdateStatus status() const;

private:
    enum class Job : std::uint8_t { None, Check, Install };

    void run() override;
    [[nodiscard]] Result<void> request(Job job, UpdateState state);
    [[nodiscard]] Result<Release> fetchRelease(bool includePrereleases) const;
    /// Returns only on failure: success restarts the chip.
    [[nodiscard]] Result<void> installRelease(const Release& release);
    void addProgress(std::size_t bytes);
    void finish(UpdateState state, std::string error = {});

    const Settings settings_;
    const std::string userAgent_; ///< GitHub's API requires one
    const net::WifiManager* wifi_;

    mutable std::mutex mutex_; ///< guards everything below (httpd task vs. updater task)
    UpdateStatus status_;
    Job job_ = Job::None;
    bool includePrereleases_ = false;
};

} // namespace banana::web
