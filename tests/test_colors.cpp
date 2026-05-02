#include <lazytmux/colors.hpp>

#include <gtest/gtest.h>
#include <string_view>

namespace lazytmux {
namespace {

TEST(ParseHexTest, ParsesWithHash) {
    auto r = parse_hex("#abcdef");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (Rgb{0xab, 0xcd, 0xef}));
}

TEST(ParseHexTest, ParsesWithoutHash) {
    auto r = parse_hex("abcdef");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (Rgb{0xab, 0xcd, 0xef}));
}

TEST(ParseHexTest, ParsesUppercaseHex) {
    auto r = parse_hex("#ABCDEF");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (Rgb{0xab, 0xcd, 0xef}));
}

TEST(ParseHexTest, ParsesGruvboxFocus) {
    auto r = parse_hex("#fabd2f");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (Rgb{0xfa, 0xbd, 0x2f}));
}

TEST(ParseHexTest, RejectsThreeDigitForm) {
    EXPECT_FALSE(parse_hex("#abc").has_value());
}

TEST(ParseHexTest, RejectsEmpty) {
    EXPECT_FALSE(parse_hex("").has_value());
}

TEST(ParseHexTest, RejectsHashOnly) {
    EXPECT_FALSE(parse_hex("#").has_value());
}

TEST(ParseHexTest, RejectsEightDigit) {
    EXPECT_FALSE(parse_hex("#abcdef00").has_value());
}

TEST(ParseHexTest, RejectsNonHexAscii) {
    EXPECT_FALSE(parse_hex("zzzzzz").has_value());
}

TEST(ParseHexTest, RejectsSixByteNonAsciiWithoutCrash) {
    // Six bytes: '1' + UTF-8 'à' (0xc3 0xa0) + '2' + '3' + '4'.
    // The literal is split so the compiler does not greedy-consume
    // the trailing digits as part of the \xa0 hex escape.
    constexpr std::string_view kFixture =
        "1\xc3\xa0"
        "234";
    static_assert(kFixture.size() == 6);
    EXPECT_FALSE(parse_hex(kFixture).has_value());
}

}  // namespace
}  // namespace lazytmux
