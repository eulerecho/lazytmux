#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/tmux/parse.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lazytmux::tmux {
namespace {

struct RecordedCall {
    std::vector<std::string> args;
    RunConfig config;
};

struct FakeRunner {
    std::vector<RecordedCall> output_calls;
    std::vector<RecordedCall> status_calls;
    Result<std::string> next_output{std::string{}};
    Result<bool> next_status{true};

    [[nodiscard]] CommandRunner command_runner() {
        return CommandRunner{
            .run = [this](std::span<const std::string> args,
                          const RunConfig& config) -> Result<std::string> {
                output_calls.push_back(RecordedCall{
                    .args = std::vector<std::string>{args.begin(), args.end()}, .config = config});
                return next_output;
            },
            .run_status = [this](std::span<const std::string> args,
                                 const RunConfig& config) -> Result<bool> {
                status_calls.push_back(RecordedCall{
                    .args = std::vector<std::string>{args.begin(), args.end()}, .config = config});
                return next_status;
            },
        };
    }
};

void expect_one_output_call(const FakeRunner& fake, const std::vector<std::string>& expected_args) {
    ASSERT_EQ(fake.output_calls.size(), 1U);
    EXPECT_EQ(fake.output_calls[0].args, expected_args);
}

void expect_one_status_call(const FakeRunner& fake, const std::vector<std::string>& expected_args) {
    ASSERT_EQ(fake.status_calls.size(), 1U);
    EXPECT_EQ(fake.status_calls[0].args, expected_args);
}

TEST(TmuxCommandTest, ListSessionsBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "$0|||dotfiles|||3|||1|||1700000000\n";

    auto sessions = list_sessions(fake.command_runner());

    ASSERT_TRUE(sessions.has_value()) << sessions.error().display();
    ASSERT_EQ(sessions->size(), 1U);
    EXPECT_EQ((*sessions)[0].name.as_str(), "dotfiles");
    expect_one_output_call(fake, {"list-sessions", "-F", std::string{kSessionFormat}});
}

TEST(TmuxCommandTest, ListSessionsPassesSocketSelectionAndCommandOptions) {
    FakeRunner fake;
    fake.next_output = "";
    RunConfig config;
    config.socket = SocketSelection::named("work");
    config.command_options.timeout = std::chrono::milliseconds{1234};

    auto sessions = list_sessions(fake.command_runner(), config);

    ASSERT_TRUE(sessions.has_value()) << sessions.error().display();
    ASSERT_EQ(fake.output_calls.size(), 1U);
    EXPECT_EQ(fake.output_calls[0].config.socket.kind, SocketSelectionKind::kNamed);
    EXPECT_EQ(fake.output_calls[0].config.socket.value, "work");
    EXPECT_EQ(fake.output_calls[0].config.command_options.timeout, std::chrono::milliseconds{1234});
}

TEST(TmuxCommandTest, ListSessionsMalformedOutputReturnsParseError) {
    FakeRunner fake;
    fake.next_output = "$0|||missing-fields\n";

    auto sessions = list_sessions(fake.command_runner());

    ASSERT_FALSE(sessions.has_value());
    EXPECT_NE(sessions.error().message().find("expected 5"), std::string_view::npos);
}

TEST(TmuxCommandTest, ListWindowsBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "dotfiles|||0|||@5|||editor|||1|||2\n";
    const auto session = SessionName::from_validated("dotfiles");

    auto windows = list_windows(session, fake.command_runner());

    ASSERT_TRUE(windows.has_value()) << windows.error().display();
    ASSERT_EQ(windows->size(), 1U);
    EXPECT_EQ((*windows)[0].id.as_str(), "@5");
    expect_one_output_call(fake,
                           {"list-windows", "-t", "dotfiles", "-F", std::string{kWindowFormat}});
}

TEST(TmuxCommandTest, ListWindowsMalformedOutputReturnsError) {
    FakeRunner fake;
    fake.next_output = "dotfiles|||not-an-index|||@5|||editor|||1|||2\n";
    const auto session = SessionName::from_validated("dotfiles");

    auto windows = list_windows(session, fake.command_runner());

    ASSERT_FALSE(windows.has_value());
    EXPECT_NE(windows.error().message().find("window.index"), std::string_view::npos);
}

TEST(TmuxCommandTest, ListWindowsAllBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "api|||1|||@8|||server|||0|||1\n";

    auto windows = list_windows_all(fake.command_runner());

    ASSERT_TRUE(windows.has_value()) << windows.error().display();
    ASSERT_EQ(windows->size(), 1U);
    EXPECT_EQ((*windows)[0].session.as_str(), "api");
    expect_one_output_call(fake, {"list-windows", "-a", "-F", std::string{kWindowFormat}});
}

TEST(TmuxCommandTest, ListPanesBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "@5|||0|||%12|||nvim|||/home/p|||1\n";
    const auto window = WindowId::from_validated("@5");

    auto panes = list_panes(window, fake.command_runner());

    ASSERT_TRUE(panes.has_value()) << panes.error().display();
    ASSERT_EQ(panes->size(), 1U);
    EXPECT_EQ((*panes)[0].id.as_str(), "%12");
    expect_one_output_call(fake, {"list-panes", "-t", "@5", "-F", std::string{kPaneFormat}});
}

TEST(TmuxCommandTest, ListPanesMalformedOutputReturnsError) {
    FakeRunner fake;
    fake.next_output = "@5|||not-an-index|||%12|||nvim|||/home/p|||1\n";
    const auto window = WindowId::from_validated("@5");

    auto panes = list_panes(window, fake.command_runner());

    ASSERT_FALSE(panes.has_value());
    EXPECT_NE(panes.error().message().find("pane.index"), std::string_view::npos);
}

TEST(TmuxCommandTest, ListPanesAllBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "@8|||1|||%13|||bash|||/tmp|||0\n";

    auto panes = list_panes_all(fake.command_runner());

    ASSERT_TRUE(panes.has_value()) << panes.error().display();
    ASSERT_EQ(panes->size(), 1U);
    EXPECT_EQ((*panes)[0].current_path, "/tmp");
    expect_one_output_call(fake, {"list-panes", "-a", "-F", std::string{kPaneFormat}});
}

TEST(TmuxCommandTest, ListClientsBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "46640|||dotfiles|||/dev/pts/2|||xterm-256color\n";

    auto clients = list_clients(fake.command_runner());

    ASSERT_TRUE(clients.has_value()) << clients.error().display();
    ASSERT_EQ(clients->size(), 1U);
    EXPECT_EQ((*clients)[0].tty.as_str(), "/dev/pts/2");
    expect_one_output_call(fake, {"list-clients", "-F", std::string{kClientFormat}});
}

TEST(TmuxCommandTest, ListClientsMalformedOutputReturnsError) {
    FakeRunner fake;
    fake.next_output = "not-a-pid|||dotfiles|||/dev/pts/2|||xterm\n";

    auto clients = list_clients(fake.command_runner());

    ASSERT_FALSE(clients.has_value());
    EXPECT_NE(clients.error().message().find("client.pid"), std::string_view::npos);
}

TEST(TmuxCommandTest, ServerInfoBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "12899|||4|||1776464199\n";

    auto info = server_info(fake.command_runner());

    ASSERT_TRUE(info.has_value()) << info.error().display();
    EXPECT_EQ(info->server_pid, 12899U);
    expect_one_output_call(fake, {"display-message", "-p", std::string{kServerInfoFormat}});
}

TEST(TmuxCommandTest, ServerInfoMalformedOutputReturnsError) {
    FakeRunner fake;
    fake.next_output = "12899|||not-a-count|||1776464199\n";

    auto info = server_info(fake.command_runner());

    ASSERT_FALSE(info.has_value());
    EXPECT_NE(info.error().message().find("server_info.sessions"), std::string_view::npos);
}

TEST(TmuxCommandTest, CapturePaneBuildsArgvAndReturnsOutput) {
    FakeRunner fake;
    fake.next_output = "pane contents\n";
    const auto pane = PaneId::from_validated("%12");

    auto out = capture_pane(pane, fake.command_runner());

    ASSERT_TRUE(out.has_value()) << out.error().display();
    EXPECT_EQ(*out, "pane contents\n");
    expect_one_output_call(fake, {"capture-pane", "-p", "-t", "%12", "-S", "-"});
}

TEST(TmuxCommandTest, ContinuumLastSaveBuildsArgvAndParsesUnixTimestamp) {
    FakeRunner fake;
    fake.next_output = "1776464199\n";

    auto timestamp = continuum_last_save(fake.command_runner());

    ASSERT_TRUE(timestamp.has_value()) << timestamp.error().display();
    ASSERT_TRUE(timestamp->has_value());
    EXPECT_EQ(**timestamp, 1776464199);
    expect_one_output_call(fake, {"show-option", "-gqv", std::string{kContinuumLastSaveOption}});
}

TEST(TmuxCommandTest, ContinuumLastSaveReturnsNulloptForEmptyOutput) {
    FakeRunner fake;
    fake.next_output = "\n";

    auto timestamp = continuum_last_save(fake.command_runner());

    ASSERT_TRUE(timestamp.has_value()) << timestamp.error().display();
    EXPECT_FALSE(timestamp->has_value());
}

TEST(TmuxCommandTest, ContinuumLastSaveMalformedOutputReturnsParseError) {
    FakeRunner fake;
    fake.next_output = "not-a-timestamp\n";

    auto timestamp = continuum_last_save(fake.command_runner());

    ASSERT_FALSE(timestamp.has_value());
    EXPECT_EQ(timestamp.error().kind(), ErrorKind::kParse);
}

TEST(TmuxCommandTest, RunnerErrorIsPropagatedWithContext) {
    FakeRunner fake;
    fake.next_output = std::unexpected(Error{ErrorKind::kTimeout, "tmux command timed out"});

    auto sessions = list_sessions(fake.command_runner());

    ASSERT_FALSE(sessions.has_value());
    EXPECT_EQ(sessions.error().kind(), ErrorKind::kTimeout);
    ASSERT_FALSE(sessions.error().contexts().empty());
    EXPECT_EQ(sessions.error().contexts().back(), "tmux list-sessions");
}

TEST(TmuxCommandTest, SwitchClientBuildsSessionTargetArgv) {
    FakeRunner fake;
    const SwitchTarget target{SessionName::from_validated("dotfiles")};

    auto result = switch_client(target, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"switch-client", "-t", "dotfiles"});
}

