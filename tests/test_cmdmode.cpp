#include <lazytmux/cmdmode/parse.hpp>

#include <array>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lazytmux::cmdmode {
namespace {

constexpr std::array<std::string_view, 6> kTestCommandsArr{
    "kill-pane",
    "kill-server",
    "kill-session",
    "kill-window",
    "rename-session",
    "switch-client",
};
constexpr std::span<const std::string_view> kTestCommands{kTestCommandsArr};

std::vector<std::string> no_candidates(ArgKind) {
    return {};
}

std::vector<std::string> fixture_sessions(ArgKind) {
    return {"alpha", "beta", "my session"};
}

TEST(TokenizeTest, WhitespaceSplitUnchangedForSimpleInput) {
    auto t = tokenize("kill-session -t foo");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, (std::vector<std::string>{"kill-session", "-t", "foo"}));
}

TEST(TokenizeTest, DoubleQuotedArgIsOneToken) {
    auto t = tokenize(R"(rename-session "my session")");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, (std::vector<std::string>{"rename-session", "my session"}));
}

TEST(TokenizeTest, SingleQuotedArgIsOneToken) {
    auto t = tokenize("rename-session 'my session'");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, (std::vector<std::string>{"rename-session", "my session"}));
}

TEST(TokenizeTest, EscapedSpaceKeepsTokenTogether) {
    auto t = tokenize(R"(rename-session my\ session)");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, (std::vector<std::string>{"rename-session", "my session"}));
}

TEST(TokenizeTest, EmptyInputYieldsEmptyVec) {
    auto t = tokenize("");
    ASSERT_TRUE(t.has_value());
    EXPECT_TRUE(t->empty());
}

TEST(TokenizeTest, WhitespaceOnlyInputYieldsEmptyVec) {
    auto t = tokenize("   \t  ");
    ASSERT_TRUE(t.has_value());
    EXPECT_TRUE(t->empty());
}

TEST(TokenizeTest, UnbalancedDoubleQuoteReturnsNullopt) {
    EXPECT_FALSE(tokenize(R"(rename-session "no end)").has_value());
}

TEST(TokenizeTest, UnbalancedSingleQuoteReturnsNullopt) {
    EXPECT_FALSE(tokenize("rename-session 'no end").has_value());
}

TEST(TokenizeTest, MixedQuotingRoundTrips) {
    auto t = tokenize(R"(set-option -g status-style "bg=#fb4934")");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, (std::vector<std::string>{"set-option", "-g", "status-style", "bg=#fb4934"}));
}

TEST(LookupSchemaTest, FindsKnownCommands) {
    const auto* k = lookup_schema("kill-session");
    ASSERT_NE(k, nullptr);
    EXPECT_EQ(k->args.size(), 1U);
    const auto* r = lookup_schema("rename-session");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->args.size(), 2U);
}

TEST(LookupSchemaTest, ReturnsNullForUnknown) {
    EXPECT_EQ(lookup_schema("not-a-tmux-command"), nullptr);
}

TEST(ShellQuoteTest, BareForSimpleStrings) {
    EXPECT_EQ(shell_quote("dotfiles"), "dotfiles");
    EXPECT_EQ(shell_quote("api-prod"), "api-prod");
}

TEST(ShellQuoteTest, DoubleQuotesStringsWithWhitespace) {
    EXPECT_EQ(shell_quote("my session"), R"("my session")");
}

TEST(ShellQuoteTest, EscapesInnerDoubleQuotesAndBackslashes) {
    EXPECT_EQ(shell_quote(R"(a"b\c)"), R"("a\"b\\c")");
}

TEST(CompleteTest, EmptyInputReturnsAllCommands) {
    auto out = complete("", kTestCommands, no_candidates);
    EXPECT_EQ(out.size(), kTestCommandsArr.size());
}

TEST(CompleteTest, PartialCommandFiltersHead) {
    auto out = complete("kil", kTestCommands, no_candidates);
    EXPECT_EQ(
        out, (std::vector<std::string>{"kill-pane", "kill-server", "kill-session", "kill-window"}));
}

TEST(CompleteTest, PartialHeadWithTrailingArgsPreservesArgs) {
    auto out = complete("kill-s -t foo", kTestCommands, no_candidates);
    EXPECT_EQ(out, (std::vector<std::string>{"kill-server -t foo", "kill-session -t foo"}));
}

TEST(CompleteTest, CommandThenSpaceOffersArgCandidates) {
    auto out = complete("switch-client ", kTestCommands, fixture_sessions);
    EXPECT_EQ(out,
              (std::vector<std::string>{
                  "switch-client alpha",
                  "switch-client beta",
                  R"(switch-client "my session")",
              }));
}

TEST(CompleteTest, CommandThenPartialArgFiltersCandidates) {
    auto out = complete("switch-client a", kTestCommands, fixture_sessions);
    EXPECT_EQ(out, (std::vector<std::string>{"switch-client alpha"}));
}

TEST(CompleteTest, ArgPositionPastSchemaReturnsEmpty) {
    auto out = complete("kill-session foo bar", kTestCommands, fixture_sessions);
    EXPECT_TRUE(out.empty());
}

TEST(CompleteTest, FreetextArgReturnsEmpty) {
    auto out = complete("rename-session foo ", kTestCommands, fixture_sessions);
    EXPECT_TRUE(out.empty());
}

TEST(CompleteTest, UnknownCommandWithArgsFallsThrough) {
    auto out = complete("not-a-cmd ", kTestCommands, fixture_sessions);
    EXPECT_TRUE(out.empty());
}

TEST(CompleteTest, NoMatchReturnsEmpty) {
    auto out = complete("zzz", kTestCommands, no_candidates);
    EXPECT_TRUE(out.empty());
}

TEST(CompleteTest, UnbalancedQuoteDegradesToHeadCompletion) {
    auto out = complete(R"(switch-client "my)", kTestCommands, fixture_sessions);
    EXPECT_EQ(out, (std::vector<std::string>{R"(switch-client "my)"}));
}

}  // namespace
}  // namespace lazytmux::cmdmode
