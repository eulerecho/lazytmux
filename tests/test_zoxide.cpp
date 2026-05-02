#include <lazytmux/zoxide.hpp>

#include <filesystem>
#include <gtest/gtest.h>
#include <string_view>

namespace lazytmux::zoxide {
namespace {

TEST(ZoxideParseTest, ParsesTwoLineFixture) {
    auto entries = parse_entries("3.50 /home/u/foo\n2.10 /home/u/bar\n");
    ASSERT_TRUE(entries.has_value()) << entries.error().display();
    ASSERT_EQ(entries->size(), 2u);
    EXPECT_EQ((*entries)[0].path, std::filesystem::path("/home/u/foo"));
    EXPECT_DOUBLE_EQ((*entries)[0].rank, 3.5);
    EXPECT_EQ((*entries)[1].path, std::filesystem::path("/home/u/bar"));
    EXPECT_DOUBLE_EQ((*entries)[1].rank, 2.1);
}

TEST(ZoxideParseTest, EmptyTextYieldsEmptyVector) {
    auto entries = parse_entries("");
    ASSERT_TRUE(entries.has_value()) << entries.error().display();
    EXPECT_TRUE(entries->empty());
}

TEST(ZoxideParseTest, SkipsBlankLinesAndHandlesPadding) {
    auto entries = parse_entries("    3.50 /home/u/foo\n\n   12.34 /home/u/bar\n");
    ASSERT_TRUE(entries.has_value()) << entries.error().display();
    ASSERT_EQ(entries->size(), 2u);
    EXPECT_DOUBLE_EQ((*entries)[0].rank, 3.5);
    EXPECT_DOUBLE_EQ((*entries)[1].rank, 12.34);
}

TEST(ZoxideParseTest, PathsWithSpacesKeepSpaces) {
    auto entries = parse_entries("1.00 /home/u/dir with spaces\n");
    ASSERT_TRUE(entries.has_value()) << entries.error().display();
    ASSERT_EQ(entries->size(), 1u);
    EXPECT_EQ((*entries)[0].path, std::filesystem::path("/home/u/dir with spaces"));
}

TEST(ZoxideParseTest, RejectsMissingSeparator) {
    auto entries = parse_entries("3.5/home/u/foo\n");
    ASSERT_FALSE(entries.has_value());
    EXPECT_NE(entries.error().message().find("missing"), std::string_view::npos);
}

TEST(ZoxideParseTest, RejectsGarbageScore) {
    auto entries = parse_entries("notascore /home/u/foo\n");
    ASSERT_FALSE(entries.has_value());
    EXPECT_NE(entries.error().message().find("score"), std::string_view::npos);
}

TEST(ZoxideLoadTest, MissingExecutableReturnsEmpty) {
    auto entries = load_top(20, "lazytmux-zoxide-does-not-exist");
    ASSERT_TRUE(entries.has_value()) << entries.error().display();
    EXPECT_TRUE(entries->empty());
}

}  // namespace
}  // namespace lazytmux::zoxide
