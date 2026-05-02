#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/nesting.hpp>

#include <cstdint>
#include <filesystem>

/// @file
/// @brief Readers for the small `/proc` surface needed by
/// nesting detection.

namespace lazytmux::proc {

/// @brief One process row as needed by the nesting walker.
struct ProcessInfo {
    /// @brief Parent process id from `/proc/<pid>/status`.
    std::uint32_t parent_pid{};

    /// @brief Whether `/proc/<pid>/exe` resolves to a `tmux`
    /// executable.
    bool is_tmux{};
};

/// @brief Read parent PID and tmux-executable status for a
/// process.
/// @param pid Process id to inspect.
/// @param proc_root Directory that contains PID subdirectories,
///        normally `/proc`.
/// @return The process row on success.
/// @return @ref Error on missing files, malformed `PPid`, or
///         unreadable executable symlink.
[[nodiscard]] Result<ProcessInfo> read_process_info(
    std::uint32_t pid, const std::filesystem::path& proc_root = "/proc");

/// @brief Build a @ref nesting::Walker backed by
/// @ref read_process_info.
/// @param proc_root Directory that contains PID subdirectories,
///        normally `/proc`.
/// @return A walker that returns `std::nullopt` when the process
///         row cannot be read.
[[nodiscard]] nesting::Walker make_proc_walker(std::filesystem::path proc_root = "/proc");

}  // namespace lazytmux::proc
