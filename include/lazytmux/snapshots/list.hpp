#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/snapshots/timestamp.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// @file
/// @brief tmux-resurrect snapshot file parser and listing helper.

namespace lazytmux::snapshots {

/// @brief Parsed tmux-resurrect snapshot summary.
struct Snapshot {
    /// @brief Full path to the snapshot file.
    std::filesystem::path path;
    /// @brief Timestamp parsed from the filename.
    Timestamp timestamp;
    /// @brief Number of distinct session names found.
    std::uint32_t session_count{};
    /// @brief Number of window rows found.
    std::uint32_t window_count{};
    /// @brief Sorted, deduplicated session names.
    std::vector<std::string> session_names;
    /// @brief `(session_name, window_name)` rows in file order.
    std::vector<std::pair<std::string, std::string>> windows;

    auto operator<=>(const Snapshot&) const = default;
    bool operator==(const Snapshot&) const = default;
};

/// @brief Resolve the tmux-resurrect save directory from
/// XDG-style environment values.
/// @param xdg_data_home Optional `$XDG_DATA_HOME` value.
/// @param home Optional `$HOME` value.
/// @return `$XDG_DATA_HOME/tmux/resurrect` when non-empty,
///         otherwise `$HOME/.local/share/tmux/resurrect`.
/// @return `std::nullopt` when neither value is usable.
[[nodiscard]] std::optional<std::filesystem::path> resolve_resurrect_dir(
    std::optional<std::string_view> xdg_data_home, std::optional<std::string_view> home);

/// @brief Resolve the tmux-resurrect save directory from the
/// process environment.
/// @return Directory path, or `std::nullopt` when no home-like
///         value is available.
[[nodiscard]] std::optional<std::filesystem::path> default_resurrect_dir();

/// @brief Parse one tmux-resurrect save file body.
/// @param path Snapshot file path. Filename must be a
///        `tmux_resurrect_YYYYMMDDTHHMMSS.txt` name.
/// @param text File body.
/// @return Parsed snapshot summary.
/// @return @ref Error when the filename timestamp is malformed.
[[nodiscard]] Result<Snapshot> parse_snapshot_text(std::filesystem::path path,
                                                   std::string_view text);

/// @brief List snapshots in a directory, newest first.
/// @param dir Directory to scan.
/// @return Parsed snapshot summaries. Missing directories return
///         an empty vector. Malformed snapshot files are skipped.
/// @return @ref Error when a present directory cannot be
///         iterated.
[[nodiscard]] Result<std::vector<Snapshot>> list_snapshots_in(const std::filesystem::path& dir);

/// @brief List snapshots from @ref default_resurrect_dir.
/// @return Parsed snapshot summaries, or an empty vector when no
///         save directory can be resolved.
[[nodiscard]] Result<std::vector<Snapshot>> list_snapshots();

}  // namespace lazytmux::snapshots
