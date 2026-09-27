#pragma once

#include <mutex>
#include <string>

#include "banana/core/Result.hpp"
#include "banana/storage/File.hpp"
#include "banana/storage/MeasurementCsv.hpp"

namespace banana::storage {

/// /fs/data.csv: recreated with a header at start, one row per 450 ms tick (writeMeasFile()).
class MeasurementRecorder {
public:
    [[nodiscard]] static Result<MeasurementRecorder> create(const std::string& path,
                                                            const csv::Header& header);

    /// Row times are relative to the first row (the Arduino file started at 0 when the timers started).
    [[nodiscard]] Result<void> append(csv::Row row);

private:
    explicit MeasurementRecorder(File file) : file_(std::move(file)) {}

    File file_;
    bool started_ = false;
    float startSeconds_ = 0.0F;
};

} // namespace banana::storage
