#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/snapshots/list.hpp>
#include <lazytmux/snapshots/restore.hpp>

#include <filesystem>
#include <string>

/// @file
/// @brief Higher-level snapshot operations used by CLI/TUI actions.

namespace lazytmux::snapshots {

/// @brief Delete one tmux-resurrect snapshot file.
///
/// Missing snapshots are treated as success. Deleting the special
/// `last` symlink directly is rejected; callers should delete the
/// concrete snapshot file instead. When the `last` symlink points
/// at the deleted snapshot, it is removed too.
///
/// @param snapshot_path Snapshot file to delete.
/// @return Empty success after the snapshot and matching `last`
///         symlink are removed.
/// @return @ref Error on unsafe `last` deletion, malformed `last`
///         state, or filesystem failure.
[[nodiscard]] Result<void> delete_snapshot(const std::filesystem::path& snapshot_path);

/// @brief Restore one snapshot in the background.
/// @param snapshot_path Snapshot file to restore.
/// @param config Restore configuration.
/// @return Restore worker object. Destroying it joins the worker.
[[nodiscard]] RestoreWorker restore_snapshot_async(std::filesystem::path snapshot_path,
                                                   RestoreConfig config = {});

/// @brief Render a compact, human-readable snapshot label.
/// @param snapshot Parsed snapshot summary.
/// @return Label containing timestamp, session count, and window
///         count.
[[nodiscard]] std::string snapshot_label(const Snapshot& snapshot);

}  // namespace lazytmux::snapshots
