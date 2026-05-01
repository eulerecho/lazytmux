#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>

/// @file
/// @brief Pure-logic walker for counting tmux ancestors of a
/// process plus the `PPid:` field extractor for
/// `/proc/<pid>/status` bodies.
///
/// The /proc reading itself is not in this module; callers
/// inject a closure that maps a PID to its
/// `(parent_pid, is_tmux)` pair.

namespace lazytmux::nesting {

/// @brief Maximum number of ancestors the walker will visit
/// before giving up. Bounds runtime when the injected closure
/// keeps producing fresh ancestors.
inline constexpr std::uint32_t kMaxWalk = 32;

/// @brief Closure type accepted by @ref count_tmux_in_walk.
///
/// Maps a PID to the pair `(parent_pid, is_tmux)`, or
/// `std::nullopt` when the chain breaks (PID not found, /proc
/// read error, ...).
using Walker = std::function<std::optional<std::pair<std::uint32_t, bool>>(std::uint32_t)>;

/// @brief Walk the parent-PID chain starting at @p start_pid,
/// counting ancestors that the walker reports as tmux.
///
/// @param start_pid The PID to begin at. Counted itself if the
///        walker returns `(_, true)` for it.
/// @param walker Closure mapping PID to
///        `(parent_pid, is_tmux)` or `std::nullopt`. The /proc
///        reader is the production walker; tests inject a
///        hand-built map.
/// @return Number of tmux ancestors (including @p start_pid if
///         it itself is tmux), bounded by @ref kMaxWalk.
///
/// Termination conditions:
///   1. Walker yields `std::nullopt` (chain broken).
///   2. `parent_pid == 0` (init reached).
///   3. `parent_pid == pid` (self-loop).
///   4. `pid` already in the visited set (longer cycle).
///   5. @ref kMaxWalk iterations completed.
[[nodiscard]] std::uint32_t count_tmux_in_walk(std::uint32_t start_pid, const Walker& walker);

/// @brief Extract the `PPid:` field out of the body of
/// `/proc/<pid>/status`.
///
/// @param status_body The full text of `/proc/<pid>/status`.
/// @return The parent PID on success.
/// @return `std::nullopt` if the line is missing or the value
///         doesn't parse as a 32-bit unsigned integer.
///
/// @note Accepts either tab or space as the separator after
///       `PPid:`. Linux's documented format uses tab; in
///       practice both have appeared.
[[nodiscard]] std::optional<std::uint32_t> parse_ppid(std::string_view status_body);

}  // namespace lazytmux::nesting
