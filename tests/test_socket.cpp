#include <lazytmux/tmux/socket.hpp>

#include <gtest/gtest.h>

namespace lazytmux::tmux {
namespace {

TEST(ParseTmuxSocketTest, TakesFirstField) {
    auto r = parse_tmux_socket("/tmp/tmux-1000/default,12345,7");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, "/tmp/tmux-1000/default");
}

TEST(ParseTmuxSocketTest, RejectsEmpty) {
    EXPECT_FALSE(parse_tmux_socket("").has_value());
    EXPECT_FALSE(parse_tmux_socket(",1,2").has_value());
}

TEST(ParseTmuxSocketTest, AcceptsNoCommas) {
    auto r = parse_tmux_socket("/tmp/tmux-1000/default");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, "/tmp/tmux-1000/default");
}

}  // namespace
}  // namespace lazytmux::tmux
