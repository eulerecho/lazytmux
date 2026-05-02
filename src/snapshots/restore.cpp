#include <lazytmux/io/process.hpp>
#include <lazytmux/snapshots/restore.hpp>

#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lazytmux::snapshots {
namespace {

std::vector<std::filesystem::path> default_script_candidates() {
    const char* home = std::getenv("HOME");
    if (home == nullptr || std::string_view{home}.empty()) {
        return {};
    }
    const auto base = std::filesystem::path{home};
    return {
        base / ".tmux" / "plugins" / "tmux-resurrect" / "scripts" / "restore.sh",
        base / ".local" / "share" / "tmux" / "plugins" / "tmux-resurrect" / "scripts" /
            "restore.sh",
    };
}

std::vector<std::filesystem::path> candidates_for(const RestoreConfig& config) {
    if (!config.script_candidates.empty()) {
        return config.script_candidates;
    }
    return default_script_candidates();
}

}  // namespace

Result<std::filesystem::path> locate_restore_script(const RestoreConfig& config) {
    std::error_code ec;
    for (const auto& path : candidates_for(config)) {
        if (std::filesystem::is_regular_file(path, ec)) {
            return path;
        }
        ec.clear();
    }
    return std::unexpected(Error("tmux-resurrect restore script not found"));
}

Result<void> restore_snapshot(const std::filesystem::path& snapshot_path,
                              const RestoreConfig& config) {
    const auto dir = snapshot_path.parent_path();
    if (dir.empty()) {
        return std::unexpected(Error("snapshot path has no parent directory"));
    }

    std::error_code exists_ec;
    if (!std::filesystem::exists(snapshot_path, exists_ec)) {
        if (exists_ec) {
            return std::unexpected(Error(std::format(
                "failed to inspect {}: {}", snapshot_path.string(), exists_ec.message())));
        }
        return std::unexpected(
            Error(std::format("snapshot file not found: {}", snapshot_path.string())));
    }

    std::error_code status_ec;
    if (!std::filesystem::is_regular_file(snapshot_path, status_ec)) {
        if (status_ec) {
            return std::unexpected(Error(std::format(
                "failed to inspect {}: {}", snapshot_path.string(), status_ec.message())));
        }
        return std::unexpected(
            Error(std::format("snapshot path is not a file: {}", snapshot_path.string())));
    }

    auto script = locate_restore_script(config);
    if (!script) {
        return std::unexpected(std::move(script).error());
    }

    const auto last = dir / "last";
    std::error_code remove_ec;
    std::filesystem::remove(last, remove_ec);
    if (remove_ec) {
        return std::unexpected(
            Error(std::format("failed to remove {}: {}", last.string(), remove_ec.message())));
    }

    std::error_code link_ec;
    std::filesystem::create_symlink(snapshot_path, last, link_ec);
    if (link_ec) {
        return std::unexpected(Error(std::format("failed to link {} to {}: {}",
                                                 last.string(),
                                                 snapshot_path.string(),
                                                 link_ec.message())));
    }

    const std::vector<std::string> argv{config.shell, script->string()};
    auto result = io::run_command(argv);
    if (!result) {
        return std::unexpected(std::move(result).error());
    }
    if (result->exit_code != 0) {
        return std::unexpected(Error(std::format(
            "restore script exited with code {}: {}", result->exit_code, result->stderr_text)));
    }
    return {};
}

RestoreWorker::RestoreWorker(std::filesystem::path snapshot_path, RestoreConfig config) {
    std::promise<Result<void>> promise;
    result_ = promise.get_future();
    thread_ = std::jthread([path = std::move(snapshot_path),
                            config = std::move(config),
                            promise = std::move(promise)](std::stop_token stop) mutable {
        if (stop.stop_requested()) {
            promise.set_value(std::unexpected(Error("restore worker stopped before start")));
            return;
        }
        promise.set_value(restore_snapshot(path, config));
    });
}

void RestoreWorker::request_stop() {
    thread_.request_stop();
}

bool RestoreWorker::ready() {
    if (!result_) {
        return false;
    }
    return result_->wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}

std::optional<Result<void>> RestoreWorker::try_take() {
    if (!ready()) {
        return std::nullopt;
    }
    auto result = result_->get();
    result_.reset();
    return result;
}

RestoreWorker restore_async(std::filesystem::path snapshot_path, RestoreConfig config) {
    return RestoreWorker{std::move(snapshot_path), std::move(config)};
}

}  // namespace lazytmux::snapshots
