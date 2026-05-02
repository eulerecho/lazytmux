#include <lazytmux/config.hpp>
#include <lazytmux/io/process.hpp>
#include <lazytmux/templates.hpp>
#include <lazytmux/tmux/commands.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/run.hpp>
#include <lazytmux/tmux/types.hpp>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace lazytmux {
namespace {

using namespace std::chrono_literals;

constexpr int kPollAttempts = 50;
constexpr auto kPollDelay = 20ms;

bool tmux_available() {
    auto version = io::run_command(std::vector<std::string>{"tmux", "-V"});
    return version && version->exit_code == 0;
}

class LiveTmuxServer {
public:
    explicit LiveTmuxServer(std::string_view test_name) {
        name_prefix_ = "ltlive_" + std::to_string(::getpid()) + "_" + std::string{test_name};
        root_ = std::filesystem::temp_directory_path() /
                ("lazytmux-live-" + std::to_string(::getpid()) + "-" + std::string{test_name});
        socket_path_ = root_ / "tmux.sock";
        std::filesystem::remove_all(root_);
        std::filesystem::create_directories(root_);

        config_.socket = tmux::SocketSelection::path(socket_path_.string());
        config_.command_options.timeout = 5s;
    }

    LiveTmuxServer(const LiveTmuxServer&) = delete;
    LiveTmuxServer& operator=(const LiveTmuxServer&) = delete;

    ~LiveTmuxServer() {
        const std::vector<std::string> args{"kill-server"};
        auto ignored = tmux::run_status(args, config_);
        (void)ignored;
        std::filesystem::remove_all(root_);
    }

    [[nodiscard]] const tmux::RunConfig& config() const noexcept {
        return config_;
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    [[nodiscard]] Result<void> start() const {
        const auto bootstrap = session("bootstrap");
        const auto socket = socket_path_.string();
        const auto cwd = root_.string();
        const auto name = std::string{bootstrap.as_str()};

        const pid_t child = ::fork();
        if (child == -1) {
            return std::unexpected(Error{
                ErrorKind::kIo,
                std::string{"failed to fork tmux bootstrap: "} + std::strerror(errno),
            });
        }

        if (child == 0) {
            const int devnull = ::open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                ::dup2(devnull, STDIN_FILENO);
                ::dup2(devnull, STDOUT_FILENO);
                ::dup2(devnull, STDERR_FILENO);
                if (devnull > STDERR_FILENO) {
                    ::close(devnull);
                }
            }
            ::execlp("tmux",
                     "tmux",
                     "-S",
                     socket.c_str(),
                     "new-session",
                     "-d",
                     "-s",
                     name.c_str(),
                     "-c",
                     cwd.c_str(),
                     static_cast<char*>(nullptr));
            ::_exit(127);
        }

        int status = 0;
        pid_t waited = 0;
        do {
            waited = ::waitpid(child, &status, 0);
        } while (waited == -1 && errno == EINTR);

        if (waited == -1) {
            return std::unexpected(Error{
                ErrorKind::kIo,
                std::string{"failed to wait for tmux bootstrap: "} + std::strerror(errno),
            });
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            return std::unexpected(Error{
                ErrorKind::kExternalCommandFailure,
                "tmux bootstrap failed",
            });
        }
        return {};
    }

