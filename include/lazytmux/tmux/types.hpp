#pragma once

#include <lazytmux/tmux/ids.hpp>

#include <compare>
#include <cstdint>
#include <string>

/// @file
/// @brief Plain-old-data value types for tmux entities.
///
/// Each struct is a designated-initializer-friendly aggregate
/// holding the fields the prototype's `tmux list-* -F` parsers
/// emit. Equality and ordering are auto-generated. No member
/// functions, no constructors, no invariants beyond what the
/// member types themselves enforce.

namespace lazytmux::tmux {

/// @brief A tmux session as reported by `tmux list-sessions`.
struct Session {
    SessionId id;
    SessionName name;
    std::uint32_t windows;
    bool attached;
    std::int64_t last_activity_unix;

    auto operator<=>(const Session&) const = default;
    bool operator==(const Session&) const = default;
};

/// @brief A tmux window as reported by `tmux list-windows`.
struct Window {
    SessionName session;
    std::uint32_t index;
    WindowId id;
    std::string name;
    bool active;
    std::uint32_t panes;

    auto operator<=>(const Window&) const = default;
    bool operator==(const Window&) const = default;
};

/// @brief A tmux pane as reported by `tmux list-panes`.
struct Pane {
    WindowId window_id;
    std::uint32_t index;
    PaneId id;
    std::string command;
    std::string current_path;
    bool active;

    auto operator<=>(const Pane&) const = default;
    bool operator==(const Pane&) const = default;
};

/// @brief A tmux client as reported by `tmux list-clients`.
struct Client {
    std::uint32_t pid;
    SessionName session;
    ClientTty tty;
    std::string term;

    auto operator<=>(const Client&) const = default;
    bool operator==(const Client&) const = default;
};

/// @brief Tmux server-level metadata reported by
/// `tmux display-message -p`.
struct ServerInfo {
    std::uint32_t server_pid;
    std::uint32_t session_count;
    std::int64_t start_time_unix;

    auto operator<=>(const ServerInfo&) const = default;
    bool operator==(const ServerInfo&) const = default;
};

}  // namespace lazytmux::tmux
