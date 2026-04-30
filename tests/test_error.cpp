#include <lazytmux/error.hpp>

#include <expected>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace lazytmux {
namespace {

TEST(ErrorTest, ConstructsWithMessage) {
    Error e{"boom"};
    EXPECT_EQ(e.message(), "boom");
    EXPECT_TRUE(e.contexts().empty());
}

TEST(ErrorTest, CapturesSourceLocation) {
    const auto expected_line = static_cast<std::uint_least32_t>(__LINE__ + 1);
    Error e{"boom"};
    EXPECT_EQ(e.where().line(), expected_line);
    EXPECT_NE(std::string_view{e.where().file_name()}.find("test_error.cpp"),
              std::string_view::npos);
}

TEST(ErrorTest, WithContextAppendsInOrder) {
    Error e{"boom"};
    e.with_context("first").with_context("second").with_context("third");
    ASSERT_EQ(e.contexts().size(), 3u);
    EXPECT_EQ(e.contexts()[0], "first");
    EXPECT_EQ(e.contexts()[1], "second");
    EXPECT_EQ(e.contexts()[2], "third");
}

TEST(ErrorTest, DisplayContainsAllParts) {
    Error e{"boom"};
    e.with_context("opening file").with_context("startup");
    const auto rendered = e.display();
    EXPECT_NE(rendered.find("boom"), std::string::npos);
    EXPECT_NE(rendered.find("[opening file]"), std::string::npos);
    EXPECT_NE(rendered.find("[startup]"), std::string::npos);
    EXPECT_NE(rendered.find("test_error.cpp"), std::string::npos);
}

TEST(ErrorTest, IsCopyableAndMovable) {
    Error original{"boom"};
    original.with_context("ctx");
    Error copy = original;
    EXPECT_EQ(copy.message(), "boom");
    ASSERT_EQ(copy.contexts().size(), 1u);
    EXPECT_EQ(copy.contexts()[0], "ctx");

    Error moved = std::move(copy);
    EXPECT_EQ(moved.message(), "boom");
    ASSERT_EQ(moved.contexts().size(), 1u);
}

TEST(ResultTest, PropagatesViaUnexpected) {
    auto fallible = []() -> Result<int> { return std::unexpected(Error{"computation failed"}); };
    auto r = fallible();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().message(), "computation failed");
}

}  // namespace
}  // namespace lazytmux
