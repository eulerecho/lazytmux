#include <lazytmux/tmux/run.hpp>

#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

namespace lazytmux::tmux {
namespace {

TEST(TmuxRunTest, ReturnsStdoutOnZeroExit) {
    const RunConfig config{.executable = "/bin/sh", .socket_path = std::nullopt};
    const std::vector<std::string> args{"-c", "printf sessions"};

    auto out = run(args, config);
    ASSERT_TRUE(out.has_value()) << out.error().display();
    EXPECT_EQ(*out, "sessions");
}

TEST(TmuxRunTest, NonzeroExitBecomesErrorForOutputCommand) {
    const RunConfig config{.executable = "/bin/sh", .socket_path = std::nullopt};
    const std::vector<std::string> args{"-c", "printf bad >&2; exit 9"};

    auto out = run(args, config);
    ASSERT_FALSE(out.has_value());
    EXPECT_NE(out.error().message().find("code 9"), std::string_view::npos);
    EXPECT_NE(out.error().message().find("bad"), std::string_view::npos);
}

TEST(TmuxRunTest, StatusReturnsFalseForNonzeroExit) {
    const RunConfig config{.executable = "/bin/sh", .socket_path = std::nullopt};
    const std::vector<std::string> args{"-c", "exit 1"};

    auto ok = run_status(args, config);
    ASSERT_TRUE(ok.has_value()) << ok.error().display();
    EXPECT_FALSE(*ok);
}

}  // namespace
}  // namespace lazytmux::tmux
