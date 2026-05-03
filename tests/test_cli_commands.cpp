#include <lazytmux/cli_commands.hpp>

#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace lazytmux::cli {
namespace {

struct Harness {
    int config_loads{0};
    std::string out;
    std::string err;

    [[nodiscard]] CommandDependencies dependencies() {
        return CommandDependencies{
            .load_config =
                [this] {
                    ++config_loads;
                    return config::LoadResult{};
                },
            .tmux_runner = {},
        };
    }

    [[nodiscard]] OutputSinks sinks() {
        return OutputSinks{
            .out = [this](std::string_view text) { out += text; },
            .err = [this](std::string_view text) { err += text; },
        };
    }
};

int run(Harness& harness, std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> v{args};
    return run_args(v, harness.dependencies(), harness.sinks());
}

TEST(CliCommandTest, HelpPrintsUsageWithoutLoadingConfig) {
    Harness harness;

    const int exit_code = run(harness, {"--help"});

    EXPECT_EQ(exit_code, kExitSuccess);
    EXPECT_NE(harness.out.find("usage: lazytmux"), std::string::npos);
    EXPECT_TRUE(harness.err.empty());
    EXPECT_EQ(harness.config_loads, 0);
}

TEST(CliCommandTest, VersionPrintsVersionWithoutLoadingConfig) {
    Harness harness;

    const int exit_code = run(harness, {"--version"});

    EXPECT_EQ(exit_code, kExitSuccess);
    EXPECT_NE(harness.out.find("lazytmux "), std::string::npos);
    EXPECT_TRUE(harness.err.empty());
    EXPECT_EQ(harness.config_loads, 0);
}

TEST(CliCommandTest, UsageErrorsReturnTwoAndPrintUsageToStderr) {
    Harness harness;

    const int exit_code = run(harness, {"--bogus"});

    EXPECT_EQ(exit_code, kExitUsage);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("unknown option"), std::string::npos);
    EXPECT_NE(harness.err.find("usage: lazytmux"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
}

TEST(CliCommandTest, UnsupportedCommandsReturnFailure) {
    Harness harness;

    const int exit_code = run(harness, {"save"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("not implemented"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
}

}  // namespace
}  // namespace lazytmux::cli
