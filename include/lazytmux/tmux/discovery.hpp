#pragma once

#include <lazytmux/error.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/// @file
/// @brief Filesystem helpers for tmux socket discovery.

namespace lazytmux::tmux {

/// @brief One discovered tmux socket.
struct SocketInfo {
    /// @brief Full path to the socket entry.
    std::filesystem::path path;

    /// @brief Basename of @ref path.
    std::string name;
};

/// @brief List Unix-socket entries in a directory.
/// @param dir Directory to scan.
/// @return Sorted socket entries on success.
/// @return @ref Error when the directory cannot be read or an
///         entry cannot be inspected.
[[nodiscard]] Result<std::vector<SocketInfo>> list_sockets_in(const std::filesystem::path& dir);

/// @brief Format the default tmux socket path for `-L <name>`.
/// @param uid Numeric user id used in `/tmp/tmux-<uid>`.
/// @param name Socket name.
/// @return `/tmp/tmux-<uid>/<name>`.
[[nodiscard]] std::filesystem::path socket_path_for_name(std::uint32_t uid, std::string_view name);

}  // namespace lazytmux::tmux
