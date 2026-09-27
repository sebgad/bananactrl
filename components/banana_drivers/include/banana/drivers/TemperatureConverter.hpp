#pragma once

#include <algorithm>
#include <span>

/// Strategies for bridge voltage -> temperature (replaces #ifdef Pt1000_CONV_*).
namespace banana::drivers {

class ITemperatureConverter {
public:
    ITemperatureConverter() = default;
    ITemperatureConverter(const ITemperatureConverter&) = default;
    ITemperatureConverter& operator=(const ITemperatureConverter&) = default;
    ITemperatureConverter(ITemperatureConverter&&) = default;
    ITemperatureConverter& operator=(ITemperatureConverter&&) = default;
    virtual ~ITemperatureConverter() = default;

    /// Differential bridge voltage [V] -> temperature [°C].
    [[nodiscard]] virtual float toCelsius(float volts) const = 0;
};

/// t = x1 * u + x0
class LinearConverter final : public ITemperatureConverter {
public:
    constexpr LinearConverter(float x1, float x0) : x1_(x1), x0_(x0) {}
    [[nodiscard]] float toCelsius(float volts) const override;

private:
    float x1_;
    float x0_;
};

/// t = x2 * u^2 + x1 * u + x0
class QuadraticConverter final : public ITemperatureConverter {
public:
    constexpr QuadraticConverter(float x2, float x1, float x0) : x2_(x2), x1_(x1), x0_(x0) {}
    [[nodiscard]] float toCelsius(float volts) const override;

private:
    float x2_;
    float x1_;
    float x0_;
};

namespace detail {
/// Deliberately not constexpr: reaching it during constant evaluation is a compile error.
void invalidLookupTable();
} // namespace detail

/// Piecewise linear interpolation over a table sorted by ascending voltage.
/// Outside the table the result is 0 °C, which Diagnostics reports as out of range.
class LookupTableConverter final : public ITemperatureConverter {
public:
    struct Point {
        float volts;
        float celsius;
    };

    /// The table is referenced, not copied, so it must be constexpr data. A table with fewer
    /// than two points or unsorted voltages does not compile.
    consteval explicit LookupTableConverter(std::span<const Point> table) : table_(table)
    {
        if (table_.size() < 2 || !std::ranges::is_sorted(table_, {}, &Point::volts)) {
            detail::invalidLookupTable();
        }
    }

    [[nodiscard]] float toCelsius(float volts) const override;
    [[nodiscard]] std::span<const Point> table() const { return table_; }

private:
    std::span<const Point> table_;
};

} // namespace banana::drivers
