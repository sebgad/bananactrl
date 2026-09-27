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
    return file_.flush(); // visible to /data.csv downloads right away
}

} // namespace banana::storage
