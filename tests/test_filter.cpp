#include <lazytmux/filter.hpp>

#include <gtest/gtest.h>
#include <string_view>

namespace lazytmux {
namespace {

KeyEvent ch(char c, bool ctrl = false) {
    return KeyEvent{.code = KeyEvent::Code::Char, .ch = c, .ctrl = ctrl};
}

KeyEvent special(KeyEvent::Code code) {
    return KeyEvent{.code = code};
}

void push_chars(FilterState& f, std::string_view text) {
    for (char c : text) {
        f.handle_key(ch(c));
    }
}

FilterState filter_with(std::string_view pattern) {
    FilterState f;
    f.enter();
    push_chars(f, pattern);
    return f;
}

TEST(FilterStateTest, TypingBuffersText) {
    FilterState f;
    f.enter();
    f.handle_key(ch('a'));
    f.handle_key(ch('b'));
    EXPECT_TRUE(f.is_active());
    EXPECT_EQ(f.buffer(), "ab");
}

TEST(FilterStateTest, EscClearsAndExits) {
    FilterState f;
    f.enter();
    f.handle_key(ch('a'));
    f.handle_key(special(KeyEvent::Code::Esc));
    EXPECT_FALSE(f.is_active());
    EXPECT_EQ(f.buffer(), "");
}

TEST(FilterStateTest, EnterKeepsBufferButExits) {
    FilterState f;
    f.enter();
    f.handle_key(ch('a'));
    f.handle_key(special(KeyEvent::Code::Enter));
    EXPECT_FALSE(f.is_active());
    EXPECT_EQ(f.buffer(), "a");
}

TEST(FilterStateTest, CtrlUClearsWithoutExit) {
    FilterState f;
    f.enter();
    f.handle_key(ch('a'));
    f.handle_key(ch('u', true));
    EXPECT_TRUE(f.is_active());
    EXPECT_EQ(f.buffer(), "");
}

TEST(FilterStateTest, CtrlWDeletesPreviousWord) {
    FilterState f;
    f.enter();
    push_chars(f, "foo bar baz");
    f.handle_key(ch('w', true));
    EXPECT_EQ(f.buffer(), "foo bar ");
    f.handle_key(ch('w', true));
    EXPECT_EQ(f.buffer(), "foo ");
    f.handle_key(ch('w', true));
    EXPECT_EQ(f.buffer(), "");
}

TEST(FilterStateTest, CtrlWEatsTrailingWhitespaceThenWord) {
    FilterState f;
    f.enter();
    push_chars(f, "foo bar  ");
    f.handle_key(ch('w', true));
    EXPECT_EQ(f.buffer(), "foo ");
}

TEST(FilterStateTest, HandleKeyReturnsFalseWhenInactive) {
    FilterState f;
    EXPECT_FALSE(f.handle_key(ch('a')));
}

TEST(FilterStateTest, BackspaceOnEmptyBufferIsNoop) {
    FilterState f;
    f.enter();
    f.handle_key(special(KeyEvent::Code::Backspace));
    EXPECT_EQ(f.buffer(), "");
    EXPECT_TRUE(f.is_active());
}

TEST(FilterScoreTest, SmartCaseSubsequenceMatches) {
    auto f = filter_with("foo");
    EXPECT_TRUE(f.score("foobar").has_value());
    EXPECT_TRUE(f.score("BAZFOO").has_value());
    EXPECT_TRUE(f.score("f-o-o").has_value());
    EXPECT_TRUE(f.score("flame_orchard_overlook").has_value());
    EXPECT_FALSE(f.score("oof").has_value());
    EXPECT_FALSE(f.score("baz").has_value());
}

TEST(FilterScoreTest, ReturnsNulloptForEmptyBuffer) {
    FilterState f;
    f.enter();
    EXPECT_FALSE(f.score("anything").has_value());
}

TEST(FilterScoreTest, ReturnsSomeForMatchNoneForNoMatch) {
    auto f = filter_with("foo");
    EXPECT_TRUE(f.score("foobar").has_value());
    EXPECT_FALSE(f.score("baz").has_value());
}

TEST(FilterScoreTest, RanksContiguousMatchesHigher) {
    auto f = filter_with("foo");
    auto contiguous = f.score("foobar");
    auto scattered = f.score("flame_orchard_overlook");
    ASSERT_TRUE(contiguous.has_value());
    ASSERT_TRUE(scattered.has_value());
    EXPECT_GT(*contiguous, *scattered);
}

TEST(FilterScoreTest, UppercaseInPatternIsCaseSensitive) {
    auto f = filter_with("FOO");
    EXPECT_TRUE(f.score("FOO").has_value());
    EXPECT_FALSE(f.score("foo").has_value());
}

TEST(FilterScoreTest, RepeatedScoreCallsAreConsistent) {
    auto f = filter_with("foo");
    auto a = f.score("foobar");
    auto b = f.score("foobar");
    auto c = f.score("foobar");
    EXPECT_EQ(a, b);
    EXPECT_EQ(b, c);
}

TEST(FilterScoreTest, ReflectsBufferChangeAfterMutation) {
    FilterState f;
    f.enter();
    push_chars(f, "baz");
    EXPECT_TRUE(f.score("baz").has_value());
    EXPECT_FALSE(f.score("bar").has_value());

    f.handle_key(special(KeyEvent::Code::Backspace));
    f.handle_key(ch('r'));
    EXPECT_EQ(f.buffer(), "bar");

    EXPECT_FALSE(f.score("baz").has_value());
    EXPECT_TRUE(f.score("bar").has_value());
}

}  // namespace
}  // namespace lazytmux