TEST(TmuxCommandTest, SwitchClientBuildsWindowTargetArgv) {
    FakeRunner fake;
    const SwitchTarget target{WindowId::from_validated("@5")};

    auto result = switch_client(target, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"switch-client", "-t", "@5"});
}

TEST(TmuxCommandTest, SwitchClientBuildsPaneTargetArgv) {
    FakeRunner fake;
    const SwitchTarget target{PaneId::from_validated("%12")};

    auto result = switch_client(target, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"switch-client", "-t", "%12"});
}

TEST(TmuxCommandTest, DetachClientBuildsArgv) {
    FakeRunner fake;
    const auto tty = ClientTty::from_validated("/dev/pts/2");

    auto result = detach_client(tty, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"detach-client", "-t", "/dev/pts/2"});
}

TEST(TmuxCommandTest, SessionExistsReturnsTrueAndFalse) {
    FakeRunner fake;
    const auto name = SessionName::from_validated("dotfiles");

    fake.next_status = true;
    auto exists = session_exists(name, fake.command_runner());
    ASSERT_TRUE(exists.has_value()) << exists.error().display();
    EXPECT_TRUE(*exists);
    expect_one_status_call(fake, {"has-session", "-t", "dotfiles"});

    fake.status_calls.clear();
    fake.next_status = false;
    exists = session_exists(name, fake.command_runner());
    ASSERT_TRUE(exists.has_value()) << exists.error().display();
    EXPECT_FALSE(*exists);
    expect_one_status_call(fake, {"has-session", "-t", "dotfiles"});
}

