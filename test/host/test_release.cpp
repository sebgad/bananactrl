#include <algorithm>
#include <string>
#include <string_view>

#include "banana/web/Release.hpp"

#include <ArduinoJson.h>
#include <gtest/gtest.h>

namespace {

using namespace banana::web;

constexpr std::string_view kRepository = "sebgad/bananactrl";

/// Serves a string in small pieces, like the HTTP connection.
class StringReader final : public IByteReader {
public:
    explicit StringReader(std::string_view text) : text_(text) {}

    int read() override
    {
        if (text_.empty()) {
            return -1;
        }
        const char c = text_.front();
        text_.remove_prefix(1);
        return static_cast<unsigned char>(c);
    }

    std::size_t readBytes(char* buffer, std::size_t length) override
    {
        const std::size_t count = std::min({length, text_.size(), std::size_t{7}});
        std::copy_n(text_.data(), count, buffer);
        text_.remove_prefix(count);
        return count;
    }

private:
    std::string_view text_;
};

std::string asset(std::string_view name, int size)
{
    return R"({"url": "https://api.github.com/repos/sebgad/bananactrl/releases/assets/1", "name": ")" +
           std::string{name} + R"(", "uploader": {"login": "github-actions[bot]"}, "size": )" +
           std::to_string(size) +
           R"(, "digest": "sha256:00", "browser_download_url": "https://github.com/x"})";
}

std::string release(std::string_view tag, bool prerelease, bool draft, std::string_view assets)
{
    return R"({"tag_name": ")" + std::string{tag} + R"(", "prerelease": )" + (prerelease ? "true" : "false") +
           R"(, "draft": )" + (draft ? "true" : "false") +
           R"(, "body": "### Added\n- \"quoted\" notes [with] {braces}", "author": {"login": "x", "id": 1},
             "assets": [)" +
           std::string{assets} + "]}";
}

std::string fullAssets(std::string_view version)
{
    const std::string v{version};
    return asset("bananactrl-" + v + ".bin", 1192496) + "," +
           asset("bananactrl-factory-" + v + ".bin", 12648448) + "," + asset("MD5SUMS", 343) + "," +
           asset("webui-" + v + ".tar", 81920);
}

TEST(Version, ParsesTagsAndDescribeOutput)
{
    const auto plain = parseVersion("1.2.3");
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->major, 1U);
    EXPECT_EQ(plain->minor, 2U);
    EXPECT_EQ(plain->patch, 3U);
    EXPECT_TRUE(plain->prerelease.empty());
    EXPECT_FALSE(plain->derived);

    const auto rc = parseVersion("v1.0.0-rc.1");
    ASSERT_TRUE(rc);
    EXPECT_EQ(rc->prerelease, "rc.1");
    EXPECT_FALSE(rc->derived);

    const auto after = parseVersion("1.0.0-rc.1-3-gabc1234-dirty");
    ASSERT_TRUE(after);
    EXPECT_EQ(after->prerelease, "rc.1");
    EXPECT_TRUE(after->derived);

    const auto untagged = parseVersion("0.0.0-gabc1234");
    ASSERT_TRUE(untagged);
    EXPECT_TRUE(untagged->prerelease.empty());
    EXPECT_TRUE(untagged->derived);

    EXPECT_FALSE(parseVersion(""));
    EXPECT_FALSE(parseVersion("1.2"));
    EXPECT_FALSE(parseVersion("1.2.x"));
    EXPECT_FALSE(parseVersion("latest"));
}

TEST(Version, SemVerPrecedence)
{
    // SemVer 2.0.0, section 11
    EXPECT_TRUE(isNewer("1.0.0-alpha.1", "1.0.0-alpha"));
    EXPECT_TRUE(isNewer("1.0.0-alpha.beta", "1.0.0-alpha.1"));
    EXPECT_TRUE(isNewer("1.0.0-beta", "1.0.0-alpha.beta"));
    EXPECT_TRUE(isNewer("1.0.0-beta.2", "1.0.0-beta"));
    EXPECT_TRUE(isNewer("1.0.0-beta.11", "1.0.0-beta.2"));
    EXPECT_TRUE(isNewer("1.0.0-rc.1", "1.0.0-beta.11"));
    EXPECT_TRUE(isNewer("1.0.0", "1.0.0-rc.1"));
    EXPECT_TRUE(isNewer("1.0.1", "1.0.0"));
    EXPECT_TRUE(isNewer("1.10.0", "1.9.0"));
    EXPECT_TRUE(isNewer("2.0.0", "1.99.99"));

    EXPECT_FALSE(isNewer("1.0.0", "1.0.0"));
    EXPECT_FALSE(isNewer("1.0.0-rc.1", "1.0.0"));
    EXPECT_FALSE(isNewer("0.9.0", "1.0.0"));
}

