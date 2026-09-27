#pragma once

#include <mutex>
#include <string>

#include "banana/core/Result.hpp"
#include "banana/storage/File.hpp"
#include "banana/storage/MeasurementCsv.hpp"

namespace banana::storage {

/// /fs/data.csv: recreated with a header at start, one row per 450 ms tick (writeMeasFile()).
///
/// Committed to flash every kSyncEveryRows rows, not per row: a LittleFS sync of a partly filled block
/// rewrites that block (43-49 ms, up to 320 ms with a metadata compaction, measured), which blocked the
/// heater task on every tick. A reset loses at most the last kSyncEveryRows rows.
class MeasurementRecorder {
public:
    static constexpr unsigned kSyncEveryRows = 10; ///< 4.5 s

    [[nodiscard]] static Result<MeasurementRecorder> create(const std::string& path,
                                                            const csv::Header& header);

    /// Row times are relative to the first row (the Arduino file started at 0 when the timers started).
    [[nodiscard]] Result<void> append(csv::Row row);

private:
    explicit MeasurementRecorder(File file) : file_(std::move(file)) {}

    File file_;
    bool started_ = false;
    unsigned unsyncedRows_ = 0;
    float startSeconds_ = 0.0F;
};

} // namespace banana::storage
