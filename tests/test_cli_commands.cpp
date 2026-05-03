#include <lazytmux/cli_commands.hpp>

#include <deque>
#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lazytmux::cli {
namespace {

enum class TmuxCallKind {
    kRun,
    kStatus,
};

struct RecordedTmuxCall {
    TmuxCallKind kind;
    std::vector<std::string> args;
    tmux::RunConfig config;
};

struct FakeTmuxRunner {
    std::vector<RecordedTmuxCall> calls;
    std::deque<Result<std::string>> outputs;
    std::deque<Result<bool>> statuses;

    [[nodiscard]] tmux::CommandRunner command_runner() {
        return tmux::CommandRunner{
            .run = [this](std::span<const std::string> args,
                          const tmux::RunConfig& config) -> Result<std::string> {
                calls.push_back(RecordedTmuxCall{
                    .kind = TmuxCallKind::kRun,
                    .args = std::vector<std::string>{args.begin(), args.end()},
                    .config = config,
                });
                if (outputs.empty()) {
                    return std::unexpected(Error{ErrorKind::kInternal, "unexpected run call"});
                }
                auto result = std::move(outputs.front());
                outputs.pop_front();
                return result;
            },
            .run_status = [this](std::span<const std::string> args,
                                 const tmux::RunConfig& config) -> Result<bool> {
                calls.push_back(RecordedTmuxCall{
                    .kind = TmuxCallKind::kStatus,
                    .args = std::vector<std::string>{args.begin(), args.end()},
                    .config = config,
                });
                if (statuses.empty()) {
                    return std::unexpected(Error{ErrorKind::kInternal, "unexpected status call"});
                }
                auto result = std::move(statuses.front());
                statuses.pop_front();
                return result;
            },
        };
    }
};

struct Harness {
    int config_loads{0};
    config::LoadResult loaded;
    FakeTmuxRunner tmux_runner;
    Result<std::filesystem::path> save_script{
        std::filesystem::path{"/home/u/.tmux/plugins/tmux-resurrect/scripts/save.sh"}};
    std::vector<std::vector<std::string>> exec_calls;
    Result<void> exec_result{};
    std::string out;
    std::string err;

