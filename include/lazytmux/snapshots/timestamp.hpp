#pragma once

#include <lazytmux/error.hpp>

#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

/// @file
/// @brief Parser for tmux-resurrect snapshot filenames of the
/// form `tmux_resurrect_YYYYMMDDTHHMMSS.txt`.

namespace lazytmux::snapshots {

/// @brief A discrete-field timestamp extracted from a
/// tmux-resurrect snapshot filename.
///
/// Validation is by numeric shape only. A field that fails to
/// parse as a decimal integer is rejected; a field that parses
/// but is out of Gregorian range (e.g. month=13, day=32) is
/// accepted. Tightening the semantic check is deferred to a
/// later phase under audit-finding-mirror review.
struct Timestamp {
    std::uint16_t year;
    std::uint8_t month;
    std::uint8_t day;
    std::uint8_t hour;
    std::uint8_t minute;
    std::uint8_t second;

    auto operator<=>(const Timestamp&) const = default;
    bool operator==(const Timestamp&) const = default;

    /// Render as `"YYYY-MM-DD HH:MM:SS"`, zero-padded, ASCII.
    [[nodiscard]] std::string display() const;
};

/// @brief Parse a tmux-resurrect snapshot filename into a
/// @ref Timestamp.
/// @param filename E.g. `"tmux_resurrect_20260415T004643.txt"`.
/// @return Parsed timestamp on success.
/// @return @ref Error on missing prefix/suffix, missing `T`
///         separator, wrong segment width, or non-numeric field.
[[nodiscard]] Result<Timestamp> parse_snapshot_filename(std::string_view filename);

}  // namespace lazytmux::snapshots
