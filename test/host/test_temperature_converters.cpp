// Converters against the DIN Pt1000 table, run through the Wheatstone bridge model of
// tools/calc_wheat_stone.py (3 x 1298 Ohm).

#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "banana/drivers/Pt1000.hpp"

#include <gtest/gtest.h>

namespace {

using banana::drivers::ITemperatureConverter;
using banana::drivers::LookupTableConverter;
namespace pt1000 = banana::drivers::pt1000;

/// °C -> Ohm for 0..169 °C
std::map<int, double> loadPt1000Table()
{
    std::ifstream file(std::string{BANANA_TEST_DATA_DIR} + "/PT_1000_tabelle.csv");
    std::map<int, double> ohms;
    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line)) {
        std::istringstream row(line);
        std::string cell;
        std::getline(row, cell, ',');
        const int start = std::stoi(cell);
        for (int n = 0; n < 10 && std::getline(row, cell, ','); ++n) {
            if (start >= 0) {
                ohms[start + n] = std::stod(cell);
            }
        }
    }
    return ohms;
}

double bridgeVolts(double ohm, double vcc)
{
    constexpr double kR = 1298.0;
    return vcc * (kR * ohm - kR * kR) / ((kR + kR) * (kR + ohm));
}

/// Largest |error| over 0..159 °C.
double maxError(const ITemperatureConverter& converter, double vcc)
{
    const auto table = loadPt1000Table();
    double worst = 0.0;
    for (int celsius = 0; celsius < 160; ++celsius) {
        const float volts = static_cast<float>(bridgeVolts(table.at(celsius), vcc));
        worst = std::max(worst, std::abs(static_cast<double>(converter.toCelsius(volts)) - celsius));
    }
    return worst;
}

TEST(TemperatureConverters, TableModelMatchesGeneratedLookupPoints)
{
    const auto table = loadPt1000Table();
    for (const auto& point : pt1000::kLookup5V0) {
        EXPECT_NEAR(bridgeVolts(table.at(static_cast<int>(point.celsius)), 5.0), point.volts, 1e-6);
    }
}

TEST(TemperatureConverters, Lookup5V0)
{
    const LookupTableConverter converter{pt1000::kLookup5V0};
    EXPECT_LT(maxError(converter, 5.0), 0.05);
}

// The Arduino "3.3 V" table was generated for a 5.08 V bridge supply.
TEST(TemperatureConverters, Lookup3V3IsActuallyFor5V08)
{
    const LookupTableConverter converter{pt1000::kLookup3V3};
    EXPECT_LT(maxError(converter, 5.08), 0.05);
}

// The regression fits are coarse; these bounds document their real accuracy.
TEST(TemperatureConverters, LinearFitFor3V3)
{
    EXPECT_LT(maxError(pt1000::kLinear3V3, 3.3), 10.8);
}

TEST(TemperatureConverters, QuadraticFitFor3V3)
{
    EXPECT_LT(maxError(pt1000::kQuadratic3V3, 3.3), 1.05);
}

TEST(TemperatureConverters, RegressionFitsDoNotFit5V0Hardware)
{
    EXPECT_GT(maxError(pt1000::kQuadratic3V3, 5.0), 40.0);
}

TEST(TemperatureConverters, LookupHitsTablePointsExactly)
{
    const LookupTableConverter converter{pt1000::kLookup5V0};
    for (std::size_t i = 0; i + 1 < pt1000::kLookup5V0.size(); ++i) {
        const auto& point = pt1000::kLookup5V0[i];
        EXPECT_NEAR(converter.toCelsius(point.volts), point.celsius, 1e-3F) << "row " << i;
    }
}

// Same as ADS1115::getPhysVal(): outside [first, last) the result is 0 °C.
TEST(TemperatureConverters, LookupOutsideTableIsZero)
{
    const LookupTableConverter converter{pt1000::kLookup5V0};
    EXPECT_EQ(converter.toCelsius(pt1000::kLookup5V0.front().volts - 0.01F), 0.0F);
    EXPECT_EQ(converter.toCelsius(pt1000::kLookup5V0.back().volts), 0.0F);
    EXPECT_EQ(converter.toCelsius(1.0F), 0.0F);
    EXPECT_NEAR(converter.toCelsius(pt1000::kLookup5V0.front().volts), 0.0F, 1e-4F);
}

TEST(TemperatureConverters, UsableThroughInterface)
{
    const LookupTableConverter lookup{pt1000::kLookup5V0};
    const ITemperatureConverter& converter = lookup;
    EXPECT_NEAR(converter.toCelsius(0.0F), 77.0F, 0.2F); // bridge balanced at R = 1298 Ohm
}

} // namespace