    [[nodiscard]] CommandDependencies dependencies() {
        return CommandDependencies{
            .load_config =
                [this] {
                    ++config_loads;
                    return loaded;
                },
            .tmux_runner = tmux_runner.command_runner(),
            .locate_save_script = [this] { return save_script; },
            .replace_process = [this](std::span<const std::string> argv) -> Result<void> {
                exec_calls.emplace_back(argv.begin(), argv.end());
                return exec_result;
            },
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

tmux::TemplateName template_name(std::string_view value) {
    auto name = tmux::TemplateName::make(value);
    return std::move(name).value();
}

config::Template make_template(std::vector<config::TemplateWindow> windows) {
    return config::Template{.windows = std::move(windows)};
}

config::TemplateWindow window(std::string name, std::string command = "") {
    return config::TemplateWindow{.name = std::move(name), .command = std::move(command)};
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

TEST(CliCommandTest, SaveRunsResurrectScriptThroughTmuxWithSocketSelection) {
    Harness harness;
    harness.tmux_runner.statuses.push_back(true);

    const int exit_code = run(harness, {"-S", "/tmp/tmux-test", "save"});

    EXPECT_EQ(exit_code, kExitSuccess) << harness.err;
    EXPECT_TRUE(harness.out.empty());
    EXPECT_TRUE(harness.err.empty());
    EXPECT_EQ(harness.config_loads, 0);
    ASSERT_EQ(harness.tmux_runner.calls.size(), 1U);
    EXPECT_EQ(harness.tmux_runner.calls[0].kind, TmuxCallKind::kStatus);
    EXPECT_EQ(harness.tmux_runner.calls[0].args,
              (std::vector<std::string>{"run-shell",
                                        "/home/u/.tmux/plugins/tmux-resurrect/scripts/save.sh"}));
    EXPECT_EQ(harness.tmux_runner.calls[0].config.socket.kind, tmux::SocketSelectionKind::kPath);
    EXPECT_EQ(harness.tmux_runner.calls[0].config.socket.value, "/tmp/tmux-test");
    EXPECT_TRUE(harness.exec_calls.empty());
}

TEST(CliCommandTest, SaveMissingResurrectScriptFailsBeforeTmuxCalls) {
    Harness harness;
    harness.save_script =
        std::unexpected(Error{ErrorKind::kNotFound, "tmux-resurrect save script not found"});

    const int exit_code = run(harness, {"save"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("save script not found"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
    EXPECT_TRUE(harness.tmux_runner.calls.empty());
    EXPECT_TRUE(harness.exec_calls.empty());
}

TEST(CliCommandTest, SavePropagatesTmuxFailure) {
    Harness harness;
    harness.tmux_runner.statuses.push_back(false);

    const int exit_code = run(harness, {"save"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("tmux run-shell failed"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
    ASSERT_EQ(harness.tmux_runner.calls.size(), 1U);
    EXPECT_TRUE(harness.exec_calls.empty());
}

TEST(CliCommandTest, AttachMissingSessionFailsWithoutExec) {
    Harness harness;
    harness.tmux_runner.statuses.push_back(false);

    const int exit_code = run(harness, {"attach", "dev"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("session not found: dev"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
    ASSERT_EQ(harness.tmux_runner.calls.size(), 1U);
    EXPECT_EQ(harness.tmux_runner.calls[0].args,
              (std::vector<std::string>{"has-session", "-t", "dev"}));
    EXPECT_TRUE(harness.exec_calls.empty());
}

TEST(CliCommandTest, AttachExecsTmuxWithNamedSocketAfterSessionExists) {
    Harness harness;
    harness.tmux_runner.statuses.push_back(true);

    const int exit_code = run(harness, {"-L", "work", "attach", "dev"});

    EXPECT_EQ(exit_code, kExitSuccess) << harness.err;
    EXPECT_TRUE(harness.out.empty());
    EXPECT_TRUE(harness.err.empty());
    EXPECT_EQ(harness.config_loads, 0);
    ASSERT_EQ(harness.tmux_runner.calls.size(), 1U);
    EXPECT_EQ(harness.tmux_runner.calls[0].args,
              (std::vector<std::string>{"has-session", "-t", "dev"}));
    EXPECT_EQ(harness.tmux_runner.calls[0].config.socket.kind, tmux::SocketSelectionKind::kNamed);
    EXPECT_EQ(harness.tmux_runner.calls[0].config.socket.value, "work");
    ASSERT_EQ(harness.exec_calls.size(), 1U);
    EXPECT_EQ(harness.exec_calls[0],
              (std::vector<std::string>{"tmux", "-L", "work", "attach-session", "-t", "dev"}));
}

TEST(CliCommandTest, AttachPropagatesSessionLookupError) {
    Harness harness;
    harness.tmux_runner.statuses.push_back(
        std::unexpected(Error{ErrorKind::kTimeout, "tmux timed out"}));

    const int exit_code = run(harness, {"attach", "dev"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("tmux timed out"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
    EXPECT_TRUE(harness.exec_calls.empty());
}

TEST(CliCommandTest, AttachExecFailureReturnsFailure) {
    Harness harness;
    harness.tmux_runner.statuses.push_back(true);
    harness.exec_result = std::unexpected(Error{ErrorKind::kIo, "execvp tmux failed: nope"});

    const int exit_code = run(harness, {"attach", "dev"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("execvp tmux failed"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 0);
    ASSERT_EQ(harness.exec_calls.size(), 1U);
}

TEST(CliCommandTest, ListTemplatesPrintsValidNamesAndWarnings) {
    Harness harness;
    harness.loaded.config.templates.map.emplace(template_name("zeta"),
                                                make_template({window("shell")}));
    harness.loaded.config.templates.map.emplace(template_name("alpha"),
                                                make_template({window("editor")}));
    harness.loaded.warning = "config warning";

    const int exit_code = run(harness, {"list-templates"});

    EXPECT_EQ(exit_code, kExitSuccess);
    EXPECT_EQ(harness.out, "alpha\nzeta\n");
    EXPECT_NE(harness.err.find("warning: config warning"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 1);
    EXPECT_TRUE(harness.tmux_runner.calls.empty());
}

TEST(CliCommandTest, ListTemplatesFailsWhenConfigDefaultedAfterLoadError) {
    Harness harness;
    harness.loaded.warning = "config bad; using defaults";
    harness.loaded.used_defaults_due_to_error = true;

    const int exit_code = run(harness, {"list-templates"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("warning: config bad; using defaults"), std::string::npos);
    EXPECT_NE(harness.err.find("refusing to run command with defaults"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 1);
    EXPECT_TRUE(harness.tmux_runner.calls.empty());
}

TEST(CliCommandTest, MaterializeSelectedTemplateWithSocketSelection) {
    Harness harness;
    harness.loaded.config.templates.map.emplace(template_name("dev"),
                                                make_template({window("editor", "nvim")}));
    harness.tmux_runner.statuses.push_back(false);
    harness.tmux_runner.outputs.push_back(std::string{"@5|||%12\n"});
    harness.tmux_runner.statuses.push_back(true);

    const int exit_code = run(harness, {"-L", "work", "materialize", "dev", "/repo"});

    EXPECT_EQ(exit_code, kExitSuccess) << harness.err;
    EXPECT_TRUE(harness.out.empty());
    EXPECT_TRUE(harness.err.empty());
    ASSERT_EQ(harness.tmux_runner.calls.size(), 3U);
    EXPECT_EQ(harness.tmux_runner.calls[0].kind, TmuxCallKind::kStatus);
    EXPECT_EQ(harness.tmux_runner.calls[0].args,
              (std::vector<std::string>{"has-session", "-t", "dev"}));
    EXPECT_EQ(harness.tmux_runner.calls[0].config.socket.kind, tmux::SocketSelectionKind::kNamed);
    EXPECT_EQ(harness.tmux_runner.calls[0].config.socket.value, "work");
    EXPECT_EQ(harness.tmux_runner.calls[1].kind, TmuxCallKind::kRun);
    EXPECT_EQ(harness.tmux_runner.calls[1].args,
              (std::vector<std::string>{"new-session",
                                        "-d",
                                        "-s",
                                        "dev",
                                        "-n",
                                        "editor",
                                        "-c",
                                        "/repo",
                                        "-P",
                                        "-F",
                                        std::string{tmux::kCreatedPaneFormat}}));
    EXPECT_EQ(harness.tmux_runner.calls[2].kind, TmuxCallKind::kStatus);
    EXPECT_EQ(harness.tmux_runner.calls[2].args,
              (std::vector<std::string>{"send-keys", "-t", "%12", "--", "nvim", "Enter"}));
    EXPECT_EQ(harness.config_loads, 1);
}

TEST(CliCommandTest, MaterializeUnknownTemplateFailsBeforeTmuxCalls) {
    Harness harness;
    harness.loaded.config.templates.map.emplace(template_name("dev"),
                                                make_template({window("editor")}));

    const int exit_code = run(harness, {"materialize", "missing", "/repo"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("template not found: missing"), std::string::npos);
    EXPECT_EQ(harness.config_loads, 1);
    EXPECT_TRUE(harness.tmux_runner.calls.empty());
}

TEST(CliCommandTest, MaterializePropagatesMaterializationFailure) {
    Harness harness;
    harness.loaded.config.templates.map.emplace(template_name("dev"),
                                                make_template({window("editor")}));
    harness.tmux_runner.statuses.push_back(true);

    const int exit_code = run(harness, {"materialize", "dev", "/repo"});

    EXPECT_EQ(exit_code, kExitFailure);
    EXPECT_TRUE(harness.out.empty());
    EXPECT_NE(harness.err.find("target session already exists"), std::string::npos);
    ASSERT_EQ(harness.tmux_runner.calls.size(), 1U);
    EXPECT_EQ(harness.tmux_runner.calls[0].args,
              (std::vector<std::string>{"has-session", "-t", "dev"}));
}

}  // namespace
}  // namespace lazytmux::cli
