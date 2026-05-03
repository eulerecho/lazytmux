#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/run.hpp>
#include <lazytmux/tmux/types.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// @file
/// @brief Typed tmux command helpers built on @ref tmux::run.

namespace lazytmux::tmux {

/// @brief tmux-continuum option that stores the last save as Unix seconds.
inline constexpr std::string_view kContinuumLastSaveOption = "@continuum-save-last-timestamp";

/// @brief Format string used by create helpers that return stable ids.
inline constexpr std::string_view kCreatedPaneFormat = "#{window_id}|||#{pane_id}";

/// @brief Injectable command runner used by hermetic command-helper tests.
///
/// Production callers normally use the overloads that accept only
/// @ref RunConfig. Tests can pass a @ref CommandRunner that records
/// argv/config and returns canned stdout or status values.
struct CommandRunner {
    /// @brief Function used by helpers that need stdout.
    std::function<Result<std::string>(std::span<const std::string>, const RunConfig&)> run;

    /// @brief Function used by predicate/mutation helpers.
    std::function<Result<bool>(std::span<const std::string>, const RunConfig&)> run_status;
};

/// @brief Target accepted by `tmux switch-client -t`.
using SwitchTarget = std::variant<SessionName, WindowId, PaneId>;

/// @brief Stable ids returned by tmux when a command creates a window and pane.
struct CreatedPane {
    WindowId window_id;
    PaneId pane_id;

