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

Result<void> MeasurementRecorder::append(csv::Row row)
{
    if (!started_) {
        started_ = true;
        startSeconds_ = row.seconds;
    }
    row.seconds -= startSeconds_;
    if (auto res = file_.write(csv::row(row)); !res) {
        return res;
    }
    if (++unsyncedRows_ < kSyncEveryRows) {
        return {};
    }
    unsyncedRows_ = 0;
    return file_.flush(); // committed: survives a reset and is visible to /data.csv downloads
}

} // namespace banana::storage
