#include "banana/web/ReleaseUpdater.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_md5.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "banana/net/HttpClient.hpp"
#include "banana/rtos/Restart.hpp"
#include "banana/storage/File.hpp"
#include "banana/web/OtaUpdater.hpp"
#include "banana/web/TarReader.hpp"
#include "banana/web/WebPaths.hpp"

namespace banana::web {
namespace {

constexpr const char* kTag = "update";
constexpr std::uint32_t kStackBytes = 8192; // TLS handshake with certificate bundle verification
constexpr UBaseType_t kPriority = 3;        // below heater and httpd
constexpr std::size_t kChunkBytes = 4096;
constexpr std::size_t kMaxMd5SumsBytes = 4096;
constexpr std::size_t kMaxWebFileBytes = 512U * 1024U; // as for uploads on ota.html
constexpr std::size_t kMaxWebFiles = 32;
constexpr std::chrono::milliseconds kRestartDelay{3000}; // the page sees "restarting" before the reset

/// parseRelease() reads the API response straight from the connection.
class HttpByteReader final : public IByteReader {
public:
    explicit HttpByteReader(net::HttpClient& http) : http_(&http) {}

    int read() override
    {
        char c = 0;
        return readBytes(&c, 1) == 1 ? static_cast<unsigned char>(c) : -1;
    }

    std::size_t readBytes(char* buffer, std::size_t length) override
    {
        std::size_t total = 0;
        while (total < length && !error_) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic): ArduinoJson reader interface
            auto count = http_->read({buffer + total, length - total});
            if (!count) {
                error_ = count.error();
            } else if (*count == 0) {
                break;
            } else {
                total += *count;
            }
        }
        return total;
    }

    /// Transfer error that ended the stream early (parseRelease() then only sees truncated JSON).
    [[nodiscard]] std::optional<esp_err_t> error() const { return error_; }

private:
    net::HttpClient* http_;
    std::optional<esp_err_t> error_;
};

/// Web UI files from the release archive, first written under hidden names (".index.html.update": neither
/// served nor uploadable), then renamed into place by commit(). Uncommitted files are removed again.
class StagedFiles final : public ITarSink {
public:
    explicit StagedFiles(std::string root) : root_(std::move(root)) {}

    StagedFiles(const StagedFiles&) = delete;
    StagedFiles& operator=(const StagedFiles&) = delete;
    StagedFiles(StagedFiles&&) = delete;
    StagedFiles& operator=(StagedFiles&&) = delete;
    ~StagedFiles() { discard(); }

    Result<void> beginFile(std::string_view name, std::size_t size) override
    {
        // The same rules as uploads: no paths, no hidden names, never params.json, data.csv or the logs
        if (!isValidUploadName(name) || size > kMaxWebFileBytes) {
            ESP_LOGE(kTag, "web UI archive: entry '%.*s' (%zu bytes) not allowed",
                     static_cast<int>(name.size()), name.data(), size);
            return fail(ESP_ERR_INVALID_RESPONSE);
        }
        std::string file{name};
        if (std::ranges::find(names_, file) == names_.end()) {
            if (names_.size() == kMaxWebFiles) {
                return fail(ESP_ERR_INVALID_SIZE);
            }
            names_.push_back(file);
        }
        auto opened = storage::File::open(stagedPath(file), "w");
        if (!opened) {
            return fail(opened.error());
        }
        file_.emplace(std::move(*opened));
        return {};
    }

    Result<void> fileData(std::span<const char> data) override
    {
        return file_ ? file_->write({data.data(), data.size()}) : fail(ESP_ERR_INVALID_STATE);
    }

    Result<void> endFile() override
    {
        if (!file_) {
            return fail(ESP_ERR_INVALID_STATE);
        }
        auto flushed = file_->flush();
        file_.reset();
        return flushed;
    }

    [[nodiscard]] std::size_t count() const { return names_.size(); }

    /// Renames every staged file over its target. Stops at the first failure (then the pages are mixed,
    /// the rest is removed).
    Result<void> commit()
    {
        file_.reset();
        while (!names_.empty()) {
            const std::string name = std::move(names_.back());
            names_.pop_back();
            if (auto res = storage::rename(stagedPath(name), root_ + "/" + name); !res) {
                ESP_LOGE(kTag, "%s not replaced: %s", name.c_str(), esp_err_to_name(res.error()));
                removeQuietly(stagedPath(name));
                return res;
            }
        }
        return {};
    }

    void discard()
    {
        file_.reset();
        for (const std::string& name : names_) {
            removeQuietly(stagedPath(name));
        }
        names_.clear();
    }

private:
    [[nodiscard]] std::string stagedPath(const std::string& name) const
    {
        return root_ + "/." + name + ".update";
    }

