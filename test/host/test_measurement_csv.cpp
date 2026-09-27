// data.csv format of writeMeasFile() and the header block of setup().

#include <string>

#include "banana/storage/MeasurementCsv.hpp"

#include <gtest/gtest.h>

namespace {

namespace csv = banana::storage::csv;

TEST(MeasurementCsv, BinaryWithoutLeadingZeros)
{
    EXPECT_EQ(csv::binary(0), "0");
    EXPECT_EQ(csv::binary(0x080C), "100000001100");
    EXPECT_EQ(csv::binary(0xFFFF), "1111111111111111");
}

TEST(MeasurementCsv, HeaderLayoutKeepsGraphsHtmlWorking)
{
    const std::string header = csv::header({.timestamp = "1727430000",
                                            .adsConfig = 0x080C,
                                            .adsLowThreshold = 0x0000,
                                            .adsHighThreshold = 0xFFFF});
    EXPECT_EQ(header, "Measurement File created on 1727430000\r\n"
                      "ADS1115 Register Settings\r\n"
                      "Config Register: 0b100000001100\r\n"
                      "Low Threshold Register: 0b0\r\n"
                      "High Threshold Register: 0b1111111111111111\r\n"
                      "\r\n"
                      "Time,Temperature,TargetPWM,TargetTemperature,Brewing\r\n");
}

TEST(MeasurementCsv, RowFormat)
{
    EXPECT_EQ(csv::row({.seconds = 12.3456F,
                        .celsius = 84.567F,
                        .heaterPercent = 31.25F,
                        .target = 85.0F,
                        .brewing = true}),
              "12.346,84.57,31.25,85.00,1\r\n");
    EXPECT_EQ(
        csv::row(
            {.seconds = 0.0F, .celsius = 20.0F, .heaterPercent = 100.0F, .target = 85.0F, .brewing = false}),
        "0.000,20.00,100.00,85.00,0\r\n");
}

} // namespace
