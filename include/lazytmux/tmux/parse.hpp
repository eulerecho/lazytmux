#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/tmux/types.hpp>

#include <string_view>
#include <vector>

/// @file
/// @brief Parsers for the `|||`-delimited format-string output of
/// `tmux list-sessions`, `list-windows`, `list-panes`,
/// `list-clients`, and `display-message`.

namespace lazytmux::tmux {

/// @brief The field delimiter used in every tmux format-string
/// constant in this header. Chosen so it cannot appear inside
/// any field tmux emits — three consecutive pipes are not
/// produced by any tmux format substitution.
inline constexpr std::string_view kFieldDelim = "|||";

/// @brief Format string passed to `tmux list-sessions -F`.
/// Parsed by @ref parse_session_row / @ref parse_sessions.
inline constexpr std::string_view kSessionFormat =
    "#{session_id}|||#{session_name}|||#{session_windows}"
    "|||#{?session_attached,1,0}|||#{session_activity}";

/// @brief Format string passed to `tmux list-windows -F`.
inline constexpr std::string_view kWindowFormat =
    "#{session_name}|||#{window_index}|||#{window_id}"
    "|||#{window_name}|||#{?window_active,1,0}|||#{window_panes}";

/// @brief Format string passed to `tmux list-panes -F`.
inline constexpr std::string_view kPaneFormat =
    "#{window_id}|||#{pane_index}|||#{pane_id}"
    "|||#{pane_current_command}|||#{pane_current_path}"
    "|||#{?pane_active,1,0}";

/// @brief Format string passed to `tmux list-clients -F`.
inline constexpr std::string_view kClientFormat =
    "#{client_pid}|||#{client_session}|||#{client_tty}"
    "|||#{client_termname}";

/// @brief Format string passed to `tmux display-message -p`.
inline constexpr std::string_view kServerInfoFormat = "#{pid}|||#{server_sessions}|||#{start_time}";

/// @brief Parse a single `|||`-delimited row produced by
/// `tmux list-sessions -F kSessionFormat`.
/// @param row One line of tmux output, no trailing newline.
/// @return Parsed @ref Session on success.
/// @return @ref Error on field-count mismatch or integer parse
///         failure, with `with_context` annotating the offending
///         field name.
[[nodiscard]] Result<Session> parse_session_row(std::string_view row);

/// @brief Parse a single `tmux list-windows -F kWindowFormat`
/// row.
[[nodiscard]] Result<Window> parse_window_row(std::string_view row);

/// @brief Parse a single `tmux list-panes -F kPaneFormat` row.
/// Tabs embedded in `pane_current_path` are preserved verbatim.
[[nodiscard]] Result<Pane> parse_pane_row(std::string_view row);

/// @brief Parse a single `tmux list-clients -F kClientFormat`
/// row.
[[nodiscard]] Result<Client> parse_client_row(std::string_view row);

/// @brief Parse the single line produced by
/// `tmux display-message -p kServerInfoFormat`.
[[nodiscard]] Result<ServerInfo> parse_server_info_row(std::string_view row);

/// @brief Parse the full `tmux list-sessions -F kSessionFormat`
/// output. Empty/blank lines are silently skipped. The first
/// row that fails to parse aborts the run and surfaces with
/// `session row N` context appended.
[[nodiscard]] Result<std::vector<Session>> parse_sessions(std::string_view out);

[[nodiscard]] Result<std::vector<Window>> parse_windows(std::string_view out);

[[nodiscard]] Result<std::vector<Pane>> parse_panes(std::string_view out);

[[nodiscard]] Result<std::vector<Client>> parse_clients(std::string_view out);

/// @brief Parse a single-line `tmux display-message -p` output
/// into a @ref ServerInfo. Equivalent to
/// @ref parse_server_info_row applied to the trimmed line.
[[nodiscard]] Result<ServerInfo> parse_server_info(std::string_view out);

}  // namespace lazytmux::tmux
