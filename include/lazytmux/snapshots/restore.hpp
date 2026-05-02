#pragma once

#include <lazytmux/error.hpp>

#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <vector>

/// @file
/// @brief tmux-resurrect snapshot restore helper and worker.

namespace lazytmux::snapshots {

/// @brief Configuration for restore script execution.
struct RestoreConfig {
    /// @brief Shell executable used to run the restore script.
    std::string shell{"/bin/bash"};

    /// @brief Candidate restore script paths, in priority order.
    std::vector<std::filesystem::path> script_candidates;
};

/// @brief Locate the tmux-resurrect restore script.
/// @param config Restore configuration.
/// @return First existing script candidate.
/// @return @ref Error when no candidate exists.
[[nodiscard]] Result<std::filesystem::path> locate_restore_script(const RestoreConfig& config = {});

/// @brief Restore one snapshot synchronously.
/// @param snapshot_path Snapshot file to restore.
/// @param config Restore configuration.
/// @return Empty success when the `last` symlink is updated and
///         the restore script exits with code 0.
/// @return @ref Error on missing parent directory, symlink
///         update failure, missing restore script, spawn failure,
///         or non-zero restore script exit.
[[nodiscard]] Result<void> restore_snapshot(const std::filesystem::path& snapshot_path,
                                            const RestoreConfig& config = {});

/// @brief Background restore worker.
class RestoreWorker {
public:
    /// @brief Start a restore worker.
    /// @param snapshot_path Snapshot file to restore.
    /// @param config Restore configuration.
    RestoreWorker(std::filesystem::path snapshot_path, RestoreConfig config = {});

    RestoreWorker(const RestoreWorker&) = delete;
    RestoreWorker& operator=(const RestoreWorker&) = delete;
    RestoreWorker(RestoreWorker&&) noexcept = default;
    RestoreWorker& operator=(RestoreWorker&&) noexcept = default;

    /// @brief Request stop on the worker thread.
    void request_stop();

    /// @brief Check whether the restore result is ready.
    /// @return `true` when @ref try_take can return a result.
    [[nodiscard]] bool ready();

    /// @brief Take the restore result if ready.
    /// @return Result when the worker has completed.
    /// @return `std::nullopt` while the worker is still running
    ///         or after the result was already taken.
    [[nodiscard]] std::optional<Result<void>> try_take();

private:
    std::jthread thread_;
    std::optional<std::future<Result<void>>> result_;
};

/// @brief Start a background restore worker.
/// @param snapshot_path Snapshot file to restore.
/// @param config Restore configuration.
/// @return Restore worker object. Destroying it joins the worker.
[[nodiscard]] RestoreWorker restore_async(std::filesystem::path snapshot_path,
                                          RestoreConfig config = {});

}  // namespace lazytmux::snapshots
