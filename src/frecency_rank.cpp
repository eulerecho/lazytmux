#include <lazytmux/error.hpp>
#include <lazytmux/frecency_rank.hpp>
#include <lazytmux/frecency_store.hpp>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <string>
#include <sys/file.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

namespace lazytmux::frecency {
namespace {

std::string errno_message(std::string_view action) {
    return std::format("{}: {}", action, std::strerror(errno));
}

std::filesystem::path lock_path_for(const std::filesystem::path& store_path) {
    auto lock_path = store_path;
    lock_path += ".lock";
    return lock_path;
}

Result<void> ensure_parent_dir(const std::filesystem::path& path) {
    const auto parent = path.parent_path();
    if (parent.empty()) {
        return {};
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        return std::unexpected(Error{
            ErrorKind::kIo,
            std::format("failed to create {}: {}", parent.string(), ec.message()),
        });
    }
    return {};
}

class StoreLock {
public:
    [[nodiscard]] static Result<StoreLock> acquire(const std::filesystem::path& store_path) {
        const auto lock_path = lock_path_for(store_path);
        if (auto ensured = ensure_parent_dir(lock_path); !ensured) {
            return std::unexpected(std::move(ensured).error());
        }

        const int fd = ::open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (fd == -1) {
            return std::unexpected(Error{
                ErrorKind::kIo,
                errno_message(std::format("failed to open {}", lock_path.string())),
            });
        }

        if (::flock(fd, LOCK_EX | LOCK_NB) == -1) {
            Error err{
                ErrorKind::kIo,
                errno_message(std::format("failed to lock {}", lock_path.string())),
            };
            ::close(fd);
            return std::unexpected(std::move(err));
        }

        return StoreLock{fd};
    }

    StoreLock(const StoreLock&) = delete;
    StoreLock& operator=(const StoreLock&) = delete;

    StoreLock(StoreLock&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    StoreLock& operator=(StoreLock&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    ~StoreLock() {
        close();
    }

private:
    explicit StoreLock(int fd) : fd_(fd) {}

    void close() noexcept {
        if (fd_ != -1) {
            ::flock(fd_, LOCK_UN);
            ::close(fd_);
            fd_ = -1;
        }
    }

    int fd_{-1};
};

}  // namespace

std::vector<RankedSession> rank_sessions(const Index& index,
                                         const std::vector<tmux::Session>& sessions,
                                         std::int64_t now) {
    std::vector<RankedSession> ranked;
    ranked.reserve(sessions.size());
    for (const auto& session : sessions) {
        ranked.push_back(RankedSession{
            .session = session,
            .score = index.score(session.name, now),
        });
    }

    std::stable_sort(
        ranked.begin(), ranked.end(), [](const RankedSession& lhs, const RankedSession& rhs) {
            return lhs.score > rhs.score;
        });
    return ranked;
}

Result<std::vector<RankedSession>> load_ranked_sessions(const std::filesystem::path& store_path,
                                                        const std::vector<tmux::Session>& sessions,
                                                        std::int64_t now) {
    auto index = load_index(store_path);
    if (!index) {
        auto err = std::move(index).error();
        err.with_context("load frecency store");
        return std::unexpected(std::move(err));
    }
    return rank_sessions(*index, sessions, now);
}

Result<void> record_session_visit(const std::filesystem::path& store_path,
                                  const tmux::SessionName& name,
                                  std::int64_t now) {
    auto lock = StoreLock::acquire(store_path);
    if (!lock) {
        auto err = std::move(lock).error();
        err.with_context("lock frecency store");
        return std::unexpected(std::move(err));
    }

    auto index = load_index(store_path);
    if (!index) {
        auto err = std::move(index).error();
        err.with_context("load frecency store");
        return std::unexpected(std::move(err));
    }

    index->visit(name, now);

    auto saved = save_index(*index, store_path);
    if (!saved) {
        auto err = std::move(saved).error();
        err.with_context("save frecency store");
        return std::unexpected(std::move(err));
    }
    return {};
}

}  // namespace lazytmux::frecency