    static void removeQuietly(const std::string& path)
    {
        if (auto res = storage::remove(path); !res) {
            ESP_LOGW(kTag, "%s not removed: %s", path.c_str(), esp_err_to_name(res.error()));
        }
    }

    std::string root_;
    std::optional<storage::File> file_;
    std::vector<std::string> names_;
};

/// MD5 over a stream (esp_rom: no mbedTLS context needed).
class Md5Stream {
public:
    Md5Stream() { esp_rom_md5_init(&context_); }
    void update(std::span<const char> data)
    {
        esp_rom_md5_update(&context_, data.data(), static_cast<std::uint32_t>(data.size()));
    }
    [[nodiscard]] Md5 digest()
    {
        Md5 md5{};
        esp_rom_md5_final(md5.data(), &context_);
        return md5;
    }

private:
    md5_context_t context_{};
};

const char* describe(esp_err_t err)
{
    switch (err) {
    case ESP_ERR_NOT_FOUND:
        return "not found on GitHub";
    case ESP_ERR_INVALID_CRC:
        return "checksum mismatch";
    case ESP_ERR_TIMEOUT:
        return "network timeout";
    case ESP_ERR_NO_MEM:
        return "out of memory";
    default:
        return esp_err_to_name(err);
    }
}

} // namespace

ReleaseUpdater::ReleaseUpdater(Settings settings, const net::WifiManager& wifi)
    : settings_(std::move(settings)), userAgent_("bananactrl/" + settings_.version), wifi_(&wifi)
{
    status_.current = settings_.version;
}

Result<void> ReleaseUpdater::check(bool includePrereleases)
{
    std::scoped_lock lock{mutex_};
    includePrereleases_ = includePrereleases;
    return request(Job::Check, UpdateState::Checking);
}

Result<void> ReleaseUpdater::install()
{
    std::scoped_lock lock{mutex_};
    if (!status_.release) {
        return fail(ESP_ERR_NOT_FOUND);
    }
    return request(Job::Install, UpdateState::Installing);
}

UpdateStatus ReleaseUpdater::status() const
{
    std::scoped_lock lock{mutex_};
    return status_;
}

// Called with mutex_ held.
Result<void> ReleaseUpdater::request(Job job, UpdateState state)
{
    if (job_ != Job::None || status_.state == UpdateState::Restarting) {
        return fail(ESP_ERR_INVALID_STATE);
    }
    if (wifi_->mode() != net::WifiManager::Mode::Station || !wifi_->isConnected()) {
        return fail(ESP_ERR_NOT_SUPPORTED);
    }
    if (!running()) {
        if (auto res = rtos::Task::start("update", kStackBytes, kPriority); !res) {
            return res;
        }
    }
    job_ = job;
    status_.state = state;
    status_.error.clear();
    status_.done = 0;
    status_.total = 0;
    xTaskNotifyGive(handle());
    return {};
}

void ReleaseUpdater::run()
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        Job job = Job::None;
        bool includePrereleases = false;
        std::optional<Release> release;
        {
            std::scoped_lock lock{mutex_};
            job = job_;
            includePrereleases = includePrereleases_;
            release = status_.release;
        }
        ESP_LOGI(kTag, "%s, heap free %lu, largest block %zu", job == Job::Check ? "checking" : "installing",
                 static_cast<unsigned long>(esp_get_free_heap_size()),
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

        if (job == Job::Check) {
            auto found = fetchRelease(includePrereleases);
            if (!found) {
                ESP_LOGE(kTag, "check failed: %s", esp_err_to_name(found.error()));
                finish(UpdateState::Failed, found.error() == ESP_ERR_NOT_FOUND
                                                ? "no release found"
                                                : std::string{"check failed: "} + describe(found.error()));
                continue;
            }
            const bool newer = isNewer(found->version, settings_.version);
            ESP_LOGI(kTag, "newest release %s%s, running %s", found->version.c_str(),
                     found->prerelease ? " (pre-release)" : "", settings_.version.c_str());
            std::scoped_lock lock{mutex_};
            status_.release = std::move(*found);
            status_.state = newer ? UpdateState::Available : UpdateState::UpToDate;
            job_ = Job::None;
        } else if (job == Job::Install) {
            // Only returns on failure (install() only queues the job with a checked release)
            const auto res = release ? installRelease(*release) : fail(ESP_ERR_NOT_FOUND);
            finish(UpdateState::Failed, std::string{"installation failed: "} + describe(res.error()));
        }
    }
}

Result<Release> ReleaseUpdater::fetchRelease(bool includePrereleases) const
{
    // Newest first; ten entries reach back past a series of pre-releases to the newest release
    const std::string url = "https://api.github.com/repos/" + settings_.repository + "/releases?per_page=10";
    auto http = net::HttpClient::get(
        {.url = url.c_str(), .userAgent = userAgent_.c_str(), .accept = "application/vnd.github+json"});
    if (!http) {
        return fail(http.error());
    }
    HttpByteReader reader{*http};
    auto release = parseRelease(reader, settings_.repository, includePrereleases);
    if (reader.error()) {
        return fail(*reader.error()); // the real cause of a truncated response
    }
    return release;
}

