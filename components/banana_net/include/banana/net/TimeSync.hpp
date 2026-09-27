#pragma once

#include <chrono>
#include <ctime>
#include <optional>

#include "banana/core/Result.hpp"

namespace banana::net {

/// SNTP time with the local time zone (configTime()/getLocalTime() of the Arduino firmware, which used
/// fixed +1 h/+1 h offsets and was therefore wrong in winter). RAII: SNTP stops in the destructor.
class TimeSync {
public:
    /// Germany: CET/CEST with the EU switch rules.
    static constexpr const char* kDefaultTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3";
    static constexpr const char* kDefaultServer = "europe.pool.ntp.org";

    /// Sets the TZ and starts SNTP (needs a network interface with IP).
    [[nodiscard]] static Result<TimeSync> start(const char* server = kDefaultServer,
                                                const char* timeZone = kDefaultTimeZone);

    TimeSync(const TimeSync&) = delete;
    TimeSync& operator=(const TimeSync&) = delete;
    TimeSync(TimeSync&& other) noexcept;
    TimeSync& operator=(TimeSync&& other) noexcept;
    ~TimeSync();

    /// Waits for the first synchronisation.
    [[nodiscard]] Result<void> waitForSync(std::chrono::seconds timeout) const;

    /// Local time, or nullopt while the clock has not been set yet.
    [[nodiscard]] static std::optional<std::tm> now();

private:
    TimeSync() : running_(true) {}
    void release();

    bool running_ = false;
};

} // namespace banana::net
