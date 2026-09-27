#include "banana/storage/FileLogger.hpp"

#include <array>
#include <cstdio>

#include "banana/storage/File.hpp"

namespace banana::storage {

std::atomic<FileLogger*> FileLogger::instance_{
    nullptr}; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

FileLogger::~FileLogger()
{
    FileLogger* self = this;
    if (instance_.compare_exchange_strong(self, nullptr)) {
        esp_log_set_vprintf(previous_);
    }
    if (ring_ != nullptr) {
        vRingbufferDelete(ring_);
    }
}

Result<void> FileLogger::start(std::string recentPath, std::string lastPath, BaseType_t core)
{
    recentPath_ = std::move(recentPath);
    lastPath_ = std::move(lastPath);

    // Previous session → last (removeFile + rename in the Arduino setup())
    if (exists(recentPath_)) {
        if (auto res = rename(recentPath_, lastPath_); !res) {
            return res;
        }
    }

    ring_ = xRingbufferCreate(kBufferBytes, RINGBUF_TYPE_BYTEBUF);
    if (ring_ == nullptr) {
        return fail(ESP_ERR_NO_MEM);
    }
    FileLogger* expected = nullptr;
    if (!instance_.compare_exchange_strong(expected, this)) {
        return fail(ESP_ERR_INVALID_STATE); // only one logger
    }
    previous_ = esp_log_set_vprintf(&FileLogger::logHook);
    return Task::start("filelog", 4096, 2, core);
}

int FileLogger::logHook(const char* format, va_list args)
{
    FileLogger* self = instance_.load();
    va_list copy;
    va_copy(copy, args);
    const int written = self != nullptr && self->previous_ != nullptr ? self->previous_(format, args)
                                                                      : std::vprintf(format, args);
    if (self != nullptr) {
        // Small stack buffer: the hook runs in whatever task logs (some have small stacks).
        std::array<char, 256> line{};
        const int length = std::vsnprintf(line.data(), line.size(), format, copy);
        if (length > 0) {
            const auto size = std::min(static_cast<std::size_t>(length), line.size() - 1);
            if (xRingbufferSend(self->ring_, line.data(), size, 0) != pdTRUE) {
                self->dropped_.fetch_add(1);
            }
        }
    }
    va_end(copy);
    return written;
}

void FileLogger::run()
{
    Result<File> file = File::open(recentPath_, "a");

    // Errors cannot be logged from here (they would come back through the hook): count them and
    // reopen the file with the next lines (e.g. after the file system was full).
    const auto write = [&](const void* data, std::size_t size) {
        if (!file) {
            file = File::open(recentPath_, "a");
        }
        if (!file || !file->write({static_cast<const char*>(data), size})) {
            writeErrors_.fetch_add(1);
            file = fail(ESP_FAIL);
        }
    };

    for (;;) {
        std::size_t size = 0;
        void* data = xRingbufferReceiveUpTo(ring_, &size, portMAX_DELAY, 512);
        while (data != nullptr) {
            write(data, size);
            vRingbufferReturnItem(ring_, data);
            data = xRingbufferReceiveUpTo(ring_, &size, 0, 512); // drain, then flush once
        }
        if (!file) {
            continue;
        }
        if (!file->flush()) {
            writeErrors_.fetch_add(1);
        }
        if (file->size() > kMaxBytes) {
            file = fail(ESP_ERR_INVALID_STATE); // closes the file: LittleFS cannot rename open files
            if (!rename(recentPath_, lastPath_)) {
                writeErrors_.fetch_add(1);
            }
        }
    }
}

} // namespace banana::storage
