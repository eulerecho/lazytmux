#include <lazytmux/tmux/run.hpp>

#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace lazytmux::tmux {
namespace {

TEST(TmuxRunTest, ReturnsStdoutOnZeroExit) {
    RunConfig config;
    config.executable = "/bin/sh";
    const std::vector<std::string> args{"-c", "printf sessions"};

    auto out = run(args, config);
    ASSERT_TRUE(out.has_value()) << out.error().display();
    EXPECT_EQ(*out, "sessions");
}

TEST(TmuxRunTest, NonzeroExitBecomesErrorForOutputCommand) {
    RunConfig config;
    config.executable = "/bin/sh";
    const std::vector<std::string> args{"-c", "printf bad >&2; exit 9"};

    auto out = run(args, config);
    ASSERT_FALSE(out.has_value());
    EXPECT_EQ(out.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_NE(out.error().message().find("code 9"), std::string_view::npos);
    EXPECT_NE(out.error().message().find("bad"), std::string_view::npos);
}

TEST(TmuxRunTest, StatusReturnsFalseForNonzeroExit) {
    RunConfig config;
    config.executable = "/bin/sh";
    const std::vector<std::string> args{"-c", "exit 1"};

    auto ok = run_status(args, config);
    ASSERT_TRUE(ok.has_value()) << ok.error().display();
    EXPECT_FALSE(*ok);
}

TEST(TmuxRunTest, EmitsSocketPathSelectorBeforeArgs) {
    RunConfig config;
    config.executable = "/bin/echo";
    config.socket = SocketSelection::path("/tmp/tmux-test");
    const std::vector<std::string> args{"list-sessions"};

    auto out = run(args, config);
    ASSERT_TRUE(out.has_value()) << out.error().display();
    EXPECT_EQ(*out, "-S /tmp/tmux-test list-sessions\n");
}

TEST(TmuxRunTest, EmitsNamedSocketSelectorBeforeArgs) {
    RunConfig config;
    config.executable = "/bin/echo";
    config.socket = SocketSelection::named("work");
    const std::vector<std::string> args{"list-sessions"};

    auto out = run(args, config);
    ASSERT_TRUE(out.has_value()) << out.error().display();
    EXPECT_EQ(*out, "-L work list-sessions\n");
}

TEST(TmuxRunTest, PropagatesCommandOptions) {
    RunConfig config;
    config.executable = "/bin/sh";
    config.command_options.timeout = std::chrono::milliseconds{25};
    const std::vector<std::string> args{"-c", "sleep 1"};

    auto out = run(args, config);
    ASSERT_FALSE(out.has_value());
    EXPECT_EQ(out.error().kind(), ErrorKind::kTimeout);
}

}  // namespace
}  // namespace lazytmux::tmux