TEST(TmuxCommandTest, SessionExistsPropagatesRunnerError) {
    FakeRunner fake;
    fake.next_status = std::unexpected(Error{ErrorKind::kTimeout, "tmux timed out"});
    const auto name = SessionName::from_validated("dotfiles");

    auto exists = session_exists(name, fake.command_runner());

    ASSERT_FALSE(exists.has_value());
    EXPECT_EQ(exists.error().kind(), ErrorKind::kTimeout);
    ASSERT_FALSE(exists.error().contexts().empty());
    EXPECT_EQ(exists.error().contexts().back(), "tmux has-session");
}

TEST(TmuxCommandTest, NewSessionBuildsDetachedArgvWithPathContainingSpaces) {
    FakeRunner fake;
    const auto name = SessionName::from_validated("dotfiles");

    auto result = new_session(
        name, std::filesystem::path{"/home/p/project one"}, true, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake,
                           {"new-session", "-d", "-s", "dotfiles", "-c", "/home/p/project one"});
}

TEST(TmuxCommandTest, NewSessionBuildsAttachedArgv) {
    FakeRunner fake;
    const auto name = SessionName::from_validated("scratch");

    auto result = new_session(name, std::filesystem::path{"/tmp"}, false, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"new-session", "-s", "scratch", "-c", "/tmp"});
}

