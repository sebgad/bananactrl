#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace banana::drivers {

/// Moving average over the last N raw samples, fixed-size ring buffer (no heap).
/// Until the buffer is full, it averages over the samples received so far.
template <std::size_t N>
    requires(N > 0)
class MovingAverage {
public:
    void push(std::int16_t sample)
    {
        next_ = (next_ + 1) % N;
        buffer_.at(next_) = sample;
        filled_ = std::min(filled_ + 1, N);
    }

    /// Average of the filled part; 0 before the first sample.
    [[nodiscard]] float value() const
    {
        float sum = 0.0F;
        for (std::size_t i = 0; i < filled_; ++i) {
            sum += static_cast<float>(buffer_.at(i));
        }
        return filled_ == 0 ? 0.0F : sum / static_cast<float>(filled_);
    }

    [[nodiscard]] std::int16_t latest() const { return buffer_.at(next_); }
    [[nodiscard]] std::size_t size() const { return filled_; }
    [[nodiscard]] static constexpr std::size_t capacity() { return N; }

    void clear()
    {
        buffer_.fill(0);
        next_ = N - 1;
        filled_ = 0;
    }

private:
    std::array<std::int16_t, N> buffer_{};
    std::size_t next_ = N - 1; ///< index of the latest sample; the first push lands at 0
    std::size_t filled_ = 0;
};

} // namespace banana::drivers
