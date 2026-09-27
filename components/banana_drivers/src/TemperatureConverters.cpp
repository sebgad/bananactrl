#include <algorithm>

#include "banana/drivers/TemperatureConverter.hpp"

namespace banana::drivers {

float LinearConverter::toCelsius(float volts) const
{
    return (volts * x1_) + x0_;
}

float QuadraticConverter::toCelsius(float volts) const
{
    return (volts * volts * x2_) + (volts * x1_) + x0_;
}

float LookupTableConverter::toCelsius(float volts) const
{
    // First point with a larger voltage: the segment is [upper - 1, upper).
    const auto upper = std::ranges::upper_bound(table_, volts, {}, &Point::volts);
    if (upper == table_.begin() || upper == table_.end()) {
        return 0.0F;
    }
    const Point& p0 = *(upper - 1);
    const Point& p1 = *upper;
    const float gradient = (p1.celsius - p0.celsius) / (p1.volts - p0.volts);
    const float offset = p0.celsius - (gradient * p0.volts);
    return (volts * gradient) + offset;
}

} // namespace banana::drivers