TEST(TmuxCommandTest, NewSessionWithIdsBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "@5|||%12\n";
    const auto name = SessionName::from_validated("scratch");

    auto created = new_session_with_ids(name,
                                        "main window",
                                        std::filesystem::path{"/tmp/project one"},
                                        true,
                                        fake.command_runner());

    ASSERT_TRUE(created.has_value()) << created.error().display();
    EXPECT_EQ(created->window_id.as_str(), "@5");
    EXPECT_EQ(created->pane_id.as_str(), "%12");
    expect_one_output_call(fake,
                           {"new-session",
                            "-d",
                            "-s",
                            "scratch",
                            "-n",
                            "main window",
                            "-c",
                            "/tmp/project one",
                            "-P",
                            "-F",
                            std::string{kCreatedPaneFormat}});
}

TEST(TmuxCommandTest, NewSessionWithIdsRejectsMalformedOutput) {
    FakeRunner fake;
    fake.next_output = "@5\n";
    const auto name = SessionName::from_validated("scratch");

    auto created = new_session_with_ids(
        name, "main", std::filesystem::path{"/tmp"}, true, fake.command_runner());

    ASSERT_FALSE(created.has_value());
    EXPECT_EQ(created.error().kind(), ErrorKind::kParse);
}

TEST(TmuxCommandTest, RenameSessionBuildsArgv) {
    FakeRunner fake;
    const auto old_name = SessionName::from_validated("old");
    const auto new_name = SessionName::from_validated("new");

    auto result = rename_session(old_name, new_name, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"rename-session", "-t", "old", "new"});
}

TEST(TmuxCommandTest, KillSessionBuildsArgv) {
    FakeRunner fake;
    const auto name = SessionName::from_validated("dotfiles");

    auto result = kill_session(name, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"kill-session", "-t", "dotfiles"});
}

TEST(TmuxCommandTest, MutationFalseStatusBecomesExternalCommandError) {
    FakeRunner fake;
    fake.next_status = false;
    const auto name = SessionName::from_validated("dotfiles");

    auto result = kill_session(name, fake.command_runner());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind(), ErrorKind::kExternalCommandFailure);
    EXPECT_NE(result.error().message().find("kill-session"), std::string_view::npos);
}

TEST(TmuxCommandTest, NewWindowBuildsArgvWithSpaces) {
    FakeRunner fake;
    const auto session = SessionName::from_validated("dotfiles");

    auto result = new_window(
        session, "work tree", std::filesystem::path{"/home/p/project one"}, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(
        fake, {"new-window", "-t", "dotfiles", "-n", "work tree", "-c", "/home/p/project one"});
}

TEST(TmuxCommandTest, NewWindowWithIdsBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "@8|||%20\n";
    const auto session = SessionName::from_validated("dotfiles");

    auto created = new_window_with_ids(
        session, "work tree", std::filesystem::path{"/home/p/project one"}, fake.command_runner());

    ASSERT_TRUE(created.has_value()) << created.error().display();
    EXPECT_EQ(created->window_id.as_str(), "@8");
    EXPECT_EQ(created->pane_id.as_str(), "%20");
    expect_one_output_call(fake,
                           {"new-window",
                            "-t",
                            "dotfiles",
                            "-n",
                            "work tree",
                            "-c",
                            "/home/p/project one",
                            "-P",
                            "-F",
                            std::string{kCreatedPaneFormat}});
}

TEST(TmuxCommandTest, RenameWindowBuildsArgvWithSpaces) {
    FakeRunner fake;
    const auto window = WindowId::from_validated("@5");

    auto result = rename_window(window, "build logs", fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"rename-window", "-t", "@5", "build logs"});
}

