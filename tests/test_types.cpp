#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/types.hpp>

#include <gtest/gtest.h>

namespace lazytmux::tmux {
namespace {

TEST(TypesTest, SessionDesignatedInitializerConstructs) {
    Session s{
        .id = SessionId::from_validated("$0"),
        .name = SessionName::from_validated("dotfiles"),
        .windows = 3,
        .attached = true,
        .last_activity_unix = 1700000000,
    };
    EXPECT_EQ(s.id.as_str(), "$0");
    EXPECT_EQ(s.name.as_str(), "dotfiles");
    EXPECT_EQ(s.windows, 3u);
    EXPECT_TRUE(s.attached);
    EXPECT_EQ(s.last_activity_unix, 1700000000);
}

TEST(TypesTest, EqualityIsFieldwise) {
    Session a{
        .id = SessionId::from_validated("$0"),
        .name = SessionName::from_validated("dotfiles"),
        .windows = 3,
        .attached = true,
        .last_activity_unix = 1700000000,
    };
    Session b = a;
    EXPECT_EQ(a, b);

    b.windows = 4;
    EXPECT_NE(a, b);
}

TEST(TypesTest, AllAggregatesConstruct) {
    Window w{
        .session = SessionName::from_validated("dotfiles"),
        .index = 0,
        .id = WindowId::from_validated("@5"),
        .name = "editor",
        .active = true,
        .panes = 2,
    };
    EXPECT_EQ(w.name, "editor");

    Pane p{
        .window_id = WindowId::from_validated("@5"),
        .index = 0,
        .id = PaneId::from_validated("%12"),
        .command = "nvim",
        .current_path = "/home/p",
        .active = true,
    };
    EXPECT_EQ(p.command, "nvim");

    Client c{
        .pid = 46640,
        .session = SessionName::from_validated("viser-cpp"),
        .tty = ClientTty::from_validated("/dev/pts/2"),
        .term = "xterm-256color",
    };
    EXPECT_EQ(c.term, "xterm-256color");

    ServerInfo si{
        .server_pid = 12899,
        .session_count = 4,
        .start_time_unix = 1776464199,
    };
    EXPECT_EQ(si.session_count, 4u);
}

}  // namespace
}  // namespace lazytmux::tmux
