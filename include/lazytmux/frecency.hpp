#pragma once

#include <lazytmux/tmux/ids.hpp>

#include <compare>
#include <cstdint>
#include <map>

/// @file
/// @brief In-memory frecency index keyed by tmux session name,
/// with a 7-day-half-life exponential-decay score function.
///
/// Persistence (load / save) is not in this module; that lives
/// alongside the rest of the I/O layer.

namespace lazytmux::frecency {

/// @brief One row of the frecency table.
///
/// Aggregate POD so it round-trips through serialization without
/// conversion. `visits` is the cumulative visit count, capped
/// at @c std::numeric_limits<std::uint32_t>::max() via saturating
/// add. `last_used` is the most recent visit timestamp, in
/// Unix-epoch seconds.
struct Entry {
    std::uint32_t visits{};
    std::int64_t last_used{};

    auto operator<=>(const Entry&) const = default;
    bool operator==(const Entry&) const = default;
};

/// @brief Half-life used by @ref Index::score, in seconds. Seven
/// days.
inline constexpr double kHalfLifeSecs = 7.0 * 86400.0;

/// @brief Ordered map of session name to @ref Entry, plus the
/// score function that combines visit count with exponential
/// decay.
class Index {
public:
    /// @brief Record a visit to @p name at time @p now.
    ///
    /// Increments `visits` (saturating at u32::max) and sets
    /// `last_used = now`. Inserts a fresh @ref Entry if @p name
    /// is missing from the index.
    ///
    /// @param name The session that was visited.
    /// @param now The current Unix time in seconds.
    void visit(const tmux::SessionName& name, std::int64_t now);

    /// @brief Frecency score of @p name at time @p now.
    ///
    /// Formula: `visits * exp(-ln(2) / kHalfLifeSecs * age)`
    /// where `age = max(0, now - last_used)`. Future-`now`
    /// (clock skew) is clamped to age 0; the exponent is never
    /// positive. Unknown name returns 0.0; zero-visit entry
    /// returns 0.0.
    ///
    /// @param name The session to score.
    /// @param now The current Unix time in seconds.
    /// @return The score, with higher values indicating more
    ///         recent or more frequent visits.
    [[nodiscard]] double score(const tmux::SessionName& name, std::int64_t now) const;

    /// @brief Read access to the underlying map. Used by the
    /// persistence layer and by tests.
    [[nodiscard]] const std::map<tmux::SessionName, Entry>& entries() const noexcept;

private:
    std::map<tmux::SessionName, Entry> entries_;
};

/// @brief Return the current Unix time, in seconds.
///
/// Thin wrapper over `std::chrono::system_clock::now()`.
/// Provided so callers outside the test suite can pass a real
/// timestamp into @ref Index::visit / @ref Index::score; never
/// invoked from inside the @ref Index methods, so all logic
/// remains deterministic under tests.
[[nodiscard]] std::int64_t unix_now();

}  // namespace lazytmux::frecency
