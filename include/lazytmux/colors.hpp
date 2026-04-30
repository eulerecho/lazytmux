#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

/// @file
/// @brief 24-bit RGB triple and the hex color string parser.

namespace lazytmux {

/// @brief 24-bit RGB triple. Aggregate; designated-initialize.
struct Rgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;

    auto operator<=>(const Rgb&) const = default;
    bool operator==(const Rgb&) const = default;
};

/// @brief Parse a hex color string (`"#rrggbb"` or `"rrggbb"`)
/// into an @ref Rgb.
///
/// @param s The string to parse. Leading `#` is optional. Both
///          lowercase and uppercase hex digits are accepted.
/// @return The parsed @ref Rgb on success.
/// @return @c std::nullopt on length mismatch (anything other
///         than 6 hex digits after the optional `#`), non-ASCII
///         bytes, or non-hex ASCII characters.
///
/// @note The non-ASCII guard rejects any byte with the
///       high bit set before slicing. Without this check, a
///       6-byte UTF-8 input whose codepoint boundaries sit
///       inside the 2-byte field windows would yield an
///       integer parse over a partial codepoint.
[[nodiscard]] std::optional<Rgb> parse_hex(std::string_view s);

}  // namespace lazytmux
