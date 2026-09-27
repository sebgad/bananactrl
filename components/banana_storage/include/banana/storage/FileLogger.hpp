#pragma once

#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <string>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

#include "banana/rtos/Task.hpp"

namespace banana::storage {

/// Copies every ESP_LOG line into /fs/logfile_recent.txt (vprintf_into_FS()). The log hook only formats
/// the line and puts it into a ring buffer; this task writes it to the file. The UART output is unchanged.
///
/// At start the previous session's log becomes logfile_last.txt. The recent file is rotated the same way
/// when it grows beyond kMaxBytes (the Arduino file grew until the next boot).
class FileLogger final : public rtos::Task {
public:
    static constexpr std::size_t kBufferBytes = 8 * 1024; ///< lines are dropped when it is full
    static constexpr long kMaxBytes = 512 * 1024;

    FileLogger() = default;
    FileLogger(const FileLogger&) = delete;
    FileLogger& operator=(const FileLogger&) = delete;
    FileLogger(FileLogger&&) = delete;
    FileLogger& operator=(FileLogger&&) = delete;
    ~FileLogger();

    /// Rotates the files, installs the log hook and starts the writer task. Lines logged before the task
    /// runs are kept in the ring buffer.
    [[nodiscard]] Result<void> start(std::string recentPath, std::string lastPath);

    [[nodiscard]] std::size_t droppedLines() const { return dropped_.load(); }
    [[nodiscard]] std::size_t writeErrors() const { return writeErrors_.load(); }

protected:
    void run() override;

private:
    /// esp_log_set_vprintf() has no user argument: the one active logger.
    static std::atomic<FileLogger*> instance_; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    static int logHook(const char* format, va_list args);

    std::string recentPath_;
    std::string lastPath_;
    RingbufHandle_t ring_ = nullptr;
    vprintf_like_t previous_ = nullptr;
    std::atomic<std::size_t> dropped_{0};
    std::atomic<std::size_t> writeErrors_{0};
};

} // namespace banana::storage