TEST(Version, DevelopmentBuilds)
{
    // Commits after a tag are newer than the tag, older than the next release
    EXPECT_FALSE(isNewer("1.0.0", "1.0.0-3-gabc1234"));
    EXPECT_FALSE(isNewer("1.0.0", "1.0.0-dirty"));
    EXPECT_TRUE(isNewer("1.0.1", "1.0.0-3-gabc1234"));
    EXPECT_TRUE(isNewer("1.0.0", "1.0.0-rc.1-3-gabc1234"));
    // Untagged build or unknown version: every release is an update
    EXPECT_TRUE(isNewer("0.1.0", "0.0.0-gabc1234"));
    EXPECT_TRUE(isNewer("1.0.0", "unknown"));
    EXPECT_FALSE(isNewer("garbage", "1.0.0"));
}

TEST(Release, PicksNewestStableRelease)
{
    const std::string json = "[" + release("v1.1.0-rc.1", true, false, fullAssets("1.1.0-rc.1")) + "," +
                             release("v1.0.0", false, false, fullAssets("1.0.0")) + "]";
    StringReader reader{json};
    const auto found = parseRelease(reader, kRepository, false);
    ASSERT_TRUE(found) << found.error();
    EXPECT_EQ(found->version, "1.0.0");
    EXPECT_FALSE(found->prerelease);
    EXPECT_EQ(found->page, "https://github.com/sebgad/bananactrl/releases/tag/v1.0.0");
    ASSERT_TRUE(found->firmware);
    EXPECT_EQ(found->firmware->name, "bananactrl-1.0.0.bin");
    EXPECT_EQ(found->firmware->url,
              "https://github.com/sebgad/bananactrl/releases/download/v1.0.0/bananactrl-1.0.0.bin");
    EXPECT_EQ(found->firmware->size, 1192496U);
    ASSERT_TRUE(found->webUi);
    EXPECT_EQ(found->webUi->name, "webui-1.0.0.tar");
    EXPECT_EQ(found->webUi->size, 81920U);
    ASSERT_TRUE(found->md5sums);
    EXPECT_EQ(found->md5sums->url, "https://github.com/sebgad/bananactrl/releases/download/v1.0.0/MD5SUMS");
}

TEST(Release, PreReleasesOnRequest)
{
    const std::string json = "[" + release("v2.0.0-rc.1", false, true, fullAssets("2.0.0-rc.1")) + "," +
                             release("v1.1.0-rc.1", true, false, fullAssets("1.1.0-rc.1")) + "," +
                             release("v1.0.0", false, false, fullAssets("1.0.0")) + "]";
    StringReader reader{json};
    const auto found = parseRelease(reader, kRepository, true);
    ASSERT_TRUE(found) << found.error();
    EXPECT_EQ(found->version, "1.1.0-rc.1"); // the draft is skipped
    EXPECT_TRUE(found->prerelease);
}

TEST(Release, OlderReleaseWithoutWebUi)
{
    // v1.0.0-rc.1 as published: no webui archive
    const std::string assets = asset("bananactrl-1.0.0-rc.1.bin", 1192496) + "," + asset("MD5SUMS", 343) +
                               "," + asset("storage-1.0.0-rc.1.bin", 8388608);
    const std::string json = "[" + release("v1.0.0-rc.1", true, false, assets) + "]";
    StringReader reader{json};
    const auto found = parseRelease(reader, kRepository, true);
    ASSERT_TRUE(found) << found.error();
    EXPECT_TRUE(found->firmware);
    EXPECT_FALSE(found->webUi);
}

