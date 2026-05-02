#pragma once

#include <lazytmux/error.hpp>
#include <lazytmux/frecency.hpp>
#include <lazytmux/tmux/ids.hpp>
#include <lazytmux/tmux/types.hpp>

#include <compare>
#include <cstdint>
#include <filesystem>
#include <vector>

/// @file
/// @brief Session ranking and visit-recording helpers over the
/// frecency index and JSON store.

namespace lazytmux::frecency {

/// @brief A tmux session plus its computed frecency score.
struct RankedSession {
    tmux::Session session;
    double score{};

    auto operator<=>(const RankedSession&) const = default;
    bool operator==(const RankedSession&) const = default;
};

/// @brief Rank live tmux sessions using an already-loaded index.
/// @param index Frecency index to score against.
/// @param sessions Sessions in the tmux order callers want to
///        preserve for score ties.
/// @param now Current Unix time in seconds.
/// @return Sessions sorted by descending score. Ties preserve the
///         input tmux order.
[[nodiscard]] std::vector<RankedSession> rank_sessions(const Index& index,
                                                       const std::vector<tmux::Session>& sessions,
                                                       std::int64_t now);

/// @brief Load the persisted index and rank live tmux sessions.
/// @param store_path Path to the frecency JSON store.
/// @param sessions Sessions in the tmux order callers want to
///        preserve for score ties.
/// @param now Current Unix time in seconds.
/// @return Ranked sessions, or an error if the store cannot be
///         loaded.
[[nodiscard]] Result<std::vector<RankedSession>> load_ranked_sessions(
    const std::filesystem::path& store_path,
    const std::vector<tmux::Session>& sessions,
    std::int64_t now);

/// @brief Persist a visit to one session.
///
/// Acquires an advisory lock at `<store_path>.lock`, loads the
/// current index, records the visit, and saves the updated index
/// while still holding the lock.
///
/// @param store_path Path to the frecency JSON store.
/// @param name Session to mark as visited.
/// @param now Visit Unix time in seconds.
/// @return Empty success after the updated index is saved.
/// @return @ref Error if the lock, load, or save operation fails.
[[nodiscard]] Result<void> record_session_visit(const std::filesystem::path& store_path,
                                                const tmux::SessionName& name,
                                                std::int64_t now);

}  // namespace lazytmux::frecency