    auto operator<=>(const CreatedPane&) const = default;
    bool operator==(const CreatedPane&) const = default;
};

/// @brief Production runner backed by @ref run and @ref run_status.
[[nodiscard]] CommandRunner default_command_runner();

/// @brief List tmux sessions.
[[nodiscard]] Result<std::vector<Session>> list_sessions(const RunConfig& config = {});

/// @brief List tmux sessions through an injected runner.
[[nodiscard]] Result<std::vector<Session>> list_sessions(const CommandRunner& runner,
                                                         const RunConfig& config = {});

/// @brief List windows in one session.
[[nodiscard]] Result<std::vector<Window>> list_windows(const SessionName& session,
                                                       const RunConfig& config = {});

/// @brief List windows in one session through an injected runner.
[[nodiscard]] Result<std::vector<Window>> list_windows(const SessionName& session,
                                                       const CommandRunner& runner,
                                                       const RunConfig& config = {});

/// @brief List windows across all sessions.
[[nodiscard]] Result<std::vector<Window>> list_windows_all(const RunConfig& config = {});

/// @brief List windows across all sessions through an injected runner.
[[nodiscard]] Result<std::vector<Window>> list_windows_all(const CommandRunner& runner,
                                                           const RunConfig& config = {});

/// @brief List panes in one window.
[[nodiscard]] Result<std::vector<Pane>> list_panes(const WindowId& window,
                                                   const RunConfig& config = {});

/// @brief List panes in one window through an injected runner.
[[nodiscard]] Result<std::vector<Pane>> list_panes(const WindowId& window,
                                                   const CommandRunner& runner,
                                                   const RunConfig& config = {});

/// @brief List panes across all windows.
[[nodiscard]] Result<std::vector<Pane>> list_panes_all(const RunConfig& config = {});

/// @brief List panes across all windows through an injected runner.
[[nodiscard]] Result<std::vector<Pane>> list_panes_all(const CommandRunner& runner,
                                                       const RunConfig& config = {});

/// @brief List attached tmux clients.
[[nodiscard]] Result<std::vector<Client>> list_clients(const RunConfig& config = {});

/// @brief List attached tmux clients through an injected runner.
[[nodiscard]] Result<std::vector<Client>> list_clients(const CommandRunner& runner,
                                                       const RunConfig& config = {});

/// @brief Read server-level metadata.
[[nodiscard]] Result<ServerInfo> server_info(const RunConfig& config = {});

/// @brief Read server-level metadata through an injected runner.
[[nodiscard]] Result<ServerInfo> server_info(const CommandRunner& runner,
                                             const RunConfig& config = {});

/// @brief Capture one pane's scrollback and visible content as text.
[[nodiscard]] Result<std::string> capture_pane(const PaneId& pane, const RunConfig& config = {});

/// @brief Capture one pane through an injected runner.
[[nodiscard]] Result<std::string> capture_pane(const PaneId& pane,
                                               const CommandRunner& runner,
                                               const RunConfig& config = {});

/// @brief Read tmux-continuum's last save timestamp as Unix seconds.
///
/// Returns @c std::nullopt when the option is missing or empty.
[[nodiscard]] Result<std::optional<std::int64_t>> continuum_last_save(const RunConfig& config = {});

/// @brief Read tmux-continuum's last save timestamp through an injected runner.
[[nodiscard]] Result<std::optional<std::int64_t>> continuum_last_save(const CommandRunner& runner,
                                                                      const RunConfig& config = {});

/// @brief Run a shell command through tmux.
[[nodiscard]] Result<void> run_shell(const std::filesystem::path& command,
                                     const RunConfig& config = {});

/// @brief Run a shell command through tmux with an injected runner.
[[nodiscard]] Result<void> run_shell(const std::filesystem::path& command,
                                     const CommandRunner& runner,
                                     const RunConfig& config = {});

/// @brief Switch the current tmux client to a session, window, or pane target.
[[nodiscard]] Result<void> switch_client(const SwitchTarget& target, const RunConfig& config = {});

/// @brief Switch the current tmux client through an injected runner.
[[nodiscard]] Result<void> switch_client(const SwitchTarget& target,
                                         const CommandRunner& runner,
                                         const RunConfig& config = {});

/// @brief Detach one tmux client by tty.
[[nodiscard]] Result<void> detach_client(const ClientTty& tty, const RunConfig& config = {});

/// @brief Detach one tmux client through an injected runner.
[[nodiscard]] Result<void> detach_client(const ClientTty& tty,
                                         const CommandRunner& runner,
                                         const RunConfig& config = {});

/// @brief Check whether a tmux session exists.
[[nodiscard]] Result<bool> session_exists(const SessionName& name, const RunConfig& config = {});

/// @brief Check whether a tmux session exists through an injected runner.
[[nodiscard]] Result<bool> session_exists(const SessionName& name,
                                          const CommandRunner& runner,
                                          const RunConfig& config = {});

/// @brief Create a tmux session rooted at @p path.
[[nodiscard]] Result<void> new_session(const SessionName& name,
                                       const std::filesystem::path& path,
                                       bool detached,
                                       const RunConfig& config = {});

/// @brief Create a tmux session through an injected runner.
[[nodiscard]] Result<void> new_session(const SessionName& name,
                                       const std::filesystem::path& path,
                                       bool detached,
                                       const CommandRunner& runner,
                                       const RunConfig& config = {});

/// @brief Create a tmux session and return its initial window/pane ids.
[[nodiscard]] Result<CreatedPane> new_session_with_ids(const SessionName& name,
                                                       std::string window_name,
                                                       const std::filesystem::path& path,
                                                       bool detached,
                                                       const RunConfig& config = {});

/// @brief Create a tmux session with id output through an injected runner.
[[nodiscard]] Result<CreatedPane> new_session_with_ids(const SessionName& name,
                                                       std::string window_name,
                                                       const std::filesystem::path& path,
                                                       bool detached,
                                                       const CommandRunner& runner,
                                                       const RunConfig& config = {});

/// @brief Rename a tmux session.
[[nodiscard]] Result<void> rename_session(const SessionName& old_name,
                                          const SessionName& new_name,
                                          const RunConfig& config = {});

/// @brief Rename a tmux session through an injected runner.
[[nodiscard]] Result<void> rename_session(const SessionName& old_name,
                                          const SessionName& new_name,
                                          const CommandRunner& runner,
                                          const RunConfig& config = {});

/// @brief Kill one tmux session.
[[nodiscard]] Result<void> kill_session(const SessionName& name, const RunConfig& config = {});

/// @brief Kill one tmux session through an injected runner.
[[nodiscard]] Result<void> kill_session(const SessionName& name,
                                        const CommandRunner& runner,
                                        const RunConfig& config = {});

/// @brief Create a new window in @p session rooted at @p path.
[[nodiscard]] Result<void> new_window(const SessionName& session,
                                      std::string name,
                                      const std::filesystem::path& path,
                                      const RunConfig& config = {});

/// @brief Create a new window through an injected runner.
[[nodiscard]] Result<void> new_window(const SessionName& session,
                                      std::string name,
                                      const std::filesystem::path& path,
                                      const CommandRunner& runner,
                                      const RunConfig& config = {});

/// @brief Create a new window and return its window/pane ids.
[[nodiscard]] Result<CreatedPane> new_window_with_ids(const SessionName& session,
                                                      std::string name,
                                                      const std::filesystem::path& path,
                                                      const RunConfig& config = {});

/// @brief Create a new window with id output through an injected runner.
[[nodiscard]] Result<CreatedPane> new_window_with_ids(const SessionName& session,
                                                      std::string name,
                                                      const std::filesystem::path& path,
                                                      const CommandRunner& runner,
                                                      const RunConfig& config = {});

/// @brief Rename a tmux window.
[[nodiscard]] Result<void> rename_window(const WindowId& window,
                                         std::string name,
                                         const RunConfig& config = {});

/// @brief Rename a tmux window through an injected runner.
[[nodiscard]] Result<void> rename_window(const WindowId& window,
                                         std::string name,
                                         const CommandRunner& runner,
                                         const RunConfig& config = {});

/// @brief Kill one tmux window.
[[nodiscard]] Result<void> kill_window(const WindowId& window, const RunConfig& config = {});

/// @brief Kill one tmux window through an injected runner.
[[nodiscard]] Result<void> kill_window(const WindowId& window,
                                       const CommandRunner& runner,
                                       const RunConfig& config = {});

/// @brief Swap two tmux windows.
[[nodiscard]] Result<void> swap_windows(const WindowId& lhs,
                                        const WindowId& rhs,
                                        const RunConfig& config = {});

/// @brief Swap two tmux windows through an injected runner.
[[nodiscard]] Result<void> swap_windows(const WindowId& lhs,
                                        const WindowId& rhs,
                                        const CommandRunner& runner,
                                        const RunConfig& config = {});

/// @brief Move a window into @p destination_session.
[[nodiscard]] Result<void> move_window(const WindowId& window,
                                       const SessionName& destination_session,
                                       const RunConfig& config = {});

/// @brief Move a window through an injected runner.
[[nodiscard]] Result<void> move_window(const WindowId& window,
                                       const SessionName& destination_session,
                                       const CommandRunner& runner,
                                       const RunConfig& config = {});

/// @brief Split direction for `tmux split-window`.
enum class SplitDirection {
    /// @brief `split-window -h`.
    kHorizontal,
    /// @brief `split-window -v`.
    kVertical,
};

/// @brief Pane rotation direction for `tmux rotate-window`.
enum class RotateDirection {
    /// @brief `rotate-window -D`.
    kDown,
    /// @brief `rotate-window -U`.
    kUp,
};

/// @brief Layout names accepted by `tmux select-layout`.
enum class Layout {
    kEvenHorizontal,
    kEvenVertical,
    kMainHorizontal,
    kMainVertical,
    kTiled,
};

/// @brief Whether `send_keys` appends an Enter key after literal text.
enum class SendEnter {
    kNo,
    kYes,
};

/// @brief Split one pane, creating the new pane rooted at @p path.
[[nodiscard]] Result<void> split_pane(const PaneId& pane,
                                      const std::filesystem::path& path,
                                      SplitDirection direction,
                                      const RunConfig& config = {});

/// @brief Split one pane through an injected runner.
[[nodiscard]] Result<void> split_pane(const PaneId& pane,
                                      const std::filesystem::path& path,
                                      SplitDirection direction,
                                      const CommandRunner& runner,
                                      const RunConfig& config = {});

/// @brief Split one pane and return the created pane id.
[[nodiscard]] Result<PaneId> split_pane_with_id(const PaneId& pane,
                                                const std::filesystem::path& path,
                                                SplitDirection direction,
                                                const RunConfig& config = {});

/// @brief Split one pane with id output through an injected runner.
[[nodiscard]] Result<PaneId> split_pane_with_id(const PaneId& pane,
                                                const std::filesystem::path& path,
                                                SplitDirection direction,
                                                const CommandRunner& runner,
                                                const RunConfig& config = {});

/// @brief Kill one tmux pane.
[[nodiscard]] Result<void> kill_pane(const PaneId& pane, const RunConfig& config = {});

/// @brief Kill one tmux pane through an injected runner.
[[nodiscard]] Result<void> kill_pane(const PaneId& pane,
                                     const CommandRunner& runner,
                                     const RunConfig& config = {});

/// @brief Rotate panes in one window.
[[nodiscard]] Result<void> rotate_panes(const WindowId& window,
                                        RotateDirection direction,
                                        const RunConfig& config = {});

/// @brief Rotate panes through an injected runner.
[[nodiscard]] Result<void> rotate_panes(const WindowId& window,
                                        RotateDirection direction,
                                        const CommandRunner& runner,
                                        const RunConfig& config = {});

/// @brief Break one pane into a new window.
[[nodiscard]] Result<void> break_pane(const PaneId& pane, const RunConfig& config = {});

/// @brief Break one pane through an injected runner.
[[nodiscard]] Result<void> break_pane(const PaneId& pane,
                                      const CommandRunner& runner,
                                      const RunConfig& config = {});

/// @brief Join one pane into a destination window.
[[nodiscard]] Result<void> join_pane(const PaneId& pane,
                                     const WindowId& destination,
                                     const RunConfig& config = {});

/// @brief Join one pane through an injected runner.
[[nodiscard]] Result<void> join_pane(const PaneId& pane,
                                     const WindowId& destination,
                                     const CommandRunner& runner,
                                     const RunConfig& config = {});

/// @brief Select a tmux layout for one window.
[[nodiscard]] Result<void> select_layout(const WindowId& window,
                                         Layout layout,
                                         const RunConfig& config = {});

/// @brief Select a tmux layout through an injected runner.
[[nodiscard]] Result<void> select_layout(const WindowId& window,
                                         Layout layout,
                                         const CommandRunner& runner,
                                         const RunConfig& config = {});

/// @brief Toggle zoom for one pane.
[[nodiscard]] Result<void> toggle_zoom(const PaneId& pane, const RunConfig& config = {});

/// @brief Toggle zoom through an injected runner.
[[nodiscard]] Result<void> toggle_zoom(const PaneId& pane,
                                       const CommandRunner& runner,
                                       const RunConfig& config = {});

/// @brief Send literal text to one pane and optionally append Enter.
[[nodiscard]] Result<void> send_keys(const PaneId& pane,
                                     std::string keys,
                                     SendEnter enter,
                                     const RunConfig& config = {});

/// @brief Send literal text through an injected runner.
[[nodiscard]] Result<void> send_keys(const PaneId& pane,
                                     std::string keys,
                                     SendEnter enter,
                                     const CommandRunner& runner,
                                     const RunConfig& config = {});

}  // namespace lazytmux::tmux
