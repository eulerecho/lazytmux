#include <lazytmux/tmux/ids.hpp>

#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace lazytmux::tmux {
namespace {

TEST(SessionNameTest, MakeAcceptsValid) {
    auto r = SessionName::make("dotfiles");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->as_str(), "dotfiles");
}

TEST(SessionNameTest, MakeRejectsEmpty) {
    auto r = SessionName::make("");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("empty"), std::string_view::npos);
}

TEST(SessionNameTest, MakeRejectsColon) {
    auto r = SessionName::make("foo:bar");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("':'"), std::string_view::npos);
}

TEST(SessionNameTest, MakeRejectsNewline) {
    auto r = SessionName::make("foo\nbar");
    ASSERT_FALSE(r.has_value());
}

TEST(SessionNameTest, MakeRejectsCarriageReturn) {
    auto r = SessionName::make("foo\rbar");
    ASSERT_FALSE(r.has_value());
}

TEST(SessionNameTest, MakeRejectsTripleDelim) {
    auto r = SessionName::make("a|||b");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("|||"), std::string_view::npos);
}

TEST(SessionNameTest, FromValidatedSkipsValidation) {
    // Document the trust contract: from_validated does NOT
    // reject the disallowed characters. Callers must only use
    // it on parser-trusted bytes.
    auto name = SessionName::from_validated("foo:bar");
    EXPECT_EQ(name.as_str(), "foo:bar");
}

TEST(SessionNameTest, EqualityIsValueBased) {
    auto a = SessionName::from_validated("dotfiles");
    auto b = SessionName::from_validated("dotfiles");
    auto c = SessionName::from_validated("api");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

TEST(SessionNameTest, OrderingIsLexicographic) {
    auto a = SessionName::from_validated("api");
    auto b = SessionName::from_validated("dotfiles");
    EXPECT_LT(a, b);
}

TEST(SessionNameTest, IntoInnerMovesOut) {
    auto name = SessionName::from_validated("dotfiles");
    auto s = std::move(name).into_inner();
    EXPECT_EQ(s, "dotfiles");
}

TEST(SessionIdTest, AcceptsAndRejects) {
    EXPECT_TRUE(SessionId::make("$0").has_value());
    EXPECT_FALSE(SessionId::make("").has_value());
    EXPECT_FALSE(SessionId::make("a:b").has_value());
}

TEST(WindowIdTest, AcceptsAndRejects) {
    EXPECT_TRUE(WindowId::make("@5").has_value());
    EXPECT_FALSE(WindowId::make("a|||b").has_value());
}

TEST(PaneIdTest, AcceptsAndRejects) {
    EXPECT_TRUE(PaneId::make("%12").has_value());
    EXPECT_FALSE(PaneId::make("p\nq").has_value());
}

TEST(ClientTtyTest, AcceptsAndRejects) {
    EXPECT_TRUE(ClientTty::make("/dev/pts/2").has_value());
    EXPECT_FALSE(ClientTty::make("").has_value());
}

TEST(TemplateNameTest, AcceptsLooseInput) {
    EXPECT_TRUE(TemplateName::make("dev").has_value());
    // TemplateName tolerates colons, newlines, and the delim
    // because templates come from trusted TOML config.
    EXPECT_TRUE(TemplateName::make("my:project").has_value());
    EXPECT_TRUE(TemplateName::make("a|||b").has_value());
}

TEST(TemplateNameTest, RejectsEmpty) {
    auto r = TemplateName::make("");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("empty"), std::string_view::npos);
}

}  // namespace
}  // namespace lazytmux::tmux
