#include "banana/config/ConfigStore.hpp"

#include "banana/config/ConfigJson.hpp"
#include "banana/storage/File.hpp"

namespace banana::config {

ConfigStore::Loaded ConfigStore::load()
{
    const std::scoped_lock lock{mutex_};
    Loaded loaded;

    auto file = storage::File::open(path_, "r");
    auto content = file ? file->readAll() : Result<std::string>{fail(file.error())};
    auto parsed = content ? fromJson(*content) : Result<ParsedConfig>{fail(content.error())};

    if (!parsed) {
        loaded.source = Source::Defaults;
    } else {
        loaded.config = parsed->config;
        loaded.source = parsed->complete ? Source::File : Source::FileCompleted;
    }
    if (loaded.source != Source::File) {
        if (auto res = saveLocked(loaded.config); !res) {
            loaded.writeError = res.error();
        }
    }
    return loaded;
}

Result<void> ConfigStore::save(const Config& config)
{
    const std::scoped_lock lock{mutex_};
    return saveLocked(config);
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
    const std::string temporary = path_ + ".tmp";
    {
        auto file = storage::File::open(temporary, "w");
        if (!file) {
            return fail(file.error());
        }
        if (auto res = file->write(toJson(config)); !res) {
            return res;
        }
        if (auto res = file->flush(); !res) {
            return res;
        }
    } // closed before the rename
    return storage::rename(temporary, path_);
}

} // namespace banana::config