Result<void> ReleaseUpdater::installRelease(const Release& release)
{
    if (!release.firmware || !release.md5sums) {
        return fail(ESP_ERR_INVALID_ARG); // parseRelease() accepts no release without them
    }
    const ReleaseAsset& firmware = *release.firmware;
    ESP_LOGI(kTag, "installing %s from %s", release.version.c_str(), release.page.c_str());
    {
        std::scoped_lock lock{mutex_};
        status_.total = firmware.size + (release.webUi ? release.webUi->size : 0);
    }

    // Checksums first: nothing is written without them
    auto sumsHttp =
        net::HttpClient::get({.url = release.md5sums->url.c_str(), .userAgent = userAgent_.c_str()});
    if (!sumsHttp) {
        return fail(sumsHttp.error());
    }
    const auto sums = sumsHttp->readAll(kMaxMd5SumsBytes);
    if (!sums) {
        return fail(sums.error());
    }
    const auto firmwareMd5 = md5For(*sums, firmware.name);
    const auto webUiMd5 = release.webUi ? md5For(*sums, release.webUi->name) : std::nullopt;
    if (!firmwareMd5 || (release.webUi && !webUiMd5)) {
        ESP_LOGE(kTag, "MD5SUMS lists no checksum for the images");
        return fail(ESP_ERR_INVALID_RESPONSE);
    }

    std::vector<char> buffer(kChunkBytes);
    // Streams one asset through `consume` (Result<void>(std::span<const char>)).
    const auto download = [&](const ReleaseAsset& asset, auto&& consume) -> Result<void> {
        auto http = net::HttpClient::get({.url = asset.url.c_str(), .userAgent = userAgent_.c_str()});
        if (!http) {
            return fail(http.error());
        }
        for (;;) {
            auto count = http->read(buffer);
            if (!count) {
                return fail(count.error());
            }
            if (*count == 0) {
                return {};
            }
            if (auto res = consume(std::span<const char>{buffer.data(), *count}); !res) {
                return res;
            }
            addProgress(*count);
        }
    };

    // 1. Web UI into hidden files (older releases have no archive: the pages stay as they are)
    StagedFiles webFiles{settings_.fsRoot};
    if (release.webUi) {
        Md5Stream md5;
        TarReader tar{webFiles};
        auto received = download(*release.webUi, [&](std::span<const char> data) {
            md5.update(data);
            return tar.feed(data);
        });
        if (!received) {
            ESP_LOGE(kTag, "web UI download failed: %s", esp_err_to_name(received.error()));
            return received;
        }
        if (md5.digest() != *webUiMd5) {
            ESP_LOGE(kTag, "web UI archive: MD5 mismatch");
            return fail(ESP_ERR_INVALID_CRC);
        }
        if (auto res = tar.finish(); !res) {
            ESP_LOGE(kTag, "web UI archive truncated");
            return res;
        }
        ESP_LOGI(kTag, "web UI: %zu files staged", webFiles.count());
    }

    // 2. Firmware into the next OTA slot; finish() checks MD5 and image and selects it for the next boot
    auto ota = OtaUpdater::begin(*firmwareMd5, firmware.size);
    if (!ota) {
        return fail(ota.error());
    }
    auto received = download(firmware, [&](std::span<const char> data) { return ota->write(data); });
    if (!received) {
        ESP_LOGE(kTag, "firmware download failed: %s", esp_err_to_name(received.error()));
        return received; // ~OtaUpdater aborts, the boot partition is unchanged
    }
    if (auto res = ota->finish(); !res) {
        return res;
    }

    // 3. Pages into place, then into the new firmware
    if (auto res = webFiles.commit(); !res) {
        // The firmware is already selected: restart anyway, the old pages keep working with it for the most
        // part and ota.html (also built into the firmware as /failsafe) can repeat the installation.
        ESP_LOGE(kTag, "web UI not completely installed: %s", esp_err_to_name(res.error()));
    }
    ESP_LOGI(kTag, "%s installed to %s, restarting", release.version.c_str(), ota->partition()->label);
    finish(UpdateState::Restarting);
    rtos::restartAfter(kRestartDelay);
}

void ReleaseUpdater::addProgress(std::size_t bytes)
{
    std::scoped_lock lock{mutex_};
    status_.done += bytes;
}

void ReleaseUpdater::finish(UpdateState state, std::string error)
{
    std::scoped_lock lock{mutex_};
    status_.state = state;
    status_.error = std::move(error);
    job_ = Job::None;
}

} // namespace banana::web