TEST(TmuxCommandTest, KillWindowBuildsArgv) {
    FakeRunner fake;
    const auto window = WindowId::from_validated("@5");

    auto result = kill_window(window, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"kill-window", "-t", "@5"});
}

TEST(TmuxCommandTest, SwapWindowsBuildsArgv) {
    FakeRunner fake;
    const auto lhs = WindowId::from_validated("@5");
    const auto rhs = WindowId::from_validated("@8");

    auto result = swap_windows(lhs, rhs, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"swap-window", "-s", "@5", "-t", "@8"});
}

TEST(TmuxCommandTest, MoveWindowBuildsDestinationSessionArgv) {
    FakeRunner fake;
    const auto window = WindowId::from_validated("@5");
    const auto session = SessionName::from_validated("api");

    auto result = move_window(window, session, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"move-window", "-s", "@5", "-t", "api:"});
}

TEST(TmuxCommandTest, SplitPaneBuildsHorizontalArgvWithPathContainingSpaces) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = split_pane(pane,
                             std::filesystem::path{"/home/p/project one"},
                             SplitDirection::kHorizontal,
                             fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"split-window", "-h", "-t", "%12", "-c", "/home/p/project one"});
}

TEST(TmuxCommandTest, SplitPaneBuildsVerticalArgv) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = split_pane(
        pane, std::filesystem::path{"/tmp"}, SplitDirection::kVertical, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"split-window", "-v", "-t", "%12", "-c", "/tmp"});
}

TEST(TmuxCommandTest, SplitPaneWithIdBuildsArgvAndParsesOutput) {
    FakeRunner fake;
    fake.next_output = "%20\n";
    const auto pane = PaneId::from_validated("%12");

    auto created = split_pane_with_id(
        pane, std::filesystem::path{"/tmp"}, SplitDirection::kVertical, fake.command_runner());

    ASSERT_TRUE(created.has_value()) << created.error().display();
    EXPECT_EQ(created->as_str(), "%20");
    expect_one_output_call(
        fake, {"split-window", "-v", "-t", "%12", "-c", "/tmp", "-P", "-F", "#{pane_id}"});
}

TEST(TmuxCommandTest, KillPaneBuildsArgv) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = kill_pane(pane, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"kill-pane", "-t", "%12"});
}

TEST(TmuxCommandTest, RotatePanesBuildsDownAndUpArgv) {
    FakeRunner fake;
    const auto window = WindowId::from_validated("@5");

    auto result = rotate_panes(window, RotateDirection::kDown, fake.command_runner());
    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"rotate-window", "-D", "-t", "@5"});

    fake.status_calls.clear();
    result = rotate_panes(window, RotateDirection::kUp, fake.command_runner());
    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"rotate-window", "-U", "-t", "@5"});
}

TEST(TmuxCommandTest, BreakPaneBuildsArgv) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = break_pane(pane, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"break-pane", "-t", "%12"});
}

TEST(TmuxCommandTest, JoinPaneBuildsArgv) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");
    const auto window = WindowId::from_validated("@5");

    auto result = join_pane(pane, window, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"join-pane", "-s", "%12", "-t", "@5"});
}

TEST(TmuxCommandTest, SelectLayoutBuildsArgv) {
    FakeRunner fake;
    const auto window = WindowId::from_validated("@5");

    auto result = select_layout(window, Layout::kTiled, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"select-layout", "-t", "@5", "tiled"});
}

TEST(TmuxCommandTest, ToggleZoomBuildsArgv) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = toggle_zoom(pane, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"resize-pane", "-Z", "-t", "%12"});
}

TEST(TmuxCommandTest, SendKeysBuildsLiteralTextArgvWithoutEnter) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = send_keys(pane, "echo hello", SendEnter::kNo, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"send-keys", "-t", "%12", "--", "echo hello"});
}

TEST(TmuxCommandTest, SendKeysBuildsLiteralTextArgvWithEnter) {
    FakeRunner fake;
    const auto pane = PaneId::from_validated("%12");

    auto result = send_keys(pane, "make test", SendEnter::kYes, fake.command_runner());

    ASSERT_TRUE(result.has_value()) << result.error().display();
    expect_one_status_call(fake, {"send-keys", "-t", "%12", "--", "make test", "Enter"});
}

}  // namespace
}  // namespace lazytmux::tmux
