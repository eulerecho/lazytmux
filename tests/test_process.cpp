#include <lazytmux/io/process.hpp>

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace lazytmux::io {
namespace {

TEST(ProcessRunnerTest, CapturesStdoutAndStderr) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "printf out; printf err >&2"};
    auto result = run_command(argv);
    ASSERT_TRUE(result.has_value()) << result.error().display();
    EXPECT_EQ(result->exit_code, 0);
    EXPECT_EQ(result->stdout_text, "out");
    EXPECT_EQ(result->stderr_text, "err");
}

TEST(ProcessRunnerTest, ReportsNonzeroExitCode) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "printf nope >&2; exit 7"};
    auto result = run_command(argv);
    ASSERT_TRUE(result.has_value()) << result.error().display();
    EXPECT_EQ(result->exit_code, 7);
    EXPECT_EQ(result->stderr_text, "nope");
}

TEST(ProcessRunnerTest, RejectsEmptyArgv) {
    const std::vector<std::string> argv;
    auto result = run_command(argv);
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message().find("argv"), std::string_view::npos);
}

TEST(ProcessRunnerTest, StatusIsFalseForNonzeroExit) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "exit 3"};
    auto result = run_command_status(argv);
    ASSERT_TRUE(result.has_value()) << result.error().display();
    EXPECT_FALSE(*result);
}

}  // namespace
}  // namespace lazytmux::io
