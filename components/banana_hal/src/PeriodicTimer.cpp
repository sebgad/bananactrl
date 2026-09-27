#include "banana/hal/PeriodicTimer.hpp"

#include <utility>

namespace banana::hal {

Result<PeriodicTimer> PeriodicTimer::create(const char* name, Callback callback, void* arg)
{
    esp_timer_create_args_t args{};
    args.callback = callback;
    args.arg = arg;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = name;
    esp_timer_handle_t handle = nullptr;
    if (auto res = toResult(esp_timer_create(&args, &handle)); !res) {
        return fail(res.error());
    }
    return PeriodicTimer{handle};
}

PeriodicTimer::PeriodicTimer(PeriodicTimer&& other) noexcept : handle_(std::exchange(other.handle_, nullptr))
{
}

PeriodicTimer& PeriodicTimer::operator=(PeriodicTimer&& other) noexcept
{
    if (this != &other) {
        release();
        handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
}

PeriodicTimer::~PeriodicTimer()
{
    release();
}

Result<void> PeriodicTimer::start(std::chrono::microseconds period)
{
    return toResult(esp_timer_start_periodic(handle_, static_cast<std::uint64_t>(period.count())));
}

void PeriodicTimer::stop()
{
    if (handle_ != nullptr && esp_timer_is_active(handle_)) {
        esp_timer_stop(handle_);
    }
}

void PeriodicTimer::release()
{
    if (handle_ != nullptr) {
        stop();
        esp_timer_delete(handle_);
        handle_ = nullptr;
    }
}

} // namespace banana::hal
