#include "banana/storage/MeasurementRecorder.hpp"

namespace banana::storage {

Result<MeasurementRecorder> MeasurementRecorder::create(const std::string& path, const csv::Header& header)
{
    auto file = File::open(path, "w");
    if (!file) {
        return fail(file.error());
    }
    if (auto res = file->write(csv::header(header)); !res) {
        return fail(res.error());
    }
    if (auto res = file->flush(); !res) {
        return fail(res.error());
    }
    return MeasurementRecorder{std::move(*file)};
}

Result<csv::Row> MeasurementRecorder::append(csv::Row row)
{
    if (!started_) {
        started_ = true;
        startSeconds_ = row.seconds;
    }
    row.seconds -= startSeconds_;
    if (auto res = file_.write(csv::row(row)); !res) {
        return fail(res.error());
    }
    if (++unsyncedRows_ < kSyncEveryRows) {
        return row;
    }
    unsyncedRows_ = 0;
    if (auto res = file_.flush(); !res) { // committed: survives a reset and is visible to /data.csv downloads
        return fail(res.error());
    }
    return row;
}

} // namespace banana::storage