TEST(Release, Rejects)
{
    {
        const std::string json = "[" + release("v1.0.0-rc.1", true, false, fullAssets("1.0.0-rc.1")) + "]";
        StringReader reader{json};
        EXPECT_EQ(parseRelease(reader, kRepository, false).error(), ESP_ERR_NOT_FOUND); // only a pre-release
    }
    {
        StringReader reader{"[]"};
        EXPECT_EQ(parseRelease(reader, kRepository, true).error(), ESP_ERR_NOT_FOUND);
    }
    {
        const std::string json = "[" + release("v1.0.0", false, false, asset("MD5SUMS", 343)) + "]";
        StringReader reader{json};
        EXPECT_EQ(parseRelease(reader, kRepository, true).error(), ESP_ERR_INVALID_RESPONSE); // no firmware
    }
    {
        const std::string json = "[" + release("nightly", false, false, fullAssets("nightly")) + "]";
        StringReader reader{json};
        EXPECT_EQ(parseRelease(reader, kRepository, true).error(), ESP_ERR_INVALID_RESPONSE);
    }
    {
        StringReader reader{R"({"message": "API rate limit exceeded for 1.2.3.4."})"};
        EXPECT_EQ(parseRelease(reader, kRepository, true).error(), ESP_ERR_INVALID_RESPONSE);
    }
    {
        const std::string json = "[" + release("v1.0.0", false, false, fullAssets("1.0.0")) + "]";
        StringReader reader{std::string_view{json}.substr(0, json.size() / 2)}; // connection lost
        EXPECT_EQ(parseRelease(reader, kRepository, true).error(), ESP_ERR_INVALID_RESPONSE);
    }
}

TEST(Release, Md5Sums)
{
    // md5sum output of release.yml
    const std::string_view sums = "0123456789abcdef0123456789abcdef  bananactrl-1.0.0.bin\n"
                                  "fedcba9876543210FEDCBA9876543210 *webui-1.0.0.tar\r\n"
                                  "not a checksum line\n"
                                  "00000000000000000000000000000000  storage-1.0.0.bin";
    const auto firmware = md5For(sums, "bananactrl-1.0.0.bin");
    ASSERT_TRUE(firmware);
    EXPECT_EQ(firmware->front(), 0x01);
    EXPECT_EQ(firmware->back(), 0xef);
    const auto webUi = md5For(sums, "webui-1.0.0.tar");
    ASSERT_TRUE(webUi);
    EXPECT_EQ(webUi->front(), 0xfe);
    EXPECT_TRUE(md5For(sums, "storage-1.0.0.bin"));
    EXPECT_FALSE(md5For(sums, "bananactrl-1.0.0"));
    EXPECT_FALSE(md5For(sums, "bananactrl-factory-1.0.0.bin"));
    EXPECT_FALSE(md5For("", "bananactrl-1.0.0.bin"));
}

TEST(Release, StatusJson)
{
    UpdateStatus status;
    status.current = "1.0.0-rc.1";
    JsonDocument idle;
    ASSERT_FALSE(deserializeJson(idle, updateStatusJson(status)));
    EXPECT_EQ(idle["State"], "idle");
    EXPECT_EQ(idle["Current"], "1.0.0-rc.1");
    EXPECT_FALSE(idle["Latest"].is<const char*>());

    status.state = UpdateState::Installing;
    status.release = Release{.version = "1.0.0",
                             .page = "https://github.com/sebgad/bananactrl/releases/tag/v1.0.0",
                             .prerelease = false,
                             .firmware = ReleaseAsset{},
                             .webUi = ReleaseAsset{},
                             .md5sums = ReleaseAsset{}};
    status.done = 1024;
    status.total = 4096;
    JsonDocument installing;
    ASSERT_FALSE(deserializeJson(installing, updateStatusJson(status)));
    EXPECT_EQ(installing["State"], "installing");
    EXPECT_EQ(installing["Latest"], "1.0.0");
    EXPECT_EQ(installing["Prerelease"], false);
    EXPECT_EQ(installing["WebUi"], true);
    EXPECT_EQ(installing["Done"], 1024);
    EXPECT_EQ(installing["Total"], 4096);

    status.state = UpdateState::Failed;
    status.error = "installation failed: checksum mismatch";
    JsonDocument failed;
    ASSERT_FALSE(deserializeJson(failed, updateStatusJson(status)));
    EXPECT_EQ(failed["State"], "failed");
    EXPECT_EQ(failed["Error"], "installation failed: checksum mismatch");
}

} // namespace
