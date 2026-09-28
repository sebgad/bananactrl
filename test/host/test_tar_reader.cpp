#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "banana/web/TarReader.hpp"

#include <gtest/gtest.h>

namespace {

using namespace banana::web;
using banana::Result;

constexpr std::size_t kBlock = TarReader::kBlockBytes;

/// ustar header as GNU tar writes it with --format=ustar.
std::string header(std::string_view name, std::size_t size, char type = '0', std::string_view prefix = {})
{
    std::string block(kBlock, '\0');
    block.replace(0, name.size(), name);
    std::snprintf(block.data() + 100, 8, "%07o", 0644);    // NOLINT(cppcoreguidelines-pro-type-vararg)
    std::snprintf(block.data() + 124, 12, "%011zo", size); // NOLINT(cppcoreguidelines-pro-type-vararg)
    block[156] = type;
    block.replace(257, 6, std::string_view{"ustar\0", 6});
    block.replace(263, 2, "00");
    block.replace(345, prefix.size(), prefix);
    block.replace(148, 8, 8, ' ');
    unsigned sum = 0;
    for (const char c : block) {
        sum += static_cast<unsigned char>(c);
    }
    std::snprintf(block.data() + 148, 8, "%06o", sum); // NOLINT(cppcoreguidelines-pro-type-vararg)
    return block;
}

std::string entry(std::string_view name, std::string_view content, char type = '0')
{
    std::string data = header(name, content.size(), type) + std::string{content};
    data.resize(((data.size() + kBlock - 1) / kBlock) * kBlock, '\0');
    return data;
}

std::string endOfArchive()
{
    return std::string(2 * kBlock, '\0') + std::string(8 * kBlock, '\0'); // + record padding (10 kB records)
}

class RecordingSink final : public ITarSink {
public:
    Result<void> beginFile(std::string_view name, std::size_t size) override
    {
        if (name == rejected) {
            return banana::fail(ESP_ERR_INVALID_RESPONSE);
        }
        EXPECT_TRUE(current.empty()) << "beginFile without endFile";
        current = name;
        sizes[current] = size;
        files[current].clear();
        return {};
    }
    Result<void> fileData(std::span<const char> data) override
    {
        files[current].append(data.data(), data.size());
        return {};
    }
    Result<void> endFile() override
    {
        ++ended;
        current.clear();
        return {};
    }

    std::map<std::string, std::string> files;
    std::map<std::string, std::size_t> sizes;
    std::string current;
    std::string rejected;
    int ended = 0;
};

/// Feeds `archive` in pieces of `step` bytes.
Result<void> feed(TarReader& tar, std::string_view archive, std::size_t step)
{
    for (std::size_t offset = 0; offset < archive.size(); offset += step) {
        if (auto res = tar.feed(archive.substr(offset, step)); !res) {
            return res;
        }
    }
    return tar.finish();
}

std::string sampleArchive()
{
    return entry("./", "", '5') + entry("./index.html", "<html>hello</html>") + entry("style.css", "") +
           entry("uPlot.min.js.gz", std::string(kBlock, 'x')) + // exactly one block, no padding
           entry("footer.js", std::string(1000, 'f')) + endOfArchive();
}

TEST(TarReader, ExtractsRegularFilesInAnyChunkSize)
{
    const std::string archive = sampleArchive();
    for (const std::size_t step :
         {std::size_t{1}, std::size_t{7}, kBlock, std::size_t{4096}, archive.size()}) {
        RecordingSink sink;
        TarReader tar{sink};
        ASSERT_TRUE(feed(tar, archive, step)) << "step " << step;
        EXPECT_EQ(sink.files.size(), 4U) << "step " << step;
        EXPECT_EQ(sink.ended, 4);
        EXPECT_EQ(sink.files["index.html"], "<html>hello</html>"); // "./" removed
        EXPECT_EQ(sink.sizes["index.html"], 18U);
        EXPECT_EQ(sink.files["style.css"], "");
        EXPECT_EQ(sink.files["uPlot.min.js.gz"], std::string(kBlock, 'x'));
        EXPECT_EQ(sink.files["footer.js"], std::string(1000, 'f'));
        EXPECT_FALSE(sink.files.contains("")); // directory skipped
    }
}

TEST(TarReader, PassesPathsOnForTheSinkToReject)
{
    RecordingSink sink;
    TarReader tar{sink};
    const std::string archive = header("index.html", 0, '0', "../../fs") + endOfArchive();
    ASSERT_TRUE(feed(tar, archive, 4096));
    EXPECT_TRUE(sink.files.contains("../../fs/index.html"));

    RecordingSink rejecting;
    rejecting.rejected = "params.json";
    TarReader rejected{rejecting};
    EXPECT_EQ(feed(rejected, entry("params.json", "{}") + endOfArchive(), 4096).error(),
              ESP_ERR_INVALID_RESPONSE);
}

TEST(TarReader, SkipsLinksAndOtherEntries)
{
    RecordingSink sink;
    TarReader tar{sink};
    const std::string archive =
        entry("link.html", "", '2') + entry("fifo", "", '6') + entry("index.html", "ok") + endOfArchive();
    ASSERT_TRUE(feed(tar, archive, 100));
    EXPECT_EQ(sink.files.size(), 1U);
    EXPECT_EQ(sink.files["index.html"], "ok");
}

TEST(TarReader, DetectsCorruptAndTruncatedArchives)
{
    {
        std::string archive = sampleArchive();
        archive[kBlock + 10] ^= 1; // name of the second header: checksum no longer matches
        RecordingSink sink;
        TarReader tar{sink};
        EXPECT_EQ(feed(tar, archive, 4096).error(), ESP_ERR_INVALID_RESPONSE);
    }
    {
        std::string archive = entry("index.html", "ok") + endOfArchive();
        archive.replace(257, 5, "xxxxx"); // not a ustar header
        RecordingSink sink;
        TarReader tar{sink};
        EXPECT_EQ(feed(tar, archive, 4096).error(), ESP_ERR_INVALID_RESPONSE);
    }
    {
        const std::string archive = sampleArchive();
        RecordingSink sink;
        TarReader tar{sink};
        EXPECT_EQ(feed(tar, std::string_view{archive}.substr(0, 3 * kBlock + 100), 512).error(),
                  ESP_ERR_INVALID_SIZE);
    }
    {
        RecordingSink sink;
        TarReader tar{sink};
        EXPECT_EQ(tar.finish().error(), ESP_ERR_INVALID_SIZE); // empty download
    }
}

} // namespace