    [[nodiscard]] tmux::SessionName session(std::string_view suffix) const {
        auto name = tmux::SessionName::make(name_prefix_ + "_" + std::string{suffix});
        EXPECT_TRUE(name.has_value()) << name.error().display();
        return std::move(name).value();
    }

private:
    std::string name_prefix_;
    std::filesystem::path root_;
    std::filesystem::path socket_path_;
    tmux::RunConfig config_;
};

bool has_session(const std::vector<tmux::Session>& sessions, const tmux::SessionName& name) {
    return std::ranges::any_of(sessions,
                               [&](const tmux::Session& session) { return session.name == name; });
}

std::optional<tmux::Window> find_window(const std::vector<tmux::Window>& windows,
                                        std::string_view name) {
    const auto it = std::ranges::find_if(
        windows, [&](const tmux::Window& window) { return window.name == name; });
    if (it == windows.end()) {
        return std::nullopt;
    }
    return *it;
}

bool capture_eventually_contains(const tmux::PaneId& pane,
                                 const tmux::RunConfig& config,
                                 std::string_view needle) {
    for (int i = 0; i < kPollAttempts; ++i) {
        auto captured = tmux::capture_pane(pane, config);
        if (captured && captured->find(needle) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(kPollDelay);
    }
    return false;
}

TEST(LiveTmuxTest, StartsScratchServerAndListsSessions) {
    if (!tmux_available()) {
        GTEST_SKIP() << "tmux executable is not available";
    }
    LiveTmuxServer server{"list"};
    auto started = server.start();
    ASSERT_TRUE(started.has_value()) << started.error().display();
    const auto session = server.session("list");

    auto created = tmux::new_session(session, server.root(), true, server.config());
    ASSERT_TRUE(created.has_value()) << created.error().display();

    auto sessions = tmux::list_sessions(server.config());
    ASSERT_TRUE(sessions.has_value()) << sessions.error().display();
    EXPECT_TRUE(has_session(*sessions, session));

    auto info = tmux::server_info(server.config());
    ASSERT_TRUE(info.has_value()) << info.error().display();
    EXPECT_GE(info->session_count, 1U);
}

TEST(LiveTmuxTest, RenamesSessionAndChecksExists) {
    if (!tmux_available()) {
        GTEST_SKIP() << "tmux executable is not available";
    }
    LiveTmuxServer server{"rename"};
    auto started = server.start();
    ASSERT_TRUE(started.has_value()) << started.error().display();
    const auto old_name = server.session("old");
    const auto new_name = server.session("new");

    auto created = tmux::new_session(old_name, server.root(), true, server.config());
    ASSERT_TRUE(created.has_value()) << created.error().display();

    auto old_exists = tmux::session_exists(old_name, server.config());
    ASSERT_TRUE(old_exists.has_value()) << old_exists.error().display();
    EXPECT_TRUE(*old_exists);

    auto renamed = tmux::rename_session(old_name, new_name, server.config());
    ASSERT_TRUE(renamed.has_value()) << renamed.error().display();

    auto renamed_exists = tmux::session_exists(new_name, server.config());
    ASSERT_TRUE(renamed_exists.has_value()) << renamed_exists.error().display();
    EXPECT_TRUE(*renamed_exists);

    auto killed = tmux::kill_session(new_name, server.config());
    ASSERT_TRUE(killed.has_value()) << killed.error().display();

    auto exists_after_kill = tmux::session_exists(new_name, server.config());
    ASSERT_TRUE(exists_after_kill.has_value()) << exists_after_kill.error().display();
    EXPECT_FALSE(*exists_after_kill);
}

TEST(LiveTmuxTest, CreatesWindowSplitsPaneCapturesAndKillsPane) {
    if (!tmux_available()) {
        GTEST_SKIP() << "tmux executable is not available";
    }
    LiveTmuxServer server{"pane"};
    auto started = server.start();
    ASSERT_TRUE(started.has_value()) << started.error().display();
    const auto session = server.session("pane");

    auto created =
        tmux::new_session_with_ids(session, "main", server.root(), true, server.config());
    ASSERT_TRUE(created.has_value()) << created.error().display();

    auto pane = tmux::split_pane_with_id(
        created->pane_id, server.root(), tmux::SplitDirection::kVertical, server.config());
    ASSERT_TRUE(pane.has_value()) << pane.error().display();

    auto sent =
        tmux::send_keys(*pane, "printf lazytmux-live-pane", tmux::SendEnter::kYes, server.config());
    ASSERT_TRUE(sent.has_value()) << sent.error().display();
    EXPECT_TRUE(capture_eventually_contains(*pane, server.config(), "lazytmux-live-pane"));

    auto killed = tmux::kill_pane(*pane, server.config());
    ASSERT_TRUE(killed.has_value()) << killed.error().display();

    auto panes = tmux::list_panes(created->window_id, server.config());
    ASSERT_TRUE(panes.has_value()) << panes.error().display();
    EXPECT_EQ(panes->size(), 1U);
}

TEST(LiveTmuxTest, MaterializesTwoWindowTemplate) {
    if (!tmux_available()) {
        GTEST_SKIP() << "tmux executable is not available";
    }
    LiveTmuxServer server{"template"};
    auto started = server.start();
    ASSERT_TRUE(started.has_value()) << started.error().display();
    const auto session = server.session("template");
    const auto tmpl = config::Template{.windows = {
                                           config::TemplateWindow{
                                               .name = "editor",
                                               .command = "printf lazytmux-live-editor",
                                           },
                                           config::TemplateWindow{
                                               .name = "logs",
                                               .command = "printf lazytmux-live-logs",
                                           },
                                       }};

    auto materialized =
        templates::materialize_template(session, tmpl, server.root(), server.config());
    ASSERT_TRUE(materialized.has_value()) << materialized.error().display();

    auto windows = tmux::list_windows(session, server.config());
    ASSERT_TRUE(windows.has_value()) << windows.error().display();
    ASSERT_EQ(windows->size(), 2U);

    auto editor = find_window(*windows, "editor");
    auto logs = find_window(*windows, "logs");
    ASSERT_TRUE(editor.has_value());
    ASSERT_TRUE(logs.has_value());

    auto editor_panes = tmux::list_panes(editor->id, server.config());
    ASSERT_TRUE(editor_panes.has_value()) << editor_panes.error().display();
    ASSERT_EQ(editor_panes->size(), 1U);
    EXPECT_TRUE(capture_eventually_contains(
        editor_panes->front().id, server.config(), "lazytmux-live-editor"));

    auto log_panes = tmux::list_panes(logs->id, server.config());
    ASSERT_TRUE(log_panes.has_value()) << log_panes.error().display();
    ASSERT_EQ(log_panes->size(), 1U);
    EXPECT_TRUE(
        capture_eventually_contains(log_panes->front().id, server.config(), "lazytmux-live-logs"));
}

}  // namespace
}  // namespace lazytmux
