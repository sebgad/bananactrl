#include <array>

#include "banana/web/LiveValues.hpp"
#include "banana/web/WebPaths.hpp"

#include <ArduinoJson.h>
#include <gtest/gtest.h>

namespace {

using namespace banana::web;

TEST(WebPaths, ServesOnlyTheKnownFiles)
{
    EXPECT_EQ(staticFileFor("/"), "index.html");
    EXPECT_EQ(staticFileFor("/graphs.html"), "graphs.html");
    EXPECT_EQ(staticFileFor("/footer.js"), "footer.js");
    EXPECT_EQ(staticFileFor("/recentlogfile.txt"), "logfile_recent.txt");
    EXPECT_EQ(staticFileFor("/lastlogfile.txt"), "logfile_last.txt");
    EXPECT_EQ(staticFileFor("/data.csv?nocache=1"), "data.csv");
    // params.json holds the Wi-Fi password: only via ApiRoutes (redacted)
    EXPECT_FALSE(staticFileFor("/params.json").has_value());
    EXPECT_FALSE(staticFileFor("/params.json.tmp").has_value());
    EXPECT_FALSE(staticFileFor("/../fs/params.json").has_value());
    EXPECT_FALSE(staticFileFor("/logfile_recent.txt").has_value());
}

TEST(WebPaths, ContentTypes)
{
    EXPECT_STREQ(contentType("index.html"), "text/html");
    EXPECT_STREQ(contentType("style.css"), "text/css");
    EXPECT_STREQ(contentType("favicon-32x32.png"), "image/png");
    EXPECT_STREQ(contentType("data.csv"), "text/plain");
    EXPECT_STREQ(contentType("logfile_last.txt"), "text/plain");
    EXPECT_STREQ(contentType("firmware.bin"), "application/octet-stream");
}

TEST(WebPaths, UploadNames)
{
    EXPECT_TRUE(isValidUploadName("index.html"));
    EXPECT_TRUE(isValidUploadName("favicon-16x16.png"));
    EXPECT_TRUE(isValidUploadName("my_page.v2.html"));
    EXPECT_FALSE(isValidUploadName(""));
    EXPECT_FALSE(isValidUploadName("../index.html"));
    EXPECT_FALSE(isValidUploadName("sub/index.html"));
    EXPECT_FALSE(isValidUploadName(".hidden"));
    EXPECT_FALSE(isValidUploadName("with space.html"));
    EXPECT_FALSE(isValidUploadName(std::string(33, 'a')));
    EXPECT_FALSE(isValidUploadName("params.json"));
    EXPECT_FALSE(isValidUploadName("data.csv"));
    EXPECT_FALSE(isValidUploadName("logfile_recent.txt"));
}

TEST(WebPaths, Md5)
{
    const auto md5 = parseMd5(" 0123456789abcdefABCDEF0123456789\n");
    ASSERT_TRUE(md5.has_value());
    EXPECT_EQ((*md5)[0], 0x01);
    EXPECT_EQ((*md5)[7], 0xEF);
    EXPECT_EQ((*md5)[8], 0xAB);
    EXPECT_EQ((*md5)[15], 0x89);
    EXPECT_FALSE(parseMd5("").has_value());
    EXPECT_FALSE(parseMd5("0123456789abcdef0123456789abcde").has_value());   // 31 digits
    EXPECT_FALSE(parseMd5("0123456789abcdef0123456789abcdeg").has_value());  // not hex
    EXPECT_FALSE(parseMd5("0123456789abcdef0123456789abcdef0").has_value()); // 33 digits
}

TEST(LiveValues, ArduinoKeyNames)
{
    banana::control::ProcessSnapshot snapshot;
    snapshot.seconds = 12.5F;
    snapshot.celsius = 84.25F;
    snapshot.target = 85.0F;
    snapshot.heaterPercent = 31.25F;
    snapshot.pidIntegrator = 1.5F;
    snapshot.pidErrorDiff = -0.5F;

    JsonDocument doc;
    ASSERT_EQ(deserializeJson(doc, lastValuesJson(snapshot, 64)), DeserializationError::Ok);
    EXPECT_FLOAT_EQ(doc["Time"].as<float>(), 12.5F);
    EXPECT_FLOAT_EQ(doc["Temperature"].as<float>(), 84.25F);
    EXPECT_EQ(doc["State"], "heating_up");
    EXPECT_FLOAT_EQ(doc["PID"]["TargetValue"].as<float>(), 85.0F);
    EXPECT_FLOAT_EQ(doc["PID"]["TargetPWM"].as<float>(), 31.25F);
    EXPECT_FLOAT_EQ(doc["PID"]["ErrorIntegrator"].as<float>(), 1.5F);
    EXPECT_FLOAT_EQ(doc["PID"]["ErrorDiff"].as<float>(), -0.5F);
    EXPECT_EQ(doc["WiFi"]["SignalStrength in %"].as<int>(), 64);
}

TEST(LiveValues, VersionJson)
{
    JsonDocument doc;
    ASSERT_EQ(deserializeJson(doc, versionJson("1.2.0-3-gabc1234", "v6.1", "Sep 28 2026 12:00:00")),
              DeserializationError::Ok);
    EXPECT_EQ(doc["Version"], "1.2.0-3-gabc1234");
    EXPECT_EQ(doc["IdfVersion"], "v6.1");
    EXPECT_EQ(doc["Built"], "Sep 28 2026 12:00:00");
}

TEST(WebPaths, BundledChartLibraryIsGzipped)
{
    const auto file = staticFileFor("/uPlot.min.js");
    ASSERT_TRUE(file.has_value());
    EXPECT_TRUE(isGzipped(*file));
    EXPECT_STREQ(contentType(contentName(*file)), "text/javascript");
    EXPECT_FALSE(isGzipped(*staticFileFor("/graphs.html")));
    EXPECT_EQ(contentName("index.html"), "index.html");
}

TEST(LiveValues, RowsAsJsonArray)
{
    const std::array rows{
        banana::storage::csv::Row{
            .seconds = 0.0F, .celsius = 26.674F, .heaterPercent = 100.0F, .target = 83.0F, .brewing = false},
        banana::storage::csv::Row{
            .seconds = 0.45F, .celsius = 26.68F, .heaterPercent = 99.5F, .target = 83.0F, .brewing = true},
    };
    EXPECT_EQ(rowsJson(rows), "[[0.000,26.67,100.00,83.00,0],[0.450,26.68,99.50,83.00,1]]");
    EXPECT_EQ(rowsJson({}), "[]");

    JsonDocument doc;
    ASSERT_EQ(deserializeJson(doc, rowsJson(rows)), DeserializationError::Ok);
    EXPECT_EQ(doc.size(), 2U);
}

TEST(LiveValues, ServerSentEventFormat)
{
    EXPECT_EQ(sseEvent("values", R"({"Time":1})"), "event: values\ndata: {\"Time\":1}\n\n");
}

} // namespace
