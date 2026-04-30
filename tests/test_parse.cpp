#include <lazytmux/tmux/parse.hpp>

#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace lazytmux::tmux {
namespace {

TEST(ParseSessionTest, ParsesAttachedSession) {
    auto r = parse_session_row("$0|||dotfiles|||3|||1|||1700000000");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->id.as_str(), "$0");
    EXPECT_EQ(r->name.as_str(), "dotfiles");
    EXPECT_EQ(r->windows, 3u);
    EXPECT_TRUE(r->attached);
    EXPECT_EQ(r->last_activity_unix, 1700000000);
}

TEST(ParseSessionTest, ParsesUnattachedSession) {
    auto r = parse_session_row("$1|||api|||1|||0|||1700001000");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_FALSE(r->attached);
    EXPECT_EQ(r->name.as_str(), "api");
}

TEST(ParseSessionTest, RejectsWrongFieldCount) {
    auto r = parse_session_row("$0|||dotfiles|||3");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("expected 5"), std::string_view::npos);
}

TEST(ParseSessionTest, RejectsNonNumericWindows) {
    auto r = parse_session_row("$0|||dotfiles|||abc|||1|||1700000000");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("session.windows"), std::string_view::npos);
}

TEST(ParseWindowTest, ParsesWindowRow) {
    auto r = parse_window_row("dotfiles|||0|||@5|||editor|||1|||2");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->session.as_str(), "dotfiles");
    EXPECT_EQ(r->index, 0u);
    EXPECT_EQ(r->id.as_str(), "@5");
    EXPECT_EQ(r->name, "editor");
    EXPECT_TRUE(r->active);
    EXPECT_EQ(r->panes, 2u);
}

TEST(ParsePaneTest, ParsesPaneRow) {
    auto r = parse_pane_row("@5|||0|||%12|||nvim|||/home/p|||1");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->window_id.as_str(), "@5");
    EXPECT_EQ(r->id.as_str(), "%12");
    EXPECT_EQ(r->command, "nvim");
    EXPECT_EQ(r->current_path, "/home/p");
    EXPECT_TRUE(r->active);
}

TEST(ParsePaneTest, PreservesEmbeddedTabInPath) {
    auto r = parse_pane_row("@5|||1|||%13|||vim|||/home/p\twith-tab|||0");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->current_path, "/home/p\twith-tab");
    EXPECT_FALSE(r->active);
}

TEST(ParseClientTest, ParsesClientRow) {
    auto r = parse_client_row("46640|||viser-cpp|||/dev/pts/2|||xterm-256color");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->pid, 46640u);
    EXPECT_EQ(r->session.as_str(), "viser-cpp");
    EXPECT_EQ(r->tty.as_str(), "/dev/pts/2");
    EXPECT_EQ(r->term, "xterm-256color");
}

TEST(ParseClientTest, RejectsNonNumericPid) {
    auto r = parse_client_row("notapid|||sess|||/dev/pts/0|||xterm");
    ASSERT_FALSE(r.has_value());
    EXPECT_NE(r.error().message().find("client.pid"), std::string_view::npos);
}

TEST(ParseServerInfoTest, ParsesRow) {
    auto r = parse_server_info_row("12899|||4|||1776464199");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->server_pid, 12899u);
    EXPECT_EQ(r->session_count, 4u);
    EXPECT_EQ(r->start_time_unix, 1776464199);
}

TEST(ParseServerInfoTest, ParsesWithTrailingNewline) {
    auto r = parse_server_info("12899|||4|||1776464199\n");
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->server_pid, 12899u);
}

TEST(ParseAggregateTest, ParsesMultipleSessions) {
    auto out =
        std::string_view{"$0|||dotfiles|||3|||1|||1700000000\n$1|||api|||1|||0|||1700001000"};
    auto r = parse_sessions(out);
    ASSERT_TRUE(r.has_value()) << r.error().display();
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ((*r)[0].name.as_str(), "dotfiles");
    EXPECT_EQ((*r)[1].name.as_str(), "api");
}

TEST(ParseAggregateTest, SkipsBlankLines) {
    auto out =
        std::string_view{"\n$0|||dotfiles|||3|||1|||1700000000\n\n$1|||api|||1|||0|||1700001000\n"};
    auto r = parse_sessions(out);
    ASSERT_TRUE(r.has_value()) << r.error().display();
    EXPECT_EQ(r->size(), 2u);
}

TEST(ParseAggregateTest, AnnotatesFailingRowWithIndex) {
    auto out =
        std::string_view{"$0|||dotfiles|||3|||1|||1700000000\n$1|||api|||abc|||0|||1700001000"};
    auto r = parse_sessions(out);
    ASSERT_FALSE(r.has_value());
    bool found_row_context = false;
    for (const auto& ctx : r.error().contexts()) {
        if (ctx.find("session row 1") != std::string::npos) {
            found_row_context = true;
        }
    }
    EXPECT_TRUE(found_row_context);
}

}  // namespace
}  // namespace lazytmux::tmux
