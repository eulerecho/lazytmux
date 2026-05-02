#include <lazytmux/io/process.hpp>

#include <chrono>
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
    EXPECT_EQ(result.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(result.error().message().find("argv"), std::string_view::npos);
}

TEST(ProcessRunnerTest, RejectsNonPositiveTimeout) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "true"};
    auto result = run_command(argv, CommandOptions{.timeout = std::chrono::milliseconds{0}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kInvalidInput);
    EXPECT_NE(result.error().message().find("timeout"), std::string_view::npos);
}

TEST(ProcessRunnerTest, TimesOutAndTerminatesChild) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "sleep 1"};
    const auto started = std::chrono::steady_clock::now();

    auto result = run_command(argv, CommandOptions{.timeout = std::chrono::milliseconds{25}});

    const auto elapsed = std::chrono::steady_clock::now() - started;
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kTimeout);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed),
              std::chrono::milliseconds{500});
}

TEST(ProcessRunnerTest, OutputLimitTerminatesChild) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "printf abcdef"};
    auto result = run_command(argv, CommandOptions{.max_output_bytes = 3});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_NE(result.error().message().find("capture limit"), std::string_view::npos);
}

TEST(ProcessRunnerTest, StatusIsFalseForNonzeroExit) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "exit 3"};
    auto result = run_command_status(argv);
    ASSERT_TRUE(result.has_value()) << result.error().display();
    EXPECT_FALSE(*result);
}

TEST(ProcessRunnerTest, StatusPropagatesTimeout) {
    const std::vector<std::string> argv{"/bin/sh", "-c", "sleep 1"};
    auto result =
        run_command_status(argv, CommandOptions{.timeout = std::chrono::milliseconds{25}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kTimeout);
}

}  // namespace
}  // namespace lazytmux::io
