#include "banana/config/ConfigStore.hpp"

#include "banana/config/ConfigJson.hpp"
#include "banana/storage/File.hpp"
#include "banana/storage/NvsNamespace.hpp"

namespace banana::config {
namespace {

constexpr const char* kNamespace = "banana";
constexpr const char* kKey = "params";

Result<std::string> readLegacyFile(const std::string& path)
{
    auto file = storage::File::open(path, "r");
    if (!file) {
        return fail(file.error());
    }
    return file->readAll();
}

} // namespace

ConfigStore::Loaded ConfigStore::load()
{
    const std::scoped_lock lock{mutex_};
    Loaded loaded;
    bool writeBack = false;

    auto nvs = storage::NvsNamespace::open(kNamespace);
    auto stored = nvs ? nvs->readBlob(kKey) : Result<std::string>{fail(nvs.error())};
    auto parsed = stored ? fromJson(*stored) : Result<ParsedConfig>{fail(stored.error())};

    if (parsed) {
        loaded.config = parsed->config;
        loaded.source = parsed->complete ? Source::Stored : Source::StoredCompleted;
        writeBack = !parsed->complete;
    } else if (auto legacy = readLegacyFile(legacyFile_).and_then([](const std::string& json) {
                   return fromJson(json);
               })) {
        loaded.config = legacy->config;
        loaded.source = Source::Imported;
        writeBack = true;
    } else {
        loaded.source = Source::Defaults;
        writeBack = true;
    }

    if (writeBack) {
        if (auto res = saveLocked(loaded.config); !res) {
            loaded.writeError = res.error();
        } else if (loaded.source == Source::Imported) {
            // Keep the file for reference. A failed rename is harmless: NVS now holds a configuration, so the
            // file is not imported again.
            [[maybe_unused]] const auto renamed = storage::rename(legacyFile_, legacyFile_ + ".imported");
        }
    }
    current_ = loaded.config;
    return loaded;
}

Result<void> ConfigStore::save(const Config& config)
{
    const std::scoped_lock lock{mutex_};
    return saveLocked(config);
}

Result<void> ConfigStore::addListener(IConfigListener& listener)
{
    const std::scoped_lock lock{mutex_};
    for (IConfigListener*& slot : listeners_) {
        if (slot == nullptr) {
            slot = &listener;
            return {};
        }
    }
    return fail(ESP_ERR_NO_MEM);
}

Config ConfigStore::current() const
{
    const std::scoped_lock lock{mutex_};
    return current_;
}

Result<Config> ConfigStore::reset()
{
    const Config defaults;
    if (auto res = save(defaults); !res) {
        return fail(res.error());
    }
    return defaults;
}

Result<void> ConfigStore::saveLocked(const Config& config)
{
    auto nvs = storage::NvsNamespace::open(kNamespace);
    if (!nvs) {
        return fail(nvs.error());
    }
    if (auto res = nvs->writeBlob(kKey, toJson(config)); !res) {
        return res;
    }
    current_ = config;
    // Still locked: every listener sees the saves in the order they happened
    for (IConfigListener* listener : listeners_) {
        if (listener != nullptr) {
            listener->onConfigChanged(config);
        }
    }
    return {};
}

} // namespace banana::config
