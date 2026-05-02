#include <lazytmux/error.hpp>
#include <lazytmux/snapshots/ops.hpp>

#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace lazytmux::snapshots {
namespace {

Result<std::optional<std::filesystem::path>> read_last_target(const std::filesystem::path& last) {
    std::error_code status_ec;
    const auto status = std::filesystem::symlink_status(last, status_ec);
    if (status.type() == std::filesystem::file_type::not_found) {
        return std::nullopt;
    }
    if (status_ec) {
        return std::unexpected(Error{
            ErrorKind::kIo,
            std::format("failed to inspect {}: {}", last.string(), status_ec.message()),
        });
    }
    if (!std::filesystem::is_symlink(status)) {
        return std::unexpected(Error{
            ErrorKind::kInvalidInput,
            std::format("{} exists but is not a symlink", last.string()),
        });
    }

    std::error_code read_ec;
    auto target = std::filesystem::read_symlink(last, read_ec);
    if (read_ec) {
        return std::unexpected(Error{
            ErrorKind::kIo,
            std::format("failed to read {}: {}", last.string(), read_ec.message()),
        });
    }
    return target;
}

std::filesystem::path resolve_last_target(const std::filesystem::path& last,
                                          const std::filesystem::path& target) {
    if (target.is_absolute()) {
        return target.lexically_normal();
    }
    return (last.parent_path() / target).lexically_normal();
}

bool same_path(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
    if (lhs.lexically_normal() == rhs.lexically_normal()) {
        return true;
    }

    std::error_code lhs_ec;
    std::error_code rhs_ec;
    const auto lhs_canonical = std::filesystem::weakly_canonical(lhs, lhs_ec);
    const auto rhs_canonical = std::filesystem::weakly_canonical(rhs, rhs_ec);
    return !lhs_ec && !rhs_ec && lhs_canonical == rhs_canonical;
}

bool last_points_to_snapshot(const std::filesystem::path& last,
                             const std::filesystem::path& target,
                             const std::filesystem::path& snapshot_path) {
    if (target == snapshot_path) {
        return true;
    }
    return same_path(resolve_last_target(last, target), snapshot_path);
}

Result<void> remove_path(const std::filesystem::path& path) {
    std::error_code remove_ec;
    std::filesystem::remove(path, remove_ec);
    if (remove_ec) {
        return std::unexpected(Error{
            ErrorKind::kIo,
            std::format("failed to remove {}: {}", path.string(), remove_ec.message()),
        });
    }
    return {};
}

std::string plural(std::uint32_t count, std::string_view singular, std::string_view plural_text) {
    return std::format("{} {}", count, count == 1 ? singular : plural_text);
}

}  // namespace

Result<void> delete_snapshot(const std::filesystem::path& snapshot_path) {
    if (snapshot_path.filename() == "last") {
        return std::unexpected(Error{
            ErrorKind::kInvalidInput,
            "refusing to delete tmux-resurrect last symlink directly",
        });
    }

    const auto last = snapshot_path.parent_path() / "last";
    auto target = read_last_target(last);
    if (!target) {
        return std::unexpected(std::move(target).error());
    }

    const bool remove_last = *target && last_points_to_snapshot(last, **target, snapshot_path);

    if (auto removed = remove_path(snapshot_path); !removed) {
        return removed;
    }
    if (remove_last) {
        return remove_path(last);
    }
    return {};
}

RestoreWorker restore_snapshot_async(std::filesystem::path snapshot_path, RestoreConfig config) {
    return restore_async(std::move(snapshot_path), std::move(config));
}

std::string snapshot_label(const Snapshot& snapshot) {
    return std::format("{} | {} | {}",
                       snapshot.timestamp.display(),
                       plural(snapshot.session_count, "session", "sessions"),
                       plural(snapshot.window_count, "window", "windows"));
}

}  // namespace lazytmux::snapshots
